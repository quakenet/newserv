#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "../lib/strlfunc.h"
#include "trusts.h"

int trusts_parsecidr(const char *host, uint32_t *ip, short *mask) {
  unsigned int octet1 = 0, octet2 = 0, octet3 = 0, octet4 = 0, umask = 32;

  if(sscanf(host, "%u.%u.%u.%u/%u", &octet1, &octet2, &octet3, &octet4, &umask) != 5)
    if(sscanf(host, "%u.%u.%u/%u", &octet1, &octet2, &octet3, &umask) != 4)
      if(sscanf(host, "%u.%u/%u", &octet1, &octet2, &umask) != 3)
        if(sscanf(host, "%u/%u", &octet1, &umask) != 2)
          if(sscanf(host, "%u.%u.%u.%u", &octet1, &octet2, &octet3, &octet4) != 4)
            return 0;

  if(octet1 > 255 || octet2 > 255 || octet3 > 255 || octet4 > 255 || umask > 32)
    return 0;

  *ip = (octet1 << 24) | (octet2 << 16) | (octet3 << 8) | octet4;
  *mask = umask;

  return 1;
}

/* returns mask pre-anded */
int trusts_str2cidr(const char *host, uint32_t *ip, uint32_t *mask) {
  uint32_t result;
  short smask;

  if(!trusts_parsecidr(host, &result, &smask))
    return 0;

  if(smask == 0) {
    *mask = 0;
  } else {
    *mask = 0xffffffff << (32 - smask);
  }
  *ip = result & *mask;

  return 1;
}

char *trusts_cidr2str(uint32_t ip, uint32_t mask) {
  static char buf[100];
  char maskbuf[10];

  if(mask != 0) {
    /* count number of trailing zeros */
    float f = (float)(mask & -mask);

    mask = 32 - ((*(unsigned int *)&f >> 23) - 0x7f);
  }

  if(mask < 32) {
    snprintf(maskbuf, sizeof(maskbuf), "/%u", mask);
  } else {
    maskbuf[0] = '\0';
  }

  snprintf(buf, sizeof(buf), "%u.%u.%u.%u%s", (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff, maskbuf);

  return buf;
}

char *trusts_timetostr(time_t t) {
  static char buf[100];

  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));

  return buf;
}

char *dumpth(trusthost *th, int oformat) {
  static char buf[512];

  if(oformat) {
    snprintf(buf, sizeof(buf), "#%u,%s,%u,%u,%jd", th->group->id, trusts_cidr2str(th->ip, th->mask), th->count, th->maxusage, (intmax_t)th->lastseen);
  } else {
    snprintf(buf, sizeof(buf), "%u,%s,%u,%u,%jd", th->group->id, trusts_cidr2str(th->ip, th->mask), th->id, th->maxusage, (intmax_t)th->lastseen);
  }

  return buf;
}

char *dumptg(trustgroup *tg, int oformat) {
  static char buf[512];

  if(oformat) {
    snprintf(buf, sizeof(buf), "#%u,%s,%u,%u,%d,%u,%u,%jd,%jd,%jd,%s,%s,%s", tg->id, tg->name->content, tg->count, tg->trustedfor, tg->mode, tg->maxperident, tg->maxusage, (intmax_t)tg->expires, (intmax_t)tg->lastseen, (intmax_t)tg->lastmaxuserreset, tg->createdby->content, tg->contact->content, tg->comment->content);
  } else {
    snprintf(buf, sizeof(buf), "%u,%s,%u,%d,%u,%u,%jd,%jd,%jd,%s,%s,%s", tg->id, tg->name->content, tg->trustedfor, tg->mode, tg->maxperident, tg->maxusage, (intmax_t)tg->expires, (intmax_t)tg->lastseen, (intmax_t)tg->lastmaxuserreset, tg->createdby->content, tg->contact->content, tg->comment->content);
  }

  return buf;
}

