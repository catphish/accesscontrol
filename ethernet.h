#define HTONS(n) (((((unsigned short)(n) & 0xFF)) << 8) | (((unsigned short)(n) & 0xFF00) >> 8))

struct __attribute__((packed)) dmadesc_t {
	volatile uint32_t status;
	volatile uint32_t ctrl;
	volatile uint32_t buf;
	volatile uint32_t next;
};

struct __attribute__((packed)) eth_frame_t {
	volatile uint8_t dst_addr[6];
	volatile uint8_t src_addr[6];
  volatile uint16_t ethertype;
};

struct __attribute__((packed)) arp_frame_t {
	volatile uint8_t dst_addr[6];
	volatile uint8_t src_addr[6];
  volatile uint16_t ethertype;
  volatile uint16_t htype;
  volatile uint16_t ptype;
  volatile uint8_t hlen;
  volatile uint8_t plen;
  volatile uint16_t oper;
  volatile uint8_t sha[6];
  volatile uint8_t spa[4];
  volatile uint8_t tha[6];
  volatile uint8_t tpa[4];
};

struct __attribute__((packed)) ip_frame_t {
	volatile uint8_t dst_addr[6];
	volatile uint8_t src_addr[6];
  volatile uint16_t ethertype;
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
};

struct __attribute__((packed)) icmp_frame_t {
	volatile uint8_t dst_addr[6];
	volatile uint8_t src_addr[6];
  volatile uint16_t ethertype;
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
  uint8_t type;
  uint8_t code;
  uint16_t icmp_checksum;
  uint16_t id;
  uint16_t sequence;
  uint8_t payload[];
};

void ethernet_init();
void ethernet_rx();
void ethernet_tx();

void handle_arp(volatile struct arp_frame_t* request);
void handle_ip(volatile struct ip_frame_t* request);
void handle_icmp(volatile struct icmp_frame_t* frame);