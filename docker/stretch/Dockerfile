#
# Downloads and builds Qt from source. This is simpler than using the Qt online
# installer. We do it in a separate stage to keep the number of dependencies low
# in the final Docker image.
#
FROM debian:9
LABEL Description="Debian development environment for Qbs with Qt and various dependencies for testing Qbs modules and functionality"
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
    apt-get install -qq -y --no-install-recommends \
        ca-certificates \
        gosu \
        sudo && \
    groupadd -g ${USER_UID} ${USER_NAME} && \
    useradd -s /bin/bash -u ${USER_UID} -g ${USER_NAME} -o -c "" -m ${USER_NAME} && \
    usermod -a -G sudo ${USER_NAME} && \
    echo "%devel         ALL = (ALL) NOPASSWD: ALL" >> /etc/sudoers

COPY docker/stretch/entrypoint.sh entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]


# Build and run dependencies for Qbs
RUN apt-get install -qq -y --no-install-recommends \
        build-essential \
        curl \
        git \
        help2man \
        libclang-3.9 \
        libdbus-1-3 \
        libgl1-mesa-glx \
        libfreetype6 \
        libfontconfig1 \
        libgl1-mesa-dev \
        make \
        pkg-config \
        python-pip \
        p7zip-full && \
    pip install beautifulsoup4 lxml # for building the documentation

ENV LLVM_INSTALL_DIR=/usr/lib/llvm-3.9

COPY scripts/install-qt.sh install-qt.sh

RUN ./install-qt.sh --version ${QT_VERSION} qtbase qtdeclarative qtscript qttools qtx11extras icu && \
    ./install-qt.sh --version ${QTCREATOR_VERSION} qtcreator && \
    echo "export PATH=/opt/Qt/${QT_VERSION}/gcc_64/bin:/opt/Qt/Tools/QtCreator/bin:\${PATH}" > /etc/profile.d/qt.sh

ENV PATH=/opt/Qt/${QT_VERSION}/gcc_64/bin:/opt/Qt/Tools/QtCreator/bin:${PATH}

# Configure Qbs
USER $USER_NAME
RUN qbs-setup-toolchains --detect && \
    qbs-setup-qt /opt/Qt/${QT_VERSION}/gcc_64/bin/qmake qt && \
    qbs config defaultProfile qt

# Switch back to root user for the entrypoint script.
USER root
