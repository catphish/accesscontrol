#include "cloud.h"
#include <string.h>
#include "board.h"
#include "ethernet.h"
#include "door.h"
#include "sha256.h"
#include "aes.h"

uint8_t device_id[] = { 0, 0, 0, 0, 0, 0, 0, 1 };
uint8_t psk[] = { 127, 96, 69, 7, 254, 254, 216, 148, 191, 158, 1, 127, 10, 39, 30, 35, 180, 128, 12, 20, 63, 148, 101, 34, 35, 151, 228, 172, 225, 185, 235, 167 };
uint8_t local_rng[32];
uint8_t remote_rng[32];
uint32_t local_counter;
uint32_t remote_counter;

void cloud_init() {
  // Populate the RNG with 32 byes of data (8 x 32 bit RNG outputs)
  for(int n=0; n<8; n++) {
    while(!(RNG->SR & 1));
    ((uint32_t*)local_rng)[n] = RNG->DR;
  }
  local_counter = 1;
}

void cloud_send_greeting() {
  struct cloud_message_short_t message;
  memset(&message, 0, sizeof(message));
  message.packet_type = CLOUD_MESSAGE_TYPE_GREETING;
  memcpy(message.device_id, device_id, 8);
  memcpy(message.rng, local_rng, 32);
  message.counter = HTONL(local_counter);
  ethernet_udp_tx(server_ip_address, 42424, 42424, (void*)&message, sizeof(message));
}

void cloud_send_data(uint8_t message_type, uint8_t * data) {
  struct cloud_message_full_t message;
  memset(&message, 0, sizeof(message));
  message.packet_type = message_type;
  memcpy(message.device_id, device_id, 8);
  memcpy(message.rng, remote_rng, 32);
  remote_counter++;
  message.counter = HTONL(remote_counter);
  memcpy(message.data, data, MESSAGE_DATA_SIZE);

  for(int n=0; n<4; n++) {
    while(!(RNG->SR & 1));
    ((uint32_t*)message.iv)[n] = RNG->DR;
  }

  struct AES_ctx ctx;
  AES_init_ctx_iv(&ctx, psk, message.iv);
  AES_CBC_encrypt_buffer(&ctx, message.data, MESSAGE_DATA_SIZE);

  SHA256_CTX hash_ctx;

  sha256_init(&hash_ctx);
  sha256_update(&hash_ctx, psk, 32);
  sha256_update(&hash_ctx, (void*)&message, sizeof(struct cloud_message_full_t) - 32);
  sha256_final(&hash_ctx, message.hash_full);

  sha256_init(&hash_ctx);
  sha256_update(&hash_ctx, psk, 32);
  sha256_update(&hash_ctx, message.hash_full, 32);
  sha256_final(&hash_ctx, message.hash_full);

  ethernet_udp_tx(server_ip_address, 42424, 42424, (void*)&message, sizeof(message));
}

uint8_t cloud_check_valid_short(struct cloud_message_short_t* message) {
  SHA256_CTX hash_ctx;
  uint8_t hash[32];
  sha256_init(&hash_ctx);
  sha256_update(&hash_ctx, psk, 32);
  sha256_update(&hash_ctx, (void*)message, sizeof(struct cloud_message_short_t) - 32);
  sha256_final(&hash_ctx, hash);

  sha256_init(&hash_ctx);
  sha256_update(&hash_ctx, psk, 32);
  sha256_update(&hash_ctx, hash, 32);
  sha256_final(&hash_ctx, hash);
  if(NTOHL(message->counter) > local_counter && !memcmp(message->rng, local_rng, 32) && !memcmp(hash, message->hash_header, 32)) {
    local_counter = NTOHL(message->counter);
    return 1;
  }
  return 0;
}

uint8_t cloud_check_valid_full(struct cloud_message_full_t* message) {
  SHA256_CTX hash_ctx;
  uint8_t hash[32];
  sha256_init(&hash_ctx);
  sha256_update(&hash_ctx, psk, 32);
  sha256_update(&hash_ctx, (void*)message, sizeof(struct cloud_message_full_t) - 32);
  sha256_final(&hash_ctx, hash);

  sha256_init(&hash_ctx);
  sha256_update(&hash_ctx, psk, 32);
  sha256_update(&hash_ctx, hash, 32);
  sha256_final(&hash_ctx, hash);
  if(NTOHL(message->counter) > local_counter && !memcmp(message->rng, local_rng, 32) && !memcmp(hash, message->hash_full, 32)) {
    local_counter = NTOHL(message->counter);
    return 1;
  }
  return 0;
}

void cloud_rx(struct cloud_message_full_t* message) {
  switch(message->packet_type) {
    case CLOUD_MESSAGE_TYPE_GREETING:
      memcpy(remote_rng, message->rng, 32);
      remote_counter = NTOHL(message->counter);
      break;
    case CLOUD_MESSAGE_TYPE_OPEN:
      if(cloud_check_valid_short((struct cloud_message_short_t*)message))
        door_open_timed(3);
      break;
    case CLOUD_MESSAGE_TYPE_CONFIG:
      if(cloud_check_valid_full(message)) {
        struct AES_ctx ctx;
        AES_init_ctx_iv(&ctx, psk, message->iv);
        AES_CBC_decrypt_buffer(&ctx, message->data, MESSAGE_DATA_SIZE);
        cloud_write_config(message->data);
      }
    break;
  }
}

void cloud_write_config(uint8_t * config) {
  while(FLASH->SR & FLASH_SR_BSY);
  FLASH->KEYR = 0x45670123; // Unlock
  FLASH->KEYR = 0xCDEF89AB; // Unlock
  while(FLASH->SR & FLASH_SR_BSY);
  FLASH->CR = (5<<3)| (1<<1); // Sector 5 Erase
  FLASH->CR |= (1<<16); // Start
  while(FLASH->SR & FLASH_SR_BSY);
  FLASH->CR = (1<<0); // Program
  for(int n=0; n<MESSAGE_DATA_SIZE; n++) {
    CONFIG_MEMORY[n] = config[n];
    while(FLASH->SR & FLASH_SR_BSY);
  }
  FLASH->CR = (1<<31); // Lock
}

uint8_t cloud_check_card(uint8_t * number, uint8_t length) {
  uint32_t offset = 0;
  while(1) {
    uint8_t this_length = CONFIG_MEMORY[offset];
    if (this_length == 0) break;
    if(offset + this_length + 1 > MESSAGE_DATA_SIZE) break;
    if(this_length == length && !memcmp(CONFIG_MEMORY + offset + 1, number, length))
      return 1;
    offset += this_length + 1;
  }
  return 0;
}