int parsetg(char *buf, trustgroup *tg, int oformat) {
  char *line, *createdby, *contact, *comment, *name, *id;
  unsigned long expires, lastseen, lastmaxusereset;
  char xbuf[1024];
  int pos;

/* #id,ticket35153,14,20,1,1,17,1879854575,1222639249,0,nterfacer,Qwhois&2120764,Non-Commercial Bouncer (Created by: doomie)
      ,name       ,current
                     ,trustedfor
                        ,mode
                          ,maxperident
                            ,maxusage
                               ,expires  ,lastseen   ,lastmaxusereset
                                                       ,createdby,contact       ,comment
*/
  int r;

  strlcpy(xbuf, buf, sizeof(xbuf));
  line = xbuf;

  if(!*line)
    return 0;

  if(oformat) {
    if(line[0] != '#')
      return 0;
    line++;
  }

  id = line;
  line = strchr(xbuf, ',');
  if(!line)
    return 0;
  *line++ = '\0';

  if(oformat && (id[0] == '#'))
    id++;

  tg->id = strtoul(id, NULL, 10);
  if(!tg->id)
    return 0;

  name = line;
  line = strchr(line, ',');
  if(!line)
    return 0;
  *line++ = '\0';

  if(oformat) {
    r = sscanf(line, "%*u,%u,%u,%u,%u,%lu,%lu,%lu,%n",
               /*current, */ &tg->trustedfor, &tg->mode, &tg->maxperident,
               &tg->maxusage, &expires, &lastseen, &lastmaxusereset, &pos);
  } else {
    r = sscanf(line, "%u,%u,%u,%u,%lu,%lu,%lu,%n",
               &tg->trustedfor, &tg->mode, &tg->maxperident,
               &tg->maxusage, &expires, &lastseen, &lastmaxusereset, &pos);
  }
  if(r != 7)
    return 0;

  tg->expires = (time_t)expires;
  tg->lastseen = (time_t)lastseen;
  tg->lastmaxuserreset = (time_t)lastmaxusereset;

  createdby = &line[pos];
  contact = strchr(createdby, ',');
  if(!contact)
    return 0;
  *contact++ = '\0';

  comment = strchr(contact, ',');
  if(!comment)
    return 0;
  *comment++ = '\0';

  tg->name = getsstring(name, TRUSTNAMELEN);
  tg->createdby = getsstring(createdby, NICKLEN);
  tg->comment = getsstring(comment, COMMENTLEN);
  tg->contact = getsstring(contact, CONTACTLEN);
  if(!tg->name || !tg->createdby || !tg->comment || !tg->contact) {
    freesstring(tg->name);
    freesstring(tg->createdby);
    freesstring(tg->comment);
    freesstring(tg->contact);
    return 0;
  }

  return 1;
}

int parseth(char *line, trusthost *th, unsigned int *tgid, int oformat) {
  unsigned long lastseen;
  char *ip, xbuf[1024], *id;

/* #id,213.230.192.128/26,20,23,1222732944
       ip                ,cur,max,lastseen */

  strlcpy(xbuf, line, sizeof(xbuf));
  id = line = xbuf;

  line = strchr(line, ',');
  if(!line)
    return 0;
  *line++ = '\0';

  if(oformat && (id[0] == '#'))
    id++;

  *tgid = strtoul(id, NULL, 10);
  if(!*tgid)
    return 0;

  ip = line;
  line = strchr(line, ',');
  if(!line)
    return 0;
  *line++ = '\0';

  if(!trusts_str2cidr(ip, &th->ip, &th->mask))
    return 0;

  if(oformat) {
    if(sscanf(line, "%*u,%u,%lu", /*current, */&th->maxusage, &lastseen) != 2)
      return 0;
  } else {
    if(sscanf(line, "%u,%u,%lu", &th->id, &th->maxusage, &lastseen) != 3)
      return 0;
  }

  th->lastseen = (time_t)lastseen;

  return 1;
}

char *rtrim(char *buf) {
  static char obuf[1024];
  size_t len = strlcpy(obuf, buf, sizeof(obuf));

  if((len < sizeof(obuf)) && (len > 0)) {
    size_t i;
    for(i=len-1;i>=0;i--) {
      if(obuf[i] != ' ')
        break;

      obuf[i] = '\0';
    }
  }

  return obuf;
}
