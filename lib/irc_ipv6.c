/* IPv6 helper functions */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "irc_ipv6.h"
#include <stdio.h>

#warning This source file is probably GPLed, it needs relicensing.

/** Convert an IP address to printable ASCII form.
 * This is generally deprecated in favor of ircd_ntoa_r().
 * @param[in] in Address to convert.
 * @return Pointer to a static buffer containing the readable form.
 */
const char* ircd_ntoa(const struct irc_in_addr* in)
{
  static char buf[SOCKIPLEN];
  return ircd_ntoa_r(buf, in);
}

/** Convert an IP address to printable ASCII form.
 * @param[out] buf Output buffer to write to.
 * @param[in] in Address to format.
 * @return Pointer to the output buffer \a buf.
 */
const char* ircd_ntoa_r(char* buf, const struct irc_in_addr* in)
{
    assert(buf != NULL);
    assert(in != NULL);

    if (irc_in_addr_is_ipv4(in)) {
      unsigned char *pch;

      pch = (unsigned char*)&in->in6_16[6];
      sprintf(buf,"%d.%d.%d.%d",pch[0],pch[1],pch[2],pch[3]);
      return buf;
    } else {
      unsigned int pos, part, max_start, max_zeros, curr_zeros, ii;

      /* Find longest run of zeros. */
      for (max_start = ii = 1, max_zeros = curr_zeros = 0; ii < 8; ++ii) {
        if (!in->in6_16[ii])
          curr_zeros++;
        else if (curr_zeros > max_zeros) {
          max_start = ii - curr_zeros;
          max_zeros = curr_zeros;
          curr_zeros = 0;
        }
      }
      if (curr_zeros > max_zeros) {
        max_start = ii - curr_zeros;
        max_zeros = curr_zeros;
      }

      /* Print out address. */
/** Append \a CH to the output buffer. */
#define APPEND(CH) do { buf[pos++] = (CH); } while (0)
      for (pos = ii = 0; (ii < 8); ++ii) {
        if ((max_zeros > 0) && (ii == max_start)) {
          APPEND(':');
          ii += max_zeros - 1;
          continue;
        }
        part = ntohs(in->in6_16[ii]);
        pos+=sprintf(buf+pos,"%x",part);
        if (ii < 7)
          APPEND(':');
      }
#undef APPEND

      /* Nul terminate and return number of characters used. */
      buf[pos++] = '\0';
      return buf;
    }
}

/** Attempt to parse an IPv4 address into a network-endian form.
 * @param[in] input Input string.
 * @param[out] output Network-endian representation of the address.
 * @param[out] pbits Number of bits found in pbits.
 * @return Number of characters used from \a input, or 0 if the parse failed.
 */
static unsigned int
ircd_aton_ip4(const char *input, unsigned int *output, unsigned char *pbits)
{
  unsigned int dots = 0, pos = 0, part = 0, ip = 0, bits;

  /* Intentionally no support for bizarre IPv4 formats (plain
   * integers, octal or hex components) -- only vanilla dotted
   * decimal quads.
   */
  if (input[0] == '.')
    return 0;
  bits = 32;
  while (1) switch (input[pos]) {
  case '\0':
    if (dots < 3)
      return 0;
  out:
    ip |= part << (24 - 8 * dots);
    *output = htonl(ip);
    if (pbits)
      *pbits = bits;
    return pos;
  case '.':
    if (++dots > 3)
      return 0;
    if (input[++pos] == '.')
      return 0;
    ip |= part << (32 - 8 * dots);
    part = 0;
    if (input[pos] == '*') {
      while (input[++pos] == '*' || input[pos] == '.') ;
      if (input[pos] != '\0')
        return 0;
      if (pbits)
        *pbits = dots * 8;
      *output = htonl(ip);
      return pos;
    }
    break;
  case '/':
    if (!pbits || !IsDigit(input[pos + 1]))
      return 0;
    for (bits = 0; IsDigit(input[++pos]); )
      bits = bits * 10 + input[pos] - '0';
    if (bits > 32)
      return 0;
    goto out;
  case '0': case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
    part = part * 10 + input[pos++] - '0';
    if (part > 255)
      return 0;
    break;
  default:
    return 0;
  }
}

/** Parse a numeric IPv4 or IPv6 address into an irc_in_addr.
 * @param[in] input Input buffer.
 * @param[out] ip Receives parsed IP address.
 * @param[out] pbits If non-NULL, receives number of bits specified in address mask.
 * @return Number of characters used from \a input, or 0 if the
 * address was unparseable or malformed.
 */
