SECTIONS
{
	.uk_fs_list : {
		. = ALIGN(0x1000);
		PROVIDE(uk_fslist_start = .);
		KEEP (*(.uk_fs_list))
		. = ALIGN(0x1000);
                PROVIDE(uk_fslist_end = .);
	}
}
INSERT AFTER .text;
