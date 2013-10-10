#ifndef MYSQLVIO_H_
#define MYSQLVIO_H_

#include "mysqlcommon.h"

#include <sys/socket.h>
#include <arpa/inet.h>

#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#define VIO_LOCALHOST 1                         /* a localhost connection */
#define VIO_BUFFERED_READ 2                     /* use buffered read */
#define VIO_READ_BUFFER_SIZE 16384              /* size of read buffer */

enum enum_vio_type
{
  VIO_CLOSED, VIO_TYPE_TCPIP, VIO_TYPE_SOCKET, VIO_TYPE_NAMEDPIPE,
  VIO_TYPE_SSL, VIO_TYPE_SHARED_MEMORY
};

struct st_vio;
typedef struct st_vio Vio;
struct st_vio
{
  int		sd;		/* my_socket - real or imaginary */
  bool		localhost;	/* Are we from localhost? */
  int			fcntl_mode;	/* Buffered fcntl(sd,F_GETFL) */
  struct sockaddr_in	local;		/* Local internet address */
  struct sockaddr_in	remote;		/* Remote internet address */
  enum enum_vio_type	type;		/* Type of connection */
  char			desc[30];	/* String description */
  char                  *read_buffer;   /* buffer for vio_read_buff */
  char                  *read_pos;      /* start of unfetched data in the
                                           read buffer */
  char                  *read_end;      /* end of unfetched data */
  /* function pointers. They are similar for socket/SSL/whatever */
  void    (*viodelete)(Vio*);
  int     (*vioerrno)(Vio*);
  size_t  (*read)(Vio*, uchar *, size_t);
  size_t  (*write)(Vio*, const uchar *, size_t);
  int     (*vioblocking)(Vio*, bool, bool *);
  bool (*is_blocking)(Vio*);
  int     (*viokeepalive)(Vio*, bool);
  int     (*fastsend)(Vio*);
  bool (*peer_addr)(Vio*, char *, uint16*);
  void    (*in_addr)(Vio*, struct in_addr*);
  bool (*should_retry)(Vio*);
  bool (*was_interrupted)(Vio*);
  int     (*vioclose)(Vio*);
  void	  (*timeout)(Vio*, unsigned int which, unsigned int timeout);
#ifdef HAVE_OPENSSL
  void	  *ssl_arg;
#endif
};

Vio* vio_new(int sd, enum enum_vio_type type, uint flags);
void vio_reset(Vio* vio, enum enum_vio_type type, int sd, uint flags);
void vio_delete(Vio *vio);
size_t	vio_read(Vio *vio, uchar *	buf, size_t size);
size_t  vio_read_buff(Vio *vio, uchar * buf, size_t size);
size_t	vio_write(Vio *vio, const uchar * buf, size_t size);
int	vio_close(Vio* vio);
bool vio_is_blocking(Vio *vio);
int	vio_blocking(Vio *vio, bool onoff);
int vio_keepalive(Vio* vio, bool set_keep_alive);

const char* vio_description(Vio *vio);
enum enum_vio_type vio_type(Vio* vio);
int	vio_errno(Vio*vio);

#ifdef HAVE_OPENSSL 

struct st_VioSSLFd {
    SSL_CTX *ssl_context;
    bool is_client;
};

struct st_VioSSLFd
*new_VioSSLConnectorFd(const char *key_file, const char *cert_file,
		       const char *ca_file,  const char *ca_path,
		       const char *cipher);
struct st_VioSSLFd
*new_VioSSLAcceptorFd(const char *key_file, const char *cert_file,
		      const char *ca_file,const char *ca_path,
		      const char *cipher);

int sslaccept(struct st_VioSSLFd*, Vio *, long timeout);
int sslconnect(struct st_VioSSLFd*, Vio *, long timeout);

void free_vio_ssl_acceptor_fd(struct st_VioSSLFd *fd);

#endif /* HAVE_OPENSSL */

#endif /* MYSQLVIO_H_ */
