#include <stdint.h>

#define CLOUD_MESSAGE_TYPE_GREETING 1
#define CLOUD_MESSAGE_TYPE_OPEN 2
#define CLOUD_MESSAGE_TYPE_UNLOCK 3
#define CLOUD_MESSAGE_TYPE_LOCK 4
#define CLOUD_MESSAGE_TYPE_CONFIG 5
#define CLOUD_MESSAGE_TYPE_CARD_SUCCESS 6
#define CLOUD_MESSAGE_TYPE_CARD_FAILURE 7

#define MESSAGE_DATA_SIZE 1024

#define CONFIG_MEMORY ((uint8_t *)0x08020000)

struct __attribute__((packed))  cloud_message_short_t {
  uint8_t device_id[8];
  uint8_t packet_type;
  uint32_t counter;
  uint8_t rng[32];
  uint8_t hash_header[32];
};
struct __attribute__((packed))  cloud_message_full_t {
  uint8_t device_id[8];
  uint8_t packet_type;
  uint32_t counter;
  uint8_t rng[32];
  uint8_t hash_header[32];
  uint8_t iv[16];
  uint8_t data[MESSAGE_DATA_SIZE];
  uint8_t hash_full[32];
};

void cloud_init();
void cloud_send_greeting();
void cloud_rx(struct cloud_message_full_t* message);
uint8_t cloud_check_valid_short(struct cloud_message_short_t* message);
uint8_t cloud_check_valid_full(struct cloud_message_full_t* message);
void cloud_write_config(uint8_t * config);
uint8_t cloud_check_card(uint8_t * number, uint8_t length);
void cloud_send_data(uint8_t message_type, uint8_t * data);
