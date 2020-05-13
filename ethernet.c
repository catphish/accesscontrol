#include <stdint.h>
#include "board.h"
#include "ethernet.h"
#include "util.h"
#include "gpio.h"
#include "cloud.h"
#include <string.h>

uint8_t broadcast_ip[] = {0xff,0xff,0xff,0xff};
uint8_t broadcast_mac[] = {0xff,0xff,0xff,0xff,0xff,0xff};
uint8_t my_ip_address[]  = { 0,0,0,0 };
uint8_t my_mac_address[] = { 2,0,0,0,0,1 };
struct time_t dhcp_renew;
struct time_t dhcp_expiry;

uint8_t gateway_ip_address[]  = { 0,0,0,0 };
uint8_t gateway_mac_address[]  = { 0,0,0,0,0,0 };
struct time_t gateway_arp_expiry;

uint8_t dns_ip_address[]  = { 0,0,0,0 };
uint8_t server_ip_address[]  = { 0,0,0,0 };
struct time_t dns_expiry;

char hostname[] = "\x05nutty\x02tk";
int hostname_length = sizeof(hostname);

struct eth_frame_t rx_frame[2];
struct dmadesc_t  rx_desc[2];
struct dmadesc_t* rx_desc_p;

struct eth_frame_t tx_frame[2];
struct dmadesc_t  tx_desc[2];
struct dmadesc_t* tx_desc_p;

struct time_t next_greeting;
uint32_t dhcp_request_xid = 0;

void ethernet_init() {
  // Enable Ethernet
  gpio_port_mode(GPIOB, 14, 1, 0, 0, 0); // Ethernet Enable
  GPIOB->BSRR = (1<<14);


#ifdef PROD
  gpio_port_mode(GPIOA, 1,  2, 11, 0, 0); // A1  - REF_CLK
  gpio_port_mode(GPIOA, 2,  2, 11, 0, 0); // A2  - MDIO
  gpio_port_mode(GPIOA, 7,  2, 11, 0, 0); // A7  - CRS_DV
  gpio_port_mode(GPIOB, 11, 2, 11, 0, 0); // G11 - TXDEN
  gpio_port_mode(GPIOB, 12, 2, 11, 0, 0); // G13 - TXD0
  gpio_port_mode(GPIOB, 13, 2, 11, 0, 0); // B13 - TXD1
  gpio_port_mode(GPIOC, 1,  2, 11, 0, 0); // C1  - MDC
  gpio_port_mode(GPIOC, 4,  2, 11, 0, 0); // C4  - RXD0
  gpio_port_mode(GPIOC, 5,  2, 11, 0, 0); // C5  - RXD1
#else
  gpio_port_mode(GPIOA, 1,  2, 11, 0, 0); // A1  - REF_CLK
  gpio_port_mode(GPIOA, 2,  2, 11, 0, 0); // A2  - MDIO
  gpio_port_mode(GPIOA, 7,  2, 11, 0, 0); // A7  - CRS_DV
  gpio_port_mode(GPIOB, 13, 2, 11, 0, 0); // B13 - TXD1
  gpio_port_mode(GPIOC, 1,  2, 11, 0, 0); // C1  - MDC
  gpio_port_mode(GPIOC, 4,  2, 11, 0, 0); // C4  - RXD0
  gpio_port_mode(GPIOC, 5,  2, 11, 0, 0); // C5  - RXD1
  gpio_port_mode(GPIOG, 11, 2, 11, 0, 0); // G11 - TXDEN
  gpio_port_mode(GPIOG, 13, 2, 11, 0, 0); // G13 - TXD0
#endif

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
  time_set(&dhcp_renew, 2, 0);
}

