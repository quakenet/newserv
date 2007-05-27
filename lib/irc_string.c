/* irc_string.c */

#include "irc_string.h"
#include "chattr.tab.c"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

/*-
 * For some sections, the BSD license applies, specifically:
 * ircd_strcmp ircd_strncmp:
 *
 * Copyright (c) 1987
 *      The Regents of the University of California.  All rights reserved.
 *
 * match/collapse
 *
 * Copyright (c) 1998
 *      Andrea Cocito (Nemesi).
 * Copyright (c)
 *      Run (carlo@runaway.xs4all.nl) 1996
 *
 * mmatch/match/collapse
 *
 * Copyright (c)
 *      Run (carlo@runaway.xs4all.nl) 1996
 *
 * License follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University/owner? nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


static unsigned long crc32_tab[] = {
      0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
      0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
      0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
      0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
      0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
      0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
      0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
      0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
      0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
      0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
      0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
      0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
      0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
      0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
      0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
      0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
      0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
      0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
      0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
      0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
      0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
      0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
      0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
      0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
      0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
      0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
      0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
      0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
      0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
      0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
      0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
      0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
      0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
      0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
      0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
      0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
      0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
      0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
      0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
      0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
      0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
      0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
      0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
      0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
      0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
      0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
      0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
      0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
      0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
      0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
      0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
      0x2d02ef8dL
};

int match2strings(const char *patrn, const char *string) {
  return !match(patrn,string);
}

int match2patterns(const char *patrn, const char *string) {
  return !mmatch(patrn,string);
}

/* This computes a 32 bit CRC of the data in the buffer, and returns the */
/* CRC.  The polynomial used is 0xedb88320.                              */
/* =============================================================         */
/*  COPYRIGHT (C) 1986 Gary S. Brown.  You may use this program, or      */
/*  code or tables extracted from it, as desired without restriction.    */
/* Modified by Fox: no more length parameter, always does the whole string */
unsigned long crc32(const char *s) {
  const char *cp;
  unsigned long crc32val;
  
  crc32val = 0;
  for (cp=s;*cp;cp++) {
    crc32val=crc32_tab[(crc32val^(*cp)) & 0xff] ^ (crc32val >> 8);
  }
  return crc32val;
}

/* Case insensitive version of the above */
unsigned long crc32i(const char *s) {
  const char *cp;
  unsigned long crc32val;
  
  crc32val = 0;
  for (cp=s;*cp;cp++) {
    crc32val=crc32_tab[(crc32val^(ToLower(*cp))) & 0xff] ^ (crc32val >> 8);
  }
  return crc32val;
}

/* ircd_strcmp/ircd_strncmp
 *
 * Copyright (c) 1987
 *      The Regents of the University of California.  All rights reserved.
 *
 * BSD license.
 *
 * Modified from this version for ircd usage.
 */

int ircd_strcmp(const char *s1, const char *s2) {
  register const char* u1 = s1;
  register const char* u2 = s2;

  while(ToLower(*u1) == ToLower(*u2)) {
    if(!*u1++)
      return 0;

    u2++;
  }
  return ToLower(*u1) - ToLower(*u2);
}


int ircd_strncmp(const char *s1, const char *s2, size_t len) {
  register const char* u1 = s1;
  register const char* u2 = s2;
  size_t remaining = len - 1;

  if(!remaining)
    return 0;

  while(ToLower(*u1) == ToLower(*u2)) {
    if(!*u1++ || !remaining--)
      return 0;

    u2++;
  }

  return ToLower(*u1) - ToLower(*u2);
}

/* 
 * delchars: deletes characters occuring in badchars from string 
 */
 
char *delchars(char *string, const char *badchars) {
  char *s,*d;
  const char *b;
  int isbad;
  
  s=d=string;
  while (*s) {
    isbad=0;
    for(b=badchars;*b;b++) {
      if (*b==*s) {
        isbad=1;
        break;
      }
    }
    if (isbad) {
      s++;
    } else {
      *d++=*s++;
    }
  }
  *d='\0';
  
  return string;
}

/*  
 * IPtostr:
 *  Converts a long into a "p.q.r.s" IP address
 */

const char *IPlongtostr(unsigned long IP) {
  static char buf[16];

  sprintf(buf,"%lu.%lu.%lu.%lu",(IP>>24),(IP>>16)&255,(IP>>8)&255,IP&255);
 
  return buf;
}

/*
 * longtoduration: 
 *  Converts a specified number of seconds into a duration string.  
 *  format: 0 for the "/stats u" compatible output, 1 for more human-friendly output.
 */

