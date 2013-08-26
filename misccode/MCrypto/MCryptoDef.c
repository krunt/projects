#include "MCrypto.h"

HMAC_CTX * hmac_init(const void *key, int len, int type) {
  HMAC_CTX * ctx = NULL;

  ctx = malloc(sizeof(*ctx));
  if (ctx == NULL) {
    return NULL;
  }

  switch(type) {
    case HMAC_SHA1:
      HMAC_Init(ctx, key, len, EVP_sha1());
      break;
    case HMAC_MD5:
      HMAC_Init(ctx, key, len, EVP_md5());
      break;
    default:
      free(ctx);
      ctx = NULL;
  }

  return ctx;
}

void hmac_update(HMAC_CTX *ctx, const void *data, unsigned long len) {
  HMAC_Update(ctx, data, len);
}

void hmac_final(HMAC_CTX *ctx, unsigned char *hashmacbuf, unsigned int *len) {
  HMAC_Final(ctx,hashmacbuf,len);
  HMAC_cleanup(ctx);
  free(ctx);
}

static int alloc_key(struct crypto_struct *cipher) {
    cipher->key = (void *)malloc(cipher->keylen);
    if (cipher->key == NULL) {
        return -1;
    }
    memset(cipher->key, 0, cipher->keylen);
    return 0;
}

static int blowfish_set_key(struct crypto_struct *cipher, void *key) {
    if (cipher->key == NULL) {
        if (alloc_key(cipher) < 0) {
            return -1;
        }
    }
    BF_set_key(cipher->key, 16, key);
    return 0;
}

static void blowfish_encrypt(struct crypto_struct *cipher, void *in,
        void *out, unsigned long len, void *IV) {
    BF_cbc_encrypt(in, out, len, cipher->key, IV, BF_ENCRYPT);
}

static void blowfish_decrypt(struct crypto_struct *cipher, void *in,
        void *out, unsigned long len, void *IV) {
    BF_cbc_encrypt(in, out, len, cipher->key, IV, BF_DECRYPT);
}

static int aes_set_encrypt_key(struct crypto_struct *cipher, void *key) {
    if (cipher->key == NULL) {
        if (alloc_key(cipher) < 0) {
            return -1;
        }
        if (AES_set_encrypt_key(key, cipher->keysize, cipher->key) < 0) {
            free(cipher->key);
            return -1;
        }
    }
    return 0;
}
static int aes_set_decrypt_key(struct crypto_struct *cipher, void *key) {
    if (cipher->key == NULL) {
        if (alloc_key(cipher) < 0) {
            return -1;
        }
        if (AES_set_decrypt_key(key,cipher->keysize,cipher->key) < 0) {
            free(cipher->key);
            return -1;
        }
    }
    return 0;
}

static void aes_encrypt(struct crypto_struct *cipher, void *in, void *out,
        unsigned long len, void *IV) {
    AES_cbc_encrypt(in, out, len, cipher->key, IV, AES_ENCRYPT);
}

static void aes_decrypt(struct crypto_struct *cipher, void *in, void *out,
        unsigned long len, void *IV) {
    AES_cbc_encrypt(in, out, len, cipher->key, IV, AES_DECRYPT);
}

static int des3_set_key(struct crypto_struct *cipher, void *key) {
  if (cipher->key == NULL) {
    if (alloc_key(cipher) < 0) {
      return -1;
    }

    DES_set_odd_parity(key);
    DES_set_odd_parity((void*)((uint8_t*)key + 8));
    DES_set_odd_parity((void*)((uint8_t*)key + 16));
    DES_set_key_unchecked(key, cipher->key);
    DES_set_key_unchecked((void*)((uint8_t*)key + 8), 
            (void*)((uint8_t*)cipher->key + sizeof(DES_key_schedule)));
    DES_set_key_unchecked((void*)((uint8_t*)key + 16), 
            (void*)((uint8_t*)cipher->key + 2 * sizeof(DES_key_schedule)));
  }

  return 0;
}

static void des3_encrypt(struct crypto_struct *cipher, void *in,
    void *out, unsigned long len, void *IV) {
  DES_ede3_cbc_encrypt(in, out, len, cipher->key,
      (void*)((uint8_t*)cipher->key + sizeof(DES_key_schedule)),
      (void*)((uint8_t*)cipher->key + 2 * sizeof(DES_key_schedule)),
      IV, 1);
}

static void des3_decrypt(struct crypto_struct *cipher, void *in,
    void *out, unsigned long len, void *IV) {
  DES_ede3_cbc_encrypt(in, out, len, cipher->key,
      (void*)((uint8_t*)cipher->key + sizeof(DES_key_schedule)),
      (void*)((uint8_t*)cipher->key + 2 * sizeof(DES_key_schedule)),
      IV, 0);
}

static void des3_1_encrypt(struct crypto_struct *cipher, void *in,
    void *out, unsigned long len, void *IV) {
  DES_ncbc_encrypt(in, out, len, cipher->key, IV, 1);
  DES_ncbc_encrypt(out, in, len, (void*)((uint8_t*)cipher->key + sizeof(DES_key_schedule)),
      (void*)((uint8_t*)IV + 8), 0);
  DES_ncbc_encrypt(in, out, len, (void*)((uint8_t*)cipher->key + 2 * sizeof(DES_key_schedule)),
      (void*)((uint8_t*)IV + 16), 1);
}

static void des3_1_decrypt(struct crypto_struct *cipher, void *in,
    void *out, unsigned long len, void *IV) {
  DES_ncbc_encrypt(in, out, len, (void*)((uint8_t*)cipher->key + 2 * sizeof(DES_key_schedule)),
      IV, 0);
  DES_ncbc_encrypt(out, in, len, (void*)((uint8_t*)cipher->key + sizeof(DES_key_schedule)),
      (void*)((uint8_t*)IV + 8), 1);
  DES_ncbc_encrypt(in, out, len, cipher->key, (void*)((uint8_t*)IV + 16), 0);
}

struct crypto_struct ciphertab[] = {
    {
        "blowfish-cbc",
        8,
        sizeof(BF_KEY),
        NULL,
        128,
        blowfish_set_key,
        blowfish_set_key,
        blowfish_encrypt,
        blowfish_decrypt
    },
    /*
    {
        "aes128-cbc",
        16,
        sizeof(AES_KEY),
        NULL,
        128,
        aes_set_encrypt_key,
        aes_set_decrypt_key,
        aes_encrypt,
        aes_decrypt
    },
    {
        "aes192-cbc",
        16,
        sizeof(AES_KEY),
        NULL,
        192,
        aes_set_encrypt_key,
        aes_set_decrypt_key,
        aes_encrypt,
        aes_decrypt
    },
    {
        "aes256-cbc",
        16,
        sizeof(AES_KEY),
        NULL,
        256,
        aes_set_encrypt_key,
        aes_set_decrypt_key,
        aes_encrypt,
        aes_decrypt
    },
    */
    {
        "3des-cbc",
        8,
        sizeof(DES_key_schedule) * 3,
        NULL,
        192,
        des3_set_key,
        des3_set_key,
        des3_encrypt,
        des3_decrypt
    },
    {
        "3des-cbc-ssh1",
        8,
        sizeof(DES_key_schedule) * 3,
        NULL,
        192,
        des3_set_key,
        des3_set_key,
        des3_1_encrypt,
        des3_1_decrypt
    },
    {
        NULL,
        0,
        0,
        NULL,
        0,
        NULL,
        NULL,
        NULL,
        NULL
    }
};

struct crypto_struct *get_ciphertab(){
  return ciphertab;
}

