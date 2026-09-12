# syntax=docker/dockerfile:1
#
# Cross-build the SC1000 / SC500 "xwax" binary (with the SSD1306 waveform
# display) for the Allwinner A13.
#
# Target ABI: armv7-a, EABI hardfloat, uClibc-ng, gcc 7.x
#   -> matches the SC1000 firmware's Buildroot 2018.08 toolchain.
#
# Build context must be the SC1000 source root (the folder containing
# "software/"). See build.sh / README.md next to this file.

FROM --platform=linux/amd64 debian:bookworm-slim

ARG TC=armv7-eabihf--uclibc--stable-2018.11-1
ARG ALSA=alsa-lib-1.1.6
ARG ICONV=libiconv-1.15

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates wget bzip2 xz-utils make gcc file \
 && rm -rf /var/lib/apt/lists/*

# --- Bootlin cross toolchain ------------------------------------------------
RUN wget -qO /tmp/tc.tar.bz2 \
      "https://toolchains.bootlin.com/downloads/releases/toolchains/armv7-eabihf/tarballs/${TC}.tar.bz2" \
 && mkdir -p /opt && tar xf /tmp/tc.tar.bz2 -C /opt && rm /tmp/tc.tar.bz2
ENV PATH=/opt/${TC}/bin:$PATH
ENV CROSS=arm-linux
RUN ${CROSS}-gcc --version | head -1 \
 && echo "SYSROOT=$(${CROSS}-gcc -print-sysroot)" > /sysroot.env

# --- ALSA + iconv for the target, installed into the toolchain sysroot -----
RUN . /sysroot.env \
 && wget -qO /tmp/a.tar.bz2 "https://www.alsa-project.org/files/pub/lib/${ALSA}.tar.bz2" \
 && tar xf /tmp/a.tar.bz2 -C /tmp && cd /tmp/${ALSA} \
 && ./configure --host=${CROSS} --prefix=/usr \
      --disable-python --disable-ucm --disable-alsatplg --without-versioned \
 && make -j"$(nproc)" && make install DESTDIR="$SYSROOT" \
 && cd / && rm -rf /tmp/${ALSA}* /tmp/a.tar.bz2

RUN . /sysroot.env \
 && wget -qO /tmp/i.tar.gz "https://ftp.gnu.org/pub/gnu/libiconv/${ICONV}.tar.gz" \
 && tar xf /tmp/i.tar.gz -C /tmp && cd /tmp/${ICONV} \
 && ./configure --host=${CROSS} --prefix=/usr --enable-shared --disable-nls \
 && make -j"$(nproc)" && make install DESTDIR="$SYSROOT" \
 && cd / && rm -rf /tmp/${ICONV}* /tmp/i.tar.gz

# --- build xwax -----------------------------------------------------------
WORKDIR /src
COPY software/ ./software/
RUN . /sysroot.env && cd software \
 && echo "1.7-oled" > .version \
 && make -j"$(nproc)" \
      CC=${CROSS}-gcc \
      VERSION=1.7-oled \
      EXECDIR=/root/libexec \
      SDL_CFLAGS= SDL_LIBS= \
      ALSA_LIBS=-lasound \
      CFLAGS="-O2 -Wall -pthread -mcpu=cortex-a8 -mfpu=vfpv3-d16 -mfloat-abi=hard" \
      LDFLAGS="-O2 -pthread -mcpu=cortex-a8 -mfpu=vfpv3-d16 -mfloat-abi=hard" \
 && ${CROSS}-strip xwax \
 && echo "==== result ====" && file xwax \
 && ${CROSS}-readelf -d xwax | grep NEEDED \
 && ${CROSS}-readelf -A xwax | grep -Ei "CPU_name|CPU_arch|FP_arch|VFP_args"

CMD ["sh","-c","cp /src/software/xwax /out/xwax && echo 'copied -> ./oled-build/xwax'"]
