# You can easily build it with the following command:
# $ docker build --tag flexos-dev -f flexos-dev.dockerfile .
#
# If the build fails because you are rate-limited by GitHub, generate an app
# token (https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/creating-a-personal-access-token)
# and run instead:
# $ docker build --build-arg UK_KRAFT_GITHUB_TOKEN="<YOUR TOKEN>" --tag flexos-dev
#
# and run with:
# $ docker run --privileged -ti flexos-dev bash

FROM debian:10

ARG UK_KRAFT_GITHUB_TOKEN=
ENV UK_KRAFT_GITHUB_TOKEN=${UK_KRAFT_GITHUB_TOKEN}

##############
# Dependencies

RUN echo "deb-src http://deb.debian.org/debian buster main contrib non-free" >> /etc/apt/sources.list
RUN echo "deb-src http://security.debian.org/ buster/updates main contrib non-free" >> /etc/apt/sources.list
RUN echo "deb-src http://deb.debian.org/debian/ buster-updates main contrib non-free" >> /etc/apt/sources.list
RUN apt update
RUN apt build-dep -y coccinelle
RUN apt install -y build-essential libncurses-dev python3 expect-dev moreutils \
	flex unzip bison wget libxml2-utils tclsh python python-tempita python-six \
	python-future python-ply xorriso qemu-system-x86 qemu qemu-kvm vim qemu-system \
	qemu-utils curl gawk git procps socat uuid-runtime python3-pip libsqlite3-dev \
	bc libiscsi-dev librbd1 libnfs-dev libgfapi0 libffi-dev libiperf-dev net-tools \
	bridge-utils iperf dnsmasq ninja-build gdb cscope
RUN pip3 install -U setuptools==41.0

##############
# Toolchain

WORKDIR /root

RUN git clone https://github.com/jkressel/kraft.git

WORKDIR /root/kraft

RUN pip3 install -e .

COPY kraftcleanup /usr/local/bin/kraftcleanup
COPY kraftrc.default /root/.kraftrc

WORKDIR /root

RUN wget https://raw.githubusercontent.com/unikraft/kraft/6217d48668cbdf0847c7864bc6368a6adb94f6a6/scripts/qemu-guest
RUN chmod a+x /root/qemu-guest

RUN git clone https://github.com/coccinelle/coccinelle

WORKDIR /root/coccinelle

RUN git checkout ae337fce1512ff15aabc3ad5b6d2e537f97ab62a
RUN ./autogen
RUN ./configure
RUN make
RUN make install

# fix a bug in Coccinelle
RUN mkdir /usr/local/bin/lib
RUN ln -s /usr/local/lib/coccinelle /usr/local/bin/lib/coccinelle

WORKDIR /root

##############
# FlexOS

RUN kraft list update
RUN kraft -v list pull flexos-microbenchmarks@staging iperf@staging \
		  newlib@staging tlsf@staging flexos-example@staging \
		  lwip@staging redis@staging unikraft@staging \
		  pthread-embedded@staging nginx@staging
RUN cp /root/.unikraft/unikraft/flexos-support/porthelper/* /root/.unikraft/

##############
# FlexOS EPT QEMU

RUN git clone https://github.com/qemu/qemu.git

WORKDIR /root/qemu

RUN apt install -y ninja-build
RUN git checkout 9ad4c7c9b63f89c308fd988d509bed1389953c8b
RUN cp /root/.unikraft/unikraft/flexos-support/0001-Myshmem.patch /root/0001-Myshmem.patch
RUN git apply /root/0001-Myshmem.patch
RUN apt build-dep -y qemu-system-x86
RUN ./configure --target-list=x86_64-softmmu
RUN sed -i -e 's/-lstdc++ -Wl,--end-group/-lrt -lstdc++ -Wl,--end-group/g' build/build.ninja
RUN make -j8
RUN cp build/qemu-system-x86_64 /root/qemu-system-ept
RUN cp -r build/pc-bios /root/pc-bios
RUN rm /root/0001-Myshmem.patch

WORKDIR /root/.unikraft
