#include "mysqlvio.h"
#include <unistd.h>

void
fatal_error(const char*	r)
{
	perror(r);
	exit(0);
}

int
main(int	argc, char**	argv)
{
	char	client_key[] = "../certsdir/client-key.pem",
            client_cert[] = "../certsdir/client-cert.pem";
	char	ca_file[] = "../certsdir/ca-cert.pem";
    char *ca_path = NULL, *cipher=NULL;
	struct st_VioSSLFd* ssl_connector= NULL;
	struct sockaddr_in sa;
	Vio* client_vio=0;
	int err;
	char	xbuf[10000] = {0};
    memset(xbuf, 'C', sizeof(xbuf)-1);

	ssl_connector = new_VioSSLConnectorFd(client_key, 
            client_cert, NULL, ca_path, cipher);
	if(!ssl_connector) {
        fatal_error("client:new_VioSSLConnectorFd failed");
	}

	/* ----------------------------------------------- */
	/* Create a socket and connect to server using normal socket calls. */

	client_vio = vio_new(socket (AF_INET, SOCK_STREAM, 0), VIO_TYPE_TCPIP, 1);

	memset (&sa, '\0', sizeof(sa));
	sa.sin_family      = AF_INET;
	sa.sin_addr.s_addr = inet_addr ("127.0.0.1");   /* Server IP */
	sa.sin_port        = htons     (41111);          /* Server Port number */

	err = connect(client_vio->sd, (struct sockaddr*) &sa,
		sizeof(sa));

	/* ----------------------------------------------- */
	/* Now we have TCP conncetion. Start SSL negotiation. */
	//read(client_vio->sd,xbuf, sizeof(xbuf));
	printf("client:got %s\n", xbuf);
    sslconnect(ssl_connector,client_vio,60L);
    while (1) {
	    err = client_vio->read(client_vio,(uchar*)xbuf, sizeof(xbuf));
	    client_vio->write(client_vio,(const uchar*)xbuf, err);
    }
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
