/**
 * This file is based on sample code from the OpenSSL Wiki
 * https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption
 */
#ifndef EVP_H
  #define EVP_H

  #include <string.h>
  #include <openssl/conf.h>
  #include <openssl/evp.h>
  #include <openssl/err.h>

  typedef unsigned char byte;

  /**
   * Encrypts the given data and returns the length
   * of the encrypted payload
   */
  size_t AES_encrypt(
    byte *data,       //< Source data to encrypt
    size_t length,    //< Size of the source data
    const byte *key,  //< A 256 bit (32 bytes) key
    const byte *iv,   //< A 128 bit (16 bytes) IV (Initialisation Vector)
    byte *encrypted   //< Encrypted data result
  );

  /**
   * Decrypts the given payload and returns the length
   * of the decrypted data
   */
  size_t AES_decrypt(
    byte *data,       //< Encrypted data to decrypt
    size_t length,    //< Size of the source data
    const byte *key,  //< A 256 bit (32 bytes) key
    const byte *iv,   //< A 128 bit (16 bytes) IV (Initialisation Vector)
    byte *decrypted   //< Decrypted data
  );

  /**
   * Fetches the last error into error and returns it
   */
  char *AES_errors(char *error, size_t length);
#endif

