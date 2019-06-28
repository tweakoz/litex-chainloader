#pragma once

#include <stdint.h>
#include <stddef.h>

constexpr size_t MICROUDP_BUFSIZE = (5*1532);

namespace udp {

    inline uint32_t IPTOINT(uint8_t a, uint8_t  b, uint8_t  c, uint8_t  d) {
        return ((a << 24)|(b << 16)|(c << 8)|d);
    }
    inline void delayms(int ms){
        // todo - use timer ?
        for( int i=0; i<ms*100000; i++ ){
        }
    }

    ////////////////////////////////////////////////////////////////////////////

    void service(void);
    void eth_init(void);
    void eth_mode(void);
    void start(const uint8_t *macaddr, uint32_t ip);

    int arp_resolve(uint32_t ip);
    uint8_t* get_tx_buffer(void);
    int send(uint16_t src_port, uint16_t dst_port, uint32_t length);

    ////////////////////////////////////////////////////////////////////////////

    typedef void (*udp_callback_t)( uint32_t src_ip,
                                    uint16_t src_port,
                                    uint16_t dst_port,
                                    void *data,
                                    size_t length );

    void set_callback(udp_callback_t callback);

    ////////////////////////////////////////////////////////////////////////////
    // ScopedReceiveCallback : scope based udp callback setter
    //  resets callback to nullptr when it goes out of scope
    ////////////////////////////////////////////////////////////////////////////
    struct ScopedReceiveCallback {
        inline ScopedReceiveCallback( udp_callback_t cb ) noexcept
            : _callback(cb) {
                set_callback(_callback);
            }
        inline ~ScopedReceiveCallback() noexcept {
            set_callback(nullptr);
        }
        udp_callback_t _callback;
    };
    ////////////////////////////////////////////////////////////////////////////
}
