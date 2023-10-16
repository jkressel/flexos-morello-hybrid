deps_config := \
	/root/.unikraft/apps/flexos-example/Config.uk \
	/root/.unikraft/libs/lib-libsodium/Config.uk \
	/root/.unikraft/unikraft/lib//posix-event/Config.uk \
	/root/.unikraft/libs/tlsf/Config.uk \
	/root/.unikraft/libs/flexos-example/Config.uk \
	/root/.unikraft/libs/newlib/Config.uk \
	/root/.unikraft/unikraft/lib/Config.uk \
	/root/.unikraft/unikraft/lib//vfscore/Config.uk \
	/root/.unikraft/unikraft/lib//uktimeconv/Config.uk \
	/root/.unikraft/unikraft/lib//uktime/Config.uk \
	/root/.unikraft/unikraft/lib//ukswrand/Config.uk \
	/root/.unikraft/unikraft/lib//uksp/Config.uk \
	/root/.unikraft/unikraft/lib//uksignal/Config.uk \
	/root/.unikraft/unikraft/lib//uksglist/Config.uk \
	/root/.unikraft/unikraft/lib//ukschedcoop/Config.uk \
	/root/.unikraft/unikraft/lib//uksched/Config.uk \
	/root/.unikraft/unikraft/lib//ukring/Config.uk \
	/root/.unikraft/unikraft/lib//uknetdev/Config.uk \
	/root/.unikraft/unikraft/lib//ukmpi/Config.uk \
	/root/.unikraft/unikraft/lib//ukmmap/Config.uk \
	/root/.unikraft/unikraft/lib//uklock/Config.uk \
	/root/.unikraft/unikraft/lib//uklibparam/Config.uk \
	/root/.unikraft/unikraft/lib//ukdebug/Config.uk \
	/root/.unikraft/unikraft/lib//ukbus/Config.uk \
	/root/.unikraft/unikraft/lib//ukboot/Config.uk \
	/root/.unikraft/unikraft/lib//ukblkdev/Config.uk \
	/root/.unikraft/unikraft/lib//ukargparse/Config.uk \
	/root/.unikraft/unikraft/lib//ukallocregion/Config.uk \
	/root/.unikraft/unikraft/lib//ukallocpool/Config.uk \
	/root/.unikraft/unikraft/lib//ukallocbbuddy/Config.uk \
	/root/.unikraft/unikraft/lib//ukalloc/Config.uk \
	/root/.unikraft/unikraft/lib//uk9p/Config.uk \
	/root/.unikraft/unikraft/lib//ubsan/Config.uk \
	/root/.unikraft/unikraft/lib//syscall_shim/Config.uk \
	/root/.unikraft/unikraft/lib//ramfs/Config.uk \
	/root/.unikraft/unikraft/lib//posix-user/Config.uk \
	/root/.unikraft/unikraft/lib//posix-sysinfo/Config.uk \
	/root/.unikraft/unikraft/lib//posix-process/Config.uk \
	/root/.unikraft/unikraft/lib//posix-mmap/Config.uk \
	/root/.unikraft/unikraft/lib//posix-libdl/Config.uk \
	/root/.unikraft/unikraft/lib//nolibc/Config.uk \
	/root/.unikraft/unikraft/lib//kasan/Config.uk \
	/root/.unikraft/unikraft/lib//flexos-core/Config.uk \
	/root/.unikraft/unikraft/lib//fdt/Config.uk \
	/root/.unikraft/unikraft/lib//devfs/Config.uk \
	/root/.unikraft/unikraft/lib//9pfs/Config.uk \
	/root/.unikraft/apps/flexos-example/build/kconfig/libs.uk \
	/root/.unikraft/unikraft/plat/Config.uk \
	/root/.unikraft/unikraft/plat//xen/Config.uk \
	/root/.unikraft/unikraft/plat//morello/Config.uk \
	/root/.unikraft/unikraft/plat//linuxu/Config.uk \
	/root/.unikraft/unikraft/plat//kvm/Config.uk \
	/root/.unikraft/apps/flexos-example/build/kconfig/plat.uk \
	/root/.unikraft/unikraft/arch/arm/arm64/Config.uk \
	/root/.unikraft/unikraft/arch/arm/arm/Config.uk \
	/root/.unikraft/unikraft/arch/x86/x86_64/Config.uk \
	/root/.unikraft/unikraft/arch/Config.uk \
	/root/.unikraft/unikraft/Config.uk

/root/.unikraft/apps/flexos-example/build/kconfig/auto.conf: \
	$(deps_config)

