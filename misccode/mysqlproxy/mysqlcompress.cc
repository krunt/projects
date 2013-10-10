
#include "mysqlcommon.h"

#include <unistd.h>
#include <string.h>
#include <zlib.h>

#define swap_variables(t, a, b) { t dummy; dummy= a; a= b; b= dummy; }

static uchar *my_compress_alloc(const uchar *packet, size_t *len, size_t *complen)
{
  uchar *compbuf;
  uLongf tmp_complen;
  int res;
  *complen=  *len * 120 / 100 + 12;

  if (!(compbuf= (uchar *) malloc(*complen)))
    return 0;					/* Not enough memory */

  tmp_complen= (uint) *complen;
  res= compress((uchar *) compbuf, &tmp_complen, (uchar*) packet, (uint) *len);
  *complen=    tmp_complen;

  if (res != Z_OK)
  {
    free(compbuf);
    return 0;
  }

  if (*complen >= *len)
  {
    *complen= 0;
    free(compbuf);
    return 0;
  }
  /* Store length of compressed packet in *len */
  swap_variables(size_t, *len, *complen);
  return compbuf;
}

#define MIN_COMPRESS_LENGTH		50	/* Don't compress small bl. */
bool my_compress(uchar *packet, size_t *len, size_t *complen)
{
  if (*len < MIN_COMPRESS_LENGTH)
  {
    *complen=0;
  }
  else
  {
    uchar *compbuf=my_compress_alloc(packet,len,complen);
    if (!compbuf)
      return *complen ? false : true;
    memcpy(packet,compbuf,*len);
    free(compbuf);
  }
  return false;
}

bool my_uncompress(uchar *packet, size_t len, size_t *complen)
{
  uLongf tmp_complen;

  if (*complen)					/* If compressed */
  {
    uchar *compbuf= (uchar *) malloc(*complen);
    int error;
    if (!compbuf)
        return true;

    tmp_complen= (uint) *complen;
    error= uncompress((uchar*) compbuf, &tmp_complen, (uchar*) packet,
                      (uLongf) len);
    *complen= tmp_complen;
    if (error != Z_OK)
    {						/* Probably wrong packet */
      free(compbuf);
      return true;
    }
    memcpy(packet, compbuf, *complen);
    free(compbuf);
  }
  else
    *complen= len;
  return false;
}
