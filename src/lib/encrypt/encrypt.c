/**
 * This file is based on sample code from the OpenSSL Wiki
 * https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption
 */
#include "encrypt.h"
#include <stdio.h>

bool SHA256Sum(const void *data, size_t size, char *digest, size_t dsize) {
  byte mdValue[EVP_MAX_MD_SIZE] = {};
  unsigned int mdSize; // => 32 (bytes) for SHA256 digest
  if (EVP_Digest(
    data, size,
    mdValue, &mdSize,
    EVP_get_digestbyname("sha256"), NULL
  )) {
    // Convert to readable text
    char *p = digest;
    char *end = digest + dsize;
    for (unsigned int i = 0; i < mdSize; i++) {
      if (p >= end) break;
      sprintf(p, "%02x", mdValue[i]);
      p += 2;
    }
    return true;
  }
  return false;
}

bool AES_keyFromString(const char *passphrase, AESKey *key) {
  char digest[65] = {};
  // Compute the digest for the passphrase
  if (!SHA256Sum(passphrase, strlen(passphrase), digest, sizeof(digest))) {
    return false;
  }
  // Minimum length for the resulting digest
  const size_t minLength = sizeof(key->key) + sizeof(key->iv);
  if (strlen(digest) >= minLength) {
    for (size_t i = 0; i < sizeof(key->key); i++) {
      key->key[i] = digest[i];
    }
    for (size_t i = sizeof(key->key); i < (sizeof(key->key) + sizeof(key->iv)); i++) {
      key->iv[i] = digest[i];
    }
    return true;
  }
  return false;
}

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

