#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

struct time_t {
  uint32_t sec;      // These are units of seconds
  uint32_t frac_sec; // These are units of 100us
};

uint8_t time_passed(volatile struct time_t * t);
void time_set(volatile struct time_t * t, uint32_t sec, uint32_t msec);
int memcmp_volatile(volatile uint8_t * str1, volatile uint8_t * str2, uint32_t count);
void memset_volatile(volatile uint8_t * str, uint8_t val, uint32_t length);
