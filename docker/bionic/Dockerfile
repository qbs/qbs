#
# Downloads and builds Qt from source. We do it in a
# separate stage to keep the number of dependencies low in
# the final Docker image.
#
FROM ubuntu:bionic as build-qt-mingw32_w64
ARG QT_VERSION

RUN apt-get update -qq && \
    apt-get install -qq -y --no-install-recommends \
        build-essential \
        ca-certificates \
        libclang-3.9-dev \
        libgl1-mesa-dev \
        mingw-w64 \
        python \
        xz-utils \
        wget

ENV LLVM_INSTALL_DIR=/usr/lib/llvm-3.9

RUN mkdir -p /qt/source && \
    wget -nv --continue --tries=20 --waitretry=10 --retry-connrefused \
        --no-dns-cache --timeout 300 -qO- \
        https://download.qt.io/official_releases/qt/${QT_VERSION%??}/${QT_VERSION}/single/qt-everywhere-src-${QT_VERSION}.tar.xz \
        | tar --strip-components=1 -C /qt/source -xJf-

RUN mkdir -p qt/build && \
    cd qt/build && \
    ../source/configure \
        -prefix /opt/Qt/${QT_VERSION}/mingw32_w64 \
        -release \
        -shared \
        -opensource \
        -confirm-license \
        -nomake examples \
        -nomake tests \
        -xplatform win32-g++ \
        -opengl desktop \
        -device-option CROSS_COMPILE=/usr/bin/x86_64-w64-mingw32- \
        -qt-sqlite -qt-libpng \
        -no-cups -no-dbus -no-pch \
        -no-feature-accessibility \
        -skip qtactiveqt \
        -skip qt3d \
        -skip qtcanvas3d \
        -skip qtcharts \
        -skip qtconnectivity \
        -skip qtdatavis3d \
        -skip qtdeclarative \
        -skip qtdoc \
        -skip qtgamepad \
        -skip qtgraphicaleffects \
        -skip qtimageformats \
        -skip qtlocation \
        -skip qtmultimedia \
        -skip qtnetworkauth \
        -skip qtquickcontrols \
        -skip qtquickcontrols2 \
        -skip qtpurchasing \
        -skip qtremoteobjects \
        -skip qtscxml \
        -skip qtsensors \
        -skip qtserialbus \
        -skip qtserialport \
        -skip qtspeech \
        -skip qtsvg \
        -skip qttranslations \
        -skip qtwayland \
        -skip qtvirtualkeyboard \
        -skip qtwebchannel \
        -skip qtwebengine \
        -skip qtwebsockets \
        -skip qtwebview \
        -skip qtwinextras \
        -skip qtxmlpatterns \
        -skip qtx11extras

# Build and transform stdout into . to reduce the noise
RUN cd qt/build && \
    make -j $(nproc --all) | stdbuf -o0 tr -cd '\n' | stdbuf -o0 tr '\n' '.' && \
    make install

#
# Install Qt and Qbs for Linux and combine that with Qt for Windows from the
# previous stage
#
FROM ubuntu:bionic
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

COPY docker/bionic/entrypoint.sh /sbin/entrypoint.sh
ENTRYPOINT ["/sbin/entrypoint.sh"]

# Qbs build dependencies
RUN apt-get update -qq && \
    apt-get install -qq -y --no-install-recommends \
        bison \
        build-essential \
        ca-certificates \
        ccache \
        clang-8 \
        clang-tidy-8 \
        curl \
        flex \
        git \
        help2man \
        icoutils \
        libclang-3.9-dev \
        libdbus-1-3 \
        libfreetype6 \
        libfontconfig1 \
        libgl1-mesa-dev \
        libgl1-mesa-glx \
        libprotobuf-dev \
        libgrpc++-dev \
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
    update-alternatives --install /usr/bin/pip pip /usr/bin/pip3 100 && \
    pip install beautifulsoup4 lxml pyyaml

ENV LLVM_INSTALL_DIR=/usr/lib/llvm-8


#
# Install Qt and Qbs for Linux from qt.io
#
COPY scripts/install-qt.sh install-qt.sh

RUN ./install-qt.sh --version ${QT_VERSION} qtbase qtdeclarative qtscript qttools qtx11extras qtscxml icu && \
    ./install-qt.sh --version ${QTCREATOR_VERSION} qtcreator && \
    echo "export PATH=/opt/Qt/${QT_VERSION}/gcc_64/bin:/opt/Qt/Tools/QtCreator/bin:\${PATH}" > /etc/profile.d/qt.sh

ENV PATH=/opt/Qt/${QT_VERSION}/gcc_64/bin:/opt/Qt/Tools/QtCreator/bin:${PATH}


#
# Install Qt installation from build stage
#
COPY --from=build-qt-mingw32_w64 /opt/Qt/${QT_VERSION}/mingw32_w64 /opt/Qt/${QT_VERSION}/mingw32_w64

#
# Install mingw toolchain to cross build for Windows and select
# POSIX API to make use of threading support in the stl. That
# is required by Qbs.
#
RUN apt-get install -qq -y --no-install-recommends \
        mingw-w64 && \
    printf "1\n" | update-alternatives --config x86_64-w64-mingw32-g++


# Configure Qbs
USER $USER_NAME
RUN qbs-setup-toolchains /usr/bin/g++ gcc && \
    qbs-setup-toolchains /usr/bin/clang clang && \
    qbs-setup-qt /opt/Qt/${QT_VERSION}/gcc_64/bin/qmake qt-gcc_64 && \
    qbs config profiles.qt-gcc_64.baseProfile gcc && \
    qbs-setup-qt /opt/Qt/${QT_VERSION}/gcc_64/bin/qmake qt-clang_64 && \
    qbs config profiles.qt-clang_64.baseProfile clang && \
    qbs config defaultProfile qt-gcc_64 && \
    \
    qbs-setup-toolchains /usr/bin/x86_64-w64-mingw32-g++ mingw && \
    qbs-setup-qt /opt/Qt/${QT_VERSION}/mingw32_w64/bin/qmake qt-mingw32_w64 && \
    qbs config profiles.qt-mingw32_w64.baseProfile mingw

# Switch back to root user for the entrypoint script.
USER root

# Work-around for QTBUG-79020
RUN echo "export QT_NO_GLIB=1" >> /etc/profile.d/qt.sh
