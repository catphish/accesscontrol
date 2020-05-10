#include "cloud.h"
#include <string.h>
#include "board.h"
#include "ethernet.h"
#include "door.h"
#include "sha256.h"

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

void cloud_rx(struct cloud_message_full_t* message) {
  SHA256_CTX hash_ctx;
  uint8_t hash[32];
  switch(message->packet_type) {
    case CLOUD_MESSAGE_TYPE_GREETING:
      memcpy(remote_rng, message->rng, 32);
      remote_counter = NTOHL(message->counter);
      break;
    case CLOUD_MESSAGE_TYPE_OPEN:
      sha256_init(&hash_ctx);
      sha256_update(&hash_ctx, psk, 32);
      sha256_update(&hash_ctx, (void*)message, sizeof(struct cloud_message_short_t) - 32);
      sha256_final(&hash_ctx, hash);

      sha256_init(&hash_ctx);
      sha256_update(&hash_ctx, psk, 32);
      sha256_update(&hash_ctx, hash, 32);
      sha256_final(&hash_ctx, hash);
      if(
        NTOHL(message->counter) > local_counter &&
        !memcmp(message->rng, local_rng, 32) &&
        !memcmp(hash, message->hash_header, 32)
      ) {
        local_counter = NTOHL(message->counter);
        door_open_timed(2);
      }
      break;
  }
}
