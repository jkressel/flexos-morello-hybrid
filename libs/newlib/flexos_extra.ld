SECTIONS
{
	.data_comp0 : {
		* (.data .data.* .rodata .rodata.*)
	}
}
INSERT AFTER .text;

SECTIONS
{
	.bss_comp0 : {
		* (.bss .bss.* COMMON)
	}
}
INSERT AFTER .data;

SECTIONS
{
	.initarray_comp0 : {
		* (.initarray .initarray.*)
	}
}

/* discard rules would come here */
