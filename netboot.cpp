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
    extern void boot_helper(uint32_t r1, uint32_t r2, uint32_t r3, void* addr);
}
#include "udp.h"
#include "tftp.h"
#include "assert.h"
////////////////////////////////////////////////////////////////////////////////
namespace netboot {
////////////////////////////////////////////////////////////////////////////////
static auto KMANIFEST_STRING_LOC = (char*) 0xc8000000; // TODO - set from envvar
static const size_t KMANIFEST_STRING_SIZE = 1024; // 1024 bytes should be enough for everyone
static const int KLOCALIP[] = {__LOCALIP__};
static const int KREMOTEIP[] = {__REMOTEIP__};
static constexpr int KTFTP_SERVER_PORT = __TFTPPORT__;
static const uint8_t KMACADDR[6] = {0x10, 0xe2, 0xd5, 0x00, 0x00, 0x00}; // TODO - set from envvar
////////////////////////////////////////////////////////////////////////////////
static void __attribute__((noreturn)) _do_boot(uint32_t r1, uint32_t r2, uint32_t r3, void* addr)
{
    printf("\e[30;93m############################################\e[0m\n");
    printf("\e[30;93m# ChainLoading addr<0x%p>\e[0m\n", addr );
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
                           void* buffer,
                           size_t expectedlen )
{
    while(true){
        size_t r = tftp::get(remote_ip, remote_port, filename, buffer, expectedlen);
        if(expectedlen and r==expectedlen){
            printf("// Successfully downloaded %zu bytes from %s over TFTP\n", r, filename);
            return r;
        }
        else if(expectedlen==0 and r!=tftp::STATUS_FETCH_ERR){
            printf("// Successfully downloaded %zu bytes from %s over TFTP\n", r, filename);
            return r;
        }
        else
            printf("// Unable to download %s over TFTP, retrying...\n", filename);
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
////////////////////////////////////////////////////////////////////////////////
struct token {
    static constexpr size_t kmaxlen = 32;

    bool is(const char* othstr) const {
      bool rval = (0==strncmp(othstr,_buffer,kmaxlen));
      return rval;
    }

    char _buffer[kmaxlen];
};
////////////////////////////////////////////////////////////////////////////////
static size_t _tokenize( const char* inpstr,
                         token& the_token) {

    size_t slen = strlen(inpstr);
    char* brk = strpbrk(inpstr," \n");
    size_t brklen = strspn(brk," \n");
    size_t toklen = brk-inpstr;
    if( toklen >= token::kmaxlen ){
      printf("token overflow, uhoh...\n");
      assert(false); // TODO: get assert to working...
      //assert((brklen+toklen)<buflen);
    }
    _substring( inpstr,the_token._buffer,toklen);
    //printf( "token<%s> toklen<%zu> brklen<%zu> inpstr<%s> slen<%zu>\n", tokbuf, toklen, brklen, inpstr, slen );
    return brklen+toklen;
}
////////////////////////////////////////////////////////////////////////////////
void chainload(void)
{
    printf("///////////////////////////////\n");
    printf("// ChainLoading from network...\n");
    printf("// Local IP : %d.%d.%d.%d\n", KLOCALIP[0], KLOCALIP[1], KLOCALIP[2], KLOCALIP[3]);
    printf("// Remote IP: %d.%d.%d.%d\n", KREMOTEIP[0], KREMOTEIP[1], KREMOTEIP[2], KREMOTEIP[3]);

    uint32_t local_ip = udp::IPTOINT(KLOCALIP[0], KLOCALIP[1], KLOCALIP[2], KLOCALIP[3]);
    uint32_t remote_ip = udp::IPTOINT(KREMOTEIP[0], KREMOTEIP[1], KREMOTEIP[2], KREMOTEIP[3]);

    udp::eth_init();

    udp::start(KMACADDR,local_ip);
    uint16_t tftp_port = KTFTP_SERVER_PORT;
    printf("// Fetching from: UDP/%d\n", tftp_port);

    ////////////////////////////////////////////////////
    // parse manifest
    ////////////////////////////////////////////////////

    int boot_phase = 0;
    while(boot_phase==0){

        memset( KMANIFEST_STRING_LOC, 0, KMANIFEST_STRING_SIZE );

        size_t size = _tftp_get_v(remote_ip, tftp_port, "boot.manifest", (uint8_t*) KMANIFEST_STRING_LOC, 0 );
        if (size <= 0) {
            printf("// Network boot failed\n");
            return;
        }
        else if (size >= KMANIFEST_STRING_SIZE) {
            // TODO: its possible (though not likely) we could crash here...
            //    to prevent, _tftp_get_v would need buffer overflow protection
            printf("// Size too big!!!\n");
            return;
        }
        else{ // got manifest
            printf("///////////////////////////////\n");
            printf("// received manifest size<%d>\n", (int) size );
            printf("// received manifest data: \n");
            printf("///////////////////////////////\n");
            printf("%s\n", KMANIFEST_STRING_LOC);
            printf("//\n");
            /////////////////////////////////////////////////
            int done = 0;
            size_t tl = 0;
            while(0==done){
                token tok_command;
                tl += _tokenize(KMANIFEST_STRING_LOC+tl,tok_command);
                if(tok_command.is("download")){
                    token tok_filename, tok_addr, tok_len;
                    tl += _tokenize(KMANIFEST_STRING_LOC+tl,tok_filename);
                    tl += _tokenize(KMANIFEST_STRING_LOC+tl,tok_addr);
                    tl += _tokenize(KMANIFEST_STRING_LOC+tl,tok_len);
                    auto addr = (void*) strtoul(tok_addr._buffer,0,0);
                    size_t length = strtoul(tok_len._buffer,0,0);
                    printf( "// received download command filename<%s> addr<%s:%08x> len<%s:%d>\n", tok_filename._buffer,tok_addr._buffer,addr,tok_len._buffer, length );
                    int dl_done = 0;
                    while(0==dl_done){
                        int downloaded = _tftp_get_v(remote_ip, tftp_port, tok_filename._buffer, addr, length );
                        if (downloaded != length) {
                            printf("// tftp download<%s> failed. expectedlen<%d> received<%d>\n", tok_filename._buffer,length,downloaded);
                        }
                        else {
                            printf("// tftp download<%s> succeeded\n", tok_filename._buffer);
                            dl_done = 1;
                        }
                    }
                }
                else if(tok_command.is("boot")){
                    token tok_addr;
                    tl += _tokenize(KMANIFEST_STRING_LOC+tl,tok_addr);
                    auto addr = (void*) strtoul(tok_addr._buffer,0,0);
                    printf( "// received boot command addr<%s:%08x>\n", tok_addr._buffer,addr );
                    udp::delayms(1000);
                    uart_sync();
                    _do_boot(0, 0, 0, addr);
                }
                else if(tok_command.is("end")){
                    done = 1;
                    boot_phase = 1;
                    printf( "// received end command\n" );
                }
            }
            printf( "// DONE!!\n");
            printf( "///////////////////////////////\n");
            /////////////////////////////////////////////////
        } // else{ // got manifest
    } // while(boot_phase==0){
}

////////////////////////////////////////////////////////////////////////////////
} //namespace netboot {
////////////////////////////////////////////////////////////////////////////////
