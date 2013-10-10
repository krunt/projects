#ifndef MYSQLCOMMON_H_
#define MYSQLCOMMON_H_

#include <stdlib.h>
#include <stdint.h>

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long long int ulonglong;

typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

bool my_compress(uchar *packet, size_t *len, size_t *complen);
bool my_uncompress(uchar *packet, size_t len, size_t *complen);

#define uint2korr(A)	(*((uint16 *) (A)))
#define uint3korr(A)	(uint32) (((uint32) ((uchar) (A)[0])) +\
				  (((uint32) ((uchar) (A)[1])) << 8) +\
				  (((uint32) ((uchar) (A)[2])) << 16))
#define uint4korr(A)	(*((uint32 *) (A)))
#define uint6korr(A)	((ulonglong)(((uint32)    ((uchar) (A)[0]))          + \
                                     (((uint32)    ((uchar) (A)[1])) << 8)   + \
                                     (((uint32)    ((uchar) (A)[2])) << 16)  + \
                                     (((uint32)    ((uchar) (A)[3])) << 24)) + \
                         (((ulonglong) ((uchar) (A)[4])) << 32) +       \
                         (((ulonglong) ((uchar) (A)[5])) << 40))
#define uint8korr(A)	(*((ulonglong *) (A)))

#define int2store(T,A)	*((uint16*) (T))= (uint16) (A)
#define int3store(T,A)  do { *(T)=  (uchar) ((A));\
                            *(T+1)=(uchar) (((uint) (A) >> 8));\
                            *(T+2)=(uchar) (((A) >> 16)); } while (0)
#define int4store(T,A)	*((long *) (T))= (long) (A)
#define int6store(T,A)  do { *(T)=    (uchar)((A));          \
                             *((T)+1)=(uchar) (((A) >> 8));  \
                             *((T)+2)=(uchar) (((A) >> 16)); \
                             *((T)+3)=(uchar) (((A) >> 24)); \
                             *((T)+4)=(uchar) (((A) >> 32)); \
                             *((T)+5)=(uchar) (((A) >> 40)); } while(0)
#define int8store(T,A)	*((ulonglong *) (T))= (ulonglong) (A)


/* capability flags */
#define CLIENT_COMPRESS		32	/* Can use compression protocol */
#define CLIENT_PROTOCOL_41	512	/* New 4.1 protocol */
#define CLIENT_SSL              2048	/* Switch to SSL after handshake */

//#define DEBUG
//#define VERBOSE
#define HAVE_OPENSSL 

#endif /* MYSQLCOMMON_H_ */
