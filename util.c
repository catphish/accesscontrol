#include <stm32f439xx.h>

void usleep(unsigned int delay) {
  SysTick->LOAD = 0x00FFFFFF;
  SysTick->VAL = 0;
  SysTick->CTRL = 5;
  while(0x00FFFFFF - SysTick->VAL < delay * 180);
}

void msleep(unsigned int delay) {
	for(int n=0; n<delay; n++) {
		usleep(1000);
	}
}

void blink(uint8_t color) {
  if(color == 0)
    GPIOB->ODR = (1<<0);
  if(color == 1)
    GPIOB->ODR = (1<<7);
  if(color == 2)
    GPIOB->ODR = (1<<14);
  msleep(20);
  GPIOB->ODR = 0;
}