/**
 * This file is based on sample code from the OpenSSL Wiki
 * https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption
 */
#include "encrypt.h"

char *AES_errors(char *error, size_t length) {
  if (error) {
    ERR_error_string_n(ERR_get_error(), error, length);
    return error;
  }
  return NULL;
}

/**
 * Encrypts the given text and returns the length
 * of the encrypted data
 */
size_t AES_encrypt(
  byte *data,
  size_t length,
  const byte *key,
  const byte *iv,
  byte *encrypted
) {
  EVP_CIPHER_CTX *ctx;

  int len;

  size_t encryptedLength;

  // Initialise the context
  if (!(ctx = EVP_CIPHER_CTX_new())) return -1;

  // Initialise the encryption
  if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv)) return -1;

  // Perform the encryption (can be called multiple times)
  if (1 != EVP_EncryptUpdate(ctx, encrypted, &len, data, length)) return -1;
  encryptedLength = len;

  // Finalise the encryption,
  // start writing at encrypted + len position
  if(1 != EVP_EncryptFinal_ex(ctx, (encrypted + len), &len)) return -1;
  encryptedLength += len;

  // Cleanup
  EVP_CIPHER_CTX_free(ctx);
  return encryptedLength;
}

/**
 * Decrypts the given binary data and returns the length
 * of the decrypted data
 */
size_t AES_decrypt(
  byte *data,
  size_t length,
  const byte *key,
  const byte *iv,
  byte *decrypted
) {
  EVP_CIPHER_CTX *ctx;

  int len;

  size_t decryptedLength;

  // Initialise the context
  if (!(ctx = EVP_CIPHER_CTX_new())) return -1;

  // Initialise the decryption
  if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv)) return -1;

  // Perform the decryption
  if (1 != EVP_DecryptUpdate(ctx, decrypted, &len, data, length)) return -1;
  decryptedLength = len;

  // Finalise the decryption, starting at (decrypted + len) position
  if (1 != EVP_DecryptFinal_ex(ctx, decrypted + len, &len)) return -1;
  decryptedLength += len;

  // Cleanup
  EVP_CIPHER_CTX_free(ctx);
  return decryptedLength;
}

