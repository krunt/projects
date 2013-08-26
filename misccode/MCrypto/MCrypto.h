#ifndef CONFIG_MCRYPTO_H_
#define CONFIG_MCRYPTO_H_

#include <openssl/sha.h>
#include <openssl/md5.h>
#include <openssl/dsa.h>
#include <openssl/rsa.h>
#include <openssl/hmac.h>
#include <openssl/aes.h>
#include <openssl/blowfish.h>
#include <openssl/des.h>

#define HMAC_SHA1 1
#define HMAC_MD5  2

struct crypto_struct {
    const char *name;
    unsigned int blocksize;
    unsigned int keylen;
    void *key;
    unsigned int keysize;
    int (*set_encrypt_key)(struct crypto_struct *cipher, void *key);
    int (*set_decrypt_key)(struct crypto_struct *cipher, void *key);
    void (*xencrypt)(struct crypto_struct *cipher, void *in, void *out,
        unsigned long len, void *IV);
    void (*xdecrypt)(struct crypto_struct *cipher, void *in, void *out,
        unsigned long len, void *IV);
};

extern struct crypto_struct ciphertab[];
struct crypto_struct *get_ciphertab();

HMAC_CTX * hmac_init(const void *key, int len, int type);
void hmac_update(HMAC_CTX *ctx, const void *data, unsigned long len);
void hmac_final(HMAC_CTX *ctx, unsigned char *hashmacbuf, unsigned int *len);

#endif
