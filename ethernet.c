#include <stdint.h>
#include <stm32f439xx.h>
#include "ethernet.h"
#include "util.h"
#include "gpio.h"

uint8_t my_mac_address[] = { 2,0,0,0,0,1 };
uint8_t my_ip_address[]  = { 0,0,0,0 };

volatile struct eth_frame_t rx_frame[2];
volatile struct dmadesc_t  rx_desc[2];
volatile struct dmadesc_t* rx_desc_p;

volatile struct eth_frame_t tx_frame[2];
volatile struct dmadesc_t  tx_desc[2];
volatile struct dmadesc_t* tx_desc_p;

uint32_t previous_time = 0;
uint8_t dhcp_state = 0;
uint32_t dhcp_request_xid = 0;

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
  rx_desc[0].frame = rx_frame+0;
  rx_desc[0].next = (struct dmadesc_t*)&rx_desc[1];
  rx_desc[1].status = (1 << 31); // DMA OWNs
  rx_desc[1].ctrl = (1 << 14) | 2048; // RCH
  rx_desc[1].frame = rx_frame+1;
  rx_desc[1].next = (struct dmadesc_t*)&rx_desc[0];
  rx_desc_p = rx_desc;

  tx_desc[0].status = (3<<28) | (1<<20); // CPU OWNs
  tx_desc[0].frame = tx_frame+0;
  tx_desc[0].next = (struct dmadesc_t*)&tx_desc[1];
  tx_desc[1].status = (3<<28) | (1<<20);; // CPU OWNs
  tx_desc[1].frame = tx_frame+1;
  tx_desc[1].next = (struct dmadesc_t*)&tx_desc[0];
  tx_desc_p = tx_desc;

  ETH->DMABMR = (1 << 16) | (16 << 8);
  ETH->DMARDLAR = (uint32_t)rx_desc;
  ETH->DMATDLAR = (uint32_t)tx_desc;
  ETH->DMAOMR = (1 << 1) | (1 << 13) | (1<<21); // RXSTART, TXSTART
  ETH->MACCR |= (1 << 14) | (1 << 11) | (1 << 3) | (1 << 2);
  ETH->MACFFR |= (1 << 31);
}

void ethernet_main() {
  // Handle incoming frames
  ethernet_rx();

  // Maintenance tasks every 1s
  if(TIM2->CNT > previous_time) {
    previous_time = TIM2->CNT;
    if(dhcp_state == 0) {
      // DHCP is idle, start by sending a DISCOVER
      ethernet_send_dhcp_discover();
      return;
    }
  }
}

void ethernet_rx() {
  // Check whether we have an Ethernet frame waiting!
  if(!(rx_desc_p->status & (1 << 31))) {
    // Check the ethertype and escalate accordingly
    switch(NTOHS(rx_desc_p->frame->ethertype)) {
      case 0x0800:
        ethernet_ip_rx((volatile struct ip_packet_t*)rx_desc_p->frame->payload);
        break;
      case 0x0806:
        ethernet_arp_rx((volatile struct arp_message_t*)(rx_desc_p->frame->payload));
        break;
    }
    // Return the RX descriptor to DMA
    rx_desc_p->status = (1 << 31);
    // Update local RX descriptor pointer
    rx_desc_p = (volatile struct dmadesc_t*)rx_desc_p->next;
  }
}

void ethernet_arp_rx(volatile struct arp_message_t* message) {
  // Check whether this is a request
  if(
      NTOHS(message->htype) == 0x0001 &&
      NTOHS(message->ptype) == 0x0800 &&
      message->hlen == 6 &&
      message->plen == 4 &&
      NTOHS(message->oper) == 0x0001
  ){
    // Check whether it's for us
    if(
        message->tpa[0] == my_ip_address[0] &&
        message->tpa[1] == my_ip_address[1] &&
        message->tpa[2] == my_ip_address[2] &&
        message->tpa[3] == my_ip_address[3]
    ){
      // Respond to the ARP request
      struct arp_message_t reply;
      for(int n=0; n<6; n++) {
        reply.tha[n] = message->sha[n];
        reply.sha[n] = my_mac_address[n];
      }
      reply.htype = 0x0100;
      reply.ptype = 0x0008;
      reply.hlen = 6;
      reply.plen = 4;
      reply.oper = 0x0200;
      for(int n=0; n<4; n++) {
        reply.tpa[n] = message->spa[n];
        reply.spa[n] = my_ip_address[n];
      }
      ethernet_tx(reply.tha, 0x0806, (void*)&reply, sizeof(reply));
    }
  }
}

void ethernet_ip_rx(volatile struct ip_packet_t* packet){
  // We're quite fussy. No header extensions, no fragments.
  if(
    packet->version_ihl == 0x45 &&
    HTONS(packet->total_length) <= 1200 &&
    (packet->flags_offset == 0x0000 || packet->flags_offset == 0x0040)
  ) {
    if(
      (
        packet->destination_address[0] == 0xff &&
        packet->destination_address[1] == 0xff &&
        packet->destination_address[2] == 0xff &&
        packet->destination_address[3] == 0xff
      ) || (
        packet->destination_address[0] == my_ip_address[0] &&
        packet->destination_address[1] == my_ip_address[1] &&
        packet->destination_address[2] == my_ip_address[2] &&
        packet->destination_address[3] == my_ip_address[3]
      )
    ) {
      switch(packet->protocol) {
        case 0x01:
          ethernet_icmp_rx((volatile struct ip_packet_t*)packet);
          break;
        case 0x11:
          ethernet_udp_rx((volatile struct ip_packet_t*)packet);
          break;
      }
    }
  }
}

