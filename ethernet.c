#include <stdint.h>
#include <stm32f439xx.h>
#include "ethernet.h"
#include "util.h"
#include "gpio.h"

uint8_t my_mac_address[] = { 2,0,0,0,0,1 };
uint8_t my_ip_address[]  = { 10,99,1,2 };

volatile uint8_t rx_buf[2][2048];
volatile struct dmadesc_t  rx_desc[2];
volatile struct dmadesc_t* rx_desc_p;
volatile uint8_t tx_buf[2][2048];
volatile struct dmadesc_t  tx_desc[2];
volatile struct dmadesc_t* tx_desc_p;

void ethernet_init() {
  gpio_port_mode(GPIOA, 1,  2, 11, 0, 0); // A1  - REF_CLK
  gpio_port_mode(GPIOA, 2,  2, 11, 0, 0); // A2  - MDIO
  gpio_port_mode(GPIOA, 7,  2, 11, 0, 0); // A7  - CRS_DV
  gpio_port_mode(GPIOB, 13, 2, 11, 0, 0); // B13 - TXD1
  gpio_port_mode(GPIOC, 1,  2, 11, 0, 0); // C1  - MDC
  gpio_port_mode(GPIOC, 4,  2, 11, 0, 0); // C4  - RXD0
  gpio_port_mode(GPIOC, 5,  2, 11, 0, 0); // C5  - RXD1
  gpio_port_mode(GPIOG, 11, 2, 11, 0, 0); // G11 - TXDEN
  gpio_port_mode(GPIOG, 13, 2, 11, 0, 0); // G13 - TXD0
 
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
	SYSCFG->PMC |= (1 << 23);
  SYSCFG->CMPCR = 1;

  RCC->AHB1ENR |= RCC_AHB1ENR_ETHMACEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_ETHMACTXEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_ETHMACRXEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_ETHMACPTPEN;

  ETH->DMABMR |= 1;
  while(ETH->DMABMR & 1);

  rx_desc[0].status = (1 << 31); // DMA OWNs
  rx_desc[0].ctrl = (1 << 14) | 2048; // RCH
  rx_desc[0].buf = (uint32_t)rx_buf[0];
  rx_desc[0].next = (uint32_t)&rx_desc[1];
  rx_desc[1].status = (1 << 31); // DMA OWNs
  rx_desc[1].ctrl = (1 << 14) | 2048; // RCH
  rx_desc[1].buf = (uint32_t)rx_buf[1];
  rx_desc[1].next = (uint32_t)&rx_desc[0];
  rx_desc_p = rx_desc;

  tx_desc[0].status = (3<<28) | (1<<20); // CPU OWNs
  tx_desc[0].buf = (uint32_t)tx_buf[0];
  tx_desc[0].next = (uint32_t)&tx_desc[1];
  tx_desc[1].status = (3<<28) | (1<<20);; // CPU OWNs
  tx_desc[1].buf = (uint32_t)tx_buf[1];
  tx_desc[1].next = (uint32_t)&tx_desc[0];
  tx_desc_p = tx_desc;

  ETH->DMABMR = (1 << 16) | (16 << 8);
  ETH->DMARDLAR = (uint32_t)rx_desc;
  ETH->DMATDLAR = (uint32_t)tx_desc;
  ETH->DMAOMR = (1 << 1) | (1 << 13) | (1<<21); // RXSTART, TXSTART
  ETH->MACCR |= (1 << 14) | (1 << 11) | (1 << 3) | (1 << 2);
  ETH->MACFFR |= (1 << 31);
}

void ethernet_rx() {
  if(!(rx_desc_p->status & (1 << 31))) {
    // We have an Ethernet frame waiting!
    volatile struct eth_frame_t* frame = (volatile struct eth_frame_t*)rx_desc_p->buf;
    switch(frame->ethertype) {
      case 0x0008:
        handle_ip((volatile struct ip_frame_t*)frame);
        break;
      case 0x0608:
        handle_arp((volatile struct arp_frame_t*)frame);
        break;
    }
    // Return the RX descriptor to DMA
    rx_desc_p->status = (1 << 31);

    // Update local RX descriptor pointer
    rx_desc_p = (volatile struct dmadesc_t*)rx_desc_p->next;
  }
}

void handle_ip(volatile struct ip_frame_t* frame) {
  // We're quite fussy. No header extensions, no fragments.
  if(
    frame->version_ihl == 0x45 &&
    HTONS(frame->total_length) <= 1200 &&
    (frame->flags_offset == 0x0000 || frame->flags_offset == 0x0040) &&
    frame->destination_address[0] == my_ip_address[0] &&
    frame->destination_address[1] == my_ip_address[1] &&
    frame->destination_address[2] == my_ip_address[2] &&
    frame->destination_address[3] == my_ip_address[3]
  ) {

    switch(frame->protocol) {
      case 0x01:
        handle_icmp((volatile struct icmp_frame_t*)frame);
        break;
    }
  }
}

