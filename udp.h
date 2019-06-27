#pragma once

#include <stdint.h>

#define IPTOINT(a, b, c, d) ((a << 24)|(b << 16)|(c << 8)|d)

#define MICROUDP_BUFSIZE (5*1532)

namespace udp {
typedef void (*udp_callback)(uint32_t src_ip, uint16_t src_port, uint16_t dst_port, void *data, uint32_t length);

void service(void);
void eth_init(void);
void eth_mode(void);
void start(const uint8_t *macaddr, uint32_t ip);
inline void delayms(int ms){
    // todo - use timer ?
    for( int i=0; i<ms*100000; i++ ){
    }
}

int microudp_arp_resolve(uint32_t ip);
uint8_t* microudp_get_tx_buffer(void);
int microudp_send(uint16_t src_port, uint16_t dst_port, uint32_t length);
void microudp_set_callback(udp_callback callback);

///////////////////////////////////////////////////////////////////////////////
struct ScopedUdpCallback {
    inline ScopedUdpCallback( udp_callback cb ) noexcept
        : _callback(cb) {
            microudp_set_callback(_callback);
        }
    inline ~ScopedUdpCallback() noexcept {
        microudp_set_callback(nullptr);
    }
    udp_callback _callback;
};
///////////////////////////////////////////////////////////////////////////////
}
