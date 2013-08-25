#ifndef USE_MY_FUNC
#define USE_MY_FUNC			/* We need at least my_malloc */
#endif

#include "myisamdef.h"
#include <queues.h>
#include <my_tree.h>
#include "mysys_err.h"
#ifdef MSDOS
#include <io.h>
#endif
#ifndef __GNU_LIBRARY__
#define __GNU_LIBRARY__			/* Skip warnings in getopt.h */
#endif
#include <my_getopt.h>
#include <assert.h>

#include "myisamreader.h"

typedef struct st_myisam_fdstate {
  MI_INFO *file, *current;
  uchar *record;
} MYISAM_FDSTATE;

static int mrg_rrnd(MYISAM_FDSTATE *info,uchar *buf) {
  int error;
  MI_INFO *isam_info;
  my_off_t filepos;

  if (!info->current) {
    isam_info= info->file;
    mi_reset(isam_info);
    mi_extra(isam_info, HA_EXTRA_CACHE, 0);
    filepos=isam_info->s->pack.header_length;
    info->current= isam_info;
  } else {
    isam_info= info->file;
    filepos= isam_info->nextpos;
  }

  isam_info->update&= HA_STATE_CHANGED;
  if (!(error=(*isam_info->s->read_rnd)(isam_info,(uchar*) buf, filepos, 1)) 
          || error != HA_ERR_END_OF_FILE)
    return (error);
  return HA_ERR_END_OF_FILE;
}

static int mrg_close(MYISAM_FDSTATE *mrg) {
  int error=0;
  error= mi_close(mrg->file);
  return error;
}

static MI_INFO *open_isam_file(const char *name,int mode) {
  MI_INFO *isam_file;

  if (!(isam_file=mi_open(name,mode, HA_OPEN_ABORT_IF_LOCKED)))
  {
    VOID(fprintf(stderr, "%s gave error %d on open\n", name, my_errno));
    return NULL;
  }
  return isam_file;
}

static const char *load_default_groups[]= { "myisampack",0 };

void * myisam_get_iterator(const char *fname, void *record) {
  int error, argc= 1;
  char *argv[2] = { (char*)"stub", 0 };
  MI_INFO *isam_file;
  MYISAM_FDSTATE *mrg;

  MY_INIT(argv[0]);
  load_defaults("my",load_default_groups,&argc,(char ***)&argv);

  error= 0;
  if (!(isam_file=open_isam_file(fname,O_RDONLY))) {
    error= 1;
  } else {
    mrg = (MYISAM_FDSTATE *)my_malloc(sizeof(MYISAM_FDSTATE), MYF(0));
    memset(mrg, 0, sizeof(mrg));
    mrg->file= isam_file;
    mrg->current= NULL;
    mrg->record=(uchar*)record;
  }
  return !error ? (void*)mrg : NULL;
}

/* 1 - ok 
   0 - end-of-file
  -1 - error */
int myisam_next_iterator(void *it) {
  int error;
  MYISAM_FDSTATE *mrg;
  mrg= (MYISAM_FDSTATE *)it;

  while ((error=mrg_rrnd(mrg,mrg->record)) != HA_ERR_END_OF_FILE) {
    if (!error) {
        break;
    } else if (error == HA_ERR_RECORD_DELETED) {
      continue;
    } else {
      VOID(fprintf(stderr, "Got error %d while reading rows\n", error));
    }
    break;
  }
  return error == HA_ERR_END_OF_FILE ? 0 : (error ? -1 : 1);
}

/* > 0 if failure */
int myisam_close_iterator(void *it) {
  int err;
  MYISAM_FDSTATE *mrg = (MYISAM_FDSTATE *)it;
  my_afree((uchar*) mrg->positions);
  my_afree((uchar*) mrg->record);
  err= mrg_close(mrg);
  my_afree((uchar*) mrg);
  return err;
}

