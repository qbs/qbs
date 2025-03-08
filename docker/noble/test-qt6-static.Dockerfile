#
# Testing Qbs with static qt6
#
FROM ubuntu:noble
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
        ca-certificates \
        gosu \
        sudo && \
    userdel ubuntu && \
    groupadd -g ${USER_UID} ${USER_NAME} && \
    useradd -s /bin/bash -u ${USER_UID} -g ${USER_NAME} -o -c "" -m ${USER_NAME} && \
    usermod -a -G sudo ${USER_NAME} && \
    echo "%devel         ALL = (ALL) NOPASSWD: ALL" >> /etc/sudoers

COPY docker/entrypoint.sh /sbin/entrypoint.sh
ENTRYPOINT ["/sbin/entrypoint.sh"]

RUN apt-get update -qq && \
    DEBIAN_FRONTEND="noninteractive" apt-get install -qq -y --no-install-recommends \
    apt-transport-https \
    build-essential \
    clang-18 \
    cmake \
    git \
    libassimp-dev \
    libb2-dev \
    libclang-18-dev \
    libdbus-1-dev \
    libdrm-dev \
    libegl-dev \
    libfontconfig1-dev \
    libfreetype6-dev \
    libglib2.0-dev \
    libgstreamer1.0-dev \
    libharfbuzz-dev \
    libicu-dev \
    libjpeg-dev \
    libmd4c-html0-dev \
    libssl-dev \
    libsystemd-dev \
    libudev-dev \
    libvulkan-dev \
    libwayland-dev \
    libx11-dev \
    libx11-xcb-dev \
    libxcb-composite0-dev \
    libxcb-cursor-dev \
    libxcb-damage0-dev \
    libxcb-dpms0-dev \
    libxcb-dri2-0-dev \
    libxcb-dri3-dev \
    libxcb-ewmh-dev \
    libxcb-glx0-dev \
    libxcb-icccm4-dev \
    libxcb-image0-dev \
    libxcb-keysyms1-dev \
    libxcb-present-dev \
    libxcb-present-dev \
    libxcb-randr0-dev \
    libxcb-record0-dev \
    libxcb-render-util0-dev \
    libxcb-res0-dev \
    libxcb-screensaver0-dev \
    libxcb-shape0-dev \
    libxcb-shm0-dev \
    libxcb-sync-dev \
    libxcb-sync0-dev \
    libxcb-util-dev \
    libxcb-xf86dri0-dev \
    libxcb-xfixes0-dev \
    libxcb-xinerama0-dev \
    libxcb-xinput-dev \
    libxcb-xtest0-dev \
    libxcb-xv0-dev \
    libxcb-xvmc0-dev \
    libxcb1-dev \
    libxext-dev \
    libxfixes-dev \
    libxi-dev \
    libxkbcommon-dev \
    libxkbcommon-x11-dev \
    libxrender-dev \
    libzstd-dev \
    locales \
    llvm-18-dev \
    ninja-build \
    perl \
    python3 \
    zlib1g-dev

# Set the locale
RUN sed -i '/en_US.UTF-8/s/^# //g' /etc/locale.gen && \
locale-gen
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

ENV QT_HOME="/home/${USER_NAME}/qt"
USER ${USER_NAME}
RUN mkdir ${QT_HOME}

RUN cd ${QT_HOME} && git clone https://code.qt.io/qt/qt5.git

RUN cd ${QT_HOME}/qt5 && git checkout v${QT_VERSION} && perl init-repository

COPY docker/noble/qt6.5.3-static.patch ${QT_HOME}/qt6.5.3-static.patch
# fixes build with Ubuntu 24.04, see https://bugreports.qt.io/browse/QTBUG-117950
# RUN cd ${QT_HOME}/qt5/qtbase && git cherry-pick --no-commit bcdec67cd2e8063aca6beed6b301e47974bcd2ce
# The following patch fixes by appling some commits
# https://bugreports.qt.io/browse/QTBUG-117950
# bcdec67cd2e8063aca6beed6b301e47974bcd2ce
# https://bugreports.qt.io/browse/QTBUG-119469
# 3f45905953d57e0174059d7d9d6bc75c3c1c406c
# 7d9d1220f367d9275dfaa7ce12e89b0a9f4c1978
# 3073b9c4dec5e5877363794bf81cbd4b84fdb9ee
RUN cd ${QT_HOME}/qt5/qtbase && git apply ${QT_HOME}/qt6.5.3-static.patch