const char *longtoduration(unsigned long interval, int format) {
  int days,hours,minutes,seconds;
  static char outstring[50];
  int pos=0;

  seconds=interval%60;
  minutes=(interval%3600)/60;
  hours=(interval%(3600*24))/3600;
  days=interval/(3600*24);

  if (days>0 || format==0) {
    sprintf(outstring,"%d day%s, %02d:%02d:%02d",
            days,(days==1)?"":"s",hours,minutes,seconds);
  } else {
    if (hours>0) {
      pos += sprintf(outstring+pos,"%d hour%s ",hours,hours==1?"":"s");
    }
    if (minutes>0 || (hours>0 && seconds>0)) {
      pos += sprintf(outstring+pos,"%d minute%s ",minutes,minutes==1?"":"s");
    }
    if (seconds>0) {
      pos += sprintf(outstring+pos,"%d second%s ",seconds,seconds==1?"":"s");
    }
  }

  return outstring;
}

/*
 * durationtolong:
 *  Converts the specified string into a number of seconds, as per O
 */

int durationtolong(const char *string) {
  int total=0;
  int current=0;
  char *ch=(char *)string,*och;

  while (*ch) {
    och=ch;
    current=strtol(ch,&ch,10);
    if (och==ch) /* No numbers found */
      break;
    if (current) {
      switch (*ch++) {
      case '\0':
	ch--; 
	/* Drop through */
      case 's':  
	total+=current;
	break;

      case 'm': 
	total+=(current * 60);
	break;

      case 'h':
	total+=(current * 3600);
	break;
	
      case 'd':
	total+=(current * 86400);
	break;
	
      case 'w':
        total+=(current * 604800);
        break;

      case 'M':
	total+=(current * 2592000);
	break;

      case 'y':
	total+=(current * 31557600);
      }
    }
  }
  return total;
}

/*
 * mmatch()
 *
 * Written by Run (carlo@runaway.xs4all.nl), 25-10-96
 *
 *
 * From: Carlo Wood <carlo@runaway.xs4all.nl>
 * Message-Id: <199609021026.MAA02393@runaway.xs4all.nl>
 * Subject: [C-Com] Analysis for `mmatch' (was: gline4 problem)
 * To: coder-com@mail.undernet.org (coder committee)
 * Date: Mon, 2 Sep 1996 12:26:01 +0200 (MET DST)
 *
 * We need a new function `mmatch(const char *old_mask, const char *new_mask)'
 * which returns `true' likewise the current `match' (start with copying it),
 * but which treats '*' and '?' in `new_mask' differently (not "\*" and "\?" !)
 * as follows:  a '*' in `new_mask' does not match a '?' in `old_mask' and
 * a '?' in `new_mask' does not match a '\?' in `old_mask'.
 * And ofcourse... a '*' in `new_mask' does not match a '\*' in `old_mask'...
 * And last but not least, '\?' and '\*' in `new_mask' now become one character.
 *
 * Kindly BSD licensed by Run for Newserv.
 */

int mmatch(const char *old_mask, const char *new_mask)
{
  const char *m = old_mask;
  const char *n = new_mask;
  const char *ma = m;
  const char *na = n;
  int wild = 0;
  int mq = 0, nq = 0;

  while (1)
  {
    if (*m == '*')
    {
      while (*m == '*')
        m++;
      wild = 1;
      ma = m;
      na = n;
    }

    if (!*m)
    {
      if (!*n)
        return 0;
      for (m--; (m > old_mask) && (*m == '?'); m--)
        ;
      if ((*m == '*') && (m > old_mask) && (m[-1] != '\\'))
        return 0;
      if (!wild)
        return 1;
      m = ma;

      /* Added to `mmatch' : Because '\?' and '\*' now is one character: */
      if ((*na == '\\') && ((na[1] == '*') || (na[1] == '?')))
        ++na;

      n = ++na;
    }
    else if (!*n)
    {
      while (*m == '*')
        m++;
      return (*m != 0);
    }
    if ((*m == '\\') && ((m[1] == '*') || (m[1] == '?')))
    {
      m++;
      mq = 1;
    }
    else
      mq = 0;

    /* Added to `mmatch' : Because '\?' and '\*' now is one character: */
    if ((*n == '\\') && ((n[1] == '*') || (n[1] == '?')))
    {
      n++;
      nq = 1;
    }
    else
      nq = 0;

/*
 * This `if' has been changed compared to match() to do the following:
 * Match when:
 *   old (m)         new (n)         boolean expression
 *    *               any             (*m == '*' && !mq) ||
 *    ?               any except '*'  (*m == '?' && !mq && (*n != '*' || nq)) ||
 * any except * or ?  same as m       (!((*m == '*' || *m == '?') && !mq) &&
 *                                      ToLower(*m) == ToLower(*n) &&
 *                                        !((mq && !nq) || (!mq && nq)))
 *
 * Here `any' also includes \* and \? !
 *
 * After reworking the boolean expressions, we get:
 * (Optimized to use boolean shortcircuits, with most frequently occuring
 *  cases upfront (which took 2 hours!)).
 */
    if ((*m == '*' && !mq) ||
        ((!mq || nq) && ToLower(*m) == ToLower(*n)) ||
        (*m == '?' && !mq && (*n != '*' || nq)))
    {
      if (*m)
        m++;
      if (*n)
        n++;
    }
    else
    {
      if (!wild)
        return 1;
      m = ma;

      /* Added to `mmatch' : Because '\?' and '\*' now is one character: */
      if ((*na == '\\') && ((na[1] == '*') || (na[1] == '?')))
        ++na;

      n = ++na;
    }
  }
}

