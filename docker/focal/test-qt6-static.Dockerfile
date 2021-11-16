#
# Testing Qbs with static qt6
#
FROM ubuntu:focal
LABEL Description="Ubuntu static qt6 test environment for Qbs"
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
        wget \
        ca-certificates \
        gosu \
        software-properties-common \
        sudo && \
    groupadd -g ${USER_UID} ${USER_NAME} && \
    useradd -s /bin/bash -u ${USER_UID} -g ${USER_NAME} -o -c "" -m ${USER_NAME} && \
    usermod -a -G sudo ${USER_NAME} && \
    echo "%devel         ALL = (ALL) NOPASSWD: ALL" >> /etc/sudoers

COPY docker/focal/entrypoint.sh /sbin/entrypoint.sh
ENTRYPOINT ["/sbin/entrypoint.sh"]

RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null
RUN echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ focal main' | tee /etc/apt/sources.list.d/kitware.list >/dev/null
RUN cat /etc/apt/sources.list.d/kitware.list

RUN apt-get update -qq && \
    DEBIAN_FRONTEND="noninteractive" apt-get install -qq -y --no-install-recommends \
    build-essential \
    git \
    perl \
    cmake \
    python \
    zlib1g-dev \
    libzstd-dev \
    libdbus-1-dev \
    libglib2.0-dev \
    libssl-dev \
    libdrm-dev \
    libegl-dev \
    libwayland-dev \
    libvulkan-dev \
    libicu-dev \
    libb2-dev \
    libsystemd-dev \
    libfontconfig1-dev \
    libfreetype6-dev \
    libx11-dev \
    libx11-xcb-dev \
    libxext-dev \
    libxfixes-dev \
    libxi-dev \
    libxrender-dev \
    libxcb1-dev \
    libxcb-glx0-dev \
    libxcb-keysyms1-dev \
    libxcb-image0-dev \
    libxcb-shm0-dev \
    libxcb-icccm4-dev \
    libxcb-sync0-dev \
    libxcb-xfixes0-dev \
    libxcb-shape0-dev \
    libxcb-randr0-dev \
    libxcb-render-util0-dev \
    libxcb-xinerama0-dev \
    libxkbcommon-dev \
    libxkbcommon-x11-dev \
    libxcb-xinput-dev \
    ninja-build \
    libudev-dev \
    libxcb-cursor-dev \
    libxcb-composite0-dev \
    libxcb-dri2-0-dev \
    libxcb-dri3-dev \
    libxcb-ewmh-dev \
    libxcb-present-dev \
    libxcb-present-dev \
    libxcb-record0-dev \
    libxcb-res0-dev \
    libxcb-screensaver0-dev \
    libxcb-sync-dev \
    libxcb-util-dev \
    libxcb-xf86dri0-dev \
    libxcb-xtest0-dev \
    libxcb-xv0-dev \
    libxcb-xvmc0-dev \
    libxcb-damage0-dev \
    libxcb-dpms0-dev \
    libgstreamer1.0-dev \
    apt-transport-https

ENV QT_HOME="/home/${USER_NAME}/qt"
USER ${USER_NAME}
RUN mkdir ${QT_HOME}

RUN cd ${QT_HOME} && git clone https://code.qt.io/qt/qt5.git

RUN cd ${QT_HOME}/qt5 && git checkout v${QT_VERSION} && perl init-repository

RUN mkdir ${QT_HOME}/static-build && cd ${QT_HOME}/static-build && ../qt5/configure -prefix /opt/Qt/${QT_VERSION}/gcc_64

RUN cd ${QT_HOME}/static-build && cmake --build . --parallel

USER root

RUN cd ${QT_HOME}/static-build && cmake --install .

FROM ubuntu:focal
LABEL Description="Ubuntu static qt6 test environment for Qbs"
ARG QT_VERSION
ARG QTCREATOR_VERSION

RUN mkdir -p /opt/Qt
COPY --from=0 /opt/Qt/${QT_VERSION} /opt/Qt/${QT_VERSION}

# Allow colored output on command line.
ENV TERM=xterm-color
ARG USER_UID=1000
ARG USER_NAME=devel
RUN apt-get update -qq && \
    apt-get install -qq -y \
        ca-certificates \
        gosu \
        software-properties-common \
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
        zip \
        libb2-1 \
        libpcre++ && \
    update-alternatives --install /usr/bin/clang clang /usr/bin/clang-8 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-8 100 && \
    update-alternatives --install /usr/bin/clang-check clang-check /usr/bin/clang-check-8 100 && \
    update-alternatives --install /usr/bin/python python /usr/bin/python3 100 && \
    pip install beautifulsoup4 lxml protobuf pyyaml

ENV LLVM_INSTALL_DIR=/usr/lib/llvm-8

#
# Install Qbs for Linux from qt.io
#
COPY scripts/install-qt.sh install-qt.sh

RUN ./install-qt.sh --version ${QTCREATOR_VERSION} qtcreator && \
    echo "export PATH=/opt/Qt/${QT_VERSION}/gcc_64/bin:/opt/Qt/Tools/QtCreator/bin:\${PATH}" > /etc/profile.d/qt.sh

ENV PATH=/opt/Qt/${QT_VERSION}/gcc_64/bin:/opt/Qt/Tools/QtCreator/bin:${PATH}

# Configure Qbs
USER $USER_NAME
RUN qbs-setup-toolchains /usr/bin/g++ gcc && \
    qbs-setup-qt /opt/Qt/${QT_VERSION}/gcc_64/bin/qmake qt-gcc_64 && \
    qbs config profiles.qt-gcc_64.baseProfile gcc && \
    qbs config defaultProfile qt-gcc_64

# Switch back to root user for the entrypoint script.
USER root

# Work-around for QTBUG-79020
RUN echo "export QT_NO_GLIB=1" >> /etc/profile.d/qt.sh