int
ipmask_parse(const char *input, struct irc_in_addr *ip, unsigned char *pbits)
{
  char *colon;
  char *dot;

  assert(ip);
  assert(input);
  memset(ip, 0, sizeof(*ip));
  colon = strchr(input, ':');
  dot = strchr(input, '.');

  if (colon && (!dot || (dot > colon))) {
    unsigned int part = 0, pos = 0, ii = 0, colon = 8;
    const char *part_start = NULL;

    /* Parse IPv6, possibly like ::127.0.0.1.
     * This is pretty straightforward; the only trick is borrowed
     * from Paul Vixie (BIND): when it sees a "::" continue as if
     * it were a single ":", but note where it happened, and fill
     * with zeros afterward.
     */
    if (input[pos] == ':') {
      if ((input[pos+1] != ':') || (input[pos+2] == ':'))
        return 0;
      colon = 0;
      pos += 2;
      part_start = input + pos;
    }
    while (ii < 8) switch (input[pos]) {
      unsigned char chval;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      chval = input[pos] - '0';
    use_chval:
      part = (part << 4) | chval;
      if (part > 0xffff)
        return 0;
      pos++;
      break;
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
      chval = input[pos] - 'A' + 10;
      goto use_chval;
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
      chval = input[pos] - 'a' + 10;
      goto use_chval;
    case ':':
      part_start = input + ++pos;
      if (input[pos] == '.')
        return 0;
      ip->in6_16[ii++] = htons(part);
      part = 0;
      if (input[pos] == ':') {
        if (colon < 8)
          return 0;
        colon = ii;
        pos++;
      }
      break;
    case '.': {
      uint32_t ip4;
      unsigned int len;
      len = ircd_aton_ip4(part_start, &ip4, pbits);
      if (!len || (ii > 6))
        return 0;
      ip->in6_16[ii++] = htons(ntohl(ip4) >> 16);
      ip->in6_16[ii++] = htons(ntohl(ip4) & 65535);
      if (pbits)
        *pbits += 96;
      pos = part_start + len - input;
      goto finish;
    }
    case '/':
      if (!pbits || !IsDigit(input[pos + 1]))
        return 0;
      ip->in6_16[ii++] = htons(part);
      for (part = 0; IsDigit(input[++pos]); )
        part = part * 10 + input[pos] - '0';
      if (part > 128)
        return 0;
      *pbits = part;
      goto finish;
    case '*':
      while (input[++pos] == '*' || input[pos] == ':') ;
      if (input[pos] != '\0' || colon < 8)
        return 0;
      if (pbits)
        *pbits = ii * 16;
      return pos;
    case '\0':
      ip->in6_16[ii++] = htons(part);
      if (colon == 8 && ii < 8)
        return 0;
      if (pbits)
        *pbits = 128;
      goto finish;
    default:
      return 0;
    }
    if (input[pos] != '\0')
      return 0;
  finish:
    if (colon < 8) {
      unsigned int jj;
      /* Shift stuff after "::" up and fill middle with zeros. */
      for (jj = 0; jj < ii - colon; jj++)
        ip->in6_16[7 - jj] = ip->in6_16[ii - jj - 1];
      for (jj = 0; jj < 8 - ii; jj++)
        ip->in6_16[colon + jj] = 0;
    }
    return pos;
  } else if (dot || strchr(input, '/')) {
    unsigned int addr;
    int len = ircd_aton_ip4(input, &addr, pbits);
    if (len) {
      ip->in6_16[5] = htons(65535);
      ip->in6_16[6] = htons(ntohl(addr) >> 16);
      ip->in6_16[7] = htons(ntohl(addr) & 65535);
      if (pbits)
        *pbits += 96;
    }
    return len;
  } else if (input[0] == '*') {
    unsigned int pos = 0;
    while (input[++pos] == '*') ;
    if (input[pos] != '\0')
      return 0;
    if (pbits)
      *pbits = 0;
    return pos;
  } else return 0; /* parse failed */
}

/* from numnicks.c */

/**
 * Converts a numeric to the corresponding character.
 * The following characters are currently known to be forbidden:
 *
 * '\\0' : Because we use '\\0' as end of line.
 *
 * ' '  : Because parse_*() uses this as parameter separator.
 *
 * ':'  : Because parse_server() uses this to detect if a prefix is a
 *        numeric or a name.
 *
 * '+'  : Because m_nick() uses this to determine if parv[6] is a
 *        umode or not.
 *
 * '&', '#', '$', '@' and '%' :
 *        Because m_message() matches these characters to detect special cases.
 */
static const char convert2y[] = {
  'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
  'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
  'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
  'w','x','y','z','0','1','2','3','4','5','6','7','8','9','[',']'
};

