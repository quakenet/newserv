/* IPv6 helper prototypes */

#ifndef __irc_ipv6_H
#define __irc_ipv6_H

#include <limits.h>

/* from res.h */

/** Structure to store an IP address. */
struct irc_in_addr
{
  unsigned short in6_16[8]; /**< IPv6 encoded parts, little-endian. */
};

/** Structure to store an IP address and port number. */
struct irc_sockaddr
{
  struct irc_in_addr addr; /**< IP address. */
  unsigned short port;     /**< Port number, host-endian. */
};

#define irc_bitlen(ADDR, BITS) (irc_in_addr_is_ipv4(ADDR) ? (BITS) - 96 : (BITS))

/** Evaluate to non-zero if \a ADDR is a valid address (not all 0s and not all 1s). */
#define irc_in_addr_valid(ADDR) (((ADDR)->in6_16[0] && ~(ADDR)->in6_16[0]) \
                                 || (ADDR)->in6_16[1] != (ADDR)->in6_16[0] \
                                 || (ADDR)->in6_16[2] != (ADDR)->in6_16[0] \
                                 || (ADDR)->in6_16[3] != (ADDR)->in6_16[0] \
                                 || (ADDR)->in6_16[4] != (ADDR)->in6_16[0] \
                                 || (ADDR)->in6_16[5] != (ADDR)->in6_16[0] \
                                 || (ADDR)->in6_16[6] != (ADDR)->in6_16[0] \
                                 || (ADDR)->in6_16[7] != (ADDR)->in6_16[0])
/** Evaluate to non-zero if \a ADDR (of type struct irc_in_addr) is an IPv4 address. */
#define irc_in_addr_is_ipv4(ADDR) (!(ADDR)->in6_16[0] && !(ADDR)->in6_16[1] && !(ADDR)->in6_16[2] \
                                   && !(ADDR)->in6_16[3] && !(ADDR)->in6_16[4] && ((!(ADDR)->in6_16[5] \
                                   && (ADDR)->in6_16[6]) || (ADDR)->in6_16[5] == 65535))
/** Evaluate to non-zero if \a A is a different IP than \a B. */
#define irc_in_addr_cmp(A,B) (irc_in_addr_is_ipv4(A) ? ((A)->in6_16[6] != (B)->in6_16[6] \
                                  || (A)->in6_16[7] != (B)->in6_16[7] || !irc_in_addr_is_ipv4(B)) \
                              : memcmp((A), (B), sizeof(struct irc_in_addr)))
/** Evaluate to non-zero if \a ADDR is a loopback address. */
#define irc_in_addr_is_loopback(ADDR) (!(ADDR)->in6_16[0] && !(ADDR)->in6_16[1] && !(ADDR)->in6_16[2] \
                                       && !(ADDR)->in6_16[3] && !(ADDR)->in6_16[4] \
                                       && ((!(ADDR)->in6_16[5] \
                                            && ((!(ADDR)->in6_16[6] && (ADDR)->in6_16[7] == htons(1)) \
                                                || (ntohs((ADDR)->in6_16[6]) & 0xff00) == 0x7f00)) \
                                           || (((ADDR)->in6_16[5] == 65535) \
                                               && (ntohs((ADDR)->in6_16[6]) & 0xff00) == 0x7f00)))


/* from ircd_defs.h */

/** Maximum length of a numeric IP (v4 or v6) address.
 * "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"
 */
#define SOCKIPLEN 45

const char* irc_cidr_to_str(const struct irc_in_addr* in, int bitlen );

/* from ircd_string.h */

extern const char* ircd_ntoa(const struct irc_in_addr* addr);
extern const char* ircd_ntoa_r(char* buf, const struct irc_in_addr* addr);
#define ircd_aton(ADDR, STR) ipmask_parse((STR), (ADDR), NULL)
extern int ipmask_parse(const char *in, struct irc_in_addr *mask, unsigned char *bits_ptr);
extern int ipmask_check(const struct irc_in_addr *, const struct irc_in_addr *, unsigned char);

#define IPtostr(ipaddr) ircd_ntoa(&(ipaddr))
#define irc_in_addr_v4_to_int(ADDR) ((ntohs((ADDR)->in6_16[6]) << 16) | ntohs((ADDR)->in6_16[7]))

/* from numnicks.h */

extern const char* iptobase64(char* buf, const struct irc_in_addr* addr, unsigned int count, int v6_ok);
extern void base64toip(const char* s, struct irc_in_addr* addr);

/* from ircd_chattr.h */

/** Array mapping characters to attribute bitmasks. */
extern const unsigned int  IRCD_CharAttrTab[];

#define NTL_DIGIT   0x0008  /**< 0123456789                          */

/** Test whether a character is a digit. */
#define IsDigit(c)         (IRCD_CharAttrTab[(c) - CHAR_MIN] & NTL_DIGIT)

#endif /* __irc_ipv6_H */
