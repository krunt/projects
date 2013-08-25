#ifndef MYISAM_READER_DEF
#define MYISAM_READER_DEF

void *myisam_get_iterator(const char *fname, void *record);

/* 1 - ok 
   0 - end-of-file
  -1 - error */
int myisam_next_iterator(void *it);

/* > 0 error */
int myisam_close_iterator(void *it);

#endif /* MYISAM_READER_DEF */
