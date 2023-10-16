SECTIONS
{
	.data_comp1 : {
		* (.data .data.* .rodata .rodata.*)
	}
}
INSERT AFTER .text;

SECTIONS
{
	.bss_comp1 : {
		* (.bss .bss.* COMMON)
	}
}
INSERT AFTER .data;

SECTIONS
{
	.initarray_comp1 : {
		* (.initarray .initarray.*)
	}
}

/* discard rules would come here */
