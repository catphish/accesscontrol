#include <stdint.h>

#define CLOUD_MESSAGE_TYPE_GREETING 1
#define CLOUD_MESSAGE_TYPE_OPEN 2
#define MESSAGE_DATA_SIZE 1024

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
  uint8_t iv[32];
  uint8_t data[MESSAGE_DATA_SIZE];
  uint8_t hash_full[32];
};

void cloud_init();
void cloud_send_greeting();
void cloud_rx(struct cloud_message_full_t* message);
