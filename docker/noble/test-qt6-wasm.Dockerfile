#
# Testing Qbs with Web Assembly
#
FROM ubuntu:noble
LABEL Description="Ubuntu wasm test environment for Qbs"
ARG QT_VERSION
ARG EMSCRIPTEN_VERSION

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

RUN sudo apt-get update -qq && \
    apt-get install -qq -y \
        curl \
        libglib2.0-0 \
        locales \
        p7zip-full \
        python3 \
        python3-pip \
        unzip

# Set the locale
RUN sed -i '/en_US.UTF-8/s/^# //g' /etc/locale.gen && \
locale-gen
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

RUN curl -L https://github.com/emscripten-core/emsdk/archive/refs/heads/main.zip > /emsdk.zip && \
    unzip /emsdk.zip && \
    rm /emsdk.zip && \
    mv /emsdk-main /emsdk && \
    cd /emsdk && \
    ./emsdk install ${EMSCRIPTEN_VERSION} && \
    ./emsdk activate ${EMSCRIPTEN_VERSION}

ENV EMSDK=/emsdk \
    PATH="/emsdk:/emsdk/upstream/emscripten:/emsdk/node/18.20.3_64bit/bin:${PATH}"

#
# Install Qt for Wasm from qt.io
#
COPY scripts/install-qt.sh install-qt.sh

RUN ./install-qt.sh --version ${QT_VERSION} qtbase qtdeclarative qttools qtx11extras qtscxml qt5compat icu && \
    echo "export PATH=/opt/Qt/${QT_VERSION}/gcc_64/bin:\${PATH}" > /etc/profile.d/qt.sh

RUN ./install-qt.sh --version ${QT_VERSION} --target wasm --toolchain wasm_multithread qtbase qtdeclarative qttools qtx11extras qtscxml qt5compat icu && \
    echo "export PATH=/opt/Qt/${QT_VERSION}/wasm_multithread/bin:\${PATH}" > /etc/profile.d/qt.sh

# Work-around for QTBUG-79020
RUN echo "export QT_NO_GLIB=1" >> /etc/profile.d/qt.sh
