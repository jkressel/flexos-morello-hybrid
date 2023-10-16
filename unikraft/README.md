# FlexOS: Making OS Isolation Flexible

This repository contains the main tree of our FlexOS proof-of-concept. FlexOS
aims to decouple isolation from the design of the OS. FlexOS is a cooperation
between the University of Manchester, University Politehnica of Bucharest, and
NEC Laboratories Europe GmbH. It has been published in HotOS'21 and ASPLOS'22.

> **Abstract**:  At design time, modern operating systems are locked in a
> specific safety and isolation strategy that mixes one or more
> hardware/software protection mechanisms (e.g. user/kernel separation);
> revisiting these choices after deployment requires a major refactoring effort.
> This rigid approach shows its limits given the wide variety of modern
> applications' safety/performance requirements, when new hardware isolation
> mechanisms are rolled out, or when existing ones break.
>
> We present FlexOS, a novel OS allowing users to easily specialize the
> safety and isolation strategy of an OS at compilation/deployment time
> instead of design time. This modular LibOS is composed of fine-grained
> components that can be isolated via a range of hardware protection mechanisms
> with various data sharing strategies and additional software hardening. The
> OS ships with an exploration technique helping the user navigate the vast
> safety/performance design space it unlocks. We implement a prototype of the
> system and demonstrate, for several applications (Redis/Nginx/SQLite),
> FlexOS’ vast configuration space as well as the efficiency of the
> exploration technique: we evaluate 80 FlexOS configurations for Redis and
> show how that space can be probabilistically subset to the 5 safest ones under
> a given performance budget. We also show that, under equivalent
> configurations, FlexOS performs similarly or better than several
> baselines/competitors.