ifneq "$(UK_FULLVERSION)" "0.5.0~9edaa33-custom"
/root/.unikraft/apps/flexos-example/build/kconfig/auto.conf: FORCE
endif
ifneq "$(UK_CODENAME)" "Tethys"
/root/.unikraft/apps/flexos-example/build/kconfig/auto.conf: FORCE
endif
ifneq "$(UK_ARCH)" "arm64"
/root/.unikraft/apps/flexos-example/build/kconfig/auto.conf: FORCE
endif
ifneq "$(UK_BASE)" "/root/.unikraft/unikraft"
/root/.unikraft/apps/flexos-example/build/kconfig/auto.conf: FORCE
endif
ifneq "$(UK_APP)" "/root/.unikraft/apps/flexos-example"
/root/.unikraft/apps/flexos-example/build/kconfig/auto.conf: FORCE
endif
ifneq "$(UK_NAME)" "flexos-example"
/root/.unikraft/apps/flexos-example/build/kconfig/auto.conf: FORCE
endif
ifneq "$(CC)" "/root/llvm-project-releases/bin/clang"
/root/.unikraft/apps/flexos-example/build/kconfig/auto.conf: FORCE
endif
ifneq "$(KCONFIG_PLAT_DIR)" "/root/.unikraft/unikraft/plat//kvm /root/.unikraft/unikraft/plat//linuxu /root/.unikraft/unikraft/plat//morello /root/.unikraft/unikraft/plat//xen  /root/.unikraft/unikraft/plat/"
/root/.unikraft/apps/flexos-example/build/kconfig/auto.conf: FORCE
endif
ifneq "$(KCONFIG_PLAT_IN)" "/root/.unikraft/apps/flexos-example/build/kconfig/plat.uk"
/root/.unikraft/apps/flexos-example/build/kconfig/auto.conf: FORCE
endif
ifneq "$(KCONFIG_LIB_DIR)" "/root/.unikraft/unikraft/lib//9pfs /root/.unikraft/unikraft/lib//cpio /root/.unikraft/unikraft/lib//devfs /root/.unikraft/unikraft/lib//fdt /root/.unikraft/unikraft/lib//flexos-core /root/.unikraft/unikraft/lib//kasan /root/.unikraft/unikraft/lib//nolibc /root/.unikraft/unikraft/lib//posix-event /root/.unikraft/unikraft/lib//posix-libdl /root/.unikraft/unikraft/lib//posix-mmap /root/.unikraft/unikraft/lib//posix-process /root/.unikraft/unikraft/lib//posix-sysinfo /root/.unikraft/unikraft/lib//posix-user /root/.unikraft/unikraft/lib//ramfs /root/.unikraft/unikraft/lib//syscall_shim /root/.unikraft/unikraft/lib//ubsan /root/.unikraft/unikraft/lib//uk9p /root/.unikraft/unikraft/lib//ukalloc /root/.unikraft/unikraft/lib//ukallocbbuddy /root/.unikraft/unikraft/lib//ukallocpool /root/.unikraft/unikraft/lib//ukallocregion /root/.unikraft/unikraft/lib//ukargparse /root/.unikraft/unikraft/lib//ukblkdev /root/.unikraft/unikraft/lib//ukboot /root/.unikraft/unikraft/lib//ukbus /root/.unikraft/unikraft/lib//ukdebug /root/.unikraft/unikraft/lib//uklibparam /root/.unikraft/unikraft/lib//uklock /root/.unikraft/unikraft/lib//ukmmap /root/.unikraft/unikraft/lib//ukmpi /root/.unikraft/unikraft/lib//uknetdev /root/.unikraft/unikraft/lib//ukring /root/.unikraft/unikraft/lib//uksched /root/.unikraft/unikraft/lib//ukschedcoop /root/.unikraft/unikraft/lib//uksglist /root/.unikraft/unikraft/lib//uksignal /root/.unikraft/unikraft/lib//uksp /root/.unikraft/unikraft/lib//ukswrand /root/.unikraft/unikraft/lib//uktime /root/.unikraft/unikraft/lib//uktimeconv /root/.unikraft/unikraft/lib//vfscore /root/.unikraft/unikraft/lib /root/.unikraft/libs/newlib /root/.unikraft/libs/flexos-example /root/.unikraft/libs/lib-libsodium"
/root/.unikraft/apps/flexos-example/build/kconfig/auto.conf: FORCE
endif
ifneq "$(KCONFIG_LIB_IN)" "/root/.unikraft/apps/flexos-example/build/kconfig/libs.uk"
/root/.unikraft/apps/flexos-example/build/kconfig/auto.conf: FORCE
endif
ifneq "$(KCONFIG_APP_DIR)" "/root/.unikraft/apps/flexos-example"
/root/.unikraft/apps/flexos-example/build/kconfig/auto.conf: FORCE
endif

$(deps_config): ;
