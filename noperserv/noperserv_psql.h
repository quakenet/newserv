#ifndef __NOPERSERV_DB_H
#define __NOPERSERV_DB_H

#include <libpq-fe.h>

typedef void (*NoQueryHandler)(PGconn *, void *);
typedef void (*NoCreateHandler)(void);

void noperserv_async_query(NoQueryHandler handler, void *tag, char *format, ...);
#define noperserv_query(format, ...) noperserv_async_query(NULL, NULL, format, __VA_ARGS__)
void noperserv_sync_query(char *format, ...);
void noperserv_disconnect_db(void);
int noperserv_connect_db(NoCreateHandler createtables);

#endif
