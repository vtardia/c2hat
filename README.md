# C2Hat

C2Hat is an experimental TCP chat system (server and clients) written in C. The goal of the project is to refresh and improve my C/C++ skills, therefore there is no commitment to have a production-grade system at the moment.

**This is experimental software, use it at your own risk.**

## Current features

The current release of C2hat server:

 - implements its own simple [chat protocol](./docs/c2hat-protocol.md) over TCP/IP;
 - is multi-threaded;
 - can be started either as a system service or a user-space program;
 - can be started in the background and controlled by a frontend command;
 - can be started as a foreground process and embedded into a [Docker container](./docs/docker.md);
 - can send and receives [Unicode messages](./docs/unicode.md) with emojis;
 - can receive some `/` commands from the users;
 - uses the TLS protocol.

The current C2Hat client:

 - is a command line ncurses-based application;
 - runs on Linux/macOS/FreeBSD;
 - provides TLS certificate verification for non-localhost servers;

## Features in progress

 - Send and receive end-to-end encrypted messages
 - Implement some user authentication plugins
 - Implement `/admin` commands for admin users
 - Graphical cross-platform client based on wxWidgets

## Build and Install

To compile the server you need a macOS or Linux machine with:

 - standard build tools (GCC, Clang, Make, etc)
 - OpenSSL (dev and static package)
 - Mozilla CA certificates for TSL certificate handling and verification
 - Valgrind (optional) for testing

To compile the client you need the above plus:

 - ncurses (dev, static and terminfo packages)

### Build

```console
$ make [server|client]
```

### Run tests

```console
$ make test
```

**Important**: run `make clean` after running tests before (re)running `make` or `make install` to clean some debug leftovers.

### Install

After a successful build you can run both the client and the server from the `bin` directory. If you want to install the binaries to the default location (`/usr/local`) run

```console
$ [sudo] make install
```

To install the binaries to a custom location instead, run

```console
$ [sudo] make install -e PREFIX=/custom/path
```

## Usage

### Start the server

To start the server with the default configuration (`localhost:10000`, max 5 connections):

```console
$ c2hat-server start \
  --ssl-cert=</path/to/cert.pem> \
  --ssk-key=</path/to/key.pem>

Starting background server on localhost:10000 with locale 'en_GB.UTF-8' and PID 12345
```

### Start the command line client

```console
$ c2hat-cli localhost 10000 --capath=</path/to/ca/certs>
```

where `capath` is a directory containing CA certificates, for example `/etc/ssl/certs` on Linux.

You can also use a singe CA certificate file with the `cacert` option

```console
$ c2hat-cli localhost 10000 --cacert=</path/to/ca/certificate.pem>
```

### Check the server's status

```
$ c2hat-server status

The server is running with the following configuration:
         PID: 12345
 Config file: /home/someuser/.config/c2hat/server.conf
    Log file: /home/someuser/.local/state/c2hat/server.log
    PID file: /home/someuser/.local/run/c2hat.pid
        Host: localhost
        Port: 10000
    SSL cert: /home/someuser/.config/c2hat/ssl/cert.pem
     SSL key: /home/someuser/.config/c2hat/ssl/key.pem
      Locale: en_GB.UTF-8
 Max Clients: 5
 Working Dir: /home/someuser/.local/state/c2hat
```

### Stop the server

```
$ c2hat-server stop

The server is running with PID 12345
The server with PID 12345 has been successfully stopped
```

## Documentation

 - [Client Commands](./docs/client-commands.md)
 - [Server Configuration](./docs/server-configuration.md)
 - [TLS Configuration](./docs/tls-configuration.md)
 - [C2hat Protocol](./docs/c2hat-protocol.md)
 - [Unicode Support](./docs/unicode.md)
 - [Docker Support](./docs/docker.md)

## License

C2Hat is licensed under GPL-3.0. C2Hat also includes external libraries that are available under a variety of licenses. Please refer to the [LICENSE](./LICENSE) file for detailed information.

## Intellectual Property Considerations

The author asserts no intellectual property rights over the protocols and algorithms used, and is not aware of any other such claims.
