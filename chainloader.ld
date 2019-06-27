INCLUDE generated/output_format.ld
ENTRY(_start)

__DYNAMIC = 0;

INCLUDE generated/regions.ld

MEMORY
{
    CHAINLOADER_BIN : ORIGIN = 0xcff00000, LENGTH = 0x00100000
}

SECTIONS
{
	.text :
	{
		_ftext = .;
		*(.text .stub .text.* .gnu.linkonce.t.*)
		_etext = .;
	} > CHAINLOADER_BIN


	.got :
	{
		_GLOBAL_OFFSET_TABLE_ = .;
		*(.got)
	} > CHAINLOADER_BIN

	.got.plt :
	{
		*(.got.plt)
	} > CHAINLOADER_BIN

	.rodata :
	{
		. = ALIGN(4);
		_frodata = .;
		*(.rodata .rodata.* .gnu.linkonce.r.*)
		*(.rodata1)
		_erodata = .;
	} > CHAINLOADER_BIN

	.data :
	{
		. = ALIGN(4);
		_fdata = .;
		*(.data .data.* .gnu.linkonce.d.*)
		*(.data1)
		*(.sdata .sdata.* .gnu.linkonce.s.*)
		_edata = .;
	} > CHAINLOADER_BIN

	.bss :
	{
		. = ALIGN(4);
		_fbss = .;
		*(.dynsbss)
		*(.sbss .sbss.* .gnu.linkonce.sb.*)
		*(.scommon)
		*(.dynbss)
		*(.bss .bss.* .gnu.linkonce.b.*)
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
		. = ALIGN(8);
		_heapstart = .;
	} > CHAINLOADER_BIN
}

PROVIDE(_fstack = ORIGIN(CHAINLOADER_BIN) + LENGTH(CHAINLOADER_BIN) - 4);