void ethernet_udp_rx(volatile struct ip_packet_t* packet) {
  volatile struct udp_datagram_t * udp = (volatile struct udp_datagram_t *)packet->payload;
  if(NTOHS(udp->dport) == 68) {
    ethernet_dhcp_rx((volatile struct dhcp_message_t*)(udp->payload));
  }
}

void ethernet_icmp_rx(volatile struct ip_packet_t* packet) {
  volatile struct icmp_message_t * icmp = (volatile struct icmp_message_t *)packet->payload;
  if(
    icmp->type == 8 &&
    icmp->code == 0
  ) {
    // Echo request. Lets reply!
    struct icmp_message_t reply;
    reply.type = 0;
    reply.code = 0;
    reply.icmp_checksum = 0;
    reply.id = icmp->id;
    reply.sequence = icmp->sequence;
    int data_length = NTOHS(packet->total_length) - 28;
    if(data_length > 1472)
      return;
    for(int n=0; n<data_length; n++)
      reply.payload[n] = icmp->payload[n];
    ethernet_ip_tx(packet->source_address, 0x01, (void*)&reply, data_length + 28);
  }
}

void ethernet_dhcp_rx(volatile struct dhcp_message_t* message) {
  // See if this DHCP message is a reply to our last request
  if(message->xid == dhcp_request_xid && message->op == 2) {
    // Work in progress...
    // uint8_t message_type = 0;
    // while(1) {
    //   message->options
    // }
  }
}

void ethernet_send_dhcp_discover() {
  struct dhcp_message_t request;
  for(int n=0; n<sizeof(request); n++)
    ((uint8_t*)(&request))[n] = 0;
  request.op = 1;
  request.htype = 1;
  request.hlen = 6;
  request.hops = 0;
  request.xid = dhcp_request_xid = RNG->DR;
  request.secs = 0;
  request.flags = HTONS(0x8000);
  for(int n=0; n<4; n++) {
    request.ciaddr[n] = 0;
    request.yiaddr[n] = 0;
    request.siaddr[n] = 0;
    request.giaddr[n] = 0;
  }
  for(int n=0; n<6; n++) {
    request.chaddr[n] = my_mac_address[n];
  }
  request.options[0] = 99;
  request.options[1] = 130;
  request.options[2] = 83;
  request.options[3] = 99;

  request.options[4] = 53;
  request.options[5] = 1;
  request.options[6] = 1;

  request.options[7] = 0xff;
  ethernet_udp_tx((uint8_t []){0xff, 0xff, 0xff, 0xff}, 68, 67, (uint8_t*)&request, sizeof(request) + 8);

}

void ethernet_udp_tx(uint8_t * destination, uint16_t sport, uint16_t dport, uint8_t * payload, uint16_t payload_length) {
  // Check the length is valid
  if(payload_length > 1472)
    return;

  struct udp_datagram_t datagram;
  datagram.sport = HTONS(sport);
  datagram.dport = HTONS(dport);
  datagram.length = HTONS(payload_length + 8);
  datagram.checksum = 0;
  for(int n=0; n<payload_length; n++)
    datagram.payload[n] = payload[n];
  ethernet_ip_tx(destination, 17, (uint8_t*)&datagram, payload_length + 8);
}

void ethernet_ip_tx(volatile uint8_t * destination, uint8_t protocol, uint8_t * payload, uint16_t payload_length) {
  // Check the length is valid
  if(payload_length > 1480)
    return;

  struct ip_packet_t packet;
  packet.version_ihl = 0x45;
  packet.dscp_ecn = 0;
  packet.total_length = HTONS(20 + payload_length);
  packet.identification = 0;
  packet.flags_offset = 0;
  packet.ttl = 255;
  packet.protocol = protocol;
  packet.checksum = 0;
  for(int n=0; n<4; n++) {
    packet.source_address[n] = my_ip_address[n];
    packet.destination_address[n] = destination[n];
  }
  for(int n=0; n<payload_length; n++)
    packet.payload[n] = payload[n];
  if(destination[0] == 0xff && destination[1] == 0xff && destination[2] == 0xff && destination[3] == 0xff)
    ethernet_tx((uint8_t []){0xff,0xff,0xff,0xff,0xff,0xff}, 0x0800, (uint8_t*)&packet, payload_length + 20);
  else
    ethernet_tx((uint8_t []){0xdc,0x9f,0xdb,0x28,0x36,0x81}, 0x0800, (uint8_t*)&packet, payload_length + 20); // Harcoded router MAC!
}

void ethernet_tx(uint8_t * destination, uint16_t ethertype, uint8_t * payload, uint16_t payload_length) {
  // Check the length is valid
  if(payload_length > 1500)
    return;

  // Check if there's a free TX descriptor, if not we discard the frame
  if(tx_desc_p->status & (1 << 31))
    return;

  // Copy the payload into the frame
  for(int n=0; n<payload_length; n++)
    tx_desc_p->frame->payload[n] = payload[n];
  // Set the ethertype
  tx_desc_p->frame->ethertype = HTONS(ethertype);
  // Set the source and destination
  for(int n=0; n<6; n++) {
    tx_desc_p->frame->src_addr[n] = my_mac_address[n];
    tx_desc_p->frame->dst_addr[n] = destination[n];
  }

  // Set the length
  tx_desc_p->ctrl = payload_length + 14;
  // Populate the TX descriptor flags and return ownership to DMA
  tx_desc_p->status = (1 << 31) | (3<<28) | (1<<20) | (3<<22);
  // Increment local TX pointer
  tx_desc_p = (volatile struct dmadesc_t*)tx_desc_p->next;
  // Wake up the DMA
  ETH->DMATPDR = 1;
}
