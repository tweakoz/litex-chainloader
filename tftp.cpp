#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "udp.h"
#include "tftp.h"

extern "C"{
    void* __gxx_personality_v0 = nullptr;
}

namespace tftp {


/////////////////////////////////////////////////////////
// constants
/////////////////////////////////////////////////////////

static constexpr uint16_t KPORT_IN = 7642;  // Local TFTP client port (arbitrary)
static constexpr size_t KBLOCK_SIZE = 512;     // block size in bytes

/////////////////////////////////////////////////////////

enum {
    TFTP_RRQ    = 1,    /* Read request */
    TFTP_WRQ    = 2,     /* Write request */
    TFTP_DATA    = 3,    /* Data */
    TFTP_ACK    = 4,    /* Acknowledgment */
    TFTP_ERROR    = 5,    /* Error */
};

/////////////////////////////////////////////////////////

static int format_request(uint8_t *buf, uint16_t op, const char *filename)
{
    int len = strlen(filename);

    *buf++ = op >> 8; /* Opcode */
    *buf++ = op;
    memcpy(buf, filename, len);
    buf += len;
    *buf++ = 0x00;
    *buf++ = 'o';
    *buf++ = 'c';
    *buf++ = 't';
    *buf++ = 'e';
    *buf++ = 't';
    *buf++ = 0x00;
    return 9+strlen(filename);
}

/////////////////////////////////////////////////////////

static int format_ack(uint8_t* buf, uint16_t block)
{
    *buf++ = 0x00; /* Opcode: Ack */
    *buf++ = TFTP_ACK;
    *buf++ = (block & 0xff00) >> 8;
    *buf++ = (block & 0x00ff);
    return 4;
}

/////////////////////////////////////////////////////////

static int format_data(uint8_t* buf, uint16_t block, const void* data, int len)
{
    *buf++ = 0x00; /* Opcode: Data*/
    *buf++ = TFTP_DATA;
    *buf++ = (block & 0xff00) >> 8;
    *buf++ = (block & 0x00ff);
    memcpy(buf, data, len);
    return len+4;
}

/////////////////////////////////////////////////////////

static uint8_t* _packet_data = nullptr;
static uint8_t* _dst_buffer = nullptr;
static uint32_t _total_length = 0;
static uint32_t _expected_length = 0;
static int _last_ack = 0; // signed, so we can use -1
static uint16_t _data_port = 0;
static bool _transfer_finished = false;

/////////////////////////////////////////////////////////

static void rx_callback( uint32_t src_ip,
                         uint16_t src_port,
                         uint16_t dst_port,
                         void* data,
                         size_t length ) {

    ////////////////////////
    // ?
    ////////////////////////

    if(length < 4) return;

    ////////////////////////
    // reject tftp packets that we did not originate
    ////////////////////////

    if(dst_port != KPORT_IN) return;

    ////////////////////////
    // get opcode
    ////////////////////////

    auto datau8 = (uint8_t*) data;
    auto opcode = uint16_t(datau8[0] << 8 | datau8[1]);
    auto block = uint16_t(datau8[2] << 8 | datau8[3]);

    ////////////////////////
    // ack ?
    ////////////////////////

    switch( opcode ){
        case TFTP_ACK:
            _data_port = src_port;
            _last_ack = block;
            return;
        case TFTP_DATA:
            if( block<1 ){
                printf( "bad block<%d>\n", block );
            }
            else {
                length -= 4;
                int offset = (block-1)*KBLOCK_SIZE;
                for(size_t i=0;i<length;i++)
                    _dst_buffer[offset+i] = datau8[i+4];
                _total_length += length;
                //////////////////////////
                // the following is not enough to determine if done
                //   what happens if the file's size is an integer multiple of BLOCK_SIZE ??
                //////////////////////////
                if(length < KBLOCK_SIZE)
                    _transfer_finished = true;
                //////////////////////////
                // so we also terminate if all expected data has been sent
                //  and guess this only works if the length is known in advance..
                //////////////////////////
                if(_expected_length!=0 and _total_length==_expected_length)
                    _transfer_finished = true;
                //////////////////////////
                _packet_data = udp::get_tx_buffer();
                length = format_ack(_packet_data, block);
                udp::send(KPORT_IN, src_port, length);
            }
            break;
        case TFTP_ERROR:
            _total_length = -1;
            _transfer_finished = true;
            break;
        default:
            assert(0);
            break;
    }
}

/////////////////////////////////////////////////////////

size_t get( uint32_t ip,
            uint16_t server_port,
            cstr_t  filename,
            void* outbuffer,
            size_t expected_len ) {

    if(!udp::arp_resolve(ip))
        return STATUS_FETCH_ERR;

    udp::ScopedReceiveCallback callback(rx_callback);

    _dst_buffer = (uint8_t*) outbuffer;

    _expected_length = expected_len;
    _total_length = 0;
    _transfer_finished = false;
    int tries = 5;
    while(1) {
        _packet_data = udp::get_tx_buffer();
        int len = format_request(_packet_data, TFTP_RRQ, filename);
        udp::send(KPORT_IN, server_port, len);
        for(int i=0;i<2000000;i++) {
            udp::service();
            if((_total_length > 0) or _transfer_finished) break;
        }
        if((_total_length > 0) or _transfer_finished) break;
        tries--;
        if(tries == 0) {
            return STATUS_FETCH_ERR;
        }
    }
    int i = 12000000;
    uint32_t length_before = _total_length;
    while(!_transfer_finished) {
        if(length_before != _total_length) {
            i = 12000000;
            length_before = _total_length;
        }
        if(i-- == 0) {
            return STATUS_FETCH_ERR;
        }
        udp::service();
    }

    return _total_length;
}

/////////////////////////////////////////////////////////

} //namespace tftp {
