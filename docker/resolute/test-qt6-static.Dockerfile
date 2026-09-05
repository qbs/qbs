#
# Testing Qbs with static qt6
#
FROM ubuntu:resolute
LABEL Description="Ubuntu static qt6 test environment for Qbs"

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
    clang-21 \
    cmake \
    git \
    libassimp-dev \
    libb2-dev \
    libclang-21-dev \
    libclang-cpp21-dev \
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
    llvm-21-dev \
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

ARG QT_VERSION

RUN cd ${QT_HOME}/qt5 && git checkout v${QT_VERSION} && perl init-repository --module-subset=default,\
-qt3d,\
-qtactiveqt,\
-qtcanvaspainter,\
-qtcharts,\
-qtconnectivity,\
-qtcoap,\
-qtdatavis3d,\
-qtgraphs,\
-qtgrpc,\
-qthttpserver,\
-qtimageformats,\
-qtlanguageserver,\
-qtlocation,\
-qtlottie,\
-qtmqtt,\
-qtmultimedia,\
-qtnetworkauth,\
-qtopcua,\
-qtopenapi,\
-qtpositioning,\
-qtqa,\
-qtquick3d,\
-qtquick3dphysics,\
-qtquickeffectmaker,\
-qtquicktimeline,\
-qtremoteobjects,\
-qtsensors,\
-qtserialbus,\
-qtserialport,\
-qtspeech,\
-qtsvg,\
-qttasktree,\
-qttranslations,\
-qtvirtualkeyboard,\
-qtwayland,\
-qtwebengine,\
-qtwebchannel,\
-qtwebsockets,\
-qtwebview

ENV LLVM_INSTALL_DIR=/usr/lib/llvm-21

RUN mkdir ${QT_HOME}/static-build && \
    cd ${QT_HOME}/static-build && \
    ../qt5/configure -prefix /opt/Qt/${QT_VERSION}/gcc_64 -static

RUN cd ${QT_HOME}/static-build && cmake --build . --parallel

USER root

RUN cd ${QT_HOME}/static-build && cmake --install .

FROM ubuntu:resolute
LABEL Description="Ubuntu static qt6 test environment for Qbs"

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
        clang-22 \
        clang-tidy-22 \
        cmake \
        curl \
        flex \
        git \
        help2man \
        icoutils \
        libb2-dev \
        libbrotli-dev \
        libcapnp-dev \
        libclang1-21 \
        libclang-cpp21 \
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
        libwayland-dev \
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
    update-alternatives --install /usr/bin/clang clang /usr/bin/clang-22 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-22 100 && \
    update-alternatives --install /usr/bin/clang-check clang-check /usr/bin/clang-check-22 100 && \
    update-alternatives --install /usr/bin/python python /usr/bin/python3 100

ENV LLVM_INSTALL_DIR=/usr/lib/llvm-22

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
COPY scripts/install-qbs.sh install-qbs.sh

ARG QT_VERSION
ARG QBS_VERSION

RUN ./install-qbs.sh --version ${QBS_VERSION} && \
    echo "export PATH=/opt/Qt/${QT_VERSION}/gcc_64/bin:/opt/qbs/qbs-linux-x86_64-${QBS_VERSION}/bin:\${PATH}" > /etc/profile.d/qt.sh

ENV PATH=/opt/Qt/${QT_VERSION}/gcc_64/bin:/opt/qbs/qbs-linux-x86_64-${QBS_VERSION}/bin:${PATH}

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