void handle_icmp(volatile struct icmp_frame_t* frame) {
  // GPIOB->ODR = (1<<7);
  // msleep(50);
  // GPIOB->ODR = 0;

  if(
    frame->type == 8 &&
    frame->code == 0
  ) {
    // Echo request. Lets reply.
    if(!(tx_desc_p->status & (1 << 31))) {
      // Found a spare TX descriptor, sending reply.
      volatile struct icmp_frame_t* reply = (volatile struct icmp_frame_t*)tx_desc_p->buf;

      reply->dst_addr[0] = frame->src_addr[0];
      reply->dst_addr[1] = frame->src_addr[1];
      reply->dst_addr[2] = frame->src_addr[2];
      reply->dst_addr[3] = frame->src_addr[3];
      reply->dst_addr[4] = frame->src_addr[4];
      reply->dst_addr[5] = frame->src_addr[5];

      reply->src_addr[0] = my_mac_address[0];
      reply->src_addr[1] = my_mac_address[1];
      reply->src_addr[2] = my_mac_address[2];
      reply->src_addr[3] = my_mac_address[3];
      reply->src_addr[4] = my_mac_address[4];
      reply->src_addr[5] = my_mac_address[5];
      reply->ethertype = 0x0008;
      reply->version_ihl = 0x45;
      reply->dscp_ecn = 0;
      reply->identification = 0;
      reply->flags_offset = 0;
      reply->ttl = 0xff;
      reply->protocol = 1;
      reply->checksum = 0;
      reply->total_length = frame->total_length;
      reply->source_address[0] = my_ip_address[0];
      reply->source_address[1] = my_ip_address[1];
      reply->source_address[2] = my_ip_address[2];
      reply->source_address[3] = my_ip_address[3];
      reply->destination_address[0] = frame->source_address[0];
      reply->destination_address[1] = frame->source_address[1];
      reply->destination_address[2] = frame->source_address[2];
      reply->destination_address[3] = frame->source_address[3];
      reply->type = 0;
      reply->code = 0;
      reply->id = frame->id;
      reply->sequence = frame->sequence;
      reply->icmp_checksum = 0;
      for(int n=0; n<(HTONS(frame->total_length)-28); n++)
        reply->payload[n] = frame->payload[n];

      // Set the length
      tx_desc_p->ctrl = HTONS(frame->total_length) + 14;
      // Populate the TX descriptor flags and return ownership to DMA
      tx_desc_p->status = (1 << 31) | (3<<28) | (1<<20) | (3<<22);
      // Increment local TX pointer
      tx_desc_p = (volatile struct dmadesc_t*)tx_desc_p->next;
      // Wake up the DMA
      ETH->DMATPDR = 1;
    }
  }
}

void handle_arp(volatile struct arp_frame_t* frame) {
  if(
      frame->htype == 0x0100 &&
      frame->ptype == 0x0008 &&
      frame->hlen == 6 &&
      frame->plen == 4 &&
      frame->oper == 0x0100
  ){
    // Looks like a request!
    if(
        frame->tpa[0] == my_ip_address[0] &&
        frame->tpa[1] == my_ip_address[1] &&
        frame->tpa[2] == my_ip_address[2] &&
        frame->tpa[3] == my_ip_address[3]
    ){
      // It's for us!
      if(!(tx_desc_p->status & (1 << 31))) {
        // Found a spare TX descriptor, sending reply.
        volatile struct arp_frame_t* reply = (volatile struct arp_frame_t*)tx_desc_p->buf;
        reply->dst_addr[0] = reply->tha[0] = frame->sha[0];
        reply->dst_addr[1] = reply->tha[1] = frame->sha[1];
        reply->dst_addr[2] = reply->tha[2] = frame->sha[2];
        reply->dst_addr[3] = reply->tha[3] = frame->sha[3];
        reply->dst_addr[4] = reply->tha[4] = frame->sha[4];
        reply->dst_addr[5] = reply->tha[5] = frame->sha[5];
        reply->src_addr[0] = reply->sha[0] = my_mac_address[0];
        reply->src_addr[1] = reply->sha[1] = my_mac_address[1];
        reply->src_addr[2] = reply->sha[2] = my_mac_address[2];
        reply->src_addr[3] = reply->sha[3] = my_mac_address[3];
        reply->src_addr[4] = reply->sha[4] = my_mac_address[4];
        reply->src_addr[5] = reply->sha[5] = my_mac_address[5];
        reply->ethertype = 0x0608;
        reply->htype = 0x0100;
        reply->ptype = 0x0008;
        reply->hlen = 6;
        reply->plen = 4;
        reply->oper = 0x0200;
        reply->tpa[0] = frame->spa[0];
        reply->tpa[1] = frame->spa[1];
        reply->tpa[2] = frame->spa[2];
        reply->tpa[3] = frame->spa[3];
        reply->spa[0] = my_ip_address[0];
        reply->spa[1] = my_ip_address[1];
        reply->spa[2] = my_ip_address[2];
        reply->spa[3] = my_ip_address[3];

        // Set the length
        tx_desc_p->ctrl = 42;
        // Populate the TX descriptor flags and return ownership to DMA
        tx_desc_p->status = (1 << 31) | (3<<28) | (1<<20);
        // Increment local TX pointer
        tx_desc_p = (volatile struct dmadesc_t*)tx_desc_p->next;
        // Wake up the DMA
        ETH->DMATPDR = 1;
      }
    }
  }

}
