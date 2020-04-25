#include <stm32f439xx.h>

void SystemInitError(uint8_t error_source) {
  while(1);
}

void SystemInit() {
  // Enable FPU
  SCB->CPACR |= 0xf00000;

  /* Configure Main PLL */
  RCC->PLLCFGR = (7<<24)|(180<<6)|(8<<0);

  /* PLL On */
  RCC->CR |= RCC_CR_PLLON;
  /* Wait until PLL is locked */
  while ((RCC->CR & RCC_CR_PLLRDY) == 0);

  /*
   * FLASH configuration block
   * enable instruction cache
   * enable prefetch
   * set latency to 5WS (6 cycles)
   */
  FLASH->ACR |= FLASH_ACR_ICEN | FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_5WS;

  /* Set clock source to PLL */
  RCC->CFGR |= RCC_CFGR_SW_PLL;
  /* Check clock source */
  while ((RCC->CFGR & RCC_CFGR_SWS_PLL) != RCC_CFGR_SWS_PLL);

  // Set up TIM3 to measure 100us periods
  RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
  TIM3->PSC = (18000 - 1); // 180MHz -> 10KHz
  TIM3->ARR = (10000 - 1); // Reload at 1s
  TIM3->CR2 = (2<<4);      // Master Mode = Update

  // Set up TIM2 as a slave to measure 1s periods
  TIM2->SMCR = (2 << 4) | (7<<0); // Trigger = ITR2, Slave Mode = External Clock Mode 1

  // Enable both timers
  TIM2->CR1 = 1;
  TIM3->CR1 = 1;

  // Enable the RNG
  RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;
  RNG->CR = (1<<2);
}
