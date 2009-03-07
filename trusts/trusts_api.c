#include <../nick/nick.h>
#include "trusts.h"

int istrusted(nick *np) {
  return gettrusthost(np) != NULL;
}
