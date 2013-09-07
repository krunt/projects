#ifndef UTILS_DEF_
#define UTILS_DEF_

#define MIN(a,b) ((a) < (b) ? (a) : (b))

/* url-data will be modified */
int http_parse_url(char *url, char **scheme, char **hostport, char **path);

/* hostport-data will be modified */
int http_parse_hostport(char *hostport, char **host, int *port);

#endif /* UTILS_DEF_ */
