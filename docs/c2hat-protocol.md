# C2Hat Protocol

C2Hat operates in text mode on simple TCP/IP sockets. Each command is a null-terminated C-style string up to a current maximum length of 1536 bytes.

Each command must start with a command type and end with a null terminator character (`\0`). Each command cannot be longer than 1536 bytes, including the `/<command>` tag and the null terminator. Both the server and client applications will keep receiving data on their input streams until a null terminator is found or the 1536 bytes limit is reached (whichever comes first) before processing the command.

```
/msg [...message text...]\0
/nick MyUserName\0
```

Commands may include [Unicode characters](./unicode.md), encoded into UTF-8 strings.

## Client commands

A TCP client can send these commands to the server:

 - `/nick` the content of the message is the user nickname, for a maximum of
   15 Unicode characters
 - `/msg` start a message
 - `/quit` closes the chat session
 - `/auth` when authentication will be available through ssh keys (not yet implemented)
 - `/help [command]` display the general help or the help for the specific command,
   if available (not yet implemented)
 - `/list` displays a list of connected users (not yet implemented)
 - `/admin [command]` administrator commands (not yet implemented)

Once the clients send a command, the server responds with `/ok` or `/err` with this format

```
/err <error message>\0
/ok [optional message]\0
```

## Server commands

A server can also send commands and messages to the client:

 - `/msg` incoming message (e.g. `/msg [UserName] Text of the message…\0`)
 - `/nick` to prompt the user for authentication: the next client message must be the user’s nickname or the server will terminate the connection
 - `/log` activity log (e.g. `/log John55 just joined the chat\0`)
 - `/quit` tells the client to close the connection (the server will close the socket after sending the command)

## Session management

After a connection is established, a client can only send a `/nick` command to authenticate or a `/quit` command to close the session. This first command is subject to a timeout of 30 seconds.

Once the user is authenticated, the session timeout is set to 3 minutes.

## Authentication

At the moment there is no real authentication system. The server validates the nickname format from the user and checks that there is no other user with the same name already connected.

Possible future authentication methods include:

 - SQLite3 user database
 - SSH public/private key verification
