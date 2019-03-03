# Copyright (C) 2019  Christian Berger
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Part to build opendlv-device-camera-mjpegoverhttp.
FROM alpine:3.8 as builder
MAINTAINER Christian Berger "christian.berger@gu.se"
RUN echo http://dl-4.alpinelinux.org/alpine/v3.8/main > /etc/apk/repositories && \
    echo http://dl-4.alpinelinux.org/alpine/v3.8/community >> /etc/apk/repositories && \
    apk update && \
    apk --no-cache add \
        cmake \
        autoconf \
        automake \
        libtool \
        g++ \
        git \
        nasm \
        libx11-dev \
        make \
        upx
RUN cd /tmp && \
    git clone --depth 1 https://github.com/curl/curl.git && \
    cd curl && ./buildconf && \
    CFLAGS="-Os" CURL_DISABLE_FTP=1 CURL_DISABLE_FILE=1 CURL_DISABLE_TELNET=1 CURL_DISABLE_DICT=1 CURL_DISABLE_TFTP=1 CURL_DISABLE_GOPHER=1 CURL_DISABLE_IMAP=1 CURL_DISABLE_POP3=1 CURL_DISABLE_SMTP=1 ./configure --disable-ares --disable-cookies --disable-crypto-auth --disable-ipv6 --disable-manual --disable-proxy --disable-unix-sockets --disable-verbose --disable-versioned-symbols --without-libidn --without-librtmp --without-ssl --without-zlib --disable-libcurl-option --disable-rtsp --disable-shared --prefix=/usr && \
    make && make install
RUN cd /tmp && \
    git clone --depth 1 https://github.com/libjpeg-turbo/libjpeg-turbo.git && \
    cd libjpeg-turbo && mkdir build && cd build && \
    cmake -D CMAKE_INSTALL_PREFIX=/usr .. && \
    make turbojpeg-static && cp libturbojpeg.a /usr/lib && cp ../turbojpeg.h /usr/include
RUN cd /tmp && \
    git clone --depth 1 https://chromium.googlesource.com/libyuv/libyuv && \
    cd libyuv &&\
    make -f linux.mk libyuv.a && cp libyuv.a /usr/lib && cd include && cp -r * /usr/include
ADD . /opt/sources
WORKDIR /opt/sources
RUN mkdir build && \
    cd build && \
    cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX=/tmp .. && \
    make && make install && upx -9 /tmp/bin/opendlv-device-camera-mjpegoverhttp


# Part to deploy opendlv-device-camera-mjpegoverhttp.
FROM alpine:3.9
MAINTAINER Christian Berger "christian.berger@gu.se"

RUN echo http://dl-4.alpinelinux.org/alpine/v3.8/main > /etc/apk/repositories && \
    apk update && \
    apk --no-cache add \
        libx11

WORKDIR /usr/bin
COPY --from=builder /tmp/bin/opendlv-device-camera-mjpegoverhttp .
ENV NO_AT_BRIDGE=1
ENTRYPOINT ["/usr/bin/opendlv-device-camera-mjpegoverhttp"]

