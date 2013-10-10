
#include "mysqlvio.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

static bool ssl_algorithms_added = false;
static bool ssl_error_strings_loaded = false;

void vio_reset(Vio* vio, enum enum_vio_type type,
               int sd, uint flags);

void vio_delete(Vio* vio)
{
  if (!vio)
    return; /* It must be safe to delete null pointers. */

  if (vio->type != VIO_CLOSED)
    vio->vioclose(vio);
  free((uchar*) vio->read_buffer);
  free((uchar*) vio);
}

int vio_errno(Vio *vio) {
    return 0;
}

int vio_close(Vio * vio)
{
  int r=0;

 if (vio->type != VIO_CLOSED)
  {
    if (shutdown(vio->sd, SHUT_RDWR))
      r= -1;
  }
  vio->type= VIO_CLOSED;
  vio->sd=   -1;
  return r;
}

int vio_blocking(Vio * vio, bool set_blocking_mode)
{
  int r=0;

  if (vio->sd >= 0)
  {
    int old_fcntl=vio->fcntl_mode;
    if (set_blocking_mode)
      vio->fcntl_mode &= ~O_NONBLOCK; /* clear bit */
    else
      vio->fcntl_mode |= O_NONBLOCK; /* set bit */
    r= fcntl(vio->sd, F_SETFL, vio->fcntl_mode);
    if (r == -1)
    {
      vio->fcntl_mode= old_fcntl;
    }
  }
  return r;
}

bool vio_is_blocking(Vio * vio)
{
  bool r;
  r = !(vio->fcntl_mode & O_NONBLOCK);
  return r;
}

size_t vio_read(Vio * vio, uchar* buf, size_t size)
{
  size_t r;
  errno=0;					/* For linux */
  r = read(vio->sd, buf, size);
  return r;
}

size_t vio_read_buff(Vio *vio, uchar* buf, size_t size)
{
  size_t rc;
#define VIO_UNBUFFERED_READ_MIN_SIZE 2048
  if (vio->read_pos < vio->read_end)
  {
    rc= (size_t) (vio->read_end - vio->read_pos) < size
        ? (size_t) (vio->read_end - vio->read_pos) : size;
    memcpy(buf, vio->read_pos, rc);
    vio->read_pos+= rc;
  }
  else if (size < VIO_UNBUFFERED_READ_MIN_SIZE)
  {
    rc= vio_read(vio, (uchar*) vio->read_buffer, VIO_READ_BUFFER_SIZE);
    if (rc != 0 && rc != (size_t) -1)
    {
      if (rc > size)
      {
        vio->read_pos= vio->read_buffer + size;
        vio->read_end= vio->read_buffer + rc;
        rc= size;
      }
      memcpy(buf, vio->read_buffer, rc);
    }
  }
  else
    rc= vio_read(vio, buf, size);
  return rc;
#undef VIO_UNBUFFERED_READ_MIN_SIZE
}

size_t vio_write(Vio * vio, const uchar* buf, size_t size)
{
  size_t r;
  r = write(vio->sd, buf, size);
  return r;
}

const char *vio_description(Vio * vio)
{
  return vio->desc;
}

enum enum_vio_type vio_type(Vio* vio)
{
  return vio->type;
}

int vio_fd(Vio* vio)
{
  return vio->sd;
}

#ifdef HAVE_OPENSSL

static void report_errors(SSL* ssl);

int vio_ssl_close(Vio *vio)
{
  int r= 0;
  SSL *ssl= (SSL*)vio->ssl_arg;

  if (ssl)
  {
    /*
    THE SSL standard says that SSL sockets must send and receive a close_notify
    alert on socket shutdown to avoid truncation attacks. However, this can
    cause problems since we often hold a lock during shutdown and this IO can
    take an unbounded amount of time to complete. Since our packets are self
    describing with length, we aren't vunerable to these attacks. Therefore,
    we just shutdown by closing the socket (quiet shutdown).
    */
    SSL_set_quiet_shutdown(ssl, 1); 
    
    switch ((r= SSL_shutdown(ssl))) {
    case 1:
      /* Shutdown successful */
      break;
    case 0:
      /*
        Shutdown not yet finished - since the socket is going to
        be closed there is no need to call SSL_shutdown() a second
        time to wait for the other side to respond
      */
      break;
    default: /* Shutdown failed */
      break;
    }
  }
  return vio_close(vio);
}

