#pragma once

extern "C" {

#include <generated/csr.h>
#include <generated/mem.h>

#include <stdio.h>
#include <inet.h>
#include <system.h>
#include <crc.h>
#include <hw/flags.h>

#include <net/microudp.h>
#include <base/time.h>

} // extern "C"

//#define DEBUG_MICROUDP_TX
//#define DEBUG_MICROUDP_RX

#define ETHERTYPE_ARP 0x0806
#define ETHERTYPE_IP  0x0800

#ifdef CSR_ETHMAC_PREAMBLE_CRC_ADDR
#define HW_PREAMBLE_CRC
#endif

struct ethernet_header {
#ifndef HW_PREAMBLE_CRC
    uint8_t preamble[8];
#endif
    uint8_t destmac[6];
    uint8_t srcmac[6];
    uint16_t ethertype;
} __attribute__((packed));

#define ARP_HWTYPE_ETHERNET 0x0001
#define ARP_PROTO_IP        0x0800
#ifndef HW_PREAMBLE_CRC
#define ARP_PACKET_LENGTH 68
#else
#define ARP_PACKET_LENGTH 60
#endif

#define ARP_OPCODE_REQUEST  0x0001
#define ARP_OPCODE_REPLY    0x0002

struct arp_frame {
    uint16_t hwtype;
    uint16_t proto;
    uint8_t hwsize;
    uint8_t protosize;
    uint16_t opcode;
    uint8_t sender_mac[6];
    uint32_t sender_ip;
    uint8_t target_mac[6];
    uint32_t target_ip;
    uint8_t padding[18];
} __attribute__((packed));

#define IP_IPV4          0x45
#define IP_DONT_FRAGMENT 0x4000
#define IP_TTL           64
#define IP_PROTO_UDP     0x11

struct ip_header {
    uint8_t version;
    uint8_t diff_services;
    uint16_t total_length;
    uint16_t identification;
    uint16_t fragment_offset;
    uint8_t ttl;
    uint8_t proto;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed));

struct udp_header {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed));

struct udp_frame {
    struct ip_header ip;
    struct udp_header udp;
    char payload[];
} __attribute__((packed));

struct ethernet_frame {
    struct ethernet_header eth_header;
    union {
        struct arp_frame arp;
        struct udp_frame udp;
    } contents;
} __attribute__((packed));

typedef union {
    struct ethernet_frame frame;
    uint8_t raw[ETHMAC_SLOT_SIZE];
} ethernet_buffer;

struct pseudo_header {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t zero;
    uint8_t proto;
    uint16_t length;
} __attribute__((packed));