void ethernet_main() {
  // Handle incoming frames
  ethernet_rx();

  // Maintenance tasks

  if(time_passed(&dhcp_renew)) {
    // DHCP renewal due, send a new DISCOVER
    if(time_passed(&dhcp_expiry)) {
      // DHCP expired, clear IP. Retry in 5 seconds.
      my_ip_address[0] = 0; my_ip_address[1] = 0; my_ip_address[1] = 0; my_ip_address[3] = 0;
      time_set(&dhcp_renew, 5, 0);
    } else {
      // DHCP not yet expired retry in 2 minutes
      time_set(&dhcp_renew, 120, 0);
    }
    ethernet_send_dhcp_discover();
    return;
  }
  // If we don't have an IP yet, bail out!
  if(!my_ip_address[0] && !my_ip_address[1] && !my_ip_address[2] && !my_ip_address[3])
    return;
  
  if(time_passed(&gateway_arp_expiry)) {
    // ARP expired, fetch a new value. Retry in 2 seconds, keep the old value anyway
    ethernet_send_gateway_arp();
    time_set(&gateway_arp_expiry, 2, 0);
    return;
  }
  // If we don't have a gateway MAC yet, bail out!
  if(!gateway_mac_address[0] && !gateway_mac_address[1] && !gateway_mac_address[2] && !gateway_mac_address[3] && !gateway_mac_address[4] && !gateway_mac_address[5])
    return;

  if(time_passed(&dns_expiry)) {
    // DNS expired, fetch a new value, retry in 5 seconds, keep the old value anyway
    time_set(&dns_expiry, 5, 0);
    ethernet_send_dns();
    return;
  }
  // If we don't have DNS yet, bail out!
  if(!server_ip_address[0] && !server_ip_address[1] && !server_ip_address[2] && !server_ip_address[3])
    return;

  if(time_passed(&next_greeting)) {
    cloud_send_greeting();
    time_set(&next_greeting, 10, 0);
  }
}

uint8_t ethernet_ready() {
  if(!my_ip_address[0] && !my_ip_address[1] && !my_ip_address[2] && !my_ip_address[3])
    return 0;
  else if(!gateway_mac_address[0] && !gateway_mac_address[1] && !gateway_mac_address[2] && !gateway_mac_address[3] && !gateway_mac_address[4] && !gateway_mac_address[5])
    return 0;
  else if(!server_ip_address[0] && !server_ip_address[1] && !server_ip_address[2] && !server_ip_address[3])
    return 0;
  else
    return 1;
}

void ethernet_rx() {
  // Check whether we have an Ethernet frame waiting!
  if(!(rx_desc_p->status & (1 << 31))) {
    // Check the ethertype and escalate accordingly
    switch(NTOHS(rx_desc_p->frame->ethertype)) {
      case 0x0800:
        ethernet_ip_rx((struct ip_packet_t*)rx_desc_p->frame->payload);
        break;
      case 0x0806:
        ethernet_arp_rx((struct arp_message_t*)(rx_desc_p->frame->payload));
        break;
    }
    // Return the RX descriptor to DMA
    rx_desc_p->status = (1 << 31);
    // Update local RX descriptor pointer
    rx_desc_p = (struct dmadesc_t*)rx_desc_p->next;
  }
}

void ethernet_arp_rx(struct arp_message_t* message) {
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
      memcpy(reply.tha, message->sha, 6);
      memcpy(reply.sha, my_mac_address, 6);
      reply.htype = 0x0100;
      reply.ptype = 0x0008;
      reply.hlen = 6;
      reply.plen = 4;
      reply.oper = 0x0200;
      memcpy(reply.tpa, message->spa, 4);
      memcpy(reply.spa, my_ip_address, 4);
      ethernet_tx(reply.tha, 0x0806, (void*)&reply, sizeof(reply));
    }
  }

  // Check whether this is a respose
  if(
      NTOHS(message->htype) == 0x0001 &&
      NTOHS(message->ptype) == 0x0800 &&
      message->hlen == 6 &&
      message->plen == 4 &&
      NTOHS(message->oper) == 0x0002
  ){
    // See if it has the IP of our gateway
    if(!memcmp(message->spa, gateway_ip_address, 4)) {
      memcpy(gateway_mac_address, message->sha, 6);
      time_set(&gateway_arp_expiry, 60, 0);
    }
  }
}

void ethernet_ip_rx(struct ip_packet_t* packet){
  // We're quite fussy. No header extensions, no fragments.
  if(
    packet->version_ihl == 0x45 &&
    HTONS(packet->total_length) <= 1200 &&
    (packet->flags_offset == 0x0000 || packet->flags_offset == 0x0040)
  ) {
    if(!memcmp(packet->destination_address, broadcast_ip, 4) || !memcmp(packet->destination_address, my_ip_address, 4)) {
      switch(packet->protocol) {
        case 0x01:
          ethernet_icmp_rx((struct ip_packet_t*)packet);
          break;
        case 0x11:
          ethernet_udp_rx((struct ip_packet_t*)packet);
          break;
      }
    }
  }
}

void ethernet_udp_rx(struct ip_packet_t* packet) {
  struct udp_datagram_t * udp = (struct udp_datagram_t *)packet->payload;
  if(NTOHS(udp->length) > 1480)
    return;
  if(NTOHS(udp->dport) == 68) {
    ethernet_dhcp_rx((struct dhcp_message_t*)(udp->payload));
  }
  if(NTOHS(udp->dport) == 5353) {
    ethernet_dns_rx((struct dns_message_t*)(udp->payload));
  }
  if(NTOHS(udp->dport) == 42424) {
    cloud_rx((struct cloud_message_full_t*)(udp->payload));
  }
}