void vio_ssl_delete(Vio *vio)
{
  if (!vio)
    return; /* It must be safe to delete null pointer */

  if (vio->type == VIO_TYPE_SSL)
    vio_ssl_close(vio); /* Still open, close connection first */

  if (vio->ssl_arg)
  {
    SSL_free((SSL*) vio->ssl_arg);
    vio->ssl_arg= 0;
  }

  vio_delete(vio);
}

size_t vio_ssl_read(Vio *vio, uchar* buf, size_t size)
{
  size_t r;
  r= SSL_read((SSL*) vio->ssl_arg, buf, size);
  return r;
}

size_t vio_ssl_write(Vio *vio, const uchar* buf, size_t size)
{
  size_t r;
  r= SSL_write((SSL*) vio->ssl_arg, buf, size);
  return r;
}

enum enum_ssl_init_error
{
  SSL_INITERR_NOERROR= 0, SSL_INITERR_CERT, SSL_INITERR_KEY, 
  SSL_INITERR_NOMATCH, SSL_INITERR_BAD_PATHS, SSL_INITERR_CIPHERS, 
  SSL_INITERR_MEMFAIL, SSL_INITERR_LASTERR
};

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

static const char*
ssl_error_string[] = 
{
  "No error",
  "Unable to get certificate",
  "Unable to get private key",
  "Private key does not match the certificate public key"
  "SSL_CTX_set_default_verify_paths failed",
  "Failed to set ciphers to use",
  "SSL_CTX_new failed"
};

const char*
sslGetErrString(enum enum_ssl_init_error e)
{
  return ssl_error_string[e];
}

static int
vio_set_cert_stuff(SSL_CTX *ctx, const char *cert_file, const char *key_file,
                   enum enum_ssl_init_error* error)
{
  if (cert_file)
  {
    if (SSL_CTX_use_certificate_chain_file(ctx, cert_file) <= 0)
    {
      *error= SSL_INITERR_CERT;
      fprintf(stderr, "SSL error: %s from '%s'\n", sslGetErrString(*error),
              cert_file);
      fflush(stderr);
      return 1;
    }

    if (!key_file)
      key_file= cert_file;

    if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0)
    {
      *error= SSL_INITERR_KEY;
      fprintf(stderr, "SSL error: %s from '%s'\n", sslGetErrString(*error),
              key_file);
      fflush(stderr);
      return 1;
    }

    /*
      If we are using DSA, we can copy the parameters from the private key
      Now we know that a key and cert have been set against the SSL context
    */
    if (!SSL_CTX_check_private_key(ctx))
    {
      *error= SSL_INITERR_NOMATCH;
      fprintf(stderr, "SSL error: %s\n", sslGetErrString(*error));
      fflush(stderr);
      return 1;
    }
  }
  return 0;
}

static void check_ssl_init()
{
  if (!ssl_algorithms_added)
  {
    ssl_algorithms_added= true;
    SSL_library_init();
    OpenSSL_add_all_algorithms();

  }

  if (!ssl_error_strings_loaded)
  {
    ssl_error_strings_loaded= true;
    SSL_load_error_strings();
  }
}

