#include <generated/csr.h>
#include <generated/mem.h>
#include <uart.h>
#include <system.h>
#include <stdio.h>
#include <stdint.h>
#include <irq.h>

////////////////////////////////////////////////////////////////////////////////
extern "C" {
    void isr(void)
    {
		uint32_t irqs = irq_pending() & irq_getmask();
		if(irqs & (1 << UART_INTERRUPT))
			uart_isr();
    }
} // extern "C"
////////////////////////////////////////////////////////////////////////////////
int main(int c, char** argv)
{
    irq_setmask(0);
    irq_setie(1);
    uart_init();
	uart_sync();
    extern void _netboot(void);
	_netboot();
	return 0;
}
