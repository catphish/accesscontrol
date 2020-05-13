#include "board.h"
#include "util.h"

uint8_t time_passed(volatile struct time_t * t) {
  if(TIM2->CNT < t->sec) return(0);
  if(TIM2->CNT > t->sec) return(1);
  if(TIM3->CNT > t->frac_sec) return(1);
  return(0);
}

void time_set(volatile struct time_t * t, uint32_t sec, uint32_t msec) {
  t->sec = TIM2->CNT + sec + ((TIM3->CNT + msec * 10) / 10000);
  t->frac_sec = (TIM3->CNT + msec * 10) % 10000;
}

int memcmp_volatile(volatile uint8_t * str1, volatile uint8_t * str2, uint32_t count)
{
  while (count-- > 0)
    if (*str1++ != *str2++)
      return 1;
  return 0;
}

void memset_volatile(volatile uint8_t * str, uint8_t val, uint32_t length) {
  while( length -- > 0)
   *str++ = val;
}