static struct st_VioSSLFd *
new_VioSSLFd(const char *key_file, const char *cert_file,
             const char *ca_file, const char *ca_path,
             const char *cipher, bool is_client_method)
{
  DH *dh;
  struct st_VioSSLFd *ssl_fd;
  enum_ssl_init_error error;

  check_ssl_init();

  if (!(ssl_fd= ((struct st_VioSSLFd*)
                 malloc(sizeof(struct st_VioSSLFd)))))
    return 0;

  ssl_fd->is_client = is_client_method;

  if (!(ssl_fd->ssl_context= SSL_CTX_new(is_client_method ? 
                                         TLSv1_client_method() :
                                         TLSv1_server_method())))
  {
    error= SSL_INITERR_MEMFAIL;
    free((void*)ssl_fd);
    return 0;
  }

  /*
    Set the ciphers that can be used
    NOTE: SSL_CTX_set_cipher_list will return 0 if
    none of the provided ciphers could be selected
  */
  if (cipher &&
      SSL_CTX_set_cipher_list(ssl_fd->ssl_context, cipher) == 0)
  {
    error= SSL_INITERR_CIPHERS;
    SSL_CTX_free(ssl_fd->ssl_context);
    free((void*)ssl_fd);
    return 0;
  }

  /* Load certs from the trusted ca */
  if (SSL_CTX_load_verify_locations(ssl_fd->ssl_context, ca_file, ca_path) == 0)
  {
    if (SSL_CTX_set_default_verify_paths(ssl_fd->ssl_context) == 0)
    {
      error= SSL_INITERR_BAD_PATHS;
      SSL_CTX_free(ssl_fd->ssl_context);
      free((void*)ssl_fd);
      return 0;
    }
  }

  if (vio_set_cert_stuff(ssl_fd->ssl_context, cert_file, key_file, &error))
  {
    SSL_CTX_free(ssl_fd->ssl_context);
    free((void*)ssl_fd);
  }

  /* DH stuff */
  dh=get_dh512();
  SSL_CTX_set_tmp_dh(ssl_fd->ssl_context, dh);
  DH_free(dh);

  return ssl_fd;
}

struct st_VioSSLFd *
new_VioSSLConnectorFd(const char *key_file, const char *cert_file,
                      const char *ca_file, const char *ca_path,
                      const char *cipher)
{
  struct st_VioSSLFd *ssl_fd;
  int verify= SSL_VERIFY_PEER;

  /*
    Turn off verification of servers certificate if both
    ca_file and ca_path is set to NULL
  */
  if (ca_file == 0 && ca_path == 0)
    verify= SSL_VERIFY_NONE;

  if (!(ssl_fd= new_VioSSLFd(key_file, cert_file, ca_file,
                             ca_path, cipher, true)))
  {
    return 0;
  }

  /* Init the VioSSLFd as a "connector" ie. the client side */

  SSL_CTX_set_verify(ssl_fd->ssl_context, verify, NULL);

  return ssl_fd;
}

struct st_VioSSLFd *
new_VioSSLAcceptorFd(const char *key_file, const char *cert_file,
		     const char *ca_file, const char *ca_path,
		     const char *cipher)
{
  struct st_VioSSLFd *ssl_fd;
  int verify= SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE;

  if (!(ssl_fd= new_VioSSLFd(key_file, cert_file, ca_file,
                             ca_path, cipher, false)))
  {
    return 0;
  }
  /* Init the the VioSSLFd as a "acceptor" ie. the server side */

  /* Set max number of cached sessions, returns the previous size */
  SSL_CTX_sess_set_cache_size(ssl_fd->ssl_context, 128);

  //SSL_CTX_set_verify(ssl_fd->ssl_context, verify, NULL);
  SSL_CTX_set_verify(ssl_fd->ssl_context, 0, NULL);

  /*
    Set session_id - an identifier for this server session
    Use the ssl_fd pointer
   */
  SSL_CTX_set_session_id_context(ssl_fd->ssl_context,
				 (const unsigned char *)ssl_fd,
				 sizeof(ssl_fd));

  return ssl_fd;
}

void free_vio_ssl_acceptor_fd(struct st_VioSSLFd *fd)
{
  SSL_CTX_free(fd->ssl_context);
  free((uchar*) fd);
}