/*
 * Compare if a given string (name) matches the given
 * mask (which can contain wild cards: '*' - match any
 * number of chars, '?' - match any single character.
 *
 * return  0, if match
 *         1, if no match
 */

/*
 * match
 *
 * Rewritten by Andrea Cocito (Nemesi), November 1998.
 *
 * Permission kindly granted by Andrea to be used under the BSD license.
 *
 */

/****************** Nemesi's match() ***************/

int match(const char *mask, const char *string)
{
  const char *m = mask, *s = string;
  char ch;
  const char *bm, *bs;          /* Will be reg anyway on a decent CPU/compiler */

  /* Process the "head" of the mask, if any */
  while ((ch = *m++) && (ch != '*'))
    switch (ch)
    {
      case '\\':
        if (*m == '?' || *m == '*')
          ch = *m++;
      default:
        if (ToLower(*s) != ToLower(ch))
          return 1;
      case '?':
        if (!*s++)
          return 1;
    };
  if (!ch)
    return *s;

  /* We got a star: quickly find if/where we match the next char */
got_star:
  bm = m;                       /* Next try rollback here */
  while ((ch = *m++))
    switch (ch)
    {
      case '?':
        if (!*s++)
          return 1;
      case '*':
        bm = m;
        continue;               /* while */
      case '\\':
        if (*m == '?' || *m == '*')
          ch = *m++;
      default:
        goto break_while;       /* C is structured ? */
    };
break_while:
  if (!ch)
    return 0;                   /* mask ends with '*', we got it */
  ch = ToLower(ch);
  while (ToLower(*s++) != ch)
    if (!*s)
      return 1;
  bs = s;                       /* Next try start from here */

  /* Check the rest of the "chunk" */
  while ((ch = *m++))
  {
    switch (ch)
    {
      case '*':
        goto got_star;
      case '\\':
        if (*m == '?' || *m == '*')
          ch = *m++;
      default:
        if (ToLower(*s) != ToLower(ch))
        {
          /* If we've run out of string, give up */
          if (!*bs)
            return 1;
          m = bm;
          s = bs;
          goto got_star;
        };
      case '?':
        if (!*s++)
          return 1;
    };
  };
  if (*s)
  {
    m = bm;
    s = bs;
    goto got_star;
  };
  return 0;
}

/*
 * collapse()
 * Collapse a pattern string into minimal components.
 * This particular version is "in place", so that it changes the pattern
 * which is to be reduced to a "minimal" size.
 *
 * (C) Carlo Wood - 6 Oct 1998
 * Speedup rewrite by Andrea Cocito, December 1998.
 * Note that this new optimized alghoritm can *only* work in place.
 *
 * Permission kindly granted by Andrea to be used under the BSD license.
 *
 */

char *collapse(char *mask)
{
  int star = 0;
  char *m = mask;
  char *b;

  if (m)
  {
    do
    {
      if ((*m == '*') && ((m[1] == '*') || (m[1] == '?')))
      {
        b = m;
        do
        {
          if (*m == '*')
            star = 1;
          else
          {
            if (star && (*m != '?'))
            {
              *b++ = '*';
              star = 0;
            };
            *b++ = *m;
            if ((*m == '\\') && ((m[1] == '*') || (m[1] == '?')))
              *b++ = *++m;
          };
        }
        while (*m++);
        break;
      }
      else
      {
        if ((*m == '\\') && ((m[1] == '*') || (m[1] == '?')))
          m++;
      };
    }
    while (*m++);
  };
  return mask;
}

/* a protected version of atoi that will return an error if broke/etc */
int protectedatoi(char *buf, int *value) {
  char *ep;
  long lval;

  errno = 0;
  lval = strtol(buf, &ep, 10);
  if (buf[0] == '\0' || *ep != '\0')
    return 0;
  
  if ((errno == ERANGE && (lval == LONG_MAX || lval == LONG_MIN)) || (lval > INT_MAX || lval < INT_MIN))
    return 0;

  *value = lval;
  return 1;
}