void ethernet_dns_rx(struct dns_message_t* message) {
  int offset = 0;
  // We're expecting a DNS reply with exactly one question
  if(NTOHS(message->n_questions) == 1) {
    if(memcmp(message->data+offset, hostname, sizeof(hostname)))
      return;
    offset += sizeof(hostname);
    offset += 4;
    // Now look for an A record response
    for(int n=0; n<NTOHS(message->n_answers);n++) {
      // Hopefully one answer contains the data we want
      uint8_t length = message->data[offset];
      if(length & 0xc0)
        offset += 2;
      else {
        if(memcmp(message->data+offset, hostname, sizeof(hostname)))
          return;
        offset += sizeof(hostname);
      }
      // TODO lots more here.
      // What if we get something that's not an A record?
      offset += 4;
      offset += 4;
      offset += 2;
      memcpy(server_ip_address, message->data+offset, 4);
      time_set(&dns_expiry, 300, 0);
    }
  }
}

void ethernet_icmp_rx(struct ip_packet_t* packet) {
  struct icmp_message_t * icmp = (struct icmp_message_t *)packet->payload;
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
    memcpy(reply.payload, icmp->payload, data_length);
    ethernet_ip_tx(packet->source_address, 0x01, (void*)&reply, data_length + 28);
  }
}

void ethernet_dhcp_rx(struct dhcp_message_t* message) {
  // See if this DHCP message is a reply to our last request
  if(message->xid == dhcp_request_xid && message->op == 2) {
    uint8_t message_type = 0;
    uint8_t server_identifier[4];
    uint8_t gateway[4] = { 0,0,0,0 };
    uint8_t dns[4] = { 0,0,0,0 };
    uint32_t lease_time = 0;

    uint16_t offset = 4;
    while(1) {
      uint8_t option = message->options[offset];
      if(option == 0xff) break;
      offset++;
      if(option == 0x00) continue;
      uint8_t length = message->options[offset];
      offset++;
      if(offset + length > 500) break;
      if(option == 0x35) message_type = message->options[offset];
      if(option == 0x36) {
        server_identifier[0] = message->options[offset+0];
        server_identifier[1] = message->options[offset+1];
        server_identifier[2] = message->options[offset+2];
        server_identifier[3] = message->options[offset+3];
      }
      if(option == 0x3) {
        gateway[0] = message->options[offset+0];
        gateway[1] = message->options[offset+1];
        gateway[2] = message->options[offset+2];
        gateway[3] = message->options[offset+3];
      }
      if(option == 0x6) {
        dns[0] = message->options[offset+0];
        dns[1] = message->options[offset+1];
        dns[2] = message->options[offset+2];
        dns[3] = message->options[offset+3];
      }
      if(option == 0x33) {
        lease_time = (message->options[offset+0] << 24) | (message->options[offset+1] << 16) |
                     (message->options[offset+2] << 8)  | (message->options[offset+0] << 0);
      }
      offset += length;
    }
    if(message_type == 2) {
      // Offer received, build request
      struct dhcp_message_t request;
      memset(&request, 0, sizeof(request));
      request.op = 1;
      request.htype = 1;
      request.hlen = 6;
      request.hops = 0;
      request.xid = dhcp_request_xid;
      request.secs = 0;
      request.flags = HTONS(0x8000);
      memcpy(request.chaddr, my_mac_address, 6);
      request.options[0] = 99;
      request.options[1] = 130;
      request.options[2] = 83;
      request.options[3] = 99;

      request.options[4] = 53;
      request.options[5] = 1;
      request.options[6] = 3;

      request.options[7] = 50;
      request.options[8] = 4;
      request.options[9] = message->yiaddr[0];
      request.options[10] = message->yiaddr[1];
      request.options[11] = message->yiaddr[2];
      request.options[12] = message->yiaddr[3];

      request.options[13] = 54;
      request.options[14] = 4;
      request.options[15] = server_identifier[0];
      request.options[16] = server_identifier[1];
      request.options[17] = server_identifier[2];
      request.options[18] = server_identifier[3];

      request.options[19] = 55;
      request.options[20] = 3;
      request.options[21] = 1;
      request.options[22] = 3;
      request.options[23] = 6;

      request.options[24] = 0xff;
      ethernet_udp_tx(broadcast_ip, 68, 67, (uint8_t*)&request, sizeof(request) + 25);
    }
    if(message_type == 5) {
      // ACK received, we can now use the IP
      memcpy(my_ip_address, message->yiaddr, 4);
      memcpy(gateway_ip_address, gateway, 4);
      memcpy(dns_ip_address, dns, 4);
      time_set(&dhcp_renew, lease_time / 2, 0);
      time_set(&dhcp_expiry, lease_time, 0);
      gateway_arp_expiry.sec = 0;
    }
  }
}

