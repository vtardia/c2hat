# C2Hat on Docker

Both C2Hat server and client can be packaged as Docker images, where Docker is available.

## Building the images

The easy way to do this is by running

  - `make docker` to build both images
  - `make docker/server` to build a server image
  - `make docker/client` to build a client image
  - `make docker/clean` to remove all related images

If you want to build manually you can build the server with

```console
docker build --target server -t c2hat/server .
```

and the client with

```console
docker build --target client -t c2hat/client .
```

## TLS Certificates

Both client and server images have the Mozilla TLS CA certificates installed in `/etc/ssl/certs`.

## Running the Server

The server image is configured to start the server in foreground on port 10000 on all IP addresses (0.0.0.0).

It will look for TLS certificates and configuration files in the default locations so those can be injected with the `-v` switch.

The minimum command needed to run the server is

```console
docker run --rm -it \
  -v /path/to/certificate.pem:/etc/c2hat/ssl/cert.pem \
  -v /path/to/privkey.pem:/etc/c2hat/ssl/key.pem \
  -p <host port>:10000 \
  c2hat/server
```

The default locale is `en_GB.UTF-8`, to override this, for example with US English, you need to add to the above

```console
-e LANG=en_US.UTF-8 -e LANGUAGE=en_US.UTF-8
```

To use a custom configuration file, you need to inject it by adding

```console
-v /path/to/your-server.conf:/etc/c2hat/server.conf
```

If you feel curious and want to open a shell with the server image, you can run

```console
docker run --rm -it --entrypoint sh c2hat/server
```

## Running the Client

The client is pre-configured to look for CA certificates in the default location (`/etc/ssl/certs`) so unless you need to inject your own certificates you just need to run

```console
docker run --rm -it c2hat/client <host> <port>
```

Just like the server you can change language using environment variables and look into the image with

```console
docker run --rm -it --entrypoint sh c2hat/client
```
