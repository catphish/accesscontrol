#include <stdint.h>
#include "board.h"
#include "gpio.h"
#include "rfid.h"
#include "door.h"
#include "util.h"
#include "string.h"
#include "ethernet.h"
#include "cloud.h"

volatile uint8_t card_in_bits[256];
volatile uint8_t card_in_pos;
volatile struct time_t card_in_timeout;

void rfid_init()
{
  gpio_port_mode(GPIOA, 8, 0, 0, 0, 0);  // RFID Magstripe Mode Clock
  gpio_port_mode(GPIOA, 10, 0, 0, 0, 0); // RFID Magstripe Mode Data
  SYSCFG->EXTICR[3 - 1] |= SYSCFG_EXTICR3_EXTI8_PA;
  EXTI->IMR |= (1 << 8);
  EXTI->FTSR |= (1 << 8);
  NVIC->ISER[((uint32_t)(EXTI9_5_IRQn) >> 5)] |= (1 << ((uint32_t)(EXTI9_5_IRQn)&0x1F));

  card_in_pos = 0;
  card_in_timeout.sec = 0;
}
static uint8_t start[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0};

void handle_incoming_card(uint8_t *number, uint8_t length)
{
  uint8_t response_data[MESSAGE_DATA_SIZE];
  memset(response_data, 0, MESSAGE_DATA_SIZE);
  response_data[0] = length;
  memcpy(response_data + 1, number, length);
  if (cloud_check_card(number, length))
  {
    door_open_timed(8);
    cloud_send_data(CLOUD_MESSAGE_TYPE_CARD_SUCCESS, response_data);
  }
  else
    cloud_send_data(CLOUD_MESSAGE_TYPE_CARD_FAILURE, response_data);
}

void rfid_main()
{
  uint8_t bytes[32];
  if (card_in_timeout.sec && time_passed(&card_in_timeout))
  {
    // Card reader stopped sending data. Look for a valid card number.
    for (int n = 0; n < 32; n++)
    {
      if (!memcmp_volatile(start, card_in_bits + n, 15))
      {
        // Found a start sequence
        for (int m = 0; m < 32; m++)
        {
          bytes[m] = (card_in_bits[n + 15 + m * 5 + 0] << 0);
          bytes[m] |= (card_in_bits[n + 15 + m * 5 + 1] << 1);
          bytes[m] |= (card_in_bits[n + 15 + m * 5 + 2] << 2);
          bytes[m] |= (card_in_bits[n + 15 + m * 5 + 3] << 3);
          if (bytes[m] == 0xf)
          {
            // Found an end byte
            handle_incoming_card(bytes, m);
            break;
          }
        }
        break;
      }
    }
    memset_volatile(card_in_bits, 0, sizeof(card_in_bits));
    card_in_timeout.sec = 0;
    card_in_pos = 0;
  }
}

void EXTI9_5_IRQHandler()
{
  card_in_bits[card_in_pos++] = !(GPIOA->IDR & (1 << 10));
  EXTI->PR = (1 << 8);
  time_set(&card_in_timeout, 0, 100);
}
