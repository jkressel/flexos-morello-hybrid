SECTIONS
{
	.data_comp0 : {
		* (.data .data.* .rodata .rodata.*)
	}
	.bss_comp0 : {
		* (.bss .bss.* COMMON)
	}
	.initarray_comp0 : {
		* (.initarray .initarray.*)
	}
}
INSERT AFTER .text;

/* discard rules would come here */