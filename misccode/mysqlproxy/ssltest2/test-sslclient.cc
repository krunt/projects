#include <openssl/ssl.h>
#include <openssl/err.h>

#include <unistd.h>

static unsigned char dh512_p[]=
{
  0xDA,0x58,0x3C,0x16,0xD9,0x85,0x22,0x89,0xD0,0xE4,0xAF,0x75,
  0x6F,0x4C,0xCA,0x92,0xDD,0x4B,0xE5,0x33,0xB8,0x04,0xFB,0x0F,
  0xED,0x94,0xEF,0x9C,0x8A,0x44,0x03,0xED,0x57,0x46,0x50,0xD3,
  0x69,0x99,0xDB,0x29,0xD7,0x76,0x27,0x6B,0xA2,0xD3,0xD4,0x12,
  0xE2,0x18,0xF4,0xDD,0x1E,0x08,0x4C,0xF6,0xD8,0x00,0x3E,0x7C,
  0x47,0x74,0xE8,0x33,
};

static unsigned char dh512_g[]={
  0x02,
};

static DH *get_dh512(void)
{
  DH *dh;
  if ((dh=DH_new()))
  {
    dh->p=BN_bin2bn(dh512_p,sizeof(dh512_p),NULL);
    dh->g=BN_bin2bn(dh512_g,sizeof(dh512_g),NULL);
    if (! dh->p || ! dh->g)
    {
      DH_free(dh);
      dh=0;
    }
  }
  return(dh);
}

static int
vio_set_cert_stuff(SSL_CTX *ctx, const char *cert_file, const char *key_file,
                   enum enum_ssl_init_error* error)
{
  DBUG_ENTER("vio_set_cert_stuff");
  DBUG_PRINT("enter", ("ctx: 0x%lx  cert_file: %s  key_file: %s",
		       (long) ctx, cert_file, key_file));
  if (cert_file)
  {
    if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0)
    {
      *error= SSL_INITERR_CERT;
      DBUG_PRINT("error",("%s from file '%s'", sslGetErrString(*error), cert_file));
      DBUG_EXECUTE("error", ERR_print_errors_fp(DBUG_FILE););
      fprintf(stderr, "SSL error: %s from '%s'\n", sslGetErrString(*error),
              cert_file);
      fflush(stderr);
      DBUG_RETURN(1);
    }

    if (!key_file)
      key_file= cert_file;

    if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0)
    {
      *error= SSL_INITERR_KEY;
      DBUG_PRINT("error", ("%s from file '%s'", sslGetErrString(*error), key_file));
      DBUG_EXECUTE("error", ERR_print_errors_fp(DBUG_FILE););
      fprintf(stderr, "SSL error: %s from '%s'\n", sslGetErrString(*error),
              key_file);
      fflush(stderr);
      DBUG_RETURN(1);
    }

    /*
      If we are using DSA, we can copy the parameters from the private key
      Now we know that a key and cert have been set against the SSL context
    */
    if (!SSL_CTX_check_private_key(ctx))
    {
      *error= SSL_INITERR_NOMATCH;
      DBUG_PRINT("error", ("%s",sslGetErrString(*error)));
      DBUG_EXECUTE("error", ERR_print_errors_fp(DBUG_FILE););
      fprintf(stderr, "SSL error: %s\n", sslGetErrString(*error));
      fflush(stderr);
      DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}

void
fatal_error(const char*	r)
{
	perror(r);
	exit(0);
}

int
main(	int	argc, char**	argv)
{
	char	client_key[] = "../certsdir/client-key.pem",
            client_cert[] = "../certsdir/client-cert.pem";
	char	ca_file[] = "../certsdir/cacert.pem", *ca_path = NULL, *cipher=NULL;
    int fd;
	struct sockaddr_in sa;
	int err;
	char	xbuf[100]="Ohohhhhoh1234";

	/* ----------------------------------------------- */
	/* Create a socket and connect to server using normal socket calls. */

    fd = socket (AF_INET, SOCK_STREAM, 0)

	memset (&sa, '\0', sizeof(sa));
	sa.sin_family      = AF_INET;
	sa.sin_addr.s_addr = inet_addr ("127.0.0.1");   /* Server IP */
	sa.sin_port        = htons     (41111);          /* Server Port number */

	err = connect(client_vio->sd, (struct sockaddr*) &sa, sizeof(sa));

	/* ----------------------------------------------- */
	/* Now we have TCP conncetion. Start SSL negotiation. */
	read(fd, xbuf, sizeof(xbuf));
	printf("client:got %s\n", xbuf);

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    SSL_CTX *ctx; 
    if (!(ctx = SSL_CTX_new(TLSv1_server_method()))) {
        report_errors();
        return 1;
    }

    if (!SSL_CTX_load_verify_locations(ctx, ca_file, ca_path)) {
        if (!SSL_CTX_set_default_verify_paths(ctx)) {
            SSL_CTX_free(ctx);
            report_errors();
            return 2;
        }
    }

    if (vio_set_cert_stuff(ctx, client_cert, client_key)) {
      report_errors();
      SSL_CTX_free(ssl_fd->ssl_context);
      return 3;
    }

    dh=get_dh512();
    SSL_CTX_set_tmp_dh(ssl_fd->ssl_context, dh);
    DH_free(dh);

    mysslconnect(ssl_connector,client_vio,60L);

    SSL *ssl;
    if (!(ssl= SSL_new(ptr->ssl_context))) {
        report_errors();
    }
    SSL_clear(ssl);
    SSL_SESSION_set_timeout(SSL_get_session(ssl), timeout);
    SSL_set_fd(ssl, fd);

	err = vio_read(client_vio,(uchar*)xbuf, sizeof(xbuf));
	if (err<=0) {
		free(ssl_connector);
		fatal_error("client:SSL_read");
	}
	xbuf[err] = 0;
	printf("client:got %d bytes\n", err);
	free(client_vio);
	free(ssl_connector);
	return 0;
}