RUN mkdir ${QT_HOME}/static-build && \
    cd ${QT_HOME}/static-build && \
    ../qt5/configure -prefix /opt/Qt/${QT_VERSION}/gcc_64 -static \
        -skip qt3d \
        -skip qtactiveqt \
        -skip qtcharts \
        -skip qtconnectivity \
        -skip qtcoap \
        -skip qtdatavis3d \
        -skip qthttpserver \
        -skip qtimageformats \
        -skip qtlanguageserver \
        -skip qtlocation \
        -skip qtlottie \
        -skip qtmqtt \
        -skip qtmultimedia \
        -skip qtopcua \
        -skip qtpositioning \
        -skip qtqa \
        -skip qtquick3d \
        -skip qtquick3dphysics \
        -skip qtquickeffectmaker \
        -skip qtquicktimeline \
        -skip qtsensors \
        -skip qtspeech \
        -skip qtvirtualkeyboard \
        -skip qtwebchannel \
        -skip qtwebview \
        -skip qtwebengine

RUN cd ${QT_HOME}/static-build && cmake --build . --parallel

USER root

RUN cd ${QT_HOME}/static-build && cmake --install .

FROM ubuntu:noble
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
        sudo && \
    userdel ubuntu && \
    groupadd -g ${USER_UID} ${USER_NAME} && \
    useradd -s /bin/bash -u ${USER_UID} -g ${USER_NAME} -o -c "" -m ${USER_NAME} && \
    usermod -a -G sudo ${USER_NAME} && \
    echo "%devel         ALL = (ALL) NOPASSWD: ALL" >> /etc/sudoers

COPY docker/entrypoint.sh /sbin/entrypoint.sh
ENTRYPOINT ["/sbin/entrypoint.sh"]

# Qbs build dependencies
RUN apt-get update -qq && \
    DEBIAN_FRONTEND="noninteractive" apt-get install -qq -y --no-install-recommends \
        bison \
        build-essential \
        ca-certificates \
        capnproto \
        ccache \
        clang-18 \
        clang-tidy-18 \
        cmake \
        curl \
        flex \
        git \
        help2man \
        icoutils \
        libb2-dev \
        libbrotli-dev \
        libcapnp-dev \
        libdbus-1-dev \
        libdrm-dev \
        libfontconfig1-dev \
        libfreetype6-dev \
        libgl1-mesa-dev \
        libglib2.0-dev \
        libgrpc++-dev \
        libharfbuzz-dev \
        libicu-dev \
        libjpeg-dev \
        libmd4c-html0 \
        libnanopb-dev \
        libpcre++ \
        libprotobuf-dev \
        libx11-xcb-dev \
        libxcb-composite0-dev \
        libxcb-cursor-dev \
        libxcb-damage0-dev \
        libxcb-dpms0-dev \
        libxcb-dri2-0-dev \
        libxcb-dri3-dev \
        libxcb-ewmh-dev \
        libxcb-glx0-dev \
        libxcb-icccm4-dev \
        libxcb-image0-dev \
        libxcb-keysyms1-dev \
        libxcb-present-dev \
        libxcb-present-dev \
        libxcb-randr0-dev \
        libxcb-record0-dev \
        libxcb-render-util0-dev \
        libxcb-res0-dev \
        libxcb-screensaver0-dev \
        libxcb-shape0-dev \
        libxcb-shm0-dev \
        libxcb-sync-dev \
        libxcb-sync0-dev \
        libxcb-util-dev \
        libxcb-xf86dri0-dev \
        libxcb-xfixes0-dev \
        libxcb-xinerama0-dev \
        libxcb-xinput-dev \
        libxcb-xtest0-dev \
        libxcb-xv0-dev \
        libxcb-xvmc0-dev \
        libxcb1-dev \
        libxkbcommon-dev \
        libudev-dev \
        libxkbcommon-x11-dev \
        libzstd-dev \
        locales \
        nanopb \
        ninja-build \
        nsis \
        p7zip-full \
        pkg-config \
        protobuf-compiler \
        protobuf-compiler-grpc \
        psmisc \
        python3-pip \
        python3-setuptools \
        python3-venv \
        subversion \
        unzip \
        zip && \
    update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100 && \
    update-alternatives --install /usr/bin/clang-check clang-check /usr/bin/clang-check-18 100 && \
    update-alternatives --install /usr/bin/python python /usr/bin/python3 100

ENV LLVM_INSTALL_DIR=/usr/lib/llvm-18

# Set up Python
RUN python3 -m venv /venv && \
    /venv/bin/pip3 install beautifulsoup4 lxml protobuf==3.19.1 pyyaml conan

ENV PATH=/venv/bin:${PATH}

# Set the locale
RUN sed -i '/en_US.UTF-8/s/^# //g' /etc/locale.gen && \
    locale-gen
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

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
