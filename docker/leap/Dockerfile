#
# Install Qt and Qbs for Linux
#
FROM opensuse/leap:15.3
LABEL Description="OpenSUSE development environment for Qbs with Qt and various dependencies for testing Qbs modules and functionality"
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
RUN zypper in -y \
        ca-certificates \
        sudo \
        system-user-mail \
        system-group-wheel && \
    groupadd -g ${USER_UID} ${USER_NAME} && \
    useradd -s /bin/bash -u ${USER_UID} -g ${USER_NAME} -o -c "" -m ${USER_NAME} && \
    usermod -a -G wheel ${USER_NAME} && \
    echo "%devel         ALL = (ALL) NOPASSWD: ALL" >> /etc/sudoers

COPY docker/leap/entrypoint.sh /sbin/entrypoint.sh
ENTRYPOINT ["/sbin/entrypoint.sh"]

# # Qbs build dependencies
RUN zypper install -y \
        bison \
        capnproto \
        ccache \
        cmake \
        command-not-found \
        curl \
        gcc10 \
        gcc10-c++ \
        glibc-devel-static \
        flex \
        fontconfig \
        git \
        gzip \
        help2man \
        icoutils \
        libcapnp-devel \
        libgthread-2_0-0 \
        libfreetype6 \
        Mesa-libGL-devel \
        Mesa-libGL1 \
        nanopb-devel \
        ninja \
        perl \
        pkg-config \
        psmisc \
        python3-pip \
        p7zip-full \
        subversion \
        tar \
        unzip \
        which \
        zip && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10 100 && \
    pip install --upgrade pip && \
    pip install beautifulsoup4 lxml

#
# Install Qt and Qbs for Linux from qt.io
#
COPY scripts/install-qt.sh install-qt.sh

RUN ./install-qt.sh --version ${QT_VERSION} qtbase qtdeclarative qttools qtx11extras qtscxml qt5compat icu && \
    ./install-qt.sh --version ${QTCREATOR_VERSION} qtcreator && \
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
