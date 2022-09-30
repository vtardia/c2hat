/**
 * Copyright (C) 2020-2022 Vito Tardia <https://vito.tardia.me>
 *
 * This file is part of C2Hat
 *
 * C2Hat is a simple client/server TCP chat written in C
 *
 * C2Hat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * C2Hat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with C2Hat. If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * Some of the content of this file has been freely adapted from sample code
 * found on the OpenSSL Wiki
 * https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption
 */
#ifndef EVP_H
  #define EVP_H

  #include <string.h>
  #include <stdbool.h>
  #include <openssl/conf.h>
  #include <openssl/evp.h>
  #include <openssl/err.h>

  typedef unsigned char byte;

  typedef struct {
    byte key[32]; //< A 256 bit (32 bytes) key
    byte iv[16];  //< A 128 bit (16 bytes) IV
  } AESKey;

  /**
   * Creates a valid AES key and iv from a null-terminated string
   * and fills the given AESKey structure
   */
  bool AES_keyFromString(const char *passphrase, AESKey *key);

  /**
   * Computes the SHA256 digest for the input data
   * The output parameter digest must be at least 65 characters long
   * in order to contain the full result.
   *
   * @param[in]  data   Source data to hash
   * @param[in]  size   Size of the source data
   * @param[out] digest Pointer to a char array that will contain the result
   * @param[in]  dsize  Size of the output string
   */
  bool SHA256Sum(const void *data, size_t size, char *digest, size_t dsize);

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

