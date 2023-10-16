# FlexOS on ARM Morello in hybrid mode

This repo the source provides a version of [FlexOS](https://github.com/project-flexos/unikraft), a compartmentalisation-aware Unikernel which is extended to run in hybrid mode on the [ARM Morello](https://www.arm.com/architecture/cpu/morello) platform. To take advantage of the hardware capabilities offered by CHERI/Morello, a compartmentalisation backend has been implemented to allow the system to be partitioned into compartments in hybrid mode. Hybrid mode allows existing ARMv8 code to run alongside new capability aware instructions potentially lowering the effort needed to implement compartmentaisation in existing single address space software such as applications and Unikernels. 

We published a paper exploring the design space and trade-offs:

<em>John Alistair Kressel, Hugo Lefeuvre, and Pierre Olivier. 2023. Software Compartmentalization Trade-Offs with Hardware Capabilities. In Proceedings of the 12th Workshop on Programming Languages and Operating Systems (PLOS '23). Association for Computing Machinery, New York, NY, USA, 49–57.</em> DOI: [https://doi.org/10.1145/3623759.3624550](https://doi.org/10.1145/3623759.3624550)

> **Abstract**:  Compartmentalization is a form of defensive software design in which an application is broken down into isolated but communicating components. Retrofitting compartmentalization into existing applications is often thought to be expensive from the engineering effort and performance overhead points of view. Still, recent years have seen proposals of compartmentalization methods with promises of low engineering efforts and reduced performance impact. ARM Morello combines a modern ARM processor with an implementation of Capability Hardware Enhanced RISC Instructions (CHERI) aiming to provide efficient and secure compartmentalization. Past works exploring CHERI-based compartmentalization were restricted to emulated/FPGA prototypes.

> In this paper, we explore possible compartmentalization schemes with CHERI on the Morello chip. We propose two approaches representing different trade-offs in terms of engineering effort, security, scalability, and performance impact. We describe and implement these approaches on a prototype OS running bare metal on the Morello chip, compartmentalize two popular applications, and investigate the performance overheads. Furthermore, we show that compartmentalization can be achieved with an engineering cost that can be quite low if one is willing to trade off on scalability and security, and that performance overheads are similar to other intra-address space isolation mechanisms.

Learn more about CHERI including hybrid mode [here](https://www.cl.cam.ac.uk/research/security/ctsrd/cheri/).

## This repo
Provides the source code for FlexOS on Morello, as well as the applications run bare-metal on the hardware. Below are provided instructions for running these applications.

## Pre-requisites & Setup
You will need the ARM GCC toolchain and the Morello LLVM toolchain.

1. Get the ARM GCC toolchain: https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
2. Get the ARM Morello LLVM toolchain, found here: https://git.morello-project.org/morello/llvm-project-releases/-/tree/morello/baremetal-release-1.5
3. Replace [`LD`](https://github.com/jkressel/flexos-morello-hybrid/blob/main/unikraft/Makefile#L565) with the path to the `aarch64-none-elf-gcc` in the ARM GCC toolchain
4. Replace [`CC`](https://github.com/jkressel/flexos-morello-hybrid/blob/main/unikraft/Makefile#L566) with the path to `clang` in the Morello LLVM toolchain
5. Replace [`STRIP`](https://github.com/jkressel/flexos-morello-hybrid/blob/main/unikraft/Makefile#L581) with the path to `aarch64-none-elf-strip` in the ARM GCC toolchain
6. Replace [`OBJCOPY`](https://github.com/jkressel/flexos-morello-hybrid/blob/main/unikraft/Makefile#L582) with the `aarch64-none-elf-objcopy` in the ARM GCC toolchain

## SQLite
SQLite is compartmentalised using the mutual distrust model with overlapping shared data. The filesystem ([`vfscore`](https://github.com/jkressel/flexos-morello-hybrid/tree/main/unikraft/lib/vfscore) and [`ramfs`](https://github.com/jkressel/flexos-morello-hybrid/tree/main/unikraft/lib/ramfs)) are isolated. To setup the compartment bounds, uncomment lines `488` and `489` and comment out lines `492` and `493`. This sets up compartment bounds such that both compartments can access shared memory but not each others' private memory. 

Note, compartment 3 ([`uktime`](https://github.com/jkressel/flexos-morello-hybrid/tree/main/unikraft/lib/uktime)) is isolated using an additional data sharing method, macro based data sharing as a demonstration.

To build, run `make` in the `app-sqlite` root directory.

## Libsodium
The libsodium benchmark is derived from functions in the [libsodium test suite](https://github.com/jedisct1/libsodium/tree/master/test/default). 5 functions are sandboxed by manually porting those functions to use capabilities. To setup the compartment bounds, comment out lines `488` and `489` and uncomment lines `492` and `493`.

Both the original versions and the sandboxed versions of the functions are given to allow for different configurations.

| Function  | Original  | Sandboxed     |
|---------- |---------- |-----------    |
| `sodium_bin2hex`  | [Here](https://github.com/jkressel/flexos-morello-hybrid/blob/14928003e2353d49eef99b68652b1c5e2f550d52/apps/libsodium-bmk/build/libsodium/origin/libsodium-1.0.18/src/libsodium/sodium/codecs.c#L14C1-L14C15)         | [Here](https://github.com/jkressel/flexos-morello-hybrid/blob/14928003e2353d49eef99b68652b1c5e2f550d52/apps/libsodium-bmk/build/libsodium/origin/libsodium-1.0.18/src/libsodium/sodium/codecs.sandboxed.c#L15)          |
| `sodium_hex2bin`  | [Here](https://github.com/jkressel/flexos-morello-hybrid/blob/14928003e2353d49eef99b68652b1c5e2f550d52/apps/libsodium-bmk/build/libsodium/origin/libsodium-1.0.18/src/libsodium/sodium/codecs.c#L41)         | [Here](https://github.com/jkressel/flexos-morello-hybrid/blob/14928003e2353d49eef99b68652b1c5e2f550d52/apps/libsodium-bmk/build/libsodium/origin/libsodium-1.0.18/src/libsodium/sodium/codecs.sandboxed.c#L53)          |
| `chacha20_encrypt_bytes`  | [Here](https://github.com/jkressel/flexos-morello-hybrid/blob/14928003e2353d49eef99b68652b1c5e2f550d52/apps/libsodium-bmk/build/libsodium/origin/libsodium-1.0.18/src/libsodium/crypto_stream/chacha20/ref/chacha20_ref.c#L81)         | [Chacha20 Only](https://github.com/jkressel/flexos-morello-hybrid/blob/14928003e2353d49eef99b68652b1c5e2f550d52/apps/libsodium-bmk/build/libsodium/origin/libsodium-1.0.18/src/libsodium/crypto_stream/chacha20/ref/chacha20_ref.sandboxed.encrypt_only.c#L84), [Chacha20 and Store](https://github.com/jkressel/flexos-morello-hybrid/blob/14928003e2353d49eef99b68652b1c5e2f550d52/apps/libsodium-bmk/build/libsodium/origin/libsodium-1.0.18/src/libsodium/crypto_stream/chacha20/ref/chacha20_ref.sandboxed.c#L84)           | 
| `store32_le`  | [Here](https://github.com/jkressel/flexos-morello-hybrid/blob/14928003e2353d49eef99b68652b1c5e2f550d52/apps/libsodium-bmk/build/libsodium/origin/libsodium-1.0.18/src/libsodium/include/sodium/private/common.h#L119)         | [Here](https://github.com/jkressel/flexos-morello-hybrid/blob/14928003e2353d49eef99b68652b1c5e2f550d52/apps/libsodium-bmk/build/libsodium/origin/libsodium-1.0.18/src/libsodium/include/sodium/private/common.sandboxed.h#L120)          |
| `store64_be`  | [Here](https://github.com/jkressel/flexos-morello-hybrid/blob/14928003e2353d49eef99b68652b1c5e2f550d52/apps/libsodium-bmk/build/libsodium/origin/libsodium-1.0.18/src/libsodium/include/sodium/private/common.h#L152)         | [Here](https://github.com/jkressel/flexos-morello-hybrid/blob/14928003e2353d49eef99b68652b1c5e2f550d52/apps/libsodium-bmk/build/libsodium/origin/libsodium-1.0.18/src/libsodium/include/sodium/private/common.sandboxed.h#L160)          |

To build, run `make` in the `libsodium-bmk` root directory.

## Running

Create a binary image which can be used on the Morello machine using the script `make-bm-image.sh`, provided as part of the Morello LLVM bare metal toolchain. This will take a binary which was built for SQLite or Libsodium and turn it into an ELF file which can run bare metal.

```
make-bm-image.sh -i <BUILT_EXECUTABLE> -o img.bin
```

Once the image has been successfully built, we now need to actually run the image. The Morello bare metal README provides some description of the different approaches which can be taken: https://git.morello-project.org/morello/docs/-/blob/morello/mainline/standalone-baremetal-readme.rst

### Running on hardware

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

The board can now be rebooted and the image will run.

### Running on the FVP

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


