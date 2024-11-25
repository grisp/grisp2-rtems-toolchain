# Check the docker targets in ./Makefile to see how to use this.

FROM debian:bookworm AS build-stage

RUN apt-get update && apt-get install -y \
    build-essential flex bison cmake texinfo device-tree-compiler git \
    u-boot-tools lzop libusb-1.0-0-dev python3 python-is-python3 \
    unzip libpython3-dev python3-dev locales

# https://devel.rtems.org/ticket/4726
RUN echo "en_US.ISO-8859-15 ISO-8859-15" >> /etc/locale.gen \
    && locale-gen
ENV LANG=en_US.iso885915

WORKDIR /grisp2-rtems-toolchain
COPY . .
RUN make install

# ------------------------------------------------------------------------------

FROM debian:bookworm-slim AS run-stage

RUN apt-get update && apt-get install -y \
    build-essential flex bison cmake texinfo device-tree-compiler git \
    u-boot-tools lzop libusb-1.0-0-dev python3 python-is-python3 \
    unzip libpython3-dev python3-dev && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /grisp2-rtems-toolchain
COPY --from=build-stage /grisp2-rtems-toolchain/rtems/5/ rtems/5/

ENV GRISP_TOOLCHAIN=/grisp2-rtems-toolchain/rtems/5
ENV GRISP_TC_ROOT=/grisp2-rtems-toolchain/rtems/5
ENV PATH="/grisp2-rtems-toolchain/rtems/5/bin:${PATH}"
