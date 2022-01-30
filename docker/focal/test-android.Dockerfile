#
# Android SDK/NDK + Qt for Android for testing Qbs
#
FROM ubuntu:focal
LABEL Description="Ubuntu test environment for Qbs and Qt for Android"

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
    apt-get install -qq -y --no-install-recommends \
        ca-certificates \
        curl \
        libasan5 \
        libglib2.0-0 \
        openjdk-8-jdk-headless \
        p7zip-full \
        unzip

ENV JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64
RUN echo "export JAVA_HOME=${JAVA_HOME}" > /etc/profile.d/android.sh && \
    echo "export PATH=${JAVA_HOME}/bin:\${PATH}" >> /etc/profile.d/android.sh

ARG ANDROID_NDK_VERSION

ENV ANDROID_HOME="/home/${USER_NAME}/android"
ENV ANDROID_SDK_ROOT=${ANDROID_HOME}
ENV ANDROID_NDK_ROOT=${ANDROID_HOME}/"ndk"/${ANDROID_NDK_VERSION}
ENV PATH="${JAVA_HOME}:${ANDROID_HOME}/platform-tools:${ANDROID_HOME}/cmdline-tools/bin:$PATH"
RUN echo "export ANDROID_HOME=/home/${USER_NAME}/android" >> /etc/profile.d/android.sh && \
    echo "export ANDROID_SDK_ROOT=${ANDROID_SDK_ROOT}" >> /etc/profile.d/android.sh && \
    echo "export ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT}" >> /etc/profile.d/android.sh && \
    echo "export PATH=${ANDROID_HOME}/platform-tools:${ANDROID_HOME}/cmdline-tools/bin:\$PATH" >> /etc/profile.d/android.sh

#
# We ned to run the following steps as the target user
#
USER ${USER_NAME}
RUN mkdir ${ANDROID_HOME}

# Get Android command line tools
ARG COMMAND_LINE_TOOLS_VERSION="6858069"
RUN curl -s https://dl.google.com/android/repository/commandlinetools-linux-${COMMAND_LINE_TOOLS_VERSION}_latest.zip > ${ANDROID_HOME}/commandlinetools.zip && \
    unzip ${ANDROID_HOME}/commandlinetools.zip -d ${ANDROID_HOME} && \
    rm -v ${ANDROID_HOME}/commandlinetools.zip

# Accept SDK license
ARG ANDROID_PLATFORM="android-30"
ARG BUILD_TOOLS="29.0.2"
RUN yes | sdkmanager "--sdk_root=${ANDROID_HOME}" --verbose --licenses && \
          sdkmanager "--sdk_root=${ANDROID_HOME}" --update && \
          sdkmanager "--sdk_root=${ANDROID_HOME}" "platforms;${ANDROID_PLATFORM}"

RUN yes | sdkmanager "--sdk_root=${ANDROID_HOME}" "build-tools;${BUILD_TOOLS}"
RUN yes | sdkmanager "--sdk_root=${ANDROID_HOME}" "platform-tools"
RUN yes | sdkmanager "--sdk_root=${ANDROID_HOME}" "tools"
RUN yes | sdkmanager "--sdk_root=${ANDROID_HOME}" "ndk;${ANDROID_NDK_VERSION}"

RUN /usr/lib/jvm/java-8-openjdk-amd64/bin/keytool -genkey -keystore /home/${USER_NAME}/.android/debug.keystore -alias androiddebugkey -storepass android -keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname 'CN=Android Debug,O=Android,C=US'

# Install ndk samples in ${ANDROID_NDK_ROOT}/samples
RUN cd ${ANDROID_NDK_ROOT} && \
    curl -sLO https://github.com/android/ndk-samples/archive/master.zip && \
    unzip -q master.zip && \
    rm -v master.zip && \
    mv ndk-samples-master samples

# Install android-BasicMediaDecoder in ${ANDROID_SDK_ROOT}/samples
RUN mkdir ${ANDROID_SDK_ROOT}/samples && \
    cd ${ANDROID_SDK_ROOT}/samples && \
    curl -sLO https://github.com/googlearchive/android-BasicMediaDecoder/archive/master.zip && \
    unzip -q master.zip && \
    rm -v master.zip && \
    mv android-BasicMediaDecoder-master android-BasicMediaDecoder

# Download buildtool to generate aab packages in ${ANDROID_SDK_ROOT}
RUN cd ${ANDROID_SDK_ROOT} && \
    curl -sLO https://github.com/google/bundletool/releases/download/1.3.0/bundletool-all-1.3.0.jar

USER root

#
# Install Qt and Qbs for Linux from qt.io
#
ARG QT_VERSION
COPY scripts/install-qt.sh install-qt.sh
RUN if [ "${QT_VERSION}" \< "5.14" ] || [ ! "${QT_VERSION}" \< "6.0.0" ]; then \
        QT_ABIS="android_armv7 android_arm64_v8a android_x86 android_x86_64"; \
    else \
        QT_ABIS="any"; \
    fi; \
    if [ ! "${QT_VERSION}" \< "6.0.0" ]; then \
        ./install-qt.sh --version ${QT_VERSION} qtbase qtdeclarative icu; \
        if [ "${QT_VERSION}" \< "6.1.0" ]; then \
            QT_COMPONENTS="qtbase qtdeclarative qttools qtquickcontrols2 qtquicktimeline svg"; \
        else \
            QT_COMPONENTS="qtbase qtdeclarative qttools qtquicktimeline svg"; \
        fi; \
    else \
        QT_COMPONENTS="qtbase qtdeclarative qttools qtimageformats"; \
    fi; \
    for abi in ${QT_ABIS}; do \
        ./install-qt.sh --version ${QT_VERSION} --target android --toolchain ${abi} ${QT_COMPONENTS}; \
    done && \
    echo "export QT_VERSION=${QT_VERSION}" >> /etc/profile.d/qt.sh
