INCLUDE generated/output_format.ld
ENTRY(_start)

__DYNAMIC = 0;

INCLUDE generated/regions.ld

MEMORY
{
    chloader_loc : ORIGIN = 0xcff00000, LENGTH = 0x00100000
}

SECTIONS
{
	.text :
	{
		_ftext = .;
		*(.text .stub .text.* .gnu.linkonce.t.*)
		_etext = .;
	} > chloader_loc


	.got :
	{
		_GLOBAL_OFFSET_TABLE_ = .;
		*(.got)
	} > chloader_loc

	.got.plt :
	{
		*(.got.plt)
	} > chloader_loc

	.rodata :
	{
		. = ALIGN(4);
		_frodata = .;
		*(.rodata .rodata.* .gnu.linkonce.r.*)
		*(.rodata1)
		_erodata = .;
	} > chloader_loc

	.data :
	{
		. = ALIGN(4);
		_fdata = .;
		*(.data .data.* .gnu.linkonce.d.*)
		*(.data1)
		*(.sdata .sdata.* .gnu.linkonce.s.*)
		_edata = .;
	} > chloader_loc

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
	} > chloader_loc
}

PROVIDE(_fstack = ORIGIN(chloader_loc) + LENGTH(chloader_loc) - 4);
