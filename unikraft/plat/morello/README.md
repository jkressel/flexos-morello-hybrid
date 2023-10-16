# plat-morello

# Morello platform

In order to compile an application that will run on the Morello, please
select/deselect the following options in the make menuconfig:
- Architecture Selection --> Architecture --> select: Armv8 compatible (64 bits)
- Architecture Selection --> Processor Optimization --> select: Generic Armv8 CPU
- Architecture Selection --> deselect: Workaround for Cortex-A73 erratum
- Platform Configuration --> select: Morello



## Steps to build:

1. Get the ARM GCC toolchain: https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
2. Get the ARM Morello LLVM toolchain, found here: https://git.morello-project.org/morello/llvm-project-releases/-/tree/morello/baremetal-release-1.5
3. Clone `uk-plat-morello` into `<PATH TO UNIKRAFT>/.unikraft/unikraft/plat/`
4. Add the line `$(eval $(call _import_lib,$(UK_PLAT_BASE)/uk-plat-morello))` to the end of `<PATH TO UNIKRAFT>/.unikraft/unikraft/plat/Makefile.uk`
5. Add the following to `LIBMORELLOPLAT_ASFLAGS` in `<PATH TO UNIKRAFT>.unikraft/unikraft/plat/uk-plat-morello/Makefile.uk`:

```
-target aarch64-none-elf -march=morello
```

6. Build using the following command:

```
make CC=<PATH TO YOUR LLVM TOOLCHAIN>/bin/clang LD=<PATH TO YOUR GCC TOOLCHAIN>/bin/aarch64-none-elf-gcc OBJCOPY=<PATH TO YOUR GCC TOOLCHAIN>/bin/aarch64-none-elf-objcopy STRIP=<PATH TO YOUR GCC TOOLCHAIN>/bin/aarch64-none-elf-strip CFLAGS+='-target aarch64-none-elf'
```

7. Finally, create a binary image which can be used on the Morello machine using the script `make-bm-image.sh`, provided as part of the Morello LLVM bare metal toolchain:

```
make-bm-image.sh -i <UK_IMAGE> -o img.bin
```

Once the unikraft image has been successfully built, we now need to actually run the image. The Morello bare metal README provides some description of the different approaches which can be taken: https://git.morello-project.org/morello/docs/-/blob/morello/mainline/standalone-baremetal-readme.rst 


## Running on the Morello Machine

Running our unikernel on the Morello machine requires us to complete a bit of configuration first. 

For this you will need to have the Morello machine connected to your host machine via serial and have flashed the onboard SD card with the required board firmware, instructions can be found here: https://developer.arm.com/documentation/den0132/0100/Flash-the-onboard-SD-card
Once this has been done, we get on to the actual process of loading the image:

1. Boot the board (nothing will happen but we need to connect to the SD card)
2. Navigate to the SD card and locate the following: `M1SDP/MB/HBI0364B/io_v010f.txt` and change the line beginning with `SOCCON: 0x1188` to 
```
SOCCON: 0x1188 0x14000000   ;SoC SCC BOOT_GPR2. 
```
This will tell AP to reset to 0x14000000 to begin executing the image

3. Copy the file `img.bin` which we created at the end of the build process into the directory `M1SDP/SOFTWARE/`
4. Next, in the file `M1SDP/MB/HBI0364B/images.txt` add an entry, also update `TOTALIMAGES` to `4`:

```
IMAGE3ADDRESS: 0x60000000
IMAGE3UPDATE: FORCE
IMAGE3FILE: \SOFTWARE\img.bin
```

That’s it! Reboot the Morello machine and connect to UART AP via serial, you should see the familiar Unikraft banner appear once the boot has completed! 


## Running using FVP

First obtain a copy of the Morello FVP and follow the install instructions: https://developer.arm.com/downloads/-/arm-ecosystem-fvps

Also make sure to follow instructions to clone the Morello repositories: https://git.morello-project.org/morello/docs/-/blob/morello/mainline/user-guide.rst

Once installed, the FVP requires an ELF file to execute. For this reason, modify `make-bm-image.sh` (https://git.morello-project.org/morello/llvm-project-releases/-/blob/morello/baremetal-release-1.5/make-bm-image.sh) to replace the line

```
$OBJCOPY $LOADER_TMP/image.elf $LOADER_TMP/output --output-target binary
```

with the line

```
$OBJCOPY $LOADER_TMP/image.elf $LOADER_TMP/output
```

Next, run the FVP model using the following command:

```
<PATH TO FVP>/models/Linux64_GCC-6.4/FVP_Morello --data Morello_Top.css.scp.armcortexm7ct=<MORELLO REPO>/bsp/rom-binaries/scp_romfw.bin@0x0 --data Morello_Top.css.mcp.armcortexm7ct=<MORELLO REPO>/bsp/rom-binaries/mcp_romfw.bin@0x0 -C Morello_Top.soc.scp_qspi_loader.fname=<MORELLO REPO>/output/fvp/firmware/scp_fw.bin -C Morello_Top.soc.mcp_qspi_loader.fname=<MORELLO REPO>/output/fvp/firmware/mcp_fw.bin -C css.scp.armcortexm7ct.INITVTOR=0x0 -C css.mcp.armcortexm7ct.INITVTOR=0x0 -C soc.scc.boot_gpr_2=0x14000000 -C soc.scc.boot_gpr_3=0 -C css.cluster0.cpu0.semihosting-stack_base=0xffff0000 -C css.cluster0.cpu0.semihosting-stack_limit=0xff000000 -C css.cluster0.cpu0.semihosting-heap_limit=0xff000000 -C css.cluster0.cpu0.semihosting-heap_base=0 --application Morello_Top.css.cluster0.cpu0=<BARE METAL IMAGE>
```


## Notes

Runs with capabilities enabled in Morello 'hybrid' mode.

Please note, this is still very much an early WIP. Most features are not yet present.
