#include "mysqlvio.h"

#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>

typedef struct {
	int	sd;
	struct	st_VioSSLFd*	ssl_acceptor;
} TH_ARGS;

static void
do_ssl_stuff(	TH_ARGS*	args)
{
	const char*	s = "Huhuhuhuuu";
	Vio*		server_vio;
	int		err;

	server_vio = vio_new(args->sd, VIO_TYPE_TCPIP, 1);

	/* ----------------------------------------------- */
	/* TCP connection is ready. Do server side SSL. */

	err = write(server_vio->sd,(uchar*)s, strlen(s)+1);
	sslaccept(args->ssl_acceptor,server_vio,60L);
	err = server_vio->write(server_vio,(uchar*)s, strlen(s));
}

static void*
client_thread(	void*	arg)
{
  do_ssl_stuff((TH_ARGS*)arg);
  return 0;
}

int
main(int argc, char** argv)
{
	char	server_key[] = "../certsdir/server-key.pem",
		server_cert[] = "../certsdir/server-cert.pem";
	char	ca_file[] = "../certsdir/ca-cert.pem",
		*ca_path = NULL, *cipher = NULL;
	struct	st_VioSSLFd* ssl_acceptor;
	pthread_t	th;
	TH_ARGS		th_args;

	struct sockaddr_in sa_serv;
	struct sockaddr_in sa_cli;
	int listen_sd;
	int err;
    socklen_t client_len;
	int	reuseaddr = 1; /* better testing, uh? */

    th_args.ssl_acceptor = ssl_acceptor 
        = new_VioSSLAcceptorFd(server_key, server_cert, ca_file, ca_path,cipher);

	/* ----------------------------------------------- */
	/* Prepare TCP socket for receiving connections */
	listen_sd = socket (AF_INET, SOCK_STREAM, 0);
	setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, 
            &reuseaddr, sizeof(&reuseaddr));

	memset (&sa_serv, '\0', sizeof(sa_serv));
	sa_serv.sin_family      = AF_INET;
	sa_serv.sin_addr.s_addr = INADDR_ANY;
	sa_serv.sin_port        = htons (41111);          /* Server Port number */

	err = bind(listen_sd, (struct sockaddr*) &sa_serv,
	     sizeof (sa_serv));                  

	/* Receive a TCP connection. */

	err = listen (listen_sd, 5); 
	client_len = sizeof(sa_cli);
	th_args.sd = accept (listen_sd, (struct sockaddr*) &sa_cli, &client_len);
	close (listen_sd);

	printf ("Connection from %lx, port %x\n",
		  (long)sa_cli.sin_addr.s_addr, sa_cli.sin_port);

	/* ----------------------------------------------- */
	/* TCP connection is ready. Do server side SSL. */

    client_thread((void*)&th_args);

	free(ssl_acceptor);
	return 0;
}
