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

void ethernet_rx() {
  // Check whether we have an Ethernet frame waiting!
  if(!(rx_desc_p->status & (1 << 31))) {
    // Check the ethertype and escalate accordingly
    switch(NTOHS(rx_desc_p->frame->ethertype)) {
      case 0x0800:
        // handle_ip((volatile struct ip_frame_t*)rx_desc_p->frame->payload);
        break;
      case 0x0806:
        handle_arp((volatile struct arp_packet_t*)(rx_desc_p->frame->payload));
        break;
    }
    // Return the RX descriptor to DMA
    rx_desc_p->status = (1 << 31);
    // Update local RX descriptor pointer
    rx_desc_p = (volatile struct dmadesc_t*)rx_desc_p->next;
  }
}

uint8_t dhcp_state = 0;
void ethernet_1s() {
  // This function runs every second and takes care of regular network maintenance
  if(dhcp_state == 0) {
    // DHCP is idle, start by sending a DISCOVER
    send_dhcp_discover();
    return;
  }
}

void send_dhcp_discover() {
  struct dhcp_packet_t packet;
  for(int n=0; n<sizeof(packet); n++)
    ((uint8_t*)(&packet))[n] = 0;
  packet.op = 1;
  packet.htype = 1;
  packet.hlen = 6;
  packet.hops = 0;
  packet.xid = RNG->DR;
  packet.secs = 0;
  packet.flags = 0;
  for(int n=0; n<4; n++) {
    packet.ciaddr[n] = 0;
    packet.yiaddr[n] = 0;
    packet.siaddr[n] = 0;
    packet.giaddr[n] = 0;
  }
  for(int n=0; n<6; n++) {
    packet.chaddr[n] = my_mac_address[n];
  }
  packet.options[0] = 99;
  packet.options[1] = 130;
  packet.options[2] = 83;
  packet.options[3] = 99;

  packet.options[4] = 53;
  packet.options[5] = 1;
  packet.options[6] = 1;

  packet.options[7] = 0xff;
  ethernet_udp_tx((uint8_t []){0xff, 0xff, 0xff, 0xff}, 68, 67, (uint8_t*)&packet, sizeof(packet) + 8);

}

void ethernet_udp_tx(uint8_t * destination, uint16_t sport, uint16_t dport, uint8_t * payload, uint16_t payload_length) {
  struct udp_datagram_t datagram;
  datagram.sport = HTONS(sport);
  datagram.dport = HTONS(dport);
  datagram.length = HTONS(payload_length + 8);
  datagram.checksum = 0;
  for(int n=0; n<payload_length; n++)
    datagram.payload[n] = payload[n];
  ethernet_ip_tx(destination, 17, (uint8_t*)&datagram, payload_length + 8);
}

