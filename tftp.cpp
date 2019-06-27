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

static constexpr uint16_t PORT_IN = 7642;  // Local TFTP client port (arbitrary)
static constexpr int BLOCK_SIZE = 512;     // block size in bytes

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

static uint8_t* packet_data = nullptr;
static uint8_t* dst_buffer = nullptr;
static uint32_t total_length = 0;
static uint32_t expected_length = 0;
static int last_ack = 0; // signed, so we can use -1
static uint16_t data_port = 0;
static bool transfer_finished = false;

/////////////////////////////////////////////////////////

static void rx_callback( uint32_t src_ip,
                         uint16_t src_port,
                         uint16_t dst_port,
                         void* _data,
                         uint32_t length ) {

    ////////////////////////
    // ?
    ////////////////////////

    if(length < 4) return;

    ////////////////////////
    // reject tftp packets that we did not originate
    ////////////////////////

    if(dst_port != PORT_IN) return;

    ////////////////////////
    // get opcode
    ////////////////////////

    auto datau8 = (uint8_t*) _data;
    auto opcode = uint16_t(datau8[0] << 8 | datau8[1]);
    auto block = uint16_t(datau8[2] << 8 | datau8[3]);

    ////////////////////////
    // ack ?
    ////////////////////////

    switch( opcode ){
        case TFTP_ACK:
            data_port = src_port;
            last_ack = block;
            return;
        case TFTP_DATA:
            if( block<1 ){
                printf( "bad block<%d>\n", block );
            }
            else {
                length -= 4;
                int offset = (block-1)*BLOCK_SIZE;
                for(uint32_t i=0;i<length;i++)
                    dst_buffer[offset+i] = datau8[i+4];
                total_length += length;
                //////////////////////////
                //printf("block<%d> offset<%zu> gotlen<%zu> totlen<%zu>\n", block, offset, length, total_length );
                //////////////////////////
                // the following is not enough to determine if done
                //   what happens if the file's size is an integer multiple of BLOCK_SIZE ??
                //////////////////////////
                if(length < BLOCK_SIZE)
                    transfer_finished = true;
                //////////////////////////
                // so we also terminate if all expected data has been sent
                //  and guess this only works if the length is known in advance..
                //////////////////////////
                if(expected_length!=0 and total_length==expected_length)
                    transfer_finished = true;
                //////////////////////////

                packet_data = udp::microudp_get_tx_buffer();
                length = format_ack(packet_data, block);
                udp::microudp_send(PORT_IN, src_port, length);
            }
            break;
        case TFTP_ERROR:
            total_length = -1;
            transfer_finished = true;
            break;
        default:
            assert(0);
            break;
    }
}

/////////////////////////////////////////////////////////

int get( uint32_t ip,
         uint16_t server_port,
         c_str_t  filename,
         uint8_t* outbuffer,
         uint32_t expected_len ) {

    printf( "tftp_get.a\n");

    if(!udp::microudp_arp_resolve(ip))
        return -1;

    printf( "tftp_get.b\n");

    udp::ScopedUdpCallback callback(rx_callback);

    printf( "tftp_get.c\n");

    dst_buffer = outbuffer;

    expected_length = expected_len;
    total_length = 0;
    transfer_finished = false;
    int tries = 5;
    while(1) {
        printf( "tftp_get.d\n");
        packet_data = udp::microudp_get_tx_buffer();
        int len = format_request(packet_data, TFTP_RRQ, filename);
        udp::microudp_send(PORT_IN, server_port, len);
        printf( "tftp_get.e\n");
        for(int i=0;i<2000000;i++) {
            udp::service();
            if((total_length > 0) or transfer_finished) break;
        }
        printf( "tftp_get.f\n");
        if((total_length > 0) or transfer_finished) break;
        tries--;
        if(tries == 0) {
            return -1;
        }
    }
    printf( "tftp_get.g\n");
    int i = 12000000;
    uint32_t length_before = total_length;
    while(!transfer_finished) {
        if(length_before != total_length) {
            i = 12000000;
            length_before = total_length;
        }
        if(i-- == 0) {
            return -1;
        }
        udp::service();
    }

    return int(total_length);
}

} //namespace tftp {
