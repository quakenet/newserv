/*
  nterface logging functions
  Copyright (C) 2003-2004 Chris Porter.
*/

#ifndef __nterface_logging_H
#define __nterface_logging_H

#define ROTATE_LOG_EVERY 86400
#define ROTATE_LOG_TIMES 14

#define NL_INFO     0x001
#define NL_DEBUG    0x002
#define NL_WARNING  0x004
#define NL_ERROR    0x008
#define NL_SYSTEM   0x010
#define NL_LOG_ONLY 0x100
#define NL_DONT_LOG 0x200

struct nterface_auto_log {
  FILE *log;
  sstring *filename;
  sstring *name;
  int debug;
  void *schedule;
};

void nterface_log(struct nterface_auto_log *ourlog, int loglevel, char *format, ...) __attribute__ ((format (printf, 3, 4)));
struct nterface_auto_log *nterface_open_log(char *name, char *filename, int debug);
struct nterface_auto_log *nterface_close_log(struct nterface_auto_log *ourlog);

#endif