void ethernet_ip_tx(uint8_t * destination, uint8_t protocol, uint8_t * payload, uint16_t payload_length) {
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
    ethernet_tx((uint8_t []){0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, 0x0800, (uint8_t*)&packet, payload_length + 20);
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

// void handle_ip(volatile struct ip_frame_t* frame) {
//   // We're quite fussy. No header extensions, no fragments.
//   if(
//     frame->version_ihl == 0x45 &&
//     HTONS(frame->total_length) <= 1200 &&
//     (frame->flags_offset == 0x0000 || frame->flags_offset == 0x0040) &&
//     frame->destination_address[0] == my_ip_address[0] &&
//     frame->destination_address[1] == my_ip_address[1] &&
//     frame->destination_address[2] == my_ip_address[2] &&
//     frame->destination_address[3] == my_ip_address[3]
//   ) {

//     switch(frame->protocol) {
//       case 0x01:
//         handle_icmp((volatile struct icmp_frame_t*)frame);
//         break;
//     }
//   }
// }

// void handle_icmp(volatile struct icmp_frame_t* frame) {
//   if(
//     frame->type == 8 &&
//     frame->code == 0
//   ) {
//     // Echo request. Lets reply.
//     if(!(tx_desc_p->status & (1 << 31))) {
//       // Found a spare TX descriptor, sending reply.
//       volatile struct icmp_frame_t* reply = (volatile struct icmp_frame_t*)tx_desc_p->frame;

//       reply->dst_addr[0] = frame->src_addr[0];
//       reply->dst_addr[1] = frame->src_addr[1];
//       reply->dst_addr[2] = frame->src_addr[2];
//       reply->dst_addr[3] = frame->src_addr[3];
//       reply->dst_addr[4] = frame->src_addr[4];
//       reply->dst_addr[5] = frame->src_addr[5];

//       reply->src_addr[0] = my_mac_address[0];
//       reply->src_addr[1] = my_mac_address[1];
//       reply->src_addr[2] = my_mac_address[2];
//       reply->src_addr[3] = my_mac_address[3];
//       reply->src_addr[4] = my_mac_address[4];
//       reply->src_addr[5] = my_mac_address[5];
//       reply->ethertype = 0x0008;
//       reply->version_ihl = 0x45;
//       reply->dscp_ecn = 0;
//       reply->identification = 0;
//       reply->flags_offset = 0;
//       reply->ttl = 0xff;
//       reply->protocol = 1;
//       reply->checksum = 0;
//       reply->total_length = frame->total_length;
//       reply->source_address[0] = my_ip_address[0];
//       reply->source_address[1] = my_ip_address[1];
//       reply->source_address[2] = my_ip_address[2];
//       reply->source_address[3] = my_ip_address[3];
//       reply->destination_address[0] = frame->source_address[0];
//       reply->destination_address[1] = frame->source_address[1];
//       reply->destination_address[2] = frame->source_address[2];
//       reply->destination_address[3] = frame->source_address[3];
//       reply->type = 0;
//       reply->code = 0;
//       reply->id = frame->id;
//       reply->sequence = frame->sequence;
//       reply->icmp_checksum = 0;
//       for(int n=0; n<(HTONS(frame->total_length)-28); n++)
//         reply->payload[n] = frame->payload[n];

//       // Set the length
//       tx_desc_p->ctrl = HTONS(frame->total_length) + 14;
//       // Populate the TX descriptor flags and return ownership to DMA
//       tx_desc_p->status = (1 << 31) | (3<<28) | (1<<20) | (3<<22);
//       // Increment local TX pointer
//       tx_desc_p = (volatile struct dmadesc_t*)tx_desc_p->next;
//       // Wake up the DMA
//       ETH->DMATPDR = 1;
//     }
//   }
// }

void handle_arp(volatile struct arp_packet_t* request) {
  // Check whether this is a request
  if(
      NTOHS(request->htype) == 0x0001 &&
      NTOHS(request->ptype) == 0x0800 &&
      request->hlen == 6 &&
      request->plen == 4 &&
      NTOHS(request->oper) == 0x0001
  ){
    // Check whether it's for us
    if(
        request->tpa[0] == my_ip_address[0] &&
        request->tpa[1] == my_ip_address[1] &&
        request->tpa[2] == my_ip_address[2] &&
        request->tpa[3] == my_ip_address[3]
    ){
      // Respond to the ARP request
      struct arp_packet_t reply;
      for(int n=0; n<6; n++) {
        reply.tha[n] = request->sha[n];
        reply.sha[n] = my_mac_address[n];
      }
      reply.htype = 0x0100;
      reply.ptype = 0x0008;
      reply.hlen = 6;
      reply.plen = 4;
      reply.oper = 0x0200;
      for(int n=0; n<4; n++) {
        reply.tpa[n] = request->spa[n];
        reply.spa[n] = my_ip_address[n];
      }
      ethernet_tx(reply.tha, 0x0806, (void*)&reply, sizeof(reply));
    }
  }

}