void ethernet_send_dns() {
  uint8_t request_memory[sizeof(struct dns_message_t) + sizeof(hostname) + 4];
  struct dns_message_t * request = (struct dns_message_t *)&request_memory;
  request->identification = RNG->DR;
  request->flags = HTONS(0x0100);
  request->n_questions = HTONS(1);
  request->n_answers = HTONS(0);
  request->n_auth_rr = HTONS(0);
  request->n_additional_rr = HTONS(0);
  memcpy(request->data, hostname, sizeof(hostname));
  memcpy(request->data+sizeof(hostname), (uint8_t[]){0,1,0,1} , 4);
  ethernet_udp_tx(dns_ip_address, 5353, 53, (void*)request, sizeof(request_memory));
}

void ethernet_send_gateway_arp() {
  struct arp_message_t request;
  memset(&request, 0, sizeof(request));
  memcpy(request.sha, my_mac_address, 6);
  memcpy(request.spa, my_ip_address, 4);
  memcpy(request.tpa, gateway_ip_address, 4);
  request.htype = HTONS(1);
  request.ptype = HTONS(0x0800);
  request.hlen = 6;
  request.plen = 4;
  request.oper = HTONS(1);
  ethernet_tx(broadcast_mac, 0x0806, (void*)&request, sizeof(request));
}

void ethernet_send_dhcp_discover() {
  struct dhcp_message_t request;
  memset(&request, 0, sizeof(request));
  request.op = 1;
  request.htype = 1;
  request.hlen = 6;
  request.hops = 0;
  request.xid = dhcp_request_xid = RNG->DR;
  request.secs = 0;
  request.flags = HTONS(0x8000);
  memcpy(request.chaddr, my_mac_address, 6);
  request.options[0] = 99;
  request.options[1] = 130;
  request.options[2] = 83;
  request.options[3] = 99;

  request.options[4] = 53;
  request.options[5] = 1;
  request.options[6] = 1;

  request.options[7] = 0xff;
  ethernet_udp_tx(broadcast_ip, 68, 67, (void*)&request, sizeof(request) + 8);
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
  memcpy(datagram.payload, payload, payload_length);
  ethernet_ip_tx(destination, 17, (uint8_t*)&datagram, payload_length + 8);
}

void ethernet_ip_tx(uint8_t * destination, uint8_t protocol, uint8_t * payload, uint16_t payload_length) {
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
  memcpy(packet.source_address, my_ip_address, 4);
  memcpy(packet.destination_address, destination, 4);
  memcpy(packet.payload, payload, payload_length);
  if(!memcmp(destination, broadcast_ip, 4))
    ethernet_tx(broadcast_mac, 0x0800, (uint8_t*)&packet, payload_length + 20);
  else
    ethernet_tx(gateway_mac_address, 0x0800, (uint8_t*)&packet, payload_length + 20);
}

void ethernet_tx(uint8_t * destination, uint16_t ethertype, uint8_t * payload, uint16_t payload_length) {
  // Check the length is valid
  if(payload_length > 1500)
    return;

  // Check if there's a free TX descriptor, if not we discard the frame
  if(tx_desc_p->status & (1 << 31))
    return;

  // Copy the payload into the frame
  memcpy(tx_desc_p->frame->payload, payload, payload_length);
  // Set the ethertype
  tx_desc_p->frame->ethertype = HTONS(ethertype);
  // Set the source and destination
  memcpy(tx_desc_p->frame->src_addr, my_mac_address, 6);
  memcpy(tx_desc_p->frame->dst_addr, destination, 6);

  // Set the length
  tx_desc_p->ctrl = payload_length + 14;
  // Populate the TX descriptor flags and return ownership to DMA
  tx_desc_p->status = (1 << 31) | (3<<28) | (1<<20) | (3<<22);
  // Increment local TX pointer
  tx_desc_p = (struct dmadesc_t*)tx_desc_p->next;
  // Wake up the DMA
  ETH->DMATPDR = 1;
}