The name of this repository is "Unikraft", a reminder that FlexOS is a friendly
fork of Unikraft
([EuroSys'21](https://dl.acm.org/doi/abs/10.1145/3447786.3456248) Best Paper
Award).

## Installing from the Docker container

Clone this repository and build the Docker container:

```
$ git clone git@github.com:project-flexos/unikraft.git
$ pushd unikraft/docker
$ docker build -f flexos.dockerfile --tag flexos-dev .
$ popd
```

If the build fails because you are rate-limited by GitHub, generate an app
token
([instructions](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/creating-a-personal-access-token)),
and run instead:

```
$ docker build -f flexos.dockerfile --build-arg UK_KRAFT_GITHUB_TOKEN="<YOUR TOKEN>" --tag flexos-dev .
```

Run the container as following:

```
$ docker run --privileged -ti flexos-dev bash
```

This will give you access to a FlexOS development environment under
`/root/.unikraft`.

## Installing from source

We strongly recommend the Docker-based approach.  If you really want to install
FlexOS on your base system, the easiest approach is to follow the Dockerfile
instructions in `docker/flexos.dockerfile`. Note that the recommended system is
Debian 10.

## Building FlexOS

Applies to both Docker & installation from source.

Configure Redis with two compartments:

```
$ cd ~/.unikraft/apps/redis
$ cat ./kraft.yaml
---
specification: '0.6'
name: redis
unikraft:
  version: staging
  kconfig:
    - CONFIG_LIBUK9P=y
    - CONFIG_LIB9PFS=y
    - CONFIG_LIBDEVFS=y
    - CONFIG_LIBDEVFS_AUTOMOUNT=y
    - CONFIG_LIBVFSCORE_AUTOMOUNT_ROOTFS=y
    - CONFIG_LIBVFSCORE_ROOTFS_RAMFS=y
    - CONFIG_LIBUKLIBPARAM=y
    - CONFIG_LIBUKDEBUG=y
    - CONFIG_LIBUKALLOC=y
    - CONFIG_LIBUKSCHED=y
    - CONFIG_LIBPOSIX_SYSINFO=y
    - CONFIG_LIBPOSIX_LIBDL=y
    - CONFIG_LIBFLEXOS=y
targets:
  - architecture: x86_64
    platform: kvm
compartments:
  - name: comp1
    mechanism:
      driver: intel-pku
    default: true
  - name: comp2
    mechanism:
      driver: intel-pku
libraries:
  tlsf:
    version: staging
    kconfig:
      - CONFIG_LIBTLSF=y
  pthread-embedded:
    version: staging
    compartment: comp1
  newlib:
    version: staging
    kconfig:
      - CONFIG_LIBNEWLIBC=y
      - CONFIG_LIBNEWLIBC_WANT_IO_C99_FORMATS=y
      - CONFIG_LIBNEWLIBC_LINUX_ERRNO_EXTENSIONS=y
    compartment: comp1
  lwip:
    version: staging
    kconfig:
      - CONFIG_LWIP_IPV6=y
    compartment: comp2
  redis:
    version: staging
    kconfig:
      - CONFIG_LIBREDIS_SERVER=y
      - CONFIG_LIBREDIS_COMMON=y
      - CONFIG_LIBREDIS_LIBREDIS_LUA=y
      - CONFIG_LIBREDIS_SERVER_MAIN_FUNCTION=y
    compartment: comp1
volumes: {}
networks: {}
$ kraft configure
```

Now we have a fully set up system. We only have to build and run. The following
commands are what you would run as part of your development workflow.

Build Redis with two MPK compartments:

```
$ make prepare && kraft -v build --no-progress --fast --compartmentalize
```

Run the freshly built image:

```
$ kraft run --initrd ./redis.cpio -M 1024 ""
```

## FlexOS `kraft.yaml` primer

In addition to this documentation, you can find examples of `kraft.yaml` files
with MPK, EPT, and function call instanciation in our [AE
repository](https://github.com/project-flexos/asplos22-ae).

### Declaring Compartments

Declare compartments in `compartments`:

```
compartments:
  - name: comp1
    mechanism:
      driver: intel-pku
    default: true
  - name: comp2
    mechanism:
      driver: intel-pku
```

Each compartment has a `name` and a `mechanism`.

Each mechanism has a driver (for Intel MPK/PKU it's `intel-pku`, for VM-based
`vmept`, for simple function calls `fcalls`), and possibly a number of
driver-specific options (you might have come across `noisolstack`, but be aware
that, as of now, this option does nothing).

There is one `default` compartment. All libraries that have not been assigned a
specific compartment will go into the default compartment.

:warning: If you declare a compartment, make sure to actually use it! That is,
it should either be default or used by a library (see below).

### Assign compartments to libraries

Libraries can be assigned a compartment using `compartment`:

```
libraries:
[...]
  lwip:
    version: staging
    kconfig:
      - CONFIG_LWIP_IPV6=y
    compartment: comp2
```

The value has to match the name of a previously declared compartment.

Note that internal libraries (the ones whose source live under `unikraft/`)
require an additional `is_core: true` parameter:

```
libraries:
[...]
  uksched:
    is_core: true
    compartment: comp2
  ukschedcoop:
    is_core: true
    compartment: comp2
```

:warning: always put uksched and ukschedcoop together.

## Development Workflow

`kraft` rewrites the source code of microlibraries **in-place** to implement
isolation primitives. If you change the isolation profile of the image (by
editing `kraft.yaml`), make sure to thoroughly cleanup your setup.

Here is a script that does it:

```
#!/bin/bash

git checkout .
make properclean
# git checkout . && git clean -xdf in all repositories would be fine too
rm -rf ~/.kraftcache ~/.unikraft/libs ~/.unikraft/unikraft
kraft list update
kraft list pull flexos-microbenchmarks@staging iperf@staging newlib@staging \
		tlsf@staging flexos-example@staging lwip@staging redis@staging \
		unikraft@staging pthread-embedded@staging nginx@staging
```

We recommend putting this in `/usr/local/bin/kraftcleanup`. This is done automatically
by the Docker container.

The usual workflow is then:

```
$ kraftcleanup # clean
$ make prepare && kraft -v build --no-progress --fast --compartmentalize
$ kraft run [...] # run
```

:warning: Once again, if you do not run `kraftcleanup`, running `kraft build --compartmentalize` **will not** rewrite your code again because the rewriting is done in-place!

:warning: `kraftcleanup` will **erase any modification done in your build repositories**! Make sure to have separate repositories that you use for the actual development otherwise you might **loose data**!

:warning: Running `make prepare` before `kraft ... --fast ...` is necessary because of a bug in the Unikraft toolchain; the `prepare` rule does not support parallel execution.

## Backend-specific instructions

### Morello

#### Building instructions

**NOTE** You must use a debian 11+ environment for building FlexOS as opposed to the debian 10 environenment set up for regular builds

1. You will need the morello bare metal llvm toolchain: https://git.morello-project.org/morello/llvm-project-releases/-/tree/morello/baremetal-release-1.5
2. Download the arm gcc toolchain: https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
3. Add the following to `<PATH TO UNIKRAFT>/.unikraft/unikraft/Makefile`:
```
LD := <ARM GCC TOOLCHAIN>/bin/aarch64-none-elf-gcc
CC := <LLVM TOOLCHAIN>/bin/clang
STRIP := <ARM GCC TOOLCHAIN>/bin/aarch64-none-elf-strip
OBJCOPY := <ARM GCC TOOLCHAIN>/bin/aarch64-none-elf-objcopy
ASFLAGS += -target aarch64-none-elf
CFLAGS += -target aarch64-none-elf
```
4. Get the morello platform from here: https://github.com/jkressel/uk-plat-morello
5. Add the line `$(eval $(call _import_lib,$(UK_PLAT_BASE)/uk-plat-morello))` to the end of `<PATH TO UNIKRAFT>/.unikraft/unikraft/plat/Makefile.uk`
6. Locate the source directory of the kraft tool. In the file `const.py`, locate the following:
```
UK_CORE_PLATS = [
    'kvm',
    'xen',
    'linuxu',
]
```
add `'morello'` to the list. Then rebuild and install kraft using pip3.

7. Build flexos as normal using kraft.

### Intel MPK/PKU

:warning: You can build on any machine, but **running requires a CPU that supports MPK**!

Generally, if you don't have a Xeon Bronze/Silver/Gold/Platinum, you don't have MPK. You can try running the image, if your CPU doesn't support MPK, FlexOS will abort with an appropriate error message.

### VM/EPT

#### Building QEMU with VM/EPT shared memory

For the shared memory on KVM, we use a simple shared memory device in QEMU. To add this new device to QEMU, it is necessary to compile it from source code and add a patch.
Here are steps for building QEMU for x86_64 architecture, with support for the shared memory device:

```
$ git clone https://github.com/qemu/qemu.git
$ cd qemu
$ git apply 0001-Myshmem.patch
$ ./configure --target-list=x86_64-softmmu
$ vim build/build.ninja # add -lrt to LINK_ARGS for the target qemu-system-x86_64
$ make -j8
$ build/qemu-system-x86_64 -device myshmem,file=/test,size=0x1000,paddr=0x10000000 # example run to check if the device works
```

The above built QEMU binary should be used to run all Unikraft images that use the VM/EPT backend in FlexOS. You can find 0001-Myshmem.patch in this repository under `flexos-support`.

Additional remark regarding the `LINK_ARGS` edit. The relevant line is the one that begins with `LINK_ARGS = `, following the the line that begins with `build qemu-system-x86_64`.

#### Building FlexOS VM/EPT images

Building FlexOS images with the VM/EPT backend is no different from MPK (beginning from `~/.unikraft/apps`):
```
$ cd app-flexos-example
$ make menuconfig # select VM/EPT in Library Configuration -> flexos -> FlexOS backend and also the KVM platform in Platform Configuration
$ make prepare
$ kraft -v build --compartmentalize --fast
```

This should result in two images being built under `build/`, one ending in
`.comp0`, compartment 0, and the other one with `.comp1`.

#### Running FlexOS VM/EPT images

To run the application, compartment 0 has to be run first, then compartment 1, etc. This will be fixed soon.
Here is an example command for running the built compartments:
```
<PATH TO QEMU BUILT EARLIER>/qemu/build/qemu-system-x86_64 -enable-kvm -nographic -device isa-debug-exit -gdb tcp::1237 \
	-device myshmem,file=/rpc,paddr=0x800000000,size=0x100000 \
	-device myshmem,file=/heap,paddr=0x4000000000,size=0x8000000 \
	-device myshmem,file=/data_shared,paddr=0x105000,size=<size of data_shared section> \
	-kernel $KERNEL \
	-m 2G \
```

Replace the path to QEMU with the path on your system. To find out the size of
the `data_shared section`, run the following command on the built compartments:
```
$ readelf -SW ~/.unikraft/apps/app-flexos-example/build/app-flexos-example_kvm-x86_64.dbg.comp0
[...]
Section Headers:
  [Nr] Name              Type            Address          Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            0000000000000000 000000 000000 00      0   0  0
  [ 1] .multiboot_header PROGBITS        0000000000100000 001000 005000 00  WA  0   0 4096
  [ 2] .data_shared      PROGBITS        0000000000105000 006000 003000 00  WA  0   0 32
  [ 3] .text             PROGBITS        0000000000108000 009000 0e7604 00 WAX  0   0  1
[...]
```

### Function Calls (Debugging Backend)

The `fcalls` debugging backend will replace all gates with simple function calls. You can build it on any machine and run it on any machine.

## Porting Tips & Tricks

### Automatic gate insertion

We provide users with a simple tool to automatically insert gate placeholders
in their code. The tool uses [Cscope](http://cscope.sourceforge.net/) to
determine calls performed by a file outside of the current library, and
[Coccinelle](https://github.com/coccinelle/coccinelle) to automatically insert
a gate at this position. The tool is available under
`flexos-support/porthelper` and comes preinstalled in the FlexOS docker
container.

We illustrate the use of the too with the iPerf server:

```
root@host# docker run --privileged -ti flexos-dev bash
root@c2087bc9c8ca:~/.unikraft# pushd libs/iperf/; git checkout unikraft-baseline; popd
root@c2087bc9c8ca:~/.unikraft# ./porthelper.sh libs/iperf/server.c 
HANDLING: libs/iperf/server.c
diff = 
--- libs/iperf/server.c
+++ /tmp/cocci-output-187-0c6434-server.c
@@ -15,9 +15,9 @@ int run_server(int RECVBUFFERSIZE)
     struct sockaddr_in servaddr;
     char *buf = malloc(1024 * 512 * sizeof(char));
 
-    sockfd = socket(AF_INET, SOCK_STREAM, 0);
+    flexos_gate(liblwip, sockfd, socket, AF_INET, SOCK_STREAM, 0);
     if (sockfd == -1) {
-        printf("Socket failed\n");
+        flexos_gate(libc, printf, "Socket failed\n");
         return 0;
     }
 
@@ -29,32 +29,34 @@ int run_server(int RECVBUFFERSIZE)
     servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
     servaddr.sin_port = htons(12345);
 
-    rc = bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
+    flexos_gate(liblwip, rc, bind, sockfd, (struct sockaddr *)&servaddr,
+                sizeof(servaddr));
     if (rc != 0) {
-        printf("Bind failed\n");
+        flexos_gate(libc, printf, "Bind failed\n");
         return 0;
     }
 
-    rc = listen(sockfd, 5);
+    flexos_gate(liblwip, rc, listen, sockfd, 5);
     if (rc != 0) {
-        printf("Listen failed\n");
+        flexos_gate(libc, printf, "Listen failed\n");
         return 0;
     }
 
     len = sizeof(cli);
 
-    connfd = accept(sockfd, (struct sockaddr *) &cli, &len);
+    flexos_gate(liblwip, connfd, accept, sockfd, (struct sockaddr *)&cli,
+                &len);
 
     if (connfd < 0) {
-        printf("Accept failed\n");
+        flexos_gate(libc, printf, "Accept failed\n");
         return 0;
     }
 
     while (1) {
-	rc = recv(connfd, buf, RECVBUFFERSIZE, 0);
+	flexos_gate(liblwip, rc, recv, connfd, buf, RECVBUFFERSIZE, 0);
 
         if (rc < 0) {
-            printf("Read failed with %d\n", rc);
+            flexos_gate(libc, printf, "Read failed with %d\n", rc);
             break;
         }
     }
```

In the case of the iPerf server, the tool is able to correctly replace all
external function calls.

Note that this tool might miss calls that are realized with non-standard
patterns (source to source transformation is hard), as well as indirect
function calls. In these cases, the next section will come handy.

### Checking gate insertions with MPK (and other intra-AS isolation technologies)

The first part of porting a library to run as isolated component is to insert gates.

It can be hard to ensure that all function calls to the component's API are wrapped with gates if the component doesn't have much internal data; without internal data accesses, technologies such as MPK might not trigger a crash if a gate is missing.

In order to ensure that *all* function calls to the components API trigger a crash if a gate is missing, we can leverage GCC's instrumentation `-finstrument-functions`. With this instrumentation, we can make sure that each function of the target library writes to a library local variable before executing. If a gate is missing, it will crash.

For this you simply have to add the compiler flag to the relevant library, e.g., for uktime:

```
diff --git a/lib/uktime/Makefile.uk b/lib/uktime/Makefile.uk
index 8e05ed7..ca1f085 100644
--- a/lib/uktime/Makefile.uk
+++ b/lib/uktime/Makefile.uk
@@ -12,6 +12,7 @@ LIBUKTIME_SRCS-y += $(LIBUKTIME_BASE)/musl-imported/src/__tm_to_secs.c
 LIBUKTIME_SRCS-y += $(LIBUKTIME_BASE)/musl-imported/src/__year_to_secs.c
 LIBUKTIME_SRCS-y += $(LIBUKTIME_BASE)/time.c
 LIBUKTIME_SRCS-y += $(LIBUKTIME_BASE)/timer.c
+LIBUKTIME_CFLAGS-y += -finstrument-functions -finstrument-functions-exclude-function-list=__cyg_profile_func_enter,__cyg_profile_func_exit
 
 UK_PROVIDED_SYSCALLS-$(CONFIG_LIBUKTIME) += nanosleep-2
 UK_PROVIDED_SYSCALLS-$(CONFIG_LIBUKTIME) += clock_gettime-2
```

Then define the two instrumentation functions:

```
diff --git a/lib/uktime/musl-imported/include/time.h b/lib/uktime/musl-imported/include/time.h
index 7cfcdba..18125a0 100644
--- a/lib/uktime/musl-imported/include/time.h
+++ b/lib/uktime/musl-imported/include/time.h
@@ -7,6 +7,11 @@ extern "C" {
 
 #include <uk/config.h>
 
+void __cyg_profile_func_enter (void *this_fn,
+                               void *call_site);
+void __cyg_profile_func_exit  (void *this_fn,
+                               void *call_site);
+
 #define __NEED_size_t
 #define __NEED_time_t
 #define __NEED_clock_t
diff --git a/lib/uktime/time.c b/lib/uktime/time.c
index 9290635..3917f39 100644
--- a/lib/uktime/time.c
+++ b/lib/uktime/time.c
@@ -51,6 +51,19 @@
 #endif
 #include <uk/essentials.h>
 
+volatile int uktime_local;
+
+void __cyg_profile_func_enter (void *this_fn,
+                               void *call_site)
+{
+       uktime_local = 0;
+}
+void __cyg_profile_func_exit  (void *this_fn,
+                               void *call_site)
+{
+       uktime_local = 1;
+}
+
```

Then compile as usual. The image will crash if gates are missing.
