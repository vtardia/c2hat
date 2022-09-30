# TLS Configuration

Both C2Hat server and client use TLS connections to communicate. The server requires a valid TLS certificate, and the client needs to be able to verify the server certificate and identity using a trusted CA certificate.

## Server

If the server runs on a public accessible network, a Let's Encrypt certificate will work fine. The startup command will look like

```console
$ c2hat-server start \
  --ssl-cert=/path/to/letsencrypt/fullchain.pem \
  --ssk-key=/path/to/letsencrypt/privkey.pem
```

You may also use a [configuration file](./server-configuration.md):

```ini
[tls]
cert_file = /path/to/letsencrypt/fullchain.pem
key_file = /path/to/letsencrypt/privkey.pem
```

## Client

The command line client needs to know where to look for the CA certificate(s). On GNU/Linux the command will look like

```console
$ c2hat-cli localhost 10000 --capath=/etc/ssl/certs
```

On macOS, and some Linux installations, you need to install the `ca-certificates` package and use its full path

```console
$ c2hat-cli localhost 10000 \
  --capath=/opt/homebrew/Cellar/ca-certificates/YYYY-MM-DD/share/ca-certificates
```

There is no configuration file option for the client yet, but the client will look into the default directory `~/.local/share/c2hat/ssl`.

You can symlink this directory to the full `ca-certificates` path...

```console
$ ln -s /opt/homebrew/[...]/ca-certificates ~/.local/share/c2hat/ssl
```

...and then start the client with `c2hat-cli localhost 10000`.


## Self-signed certificates

You can use self-signed certificates for development, but you need to

 - create a private Certification Authority (CA)
 - create a certificate signing request (CSR) and private key for the server
 - use the CA's private key to sign the CSR above
 - create a full-chain certificate file for the server that includes
   the server's signed certificate and the CA certificate
 - use the resulting full-chain certificate and private key to configure the server
 - use the CA certificate on the client to verify the server

**Note**: the client will always verify both the certificate validity and if there's a match between the server host/IP and the certificate host names. If both server and client are running on `localhost`, the server name and IP are not checked with the certificate.
