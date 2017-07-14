FROM debian:9
LABEL Description="Debian development environment for Qbs with Qt 5.9 and various dependencies for testing Qbs modules and functionality"

# Dependencies of the Qt offline installer
RUN apt-get -y update && apt-get install -y \
    curl \
    libdbus-1-3 \
    libexpat1 \
    libfontconfig1 \
    libfreetype6 \
    libgl1-mesa-glx \
    libglib2.0-0 \
    libx11-6 \
    libx11-xcb1

COPY qtifwsilent.qs qtifwsilent.qs
RUN curl -L -O 'https://download.qt.io/official_releases/qt/5.9/5.9.1/qt-opensource-linux-x64-5.9.1.run' && \
    chmod +x qt-opensource-linux-x64-5.9.1.run && \
    QT_INSTALL_DIR=/usr/local/Qt ./qt-opensource-linux-x64-5.9.1.run --platform minimal --script qtifwsilent.qs && \
    rm -f qt-opensource-linux-x64-5.9.1.run
ENV QTDIR /usr/local/Qt/5.9.1/gcc_64
ENV PATH="/usr/local/Qt/Tools/QtCreator/bin:${PATH}"

RUN apt-get -y update && apt-get install -y \
    g++ \
    gcc \
    git
