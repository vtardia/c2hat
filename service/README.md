# C2Hat as a System Service

If desired, C2Hat server can be run as a system service under Linux, FreeBSD and macOS.

## Create a service user

Don’t run the C2Hat service as root! Do this instead:

 - create a non-admin user and group for the service (e.g. `c2hat:c2hat`)

 - place your configuration file in one of the shared paths like `/etc/c2hat/server.conf` or `/usr/local/etc/c2hat/server.conf`

 - ensure the certificate and private key are readable by the service user and group

### On Linux

```console
adduser --system --group --home /usr/local/c2hat c2hat
```

### On FreeBSD

```console
pw adduser c2hat -d /nonexistent -s /usr/sbin/nologin -c "A nice chat system"
```

### On macOS

macOS service accounts must start with `_` and have a UID between 200 and 400. In order to see other existing user IDs run:

```console
dscl . -list /Users UniqueID
```

Then create the user with:

```console
sysadminctl -addUser _c2hat -roleAccount --UID <300>
```

## Create the service

## All systems

 - Create a configuration file by customising the `example.conf` provided and save it to `/usr/local/etc/c2hat/server.conf`.

 - Ensure you have the correct certificate file and private key, otherwise, connections will fail with a `bad certificate` or `unknown ca` error.

 - Create the working directories (e.g. `/usr/local/c2hat`) and assign the correct permissions to the service user.

### Linux

Run `make service/systemd > /path/to/destfile.service` or customise the `systemd.service` example. Save the result as `/etc/systemd/system/<your-app-name>.service`, then run

```console
[sudo] systemctl start <your-app-name>
[sudo] systemctl enable <app-name>
```

### FreeBSD

 - Customise the `freebsd` example
 - or run `make service/freebsd > /path/to/destfile`
 - save it as `/usr/local/etc/rc.d/<app-name>`
 - and make it executable
 - add `<app-name>_enable="YES"` to `/etc/.rc.conf`
 - run `[sudo] service <app-name>` start to start the service
 - use `status` to check if it’s running or `stop` to stop it

### macOS

 - Customise the `macos.plist` example
 - or run `make service/macos > /path/to/destfile.plist`
 - save it as `/Library/LaunchDaemons/<app-ID>.plist`
 - run `[sudo] launchctl load /Library/LaunchDaemons/<app-ID>.plist` to load the service
 - run `[sudo] launchctl unload /Library/LaunchDaemons/<app-ID>.plist` to stop the service
