struct __attribute__((packed)) dmadesc_t {
	uint32_t status;
	uint32_t ctrl;
	struct eth_frame_t * frame;
	struct dmadesc_t * next;
};

struct __attribute__((packed)) eth_frame_t {
	uint8_t dst_addr[6];
	uint8_t src_addr[6];
  uint16_t ethertype;
  uint8_t payload[1500];
};

struct __attribute__((packed)) arp_message_t {
  uint16_t htype;
  uint16_t ptype;
  uint8_t hlen;
  uint8_t plen;
  uint16_t oper;
  uint8_t sha[6];
  uint8_t spa[4];
  uint8_t tha[6];
  uint8_t tpa[4];
};

struct __attribute__((packed)) ip_packet_t {
  uint8_t version_ihl;
  uint8_t dscp_ecn;
  uint16_t total_length;
  uint16_t identification;
  uint16_t flags_offset;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t checksum;
  uint8_t source_address[4];
  uint8_t destination_address[4];
  uint8_t payload[1480];
};

struct __attribute__((packed)) udp_datagram_t {
  uint16_t sport;
  uint16_t dport;
  uint16_t length;
  uint16_t checksum;
  uint8_t payload[1472];
};

struct __attribute__((packed)) icmp_message_t {
  uint8_t type;
  uint8_t code;
  uint16_t icmp_checksum;
  uint16_t id;
  uint16_t sequence;
  uint8_t payload[1472];
};

struct __attribute__((packed)) dhcp_message_t {
  uint8_t op;
  uint8_t htype;
  uint8_t hlen;
  uint8_t hops;
  uint32_t xid;
  uint16_t secs;
  uint16_t flags;
  uint8_t ciaddr[4];
  uint8_t yiaddr[4];
  uint8_t siaddr[4];
  uint8_t giaddr[4];
  uint8_t chaddr[16];
  uint8_t sname[64];
  uint8_t file[128];
  uint8_t options[512];
};

struct __attribute__((packed)) dns_message_t {
  uint16_t identification;
  uint16_t flags;
  uint16_t n_questions;
  uint16_t n_answers;
  uint16_t n_auth_rr;
  uint16_t n_additional_rr;
  uint8_t data[];
};

uint8_t ethernet_ready();
void ethernet_init();
void ethernet_rx();
void ethernet_main();
void ethernet_send_dhcp_discover();
void ethernet_send_gateway_arp();
void ethernet_send_dns();
void ethernet_udp_tx(uint8_t * destination, uint16_t sport, uint16_t dport, uint8_t * payload, uint16_t payload_length);
void ethernet_ip_tx(uint8_t * destination, uint8_t protocol, uint8_t * payload, uint16_t payload_length);
void ethernet_tx(uint8_t * destination, uint16_t ethertype, uint8_t * payload, uint16_t payload_length);
void ethernet_arp_rx(struct arp_message_t* message);
void ethernet_ip_rx(struct ip_packet_t* packet);
void ethernet_icmp_rx(struct ip_packet_t* packet);
void ethernet_udp_rx(struct ip_packet_t* packet);
void ethernet_dhcp_rx(struct dhcp_message_t* message);
void ethernet_dns_rx(struct dns_message_t* message);

extern uint8_t server_ip_address[];

#define HTONS(n) (((((unsigned short)(n) & 0xFF)) << 8) | (((unsigned short)(n) & 0xFF00) >> 8))
#define NTOHS(n) (((((unsigned short)(n) & 0xFF)) << 8) | (((unsigned short)(n) & 0xFF00) >> 8))
#define HTONL(n) (((((unsigned long)(n) & 0xFF)) << 24) | \
                  ((((unsigned long)(n) & 0xFF00)) << 8) | \
                  ((((unsigned long)(n) & 0xFF0000)) >> 8) | \
                  ((((unsigned long)(n) & 0xFF000000)) >> 24))

#define NTOHL(n) (((((unsigned long)(n) & 0xFF)) << 24) | \
                  ((((unsigned long)(n) & 0xFF00)) << 8) | \
                  ((((unsigned long)(n) & 0xFF0000)) >> 8) | \
                  ((((unsigned long)(n) & 0xFF000000)) >> 24))
