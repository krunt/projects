#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include "MCrypto.h"

#include "MCryptoDef.c"

void
DumpMemory(const char *label, char *v, int len) {
    const char *hex[] = {"0", "1", "2", "3", "4", "5", "6", "7", 
                         "8", "9", "A", "B", "C", "D", "E", "F"};
    int i = 0;
    printf("_xs %s(%d): ", label, len);
    for (i = 0; i < len; ++i) {
        printf("%s%s", hex[(v[i] & 0xF0) >> 4], hex[v[i] & 0x0F]);
    }
    printf("\n");
}

MODULE = MCrypto		PACKAGE = MCrypto		

SV*
packet_decrypt(sv, decrypt_key, decrypt_IV, index)
    SV *sv
    SV *decrypt_key
    SV *decrypt_IV
    SV *index
  CODE:
    char *out = malloc(SvCUR(sv));
    if (!out) {
        RETVAL = &PL_sv_undef;
        goto done;
    }

    char *buf = (char *)SvPV(sv, SvCUR(sv));
    struct crypto_struct *mas = get_ciphertab();
    struct crypto_struct *crypto = &mas[SvIV(index)];

    //printf("_xs name: %s\n", crypto->name);

    //DumpMemory("input_sv", SvPV(sv, SvCUR(sv)), SvCUR(sv));
    //DumpMemory("decrypt_key", SvPV(decrypt_key, SvCUR(decrypt_key)), SvCUR(decrypt_key));
    //DumpMemory("decrypt_IV", SvPV(SvRV(decrypt_IV), SvCUR(SvRV(decrypt_IV))), SvCUR(SvRV(decrypt_IV)));
    
    if (crypto->set_decrypt_key(crypto, (char *)SvPV(decrypt_key, SvCUR(decrypt_key))) < 0) {
        free(out);
        RETVAL = &PL_sv_undef;
        goto done;
    }

    //DumpMemory("_xs decrypt inside", crypto->key, crypto->keysize/8);

    int iv_length = SvCUR(SvRV(decrypt_IV));
    char *dec_iv = malloc(iv_length);
    if (!dec_iv) {
        free(out);
        RETVAL = &PL_sv_undef;
        goto done;
    }

    //printf("iv_length: %d\n", iv_length);

    memcpy(dec_iv, SvPV(SvRV(decrypt_IV), iv_length), iv_length);
    //DumpMemory("_xs decrypt inside", crypto->key, crypto->keysize / 8);
    crypto->xdecrypt(crypto, buf, out, SvCUR(sv), dec_iv);
    //DumpMemory("_xs decrypt inside", crypto->key, crypto->keysize / 8);
    sv_setpvn(SvRV(decrypt_IV), dec_iv, iv_length);

    RETVAL = newSVpv(out, SvCUR(sv));

    free(dec_iv);
    free(out);

    done:

  OUTPUT:
    RETVAL


SV* 
packet_encrypt(sv, encrypt_key, encrypt_IV, index)
    SV *sv
    SV *encrypt_key
    SV *encrypt_IV
    SV *index
  CODE:
    char *out = malloc(SvCUR(sv)); 
    if (!out) {
        RETVAL = &PL_sv_undef;
        goto done;
    }

    char *buf = (char *)SvPV(sv, SvCUR(sv));
    struct crypto_struct *mas = get_ciphertab();
    struct crypto_struct *crypto = &mas[SvIV(index)];
    
    if (crypto->set_encrypt_key(crypto, SvPV(encrypt_key, SvCUR(encrypt_key))) < 0) {
        free(out);
        RETVAL = &PL_sv_undef;
        goto done;
    }

    int iv_length = SvCUR(SvRV(encrypt_IV));
    char *enc_iv = malloc(iv_length);
    if (!enc_iv) {
        free(out);
        RETVAL = &PL_sv_undef;
        goto done;
    }

    memcpy(enc_iv, SvPV(SvRV(encrypt_IV), iv_length), iv_length);
    crypto->xencrypt(crypto, buf, out, SvCUR(sv), enc_iv);
    sv_setpvn(SvRV(encrypt_IV), enc_iv, iv_length);

    RETVAL = newSVpv(out, SvCUR(sv));

    free(enc_iv);
    free(out);

    done:

  OUTPUT:
    RETVAL

SV*
compute_hmac(sv, encryptMAC)
    SV *sv
    SV *encryptMAC
  CODE:
    char *out = malloc(EVP_MAX_MD_SIZE);
    if (!out) {
        RETVAL = &PL_sv_undef;
        goto done;
    }

    HMAC_CTX *ctx = hmac_init(SvPV(encryptMAC, SvCUR(encryptMAC)), 20, HMAC_SHA1);
    if (ctx == NULL) {
        free(out);
        RETVAL = &PL_sv_undef;
        goto done;
    }

    int finallen;
    hmac_update(ctx, (char *)SvPV(sv, SvCUR(sv)), (int)SvCUR(sv));
    hmac_final(ctx, out, &finallen);

    RETVAL = newSVpv(out, finallen);

    free(out);

    done:

  OUTPUT:
    RETVAL

IV
packet_hmac_verify(sv, decryptMAC, mac)
    SV *sv
    SV *decryptMAC
    SV *mac
  CODE:
    unsigned char hmacbuf[EVP_MAX_MD_SIZE] = {0};

    char *out = malloc(EVP_MAX_MD_SIZE);
    if (!out) {
        RETVAL = 0;
        goto done;
    }

    HMAC_CTX *ctx = hmac_init(SvPV(decryptMAC, SvCUR(decryptMAC)), 20, HMAC_SHA1);
    if (ctx == NULL) {
        free(out);
        RETVAL = 0;
        goto done;
    }

    int finallen;
    hmac_update(ctx, (char *)SvPV(sv, SvCUR(sv)), (int)SvCUR(sv));
    hmac_final(ctx, hmacbuf, &finallen);

    if (!memcmp((char *)SvPV(mac, SvCUR(mac)), hmacbuf, finallen)) {
        RETVAL = 1;
    } else {
        RETVAL = 0;
    }

    free(out);

    done:

  OUTPUT:
    RETVAL

AV*
list_cryptors()
  CODE:
    AV *av = newAV();

    struct crypto_struct *cipher_tab = get_ciphertab();
    for (int i = 0; cipher_tab[i].name; ++i) {
        struct crypto_struct *ct = &cipher_tab[i];
        av_push(av, newSVpv(ct->name, strlen(ct->name)));
        av_push(av, newSViv(ct->blocksize));
        av_push(av, newSViv(ct->keylen));
        av_push(av, newSViv(ct->keysize));
    }

    RETVAL = av;
    sv_2mortal((SV *)RETVAL);

  OUTPUT:
    RETVAL
