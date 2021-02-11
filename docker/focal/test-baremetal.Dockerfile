#
# Baremetal toolchains for testing Qbs
#
FROM ubuntu:focal
LABEL Description="Ubuntu baremetal test environment for Qbs"

# Allow colored output on command line.
ENV TERM=xterm-color

#
# Make it possible to change UID/GID in the entrypoint script. The docker
# container usually runs as root user on Linux hosts. When the Docker container
# mounts a folder on the host and creates files there, those files would be
# owned by root instead of the current user. Thus we create a user here who's
# UID will be changed in the entrypoint script to match the UID of the current
# host user.
#
ARG USER_UID=1000
ARG USER_NAME=devel
RUN apt-get update -qq && \
    apt-get install -qq -y \
        ca-certificates \
        gosu \
        sudo && \
    groupadd -g ${USER_UID} ${USER_NAME} && \
    useradd -s /bin/bash -u ${USER_UID} -g ${USER_NAME} -o -c "" -m ${USER_NAME} && \
    usermod -a -G sudo ${USER_NAME} && \
    echo "%devel         ALL = (ALL) NOPASSWD: ALL" >> /etc/sudoers

COPY docker/focal/entrypoint.sh /sbin/entrypoint.sh
ENTRYPOINT ["/sbin/entrypoint.sh"]

# Install baremetal toolchains and Qbs runtime dependencies.
RUN apt-get update -qq && \
    apt-get install -qq -y \
        libasan5 \
        libglib2.0-0 \
        libgssapi-krb5-2 \
        libgl1-mesa-glx \
        gcc-arm-none-eabi \
        gcc-avr \
        avr-libc \
        gcc-msp430 \
        sdcc \
        binutils-xtensa-lx106 \
        gcc-xtensa-lx106 \
        gcc-riscv64-unknown-elf

# Work-around for QTBUG-79020.
RUN echo "export QT_NO_GLIB=1" >> /etc/profile.d/qt.sh
