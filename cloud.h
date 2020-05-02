#include <stdint.h>

#define CLOUD_MESSAGE_TYPE_GREETING 1

struct __attribute__((packed))  cloud_message_t {
  char device_id[8];
  uint64_t rng;
  uint32_t counter;
  uint32_t packet_type;
  uint8_t data[1024];
};
