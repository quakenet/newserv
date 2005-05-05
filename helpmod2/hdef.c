#include "hdef.h"

const char *hlevel_name(hlevel lvl)
{
    switch (lvl)
    {
    case H_LAMER:
        return "waste of space";
    case H_PEON:
        return "normal user";
    case H_TRIAL:
        return "trial staff member";
    case H_STAFF:
        return "staff member";
    case H_OPER:
        return "operator";
    case H_ADMIN:
        return "administrator";
    case H_SERVICE:
        return "network service";
    default:
        return "error, please contact strutsi";
    }
}
