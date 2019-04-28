#
# Downloads and builds Qt from source. This is simpler than using the Qt online
# installer. We do it in a separate stage to keep the number of dependencies low
# in the final Docker image.
#
FROM debian:9 as build-qt-linux-x86_64
ARG QT_VERSION

# Downloader dependencies
RUN apt-get update -qq && \
    apt-get install -qq -y --no-install-recommends \
        ca-certificates \
        xz-utils \
        wget

# Download
RUN mkdir -p /qt/source && \
    wget -nv --continue --tries=20 --waitretry=10 --retry-connrefused \
        --no-dns-cache --timeout 300 -qO- \
        https://download.qt.io/official_releases/qt/${QT_VERSION%??}/${QT_VERSION}/single/qt-everywhere-src-${QT_VERSION}.tar.xz \
        | tar --strip-components=1 -C /qt/source -xJf-

# Build dependencies
RUN apt-get update -qq && \
    apt-get install -qq -y --no-install-recommends \
        autoconf \
        automake \
        autopoint \
        binutils \
        bison \
        build-essential \
        flex \
        intltool \
        libgdk-pixbuf2.0-dev \
        libffi-dev \
        libfontconfig1-dev \
        libfreetype6-dev \
        libgmp-dev \
        libmpc-dev \
        libmpfr-dev \
        libtool \
        libtool-bin \
        libx11-dev \
        libxext-dev \
        libxfixes-dev \
        libxi-dev \
        libxrender-dev \
        libxcb1-dev \
        libx11-xcb-dev \
        libxcb-glx0-dev \
        libz-dev \
        openssl

# Build Qt
RUN mkdir -p qt/build && \
    cd qt/build && \
    ../source/configure \
        -prefix /opt/qt5-linux-x86_64 \
        -release \
        -shared \
        -opensource \
        -confirm-license \
        -no-compile-examples \
        -nomake tests \
        -platform linux-g++ \
        -qt-freetype -qt-harfbuzz -qt-pcre -qt-xcb -qt-zlib \
        -no-sql-sqlite -no-cups -no-dbus -no-pch \
        -no-feature-accessibility -no-opengl \
        -skip qtactiveqt \
        -skip qt3d \
        -skip qtcanvas3d \
        -skip qtcharts \
        -skip qtconnectivity \
        -skip qtdeclarative  \
        -skip qtdatavis3d \
        -skip qtdoc \
        -skip qtgamepad \
        -skip qtgraphicaleffects \
        -skip qtimageformats \
        -skip qtlocation \
        -skip qtmultimedia \
        -skip qtquickcontrols \
        -skip qtquickcontrols2 \
        -skip qtpurchasing \
        -skip qtremoteobjects \
        -skip qtscxml \
        -skip qtsensors \
        -skip qtserialbus \
        -skip qtspeech \
        -skip qtsvg \
        -skip qttranslations \
        -skip qttools \
        -skip qtwayland \
        -skip qtvirtualkeyboard \
        -skip qtwebchannel \
        -skip qtwebengine \
        -skip qtwebsockets \
        -skip qtwebview \
        -skip qtwinextras \
        -skip qtxmlpatterns \
        && \
    make -j $(nproc --all) >/dev/null && \
    make install

# Build a stable Qbs release
FROM debian:9
LABEL Description="Debian development environment for Qbs with Qt and various dependencies for testing Qbs modules and functionality"
ARG QBS_VERSION=1.13.0

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
        gosu && \
    groupadd -g ${USER_UID} ${USER_NAME} && \
    useradd -s /bin/bash -u ${USER_UID} -g ${USER_NAME} -o -c "" -m ${USER_NAME}
COPY entrypoint.sh entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]

# Qbs build dependencies
RUN apt-get update -qq && \
    apt-get install -qq -y --no-install-recommends \
        build-essential \
        ca-certificates \
        git \
        pkg-config \
        make \
        help2man \
        python-pip \
        wget && \
    pip install beautifulsoup4 lxml # for building the documentation

# Install Qt installation from build stage
COPY --from=build-qt-linux-x86_64 /opt/qt5-linux-x86_64 /opt/qt5-linux-x86_64
ENV PATH=/opt/qt5-linux-x86_64/bin:${PATH}

# Download and build Qbs
RUN mkdir -p /qbs && \
    wget -nv --continue --tries=20 --waitretry=10 --retry-connrefused \
        --no-dns-cache --timeout 300 -qO- \
        http://download.qt.io/official_releases/qbs/${QBS_VERSION}/qbs-src-${QBS_VERSION}.tar.gz \
        | tar --strip-components=1 -C /qbs -xzf- && \
    cd /qbs && \
    qmake -r qbs.pro && \
    make -j $(nproc --all) && \
    make install INSTALL_ROOT=/ && \
    rm -rf /qbs

# Configure Qbs
USER $USER_NAME
RUN qbs-setup-toolchains --detect && \
    qbs-setup-qt /opt/qt5-linux-x86_64/bin/qmake qt5-linux-x86_64 && \
    qbs config defaultProfile qt5-linux-x86_64

# Switch back to root user for the entrypoint script.
USER root
