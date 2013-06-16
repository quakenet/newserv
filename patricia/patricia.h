/*
 * $Id: patricia.h,v 1.6 2005/12/07 20:53:01 dplonka Exp $
 * Dave Plonka <plonka@doit.wisc.edu>
 *
 * This product includes software developed by the University of Michigan,
 * Merit Network, Inc., and their contributors. 
 *
 * This file had been called "radix.h" in the MRT sources.
 *
 * I renamed it to "patricia.h" since it's not an implementation of a general
 * radix trie.  Also, pulled in various requirements from "mrt.h" and added
 * some other things it could be used as a standalone API.
 */

#ifndef _PATRICIA_H
#define _PATRICIA_H

#include "../lib/irc_ipv6.h"

typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned char u_char;

/* typedef unsigned int u_int; */
typedef void (*void_fn_t)();
/* { from defs.h */
#define prefix_touchar(prefix) ((u_char *)&(prefix)->sin)
#define MAXLINE 1024
/* } */

#include <sys/types.h> /* for u_* definitions (on FreeBSD 5) */

#include <errno.h> /* for EAFNOSUPPORT */
#ifndef EAFNOSUPPORT
#  defined EAFNOSUPPORT WSAEAFNOSUPPORT
#  include <winsock.h>
#else
#  include <netinet/in.h> /* for struct in_addr */
#endif

#include <sys/socket.h> /* for AF_INET */

/* { from mrt.h */

#define PATRICIA_MAXSLOTS 7 


typedef struct _prefix_t {
    u_short bitlen;		/* same as mask? */
    int ref_count;		/* reference count */
    struct irc_in_addr sin;
} prefix_t;

union prefixes {
  struct _prefix_t prefix;
  union prefixes *next;
};

/* } */

typedef struct _patricia_node_t {
   unsigned char bit;		/* flag if this node used */
   int usercount;               /* number of users on a given node */
   prefix_t *prefix;		/* who we are in patricia tree */
   struct _patricia_node_t *l, *r;	/* left and right children */
   struct _patricia_node_t *parent;/* may be used */
   void *exts[PATRICIA_MAXSLOTS]; 
} patricia_node_t;

typedef struct _patricia_tree_t {
   patricia_node_t 	*head;
   u_int		maxbits;	/* for IPv6, 128 bit addresses */
   int num_active_node;		/* for debug purpose */
} patricia_tree_t;

extern patricia_tree_t *iptree;

prefix_t *patricia_new_prefix (struct irc_in_addr *dest, int bitlen);
prefix_t * patricia_ref_prefix (prefix_t * prefix);
void patricia_deref_prefix (prefix_t * prefix);

patricia_node_t *patricia_search_exact (patricia_tree_t *patricia, struct irc_in_addr *sin, unsigned char bitlen);
patricia_node_t *patricia_search_best (patricia_tree_t *patricia, struct irc_in_addr *sin, unsigned char bitlen);
patricia_node_t * patricia_search_best2 (patricia_tree_t *patricia, struct irc_in_addr *sin, unsigned char bitlen, 
				   int inclusive);
patricia_node_t *patricia_lookup (patricia_tree_t *patricia, prefix_t *prefix);
void patricia_remove (patricia_tree_t *patricia, patricia_node_t *node);
patricia_tree_t *patricia_new_tree (int maxbits);
void patricia_clear_tree (patricia_tree_t *patricia, void_fn_t func);
void patricia_destroy_tree (patricia_tree_t *patricia, void_fn_t func);
void patricia_process (patricia_tree_t *patricia, void_fn_t func);

patricia_node_t *patricia_new_node(patricia_tree_t *patricia, unsigned char bit,prefix_t *prefix); 

int registernodeext(const char *name);
int findnodeext(const char *name);
void releasenodeext(int index);

void node_increment_usercount( patricia_node_t *node);
void node_decrement_usercount( patricia_node_t *node);
int is_normalized_ipmask( struct irc_in_addr *sin, unsigned char bitlen );

/* alloc */
void freeprefix (prefix_t *prefix);
prefix_t *newprefix();
patricia_node_t *newnode();
void freenode (patricia_node_t *node);

/* } */

patricia_node_t *refnode(patricia_tree_t *tree, struct irc_in_addr *sin, int bitlen);
void derefnode(patricia_tree_t *tree, patricia_node_t *node);

#define get_byte_for_bit(base, nr) (base)[(nr) >> 3]
#define bigendian_bitfor(nr) (0x80 >> ((nr) & 0x07))
#define is_bit_set(base, nr) (get_byte_for_bit(base, nr) & bigendian_bitfor(nr))

#define node_to_str(node) irc_cidr_to_str( &node->prefix->sin, irc_bitlen(&(node->prefix->sin),node->prefix->bitlen)) 

#define PATRICIA_MAXBITS 128

#define PATRICIA_WALK(Xhead, Xnode) \
    do { \
        patricia_node_t *Xstack[PATRICIA_MAXBITS+1]; \
        patricia_node_t **Xsp = Xstack; \
        patricia_node_t *Xrn = (Xhead); \
        while ((Xnode = Xrn)) { \
            if (Xnode->prefix)

#define PATRICIA_WALK_ALL(Xhead, Xnode) \
do { \
        patricia_node_t *Xstack[PATRICIA_MAXBITS+1]; \
        patricia_node_t **Xsp = Xstack; \
        patricia_node_t *Xrn = (Xhead); \
        while ((Xnode = Xrn)) { \
	    if (1)

#define PATRICIA_WALK_BREAK { \
	    if (Xsp != Xstack) { \
		Xrn = *(--Xsp); \
	     } else { \
		Xrn = (patricia_node_t *) 0; \
	    } \
	    continue; }

#define PATRICIA_WALK_END \
            if (Xrn->l) { \
                if (Xrn->r) { \
                    *Xsp++ = Xrn->r; \
                } \
                Xrn = Xrn->l; \
            } else if (Xrn->r) { \
                Xrn = Xrn->r; \
            } else if (Xsp != Xstack) { \
                Xrn = *(--Xsp); \
            } else { \
                Xrn = (patricia_node_t *) 0; \
            } \
        } \
    } while (0)

#define PATRICIA_WALK_CLEAR(Xhead, Xnode) \
    patricia_node_t *Xstack[PATRICIA_MAXBITS+1]; \
    patricia_node_t **Xsp = Xstack; \
    patricia_node_t *Xrn = (Xhead); \
    while ((Xnode = Xrn)) { \
      patricia_node_t *l = Xrn->l; \
      patricia_node_t *r = Xrn->r; \

#define PATRICIA_WALK_CLEAR_END \
            if (l) { \
                if (r) { \
                    *Xsp++ = r; \
                } \
                Xrn = l; \
            } else if (r) { \
                Xrn = r; \
            } else if (Xsp != Xstack) { \
                Xrn = *(--Xsp); \
            } else { \
                Xrn = (patricia_node_t *) 0; \
            } \
        } 
   
#endif /* _PATRICIA_H */
