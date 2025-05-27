# Check the docker targets in ./Makefile to see how to use this.

FROM debian:bookworm AS build-stage

RUN apt-get update && apt-get install -y \
    build-essential flex bison cmake texinfo device-tree-compiler git \
    u-boot-tools lzop libusb-1.0-0-dev python3 python-is-python3 \
    unzip libpython3-dev python3-dev locales

# Set locale to UTF-8
RUN echo "en_US.UTF-8 UTF-8" >> /etc/locale.gen \
    && locale-gen
ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8

WORKDIR /grisp2-rtems-toolchain
COPY . .

# Create necessary directory structure, fails without doing this...
RUN mkdir -p rtems/6/arm-rtems6/imx7/lib/include/{machine,sys,vm,x86}

RUN make install

# ------------------------------------------------------------------------------

FROM debian:bookworm-slim AS run-stage

RUN apt-get update && apt-get install -y \
    build-essential flex bison cmake texinfo device-tree-compiler git \
    u-boot-tools lzop libusb-1.0-0-dev python3 python-is-python3 \
    unzip libpython3-dev python3-dev && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /grisp2-rtems-toolchain
COPY --from=build-stage /grisp2-rtems-toolchain/rtems/6/ rtems/6/

ENV GRISP_TOOLCHAIN=/grisp2-rtems-toolchain/rtems/6
ENV GRISP_TC_ROOT=/grisp2-rtems-toolchain/rtems/6
ENV PATH="/grisp2-rtems-toolchain/rtems/6/bin:${PATH}"
