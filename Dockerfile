FROM alpine:3.16

WORKDIR /usr/src/app

COPY ./src ./src
COPY Makefile ./

RUN apk -U add --no-cache \
    alpine-sdk \
    ncurses \
    ncurses-dev \
    ncurses-static \
    openssl \
    openssl-dev \
    openssl-libs-static \
    ncurses-terminfo \
    ncurses-terminfo-base \
    ca-certificates \
# && cd src \
 && make \
# && make install \
# && make clean \
# && cd ..  \
 && rm -rf src \
 && rm Makefile \
 && apk del alpine-sdk ncurses-dev ncurses-static openssl-libs-static openssl-dev \
 && rm -rf /var/cache/apk/*

