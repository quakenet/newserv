#define USE_DBAPI_PGSQL
#define DBAPI2_ADAPTER_NAME "pqsql"
#define DBAPI2_CUSTOM_QUOTESTRING

#include "../dbapi2/dbapi2-adapter.inc"

MODULE_VERSION("");

void _init() {
  if(pqconnected())
    registeradapterprovider();
}

void _fini() {
  deregisteradapterprovider();
}

static int dbapi2_adapter_quotestring(const DBAPIConn *db, char *buf, size_t buflen, const char *data, size_t len) {
  StringBuf b;
  sbinit(&b, buf, buflen);
  char xbuf[len * 2 + 5];
  size_t esclen;

  esclen = dbescapestring(xbuf, (char *)data, len);
  sbaddchar(&b, '\'');
  sbaddstrlen(&b, xbuf, esclen);
  sbaddchar(&b, '\'');

  if(!sbterminate(&b))
    return 0;

  return 1;
}
