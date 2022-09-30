# Unicode Support

C2Hat can handle Unicode characters in messages, but requires user names to contain only latin characters.

## Client

The command line client will not start unless the user's locale supports UTF-8. The user will need to manually set up the terminal session to do so.

User input is read in Unicode (`wchar_t *`) and converted to UTF-8 (standard `char *`) to be processed and sent to the server.

Server input is received as standard `char *` by the client and converted into Unicode for displaying.

The maximum length of the message from the client side is set to 280 (latin) wide characters. Non-latin characters like kanji or emojis will take more space so the character count will be adjusted accordingly.

## Server

The server can be configured to use a specific locale and will fail to start if the underlying system does not have support for it.

The server almost always deals with standard `char *`, except during the authentication phase.