/** Converts a character to its (base64) numnick value. */
static const unsigned int convert2n[] = {
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  52,53,54,55,56,57,58,59,60,61, 0, 0, 0, 0, 0, 0,
   0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
  15,16,17,18,19,20,21,22,23,24,25,62, 0,63, 0, 0,
   0,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
  41,42,43,44,45,46,47,48,49,50,51, 0, 0, 0, 0, 0,

   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/** Number of bits encoded in one numnick character. */
#define NUMNICKLOG 6
/** Bitmask to select value of next numnick character. */
#define NUMNICKMASK 63          /* (NUMNICKBASE-1) */
/** Number of servers representable in a numnick. */

/* *INDENT-ON* */

/** Convert a string to its value as a numnick.
 * @param[in] s Numnick string to decode.
 * @return %Numeric nickname value.
 */
unsigned int base64toint(const char* s)
{
  unsigned int i = convert2n[(unsigned char) *s++];
  while (*s) {
    i <<= NUMNICKLOG;
    i += convert2n[(unsigned char) *s++];
  }
  return i;
}

/** Encode a number as a numnick.
 * @param[out] buf Output buffer.
 * @param[in] v Value to encode.
 * @param[in] count Number of numnick digits to write to \a buf.
 */
const char* inttobase64(char* buf, unsigned int v, unsigned int count)
{
  buf[count] = '\0';
  while (count > 0) {
    buf[--count] = convert2y[(v & NUMNICKMASK)];
    v >>= NUMNICKLOG;
  }
  return buf;
}

/** Number of bits encoded in one numnick character. */
#define NUMNICKLOG 6

/** Encode an IP address in the base64 used by numnicks.
 * For IPv4 addresses (including IPv4-mapped and IPv4-compatible IPv6
 * addresses), the 32-bit host address is encoded directly as six
 * characters.
 *
 * For IPv6 addresses, each 16-bit address segment is encoded as three
 * characters, but the longest run of zero segments is encoded using an
 * underscore.
 * @param[out] buf Output buffer to write to.
 * @param[in] addr IP address to encode.
 * @param[in] count Number of bytes writable to \a buf.
 * @param[in] v6_ok If non-zero, peer understands base-64 encoded IPv6 addresses.
 */
const char* iptobase64(char* buf, const struct irc_in_addr* addr, unsigned int count, int v6_ok)
{
  if (irc_in_addr_is_ipv4(addr)) {
    assert(count >= 6);
    inttobase64(buf, (ntohs(addr->in6_16[6]) << 16) | ntohs(addr->in6_16[7]), 6);
  } else if (!v6_ok) {
    assert(count >= 6);
    if (addr->in6_16[0] == htons(0x2002))
        inttobase64(buf, (ntohs(addr->in6_16[1]) << 16) | ntohs(addr->in6_16[2]), 6);
    else
        strcpy(buf, "AAAAAA");
  } else {
    unsigned int max_start, max_zeros, curr_zeros, zero, ii;
    char *output = buf;

    assert(count >= 25);
    /* Can start by printing out the leading non-zero parts. */
    for (ii = 0; (addr->in6_16[ii]) && (ii < 8); ++ii) {
      inttobase64(output, ntohs(addr->in6_16[ii]), 3);
      output += 3;
    }
    /* Find the longest run of zeros. */
    for (max_start = zero = ii, max_zeros = curr_zeros = 0; ii < 8; ++ii) {
      if (!addr->in6_16[ii])
        curr_zeros++;
      else if (curr_zeros > max_zeros) {
        max_start = ii - curr_zeros;
        max_zeros = curr_zeros;
        curr_zeros = 0;
      }
    }
    if (curr_zeros > max_zeros) {
      max_start = ii - curr_zeros;
      max_zeros = curr_zeros;
    }
    /* Print the rest of the address */
    for (ii = zero; ii < 8; ) {
      if ((ii == max_start) && max_zeros) {
        *output++ = '_';
        ii += max_zeros;
      } else {
        inttobase64(output, ntohs(addr->in6_16[ii]), 3);
        output += 3;
        ii++;
      }
    }
    *output = '\0';
  }
  return buf;
}

/** Decode an IP address from base64.
 * @param[in] input Input buffer to decode.
 * @param[out] addr IP address structure to populate.
 */
void base64toip(const char* input, struct irc_in_addr* addr)
{
  memset(addr, 0, sizeof(*addr));
  if (strlen(input) == 6) {
    unsigned int in = base64toint(input);
    /* An all-zero address should stay that way. */
    if (in) {
      addr->in6_16[5] = htons(65535);
      addr->in6_16[6] = htons(in >> 16);
      addr->in6_16[7] = htons(in & 65535);
    }
  } else {
    unsigned int pos = 0;
    do {
      if (*input == '_') {
        unsigned int left;
        for (left = (25 - strlen(input)) / 3 - pos; left; left--)
          addr->in6_16[pos++] = 0;
        input++;
      } else {
        unsigned short accum = convert2n[(unsigned char)*input++];
        accum = (accum << NUMNICKLOG) | convert2n[(unsigned char)*input++];
        accum = (accum << NUMNICKLOG) | convert2n[(unsigned char)*input++];
        addr->in6_16[pos++] = ntohs(accum);
      }
    } while (pos < 8);
  }
}

/** Test whether an address matches the most significant bits of a mask.
 * @param[in] addr Address to test.
 * @param[in] mask Address to test against.
 * @param[in] bits Number of bits to test.
 * @return 0 on mismatch, 1 if bits < 128 and all bits match; -1 if
 * bits == 128 and all bits match.
 */
int ipmask_check(const struct irc_in_addr *addr, const struct irc_in_addr *mask, unsigned char bits)
{
  int k;

  for (k = 0; k < 8; k++) {
    if (bits < 16)
      return !(htons(addr->in6_16[k] ^ mask->in6_16[k]) >> (16-bits));
    if (addr->in6_16[k] != mask->in6_16[k])
      return 0;
    if (!(bits -= 16))
      return 1;
  }
  return -1;
}
