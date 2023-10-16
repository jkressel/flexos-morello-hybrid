SECTIONS
{
	.data_comp2 : {
		* (.data .data.* .rodata .rodata.*)
	}
}
INSERT AFTER .text;

SECTIONS
{
	.bss_comp2 : {
		* (.bss .bss.* COMMON)
	}
}
INSERT AFTER .data;

SECTIONS
{
	.initarray_comp2 : {
		* (.initarray .initarray.*)
	}
}

/* discard rules would come here */