static int ssl_do(struct st_VioSSLFd *ptr, Vio *vio, long timeout,
                  int (*connect_accept_func)(SSL*))
{
  SSL *ssl;
  bool was_blocking;

  /* Set socket to blocking if not already set */
  was_blocking = vio_is_blocking(vio);
  vio_blocking(vio, 1);

  if (!(ssl= SSL_new(ptr->ssl_context)))
  {
    report_errors(ssl);
    vio_blocking(vio, was_blocking);
    return 1;
  }
  SSL_clear(ssl);
  SSL_SESSION_set_timeout(SSL_get_session(ssl), timeout);
  SSL_set_fd(ssl, vio->sd);

  if (connect_accept_func(ssl) < 1)
  {
    report_errors(ssl);
    SSL_free(ssl);
    vio_blocking(vio, was_blocking);
    return 1;
  }

  /*
    Connection succeeded. Install new function handlers,
    change type, set sd to the fd used when connecting
    and set pointer to the SSL structure
  */
  vio_reset(vio, VIO_TYPE_SSL, SSL_get_fd(ssl), 0);
  vio->ssl_arg= (void*)ssl;

  if (ptr->is_client) { 
    SSL_set_connect_state(ssl);
  } else {
    SSL_set_accept_state(ssl);
  }

  return 0;
}

int sslconnect(struct st_VioSSLFd *ptr, Vio *vio, long timeout)
{
  return ssl_do(ptr, vio, timeout, SSL_connect);
}

int sslaccept(struct st_VioSSLFd *ptr, Vio *vio, long timeout)
{
  return ssl_do(ptr, vio, timeout, SSL_accept);
}

static void report_errors(SSL* ssl)
{
  unsigned long	l;
  const char *file;
  const char *data;
  int line, flags;
  char buf[512];
  while ((l= ERR_get_error_line_data(&file,&line,&data,&flags)))
  {
    fprintf(stderr, "OpenSSL: %s:%s:%d:%s\n", ERR_error_string(l,buf),
			 file,line,(flags&ERR_TXT_STRING)?data:"");
  }

  if (ssl)
    fprintf(stderr, "error: %d/%s\n", SSL_get_error(ssl, l), 
            ERR_error_string(SSL_get_error(ssl, l), buf));

  fprintf(stderr, "socket_errno: %d\n", errno);
}

#endif /* HAVE_OPENSSL */

void vio_init(Vio* vio, enum enum_vio_type type,
                     int sd, uint flags)
{
  bzero((char*) vio, sizeof(*vio));
  vio->type	= type;
  vio->sd	= sd;
  vio->localhost= flags & VIO_LOCALHOST;
  if ((flags & VIO_BUFFERED_READ) &&
      !(vio->read_buffer= (char*)malloc(VIO_READ_BUFFER_SIZE)))
    flags&= ~VIO_BUFFERED_READ;
#ifdef HAVE_OPENSSL 
  if (type == VIO_TYPE_SSL)
  {
    vio->viodelete	=vio_ssl_delete;
    vio->vioerrno	=vio_errno;
    vio->read		=vio_ssl_read;
    vio->write		=vio_ssl_write;
    vio->vioclose	=vio_ssl_close;
    return;
  }
#endif /* HAVE_OPENSSL */
  vio->viodelete	=vio_delete;
  vio->vioerrno	=vio_errno;
  vio->read= (flags & VIO_BUFFERED_READ) ? vio_read_buff : vio_read;
  vio->write		=vio_write;
  vio->vioclose	=vio_close;
}

Vio *vio_new(int sd, enum enum_vio_type type, uint flags)
{
  Vio *vio;
  if ((vio = (Vio*)malloc(sizeof(*vio))))
  {
    vio_init(vio, type, sd, flags);
    sprintf(vio->desc,
	    (vio->type == VIO_TYPE_SOCKET ? "socket (%d)" : "TCP/IP (%d)"),
	    vio->sd);
    fcntl(sd, F_SETFL, 0);
    vio->fcntl_mode= fcntl(sd, F_GETFL);
  }
  return vio;
}

void vio_reset(Vio* vio, enum enum_vio_type type,
               int sd, uint flags)
{
  free(vio->read_buffer);
  vio_init(vio, type, sd, flags);
}

int vio_keepalive(Vio* vio, bool set_keep_alive)
{
  int r=0;
  uint opt = 0;
  if (vio->type != VIO_TYPE_NAMEDPIPE)
  {
    if (set_keep_alive)
      opt = 1;
    r = setsockopt(vio->sd, SOL_SOCKET, SO_KEEPALIVE, (char *) &opt,
		   sizeof(opt));
  }
  return r;
}

