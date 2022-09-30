# C2hat Server Configuration

The server can load its configuration from an INI-style configuration file and apply sensible defaults for non-specified settings.

## Working directory

 - If started as a daemon by a local user: `~/.local/state/c2hat`
 - If started as a daemon by the root user `/usr/local/c2hat`
 - If started in foreground its the shell’s current working directory

## Configuration file lookup

The server will first load the minimal default configuration and then it will look for a configuration file using the following rules. If no configuration file is found, the default values are kept.

If executed by a **local non-root user**, regardless of admin privileges, the server will first look for a user-level configuration file located at `~/.config/c2hat/server.conf`.

**If no local file is found or if the user is root**, the server will look in the following locations (in this order):

 - `/etc/c2hat/server.conf`
 - `/usr/local/etc/c2hat/server.conf`

**A custom configuration file path** can also be specified by using the `-c` or `--config-file` command-line options.

```console
$ c2hat-server start -c /path/to/my-config.conf
```

A configuration file will look like this:

```ini
locale = en_GB.UTF-8

[log]
level = info
path = /path/to/some.log

[tls]
cert_file = /path/to/cert.pem
key_file = /path/to/key.pem

[server]
host = 123.234.123.23
port = 5678
max_connections = 10
pid_file_path = /path/to/my.pid
```

For any parameters left unset, the default values will be used (see below).

The following **parameters can also be specified as command-line options**, which will override any other value either default or loaded from a file:

 - Host (`-h/--host`): must be an IP address or `localhost`
 - Port (`-p/--port`)
 - TLS certificate file (`-s/--ssl-cert`)
 - TLS private key file (`-k/--ssl-key`)
 - Max connections (`-m/--max-clients`)

## Configurable parameters

### Logging

 - Log level (default: `info`)
 - Log file path
    - local user default: `~/.local/state/c2hat/server.log`
    - system user default: `/var/log/c2hat-server.log`

### TLS

 - Certificate file path
    - local user default is `~/.config/c2hat/ssl/cert.pem`
    - system default will look for (in this order)
        - `/etc/c2hat/ssl/cert.pem`
        - `/usr/local/etc/c2hat/ssl/cert.pem`
 - Private key path
    - local user default is `~/.config/c2hat/ssl/key.pem`
    - system default will look for (in this order)
        - `/etc/c2hat/ssl/key.pem`
        - `/usr/local/etc/c2hat/ssl/key.pem`

### Locale

The default locale is fetched from the current system configuration, it must be UTF-8 compatible or the server won't start.

### Server

 - Host (default: `localhost`)
 - Port (default: `10000`)
 - Max Connections (default: `5`)
 - PID file path
    - local user default is `~/.local/run/c2hat.pid`
    - system service default is `/var/run/c2hat.pid`

## Runtime Configuration

During the startup phase, the server configuration is stored into a shared memory location (`/dev/shm/*` on Linux systems). The name of the location will be `/<appname>` if the server was started by the root user, or `/<appname>-<userid>` if the server was started by another local user.

Permissions on the shared memory are set to `0600` so that each user can only access her own instance of the configuration and related files (log file, pid file, certificates, etc).

An `EPERM` error will be thrown if a command tries to access an unauthorised location.

### Encrypted settings

The content of the shared memory location is encrypted using the AES CBC algorithm and the key is randomly generated at compile time. The first `sizeof(size_t)` bytes of the encrypted payload stores contain the length of the encrypted content. This is necessary because encryption and decryption happen in different processes and the decryption function needs to know the size of the encrypted content to work properly.
