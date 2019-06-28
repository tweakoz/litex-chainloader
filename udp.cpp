#include "udp.h"
#include "udp_impl.h"

namespace udp {

#ifdef CSR_ETHMAC_BASE

static uint8_t _my_mac[6] = {0,0,0,0,0,0};
static uint32_t _my_ip = 0;

/* ARP cache - one entry only */
static uint8_t _cached_mac[6] = {0,0,0,0,0,0};
static uint32_t _cached_ip = 0;
static uint32_t _rxslot = 0;
static uint32_t _rxlen = 0;
static ethernet_buffer* _rxbuffer = nullptr;

static uint32_t _txslot = 0;
static uint32_t _txlen = 0;
static ethernet_buffer* _txbuffer = nullptr;

////////////////////////////////////////////////////////////////////////////////

static void fill_eth_header( ethernet_header* h,
                             const uint8_t* destmac,
                             const uint8_t* srcmac,
                             uint16_t ethertype ) {

#ifndef HW_PREAMBLE_CRC
    for(int i=0;i<7;i++)
        h->preamble[i] = 0x55;
    h->preamble[7] = 0xd5;
#endif

    for(int i=0;i<6;i++)
        h->destmac[i] = destmac[i];
    for(int i=0;i<6;i++)
        h->srcmac[i] = srcmac[i];

    h->ethertype = htons(ethertype);
}

////////////////////////////////////////////////////////////////////////////////

static void send_packet(void)
{
    /* wait buffer to be available */
    while(!(ethmac_sram_reader_ready_read()));

    /* fill _txbuffer */
#ifndef HW_PREAMBLE_CRC
    uint32_t crc;
    crc = crc32(&_txbuffer->raw[8], _txlen-8);
    _txbuffer->raw[_txlen  ] = (crc & 0xff);
    _txbuffer->raw[_txlen+1] = (crc & 0xff00) >> 8;
    _txbuffer->raw[_txlen+2] = (crc & 0xff0000) >> 16;
    _txbuffer->raw[_txlen+3] = (crc & 0xff000000) >> 24;
    _txlen += 4;
#endif

#ifdef DEBUG_MICROUDP_TX
    int j;
    printf(">>>> _txlen : %d\n", _txlen);
    for(j=0;j<_txlen;j++)
        printf("%02x",_txbuffer->raw[j]);
    printf("\n");
#endif

    /* fill slot, length and send */
    ethmac_sram_reader_slot_write(_txslot);
    ethmac_sram_reader_length_write(_txlen);
    ethmac_sram_reader_start_write(1);

    /* update _txslot / _txbuffer */
    _txslot = (_txslot+1)%ETHMAC_TX_SLOTS;
    _txbuffer = (ethernet_buffer *)(ETHMAC_BASE + ETHMAC_SLOT_SIZE * (ETHMAC_RX_SLOTS + _txslot));
}

////////////////////////////////////////////////////////////////////////////////

static void process_arp(void)
{
    const arp_frame *rx_arp = &_rxbuffer->frame.contents.arp;
    arp_frame *tx_arp = &_txbuffer->frame.contents.arp;

    if(_rxlen < ARP_PACKET_LENGTH) return;
    if(ntohs(rx_arp->hwtype) != ARP_HWTYPE_ETHERNET) return;
    if(ntohs(rx_arp->proto) != ARP_PROTO_IP) return;
    if(rx_arp->hwsize != 6) return;
    if(rx_arp->protosize != 4) return;

    if(ntohs(rx_arp->opcode) == ARP_OPCODE_REPLY) {
        if(ntohl(rx_arp->sender_ip) == _cached_ip) {
            int i;
            for(i=0;i<6;i++)
                _cached_mac[i] = rx_arp->sender_mac[i];
        }
        return;
    }
    if(ntohs(rx_arp->opcode) == ARP_OPCODE_REQUEST) {
        if(ntohl(rx_arp->target_ip) == _my_ip) {
            int i;

            fill_eth_header(&_txbuffer->frame.eth_header,
                rx_arp->sender_mac,
                _my_mac,
                ETHERTYPE_ARP);
            _txlen = ARP_PACKET_LENGTH;
            tx_arp->hwtype = htons(ARP_HWTYPE_ETHERNET);
            tx_arp->proto = htons(ARP_PROTO_IP);
            tx_arp->hwsize = 6;
            tx_arp->protosize = 4;
            tx_arp->opcode = htons(ARP_OPCODE_REPLY);
            tx_arp->sender_ip = htonl(_my_ip);
            for(i=0;i<6;i++)
                tx_arp->sender_mac[i] = _my_mac[i];
            tx_arp->target_ip = htonl(ntohl(rx_arp->sender_ip));
            for(i=0;i<6;i++)
                tx_arp->target_mac[i] = rx_arp->sender_mac[i];
            send_packet();
        }
        return;
    }
}

////////////////////////////////////////////////////////////////////////////////

int arp_resolve(uint32_t ip)
{
    static const uint8_t broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    if(_cached_ip == ip) {
        for(int i=0;i<6;i++)
            if(_cached_mac[i]) return 1;
    }
    _cached_ip = ip;
    for(int i=0;i<6;i++)
        _cached_mac[i] = 0;
    for(int tries=0;tries<100;tries++) {
        /* Send an ARP request */
        fill_eth_header(&_txbuffer->frame.eth_header,
                broadcast,
                _my_mac,
                ETHERTYPE_ARP);
        _txlen = ARP_PACKET_LENGTH;
        auto arp = &_txbuffer->frame.contents.arp;
        arp->hwtype = htons(ARP_HWTYPE_ETHERNET);
        arp->proto = htons(ARP_PROTO_IP);
        arp->hwsize = 6;
        arp->protosize = 4;
        arp->opcode = htons(ARP_OPCODE_REQUEST);
        arp->sender_ip = htonl(_my_ip);
        for(int i=0;i<6;i++)
            arp->sender_mac[i] = _my_mac[i];
        arp->target_ip = htonl(ip);
        for(int i=0;i<6;i++)
            arp->target_mac[i] = 0;

        send_packet();

        /* Do we get a reply ? */
        for(int timeout=0;timeout<100000;timeout++) {
            service();
            for(int i=0;i<6;i++)
                if(_cached_mac[i]) return 1;
        }
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////

static uint16_t ip_checksum(uint32_t r, void *buffer, size_t length, int complete)
{
    length >>= 1;

    auto ptr = (uint8_t*) buffer;

    for(size_t i=0;i<length;i++)
        r += (uint32_t(ptr[2*i]) << 8) | uint32_t(ptr[2*i+1]) ;

    /* Add overflows */
    while(r >> 16)
        r = (r & 0xffff) + (r >> 16);

    if(complete) {
        r = ~r;
        r &= 0xffff;
        if(r == 0) r = 0xffff;
    }
    return uint16_t(r); // looks like we force this down to 16bits up above
}

////////////////////////////////////////////////////////////////////////////////

uint8_t* get_tx_buffer(void)
{
    return (uint8_t*) _txbuffer->frame.contents.udp.payload;
}

////////////////////////////////////////////////////////////////////////////////

int send(uint16_t src_port, uint16_t dst_port, uint32_t length)
{
    if((_cached_mac[0] == 0) and
       (_cached_mac[1] == 0) and
       (_cached_mac[2] == 0) and
       (_cached_mac[3] == 0) and
       (_cached_mac[4] == 0) and
       (_cached_mac[5] == 0))
        return 0;

    _txlen = length + sizeof(ethernet_header) + sizeof(udp_frame);

    if(_txlen < ARP_PACKET_LENGTH)
       _txlen = ARP_PACKET_LENGTH;

    fill_eth_header(&_txbuffer->frame.eth_header,
                    _cached_mac,
                    _my_mac,
                    ETHERTYPE_IP);

    pseudo_header h;

    _txbuffer->frame.contents.udp.ip.version = IP_IPV4;
    _txbuffer->frame.contents.udp.ip.diff_services = 0;
    _txbuffer->frame.contents.udp.ip.total_length = htons(length + sizeof(udp_frame));
    _txbuffer->frame.contents.udp.ip.identification = htons(0);
    _txbuffer->frame.contents.udp.ip.fragment_offset = htons(IP_DONT_FRAGMENT);
    _txbuffer->frame.contents.udp.ip.ttl = IP_TTL;
    h.proto = _txbuffer->frame.contents.udp.ip.proto = IP_PROTO_UDP;
    _txbuffer->frame.contents.udp.ip.checksum = 0;
    h.src_ip = _txbuffer->frame.contents.udp.ip.src_ip = htonl(_my_ip);
    h.dst_ip = _txbuffer->frame.contents.udp.ip.dst_ip = htonl(_cached_ip);
    _txbuffer->frame.contents.udp.ip.checksum = htons(ip_checksum(0, &_txbuffer->frame.contents.udp.ip,
        sizeof(ip_header), 1));

    _txbuffer->frame.contents.udp.udp.src_port = htons(src_port);
    _txbuffer->frame.contents.udp.udp.dst_port = htons(dst_port);
    h.length = _txbuffer->frame.contents.udp.udp.length = htons(length + sizeof(udp_header));
    _txbuffer->frame.contents.udp.udp.checksum = 0;

    h.zero = 0;

    uint32_t r = ip_checksum(0, &h, sizeof(pseudo_header), 0);
    if(length & 1) {
        _txbuffer->frame.contents.udp.payload[length] = 0;
        length++;
    }
    r = ip_checksum(r, &_txbuffer->frame.contents.udp.udp,
        sizeof(udp_header)+length, 1);
    _txbuffer->frame.contents.udp.udp.checksum = htons(r);

    send_packet();

    return 1;
}

////////////////////////////////////////////////////////////////////////////////

static udp_callback_t rx_callback;

static void process_ip(void)
{
    if(_rxlen < (sizeof(ethernet_header)+sizeof(udp_frame))) return;
    udp_frame *udp_ip = &_rxbuffer->frame.contents.udp;
    /* We don't verify UDP and IP checksums and rely on the Ethernet checksum solely */
    if(udp_ip->ip.version != IP_IPV4) return;
    // check disabled for QEMU compatibility
    //if(_rxbuffer->frame.contents.udp.ip.diff_services != 0) return;
    if(ntohs(udp_ip->ip.total_length) < sizeof(udp_frame)) return;
    // check disabled for QEMU compatibility
    //if(ntohs(_rxbuffer->frame.contents.udp.ip.fragment_offset) != IP_DONT_FRAGMENT) return;
    if(udp_ip->ip.proto != IP_PROTO_UDP) return;
    if(ntohl(udp_ip->ip.dst_ip) != _my_ip) return;
    if(ntohs(udp_ip->udp.length) < sizeof(udp_header)) return;

    if(rx_callback)
        rx_callback( ntohl(udp_ip->ip.src_ip),
                     ntohs(udp_ip->udp.src_port),
                     ntohs(udp_ip->udp.dst_port),
                     udp_ip->payload,
                     ntohs(udp_ip->udp.length)-sizeof(udp_header));
}

////////////////////////////////////////////////////////////////////////////////

void set_callback(udp_callback_t callback)
{
    rx_callback = callback;
}

////////////////////////////////////////////////////////////////////////////////

static void process_frame(void)
{
    flush_cpu_dcache();

    #ifdef DEBUG_MICROUDP_RX
    printf("<<< _rxlen : %d\n", _rxlen);
    for(int j=0;j<_rxlen;j++)
        printf("%02x", _rxbuffer->raw[j]);
    printf("\n");
    #endif

    #ifndef HW_PREAMBLE_CRC
    for(int i=0;i<7;i++)
        if(_rxbuffer->frame.eth_header.preamble[i] != 0x55) return;
    if(_rxbuffer->frame.eth_header.preamble[7] != 0xd5) return;
    #endif

    #ifndef HW_PREAMBLE_CRC

    uint32_t received_crc = ((uint32_t)_rxbuffer->raw[_rxlen-1] << 24)
        |((uint32_t)_rxbuffer->raw[_rxlen-2] << 16)
        |((uint32_t)_rxbuffer->raw[_rxlen-3] <<  8)
        |((uint32_t)_rxbuffer->raw[_rxlen-4]);

    uint32_t computed_crc = crc32(&_rxbuffer->raw[8], _rxlen-12);

    if(received_crc != computed_crc) return;

    _rxlen -= 4; /* strip CRC here to be consistent with TX */
    #endif

    if(ntohs(_rxbuffer->frame.eth_header.ethertype) == ETHERTYPE_ARP) process_arp();
    else if(ntohs(_rxbuffer->frame.eth_header.ethertype) == ETHERTYPE_IP) process_ip();
}

////////////////////////////////////////////////////////////////////////////////

void start(const uint8_t* macaddr, uint32_t ip)
{
    ethmac_sram_reader_ev_pending_write(ETHMAC_EV_SRAM_READER);
    ethmac_sram_writer_ev_pending_write(ETHMAC_EV_SRAM_WRITER);

    for(int i=0;i<6;i++)
        _my_mac[i] = macaddr[i];
    _my_ip = ip;

    _cached_ip = 0;
    for(int i=0;i<6;i++)
        _cached_mac[i] = 0;

    _txslot = 0;
    ethmac_sram_reader_slot_write(_txslot);
    _txbuffer = (ethernet_buffer*) (ETHMAC_BASE + ETHMAC_SLOT_SIZE * (ETHMAC_RX_SLOTS + _txslot));

    _rxslot = 0;
    _rxbuffer = (ethernet_buffer*) (ETHMAC_BASE + ETHMAC_SLOT_SIZE * _rxslot);
    rx_callback = nullptr;
}

////////////////////////////////////////////////////////////////////////////////

void service(void)
{
    if(ethmac_sram_writer_ev_pending_read() & ETHMAC_EV_SRAM_WRITER) {
        _rxslot = ethmac_sram_writer_slot_read();
        _rxbuffer = (ethernet_buffer *)(ETHMAC_BASE + ETHMAC_SLOT_SIZE * _rxslot);
        _rxlen = ethmac_sram_writer_length_read();
        process_frame();
        ethmac_sram_writer_ev_pending_write(ETHMAC_EV_SRAM_WRITER);
    }
}

////////////////////////////////////////////////////////////////////////////////

void eth_init(void)
{
    #ifdef CSR_ETHPHY_CRG_RESET_ADDR
    ethphy_crg_reset_write(1);
    delayms(200);
    ethphy_crg_reset_write(0);
    delayms(200);
    #endif
}

////////////////////////////////////////////////////////////////////////////////

void eth_mode(void)
{
    #ifdef CSR_ETHPHY_MODE_DETECTION_MODE_ADDR
    printf("Ethernet phy mode: ");
    if (ethphy_mode_detection_mode_read())
        printf("MII");
    else
        printf("GMII");
    printf("\n");
    #endif // #ifdef CSR_ETHPHY_MODE_DETECTION_MODE_ADDR
}

////////////////////////////////////////////////////////////////////////////////
#endif // #ifdef CSR_ETHMAC_BASE

} // namespace udp
