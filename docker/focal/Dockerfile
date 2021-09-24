#
# Install Qt and Qbs for Linux
#
FROM ubuntu:focal
LABEL Description="Ubuntu development environment for Qbs with Qt and various dependencies for testing Qbs modules and functionality"
ARG QT_VERSION
ARG QTCREATOR_VERSION

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

# Qbs build dependencies
RUN apt-get update -qq && \
    DEBIAN_FRONTEND="noninteractive" apt-get install -qq -y --no-install-recommends \
        bison \
        build-essential \
        ca-certificates \
        capnproto \
        ccache \
        clang-8 \
        clang-tidy-8 \
        cmake \
        curl \
        flex \
        git \
        help2man \
        icoutils \
        libcapnp-dev \
        libdbus-1-3 \
        libfreetype6 \
        libfontconfig1 \
        libgl1-mesa-dev \
        libgl1-mesa-glx \
        libnanopb-dev \
        libprotobuf-dev \
        libgrpc++-dev \
        libxkbcommon-x11-0 \
        nanopb \
        ninja-build \
        nsis \
        pkg-config \
        protobuf-compiler \
        protobuf-compiler-grpc \
        psmisc \
        python3-pip \
        python3-setuptools \
        p7zip-full \
        subversion \
        unzip \
        zip && \
    update-alternatives --install /usr/bin/clang clang /usr/bin/clang-8 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-8 100 && \
    update-alternatives --install /usr/bin/clang-check clang-check /usr/bin/clang-check-8 100 && \
    update-alternatives --install /usr/bin/python python /usr/bin/python3 100 && \
    pip install beautifulsoup4 lxml protobuf pyyaml

ENV LLVM_INSTALL_DIR=/usr/lib/llvm-8


#
# Install Qt and Qbs for Linux from qt.io
#
COPY scripts/install-qt.sh install-qt.sh

RUN ./install-qt.sh --version ${QT_VERSION} qtbase qtdeclarative qtscript qttools qtx11extras qtscxml qt5compat icu && \
    ./install-qt.sh --version ${QTCREATOR_VERSION} qtcreator && \
    echo "export PATH=/opt/Qt/${QT_VERSION}/gcc_64/bin:/opt/Qt/Tools/QtCreator/bin:\${PATH}" > /etc/profile.d/qt.sh

ENV PATH=/opt/Qt/${QT_VERSION}/gcc_64/bin:/opt/Qt/Tools/QtCreator/bin:${PATH}


# Configure Qbs
USER $USER_NAME
RUN qbs-setup-toolchains /usr/bin/g++ gcc && \
    qbs-setup-toolchains /usr/bin/clang clang && \
    qbs-setup-qt /opt/Qt/${QT_VERSION}/gcc_64/bin/qmake qt-gcc_64 && \
    qbs config profiles.qt-gcc_64.baseProfile gcc && \
    qbs-setup-qt /opt/Qt/${QT_VERSION}/gcc_64/bin/qmake qt-clang_64 && \
    qbs config profiles.qt-clang_64.baseProfile clang && \
    qbs config defaultProfile qt-gcc_64

# Switch back to root user for the entrypoint script.
USER root

# Work-around for QTBUG-79020
RUN echo "export QT_NO_GLIB=1" >> /etc/profile.d/qt.sh
