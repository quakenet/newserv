#include <stdio.h>
#include <stdint.h>
#include <time.h>

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

