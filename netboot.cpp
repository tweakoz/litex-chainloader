////////////////////////////////////////////////////////////////////////////////
extern "C" {
    #include <generated/csr.h>
    #include <generated/mem.h>
    #include <uart.h>
    #include <system.h>
    #include <stdio.h>
    #include <stdint.h>
    #include <stdlib.h>
    #include <string.h>
    #include <irq.h>
    extern void boot_helper(unsigned long r1, unsigned long r2, unsigned long r3, unsigned long addr);
}
#include "udp.h"
#include "tftp.h"
////////////////////////////////////////////////////////////////////////////////
static void __attribute__((noreturn)) boot(unsigned long r1, unsigned long r2, unsigned long r3, unsigned long addr)
{
    printf("\e[30;93m############################################\e[0m\n");
    printf("\e[30;93m# ChainLoading addr<0x%08x>\e[0m\n", addr );
    printf("\e[30;93m############################################\e[0m\n");
    uart_sync();
    irq_setmask(0);
    irq_setie(0);
    // FIXME: understand why flushing icache on Vexriscv make boot fail
    // flush_cpu_icache(); // __vexriscv__ does not like this
    flush_cpu_dcache();
    flush_l2_cache();
    boot_helper(r1, r2, r3, addr);
    while(1);
}
////////////////////////////////////////////////////////////////////////////////
static size_t _tftp_get_v( uint32_t remote_ip,
                           uint16_t remote_port,
                           const char* filename,
                           uint8_t* buffer,
                           size_t expectedlen )
{
    while(true){
        size_t r = tftp::get(remote_ip, remote_port, filename, buffer, expectedlen);
        printf( "got<%zu>\n", r );
        if(expectedlen and r==expectedlen){
            printf("Successfully downloaded %zu bytes from %s over TFTP\n", r, filename);
            return r;
        }
        else if(expectedlen==0 and r>0){
            printf("Successfully downloaded %zu bytes from %s over TFTP\n", r, filename);
            return r;
        }
        else
            printf("Unable to download %s over TFTP, retying...\n", filename);
    }
}
////////////////////////////////////////////////////////////////////////////////
static void _substring( const char* input_str,
                        char* output_str,
                        size_t length) {

    size_t index = 0;
    while( index<length) {
        output_str[index] = input_str[index];
        index++;
    }
    output_str[index] = '\0';
}

static size_t _tokenize( const char* inpstr,
                         char* tokbuf,
                         size_t buflen ){

    size_t slen = strlen(inpstr);
    char* brk = strpbrk(inpstr," \n");
    size_t brklen = strspn(brk," \n");
    size_t toklen = brk-inpstr;
    //assert((brklen+toklen)<buflen);
    _substring( inpstr,tokbuf,toklen );
    //printf( "token<%s> toklen<%zu> brklen<%zu> inpstr<%s> slen<%zu>\n", tokbuf, toklen, brklen, inpstr, slen );
    return brklen+toklen;
}

#define kmaxtoklen 32
struct token {
    char _buffer[kmaxtoklen];
};
////////////////////////////////////////////////////////////////////////////////
static const unsigned char _macadr[6] = {0x10, 0xe2, 0xd5, 0x00, 0x00, 0x00};
static const int LOCALIP[] = {10,0,0,1};
static const int REMOTEIP[] = {10,0,0,2};
static constexpr int TFTP_SERVER_PORT = 6069;
////////////////////////////////////////////////////////////////////////////////
void _netboot(void)
{
    printf("ChainLoading from network...\n");
    printf("Local IP : %d.%d.%d.%d\n", LOCALIP[0], LOCALIP[1], LOCALIP[2], LOCALIP[3]);
    printf("Remote IP: %d.%d.%d.%d\n", REMOTEIP[0], REMOTEIP[1], REMOTEIP[2], REMOTEIP[3]);

    uint32_t local_ip = IPTOINT(LOCALIP[0], LOCALIP[1], LOCALIP[2], LOCALIP[3]);
    uint32_t remote_ip = IPTOINT(REMOTEIP[0], REMOTEIP[1], REMOTEIP[2], REMOTEIP[3]);

    udp::eth_init();

    udp::start(_macadr,local_ip);
    uint16_t tftp_port = TFTP_SERVER_PORT;
    printf("Fetching from: UDP/%d\n", tftp_port);

    ////////////////////////////////////////////////////
    // parse manifest
    ////////////////////////////////////////////////////

    int boot_phase = 0;
    while(boot_phase==0){

        printf("f..\n");

        auto manifest_string = (char*) 0xc8000000;
        memset( manifest_string, 0, 1024 );
        memset( manifest_string, 0, 1024 );

        size_t size = _tftp_get_v(remote_ip, tftp_port, "boot.manifest", (uint8_t*) manifest_string, 0 );
        if (size <= 0) {
            printf("Network boot failed\n");
            return;
        }
        else if (size > 1023) {
            printf("Size too big!!!\n");
            return;
        }
        else{ // got manifest
            printf( "///////////////////////////////\n");
            printf("got manifest size<%d>\n", (int) size );
            printf("got manifest data<%s>\n", manifest_string);
            /////////////////////////////////////////////////
            int done = 0;
            size_t tl = 0;
            while(0==done){
                token tok_command;
                tl += _tokenize(manifest_string+tl,tok_command._buffer,kmaxtoklen);
                if(0==strcmp("download",tok_command._buffer)){
                    struct token tok_filename;
                    struct token tok_addr;
                    struct token tok_len;
                    tl += _tokenize(manifest_string+tl,tok_filename._buffer,kmaxtoklen);
                    tl += _tokenize(manifest_string+tl,tok_addr._buffer,kmaxtoklen);
                    tl += _tokenize(manifest_string+tl,tok_len._buffer,kmaxtoklen);
                    uint32_t addr = strtoul(tok_addr._buffer,0,0);
                    uint32_t length = strtoul(tok_len._buffer,0,0);
                    printf( "got download command filename<%s> addr<%s:%08x> len<%s:%d>\n", tok_filename._buffer,tok_addr._buffer,addr,tok_len._buffer, length );

                    int dl_done = 0;
                    while(0==dl_done){
                        int downloaded = _tftp_get_v(remote_ip, tftp_port, tok_filename._buffer, (uint8_t*) addr, length );
                        if (downloaded != length) {
                            printf("tftp download<%s> failed. expectedlen<%d> got<%d>\n", tok_filename._buffer,length,downloaded);
                        }
                        else {
                            printf("tftp download<%s> succeeded\n", tok_filename._buffer);
                            dl_done = 1;
                        }
                    }
                }
                else if(0==strcmp("boot",tok_command._buffer)){
                    struct token tok_addr;
                    tl += _tokenize(manifest_string+tl,tok_addr._buffer,kmaxtoklen);
                    uint32_t addr=strtoul(tok_addr._buffer,0,0);
                    printf( "got boot command addr<%s:%08x>\n", tok_addr._buffer,addr );
                    udp::delayms(1000);
                    uart_sync();
                    boot(0, 0, 0, addr);
                }
                else if(0==strcmp("end",tok_command._buffer)){
                    done = 1;
                    boot_phase = 1;
                    printf( "got end command\n" );
                }
            }
            printf( "DONE!!\n");
            printf( "///////////////////////////////\n");
            /////////////////////////////////////////////////
        } // else{ // got manifest
    } // while(boot_phase==0){
}
