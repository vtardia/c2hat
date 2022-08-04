FROM alpine:3.16 AS base

# Copy source code
WORKDIR /usr/src/app
COPY ./src ./src
COPY Makefile ./

# Add unprivileged user
RUN addgroup appuser --gid 1001
RUN adduser -S -u 1001 -G appuser -h /home/appuser -s /bin/bash appuser

# SERVER: build server executable
FROM base AS server
RUN apk -U add --no-cache \
    alpine-sdk \
    musl-locales \
    openssl \
    openssl-dev \
    openssl-libs-static \
    ca-certificates \
 && make server \
 && mv bin/* /usr/local/bin/ \
 && make clean \
 && rm -rf ./* \
 && apk del alpine-sdk openssl-dev openssl-libs-static \
 && rm -rf /var/cache/apk/*

# Switch to unprivileged user
USER appuser
WORKDIR /home/appuser
EXPOSE 10000
ENV LANG=en_GB.UTF-8 \
    LANGUAGE=en_GB.UTF-8
ENTRYPOINT ["/usr/local/bin/c2hat-server", "start", "-h", "0.0.0.0", "--foreground"]

# CLIENT: build client executable
FROM base AS client
RUN apk -U add --no-cache \
    alpine-sdk \
    musl-locales \
    ncurses \
    ncurses-dev \
    ncurses-static \
    openssl \
    openssl-dev \
    openssl-libs-static \
    ncurses-terminfo \
    ncurses-terminfo-base \
    ca-certificates \
 && make client \
 && mv bin/* /usr/local/bin/ \
 && make clean \
 && rm -rf ./* \
 && apk del alpine-sdk ncurses-dev ncurses-static openssl-libs-static openssl-dev \
 && rm -rf /var/cache/apk/*

# Switch to unprivileged user
USER appuser
ENV LANG=en_GB.UTF-8 \
    LANGUAGE=en_GB.UTF-8
WORKDIR /home/appuser
ENTRYPOINT ["/usr/local/bin/c2hat-cli", "--capath", "/etc/ssl/certs"]

