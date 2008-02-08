/*
 * $Id: patricia.c,v 1.7 2005/12/07 20:46:41 dplonka Exp $
 * Dave Plonka <plonka@doit.wisc.edu>
 *
 * This product includes software developed by the University of Michigan,
 * Merit Network, Inc., and their contributors. 
 *
 * This file had been called "radix.c" in the MRT sources.
 *
 * I renamed it to "patricia.c" since it's not an implementation of a general
 * radix trie.  Also I pulled in various requirements from "prefix.c" and
 * "demo.c" so that it could be used as a standalone API.
 */

static char copyright[] =
"This product includes software developed by the University of Michigan, Merit"
"Network, Inc., and their contributors.";

#include <assert.h> /* assert */
#include <ctype.h> /* isdigit */
#include <errno.h> /* errno */
#include <math.h> /* sin */
#include <stddef.h> /* NULL */
#include <stdio.h> /* sprintf, fprintf, stderr */
#include <stdlib.h> /* free, atol, calloc */
#include <string.h> /* memcpy, strchr, strlen */

#include "patricia.h"

/* { from prefix.c */

/* prefix_tochar
 * convert prefix information to bytes
 */
u_char *
prefix_tochar (prefix_t * prefix)
{
    if (prefix == NULL)
	return (NULL);

    return ((u_char *) & prefix->sin);
}

int 
comp_with_mask (void *addr, void *dest, u_int mask)
{

    if ( /* mask/8 == 0 || */ memcmp (addr, dest, mask / 8) == 0) {
	int n = mask / 8;
	int m = ((-1) << (8 - (mask % 8)));

	if (mask % 8 == 0 || (((u_char *)addr)[n] & m) == (((u_char *)dest)[n] & m))
	    return (1);
    }
    return (0);
}

prefix_t *
patricia_new_prefix (struct irc_in_addr *dest, int bitlen)
{
    prefix_t *prefix = NULL;

    prefix = malloc(sizeof (prefix_t));
    memcpy (&prefix->sin, dest, 16);

    prefix->bitlen = (bitlen >= 0)? bitlen: 128;    
    prefix->ref_count = 1;
    return prefix;
}

prefix_t *
patricia_ref_prefix (prefix_t * prefix)
{
    if (prefix == NULL)
	return (NULL);
    if (prefix->ref_count == 0) {
	/* make a copy in case of a static prefix */
        return (patricia_new_prefix (&prefix->sin, prefix->bitlen));
    }
    prefix->ref_count++;
/* fprintf(stderr, "[A %s, %d]\n", prefix_toa (prefix), prefix->ref_count); */
    return (prefix);
}

void 
patricia_deref_prefix (prefix_t * prefix)
{
    if (prefix == NULL)
	return;
    /* for secure programming, raise an assert. no static prefix can call this */
    assert (prefix->ref_count > 0);

    prefix->ref_count--;
    assert (prefix->ref_count >= 0);
    if (prefix->ref_count <= 0) {
	free(prefix);
	return;
    }
}

/* } */

/* #define PATRICIA_DEBUG 1 */

/* these routines support continuous mask only */

patricia_tree_t *
patricia_new_tree (int maxbits)
{
    patricia_tree_t *patricia = calloc(1, sizeof *patricia);

    patricia->maxbits = maxbits;
    patricia->head = NULL;
    patricia->num_active_node = 0;
    assert (maxbits <= PATRICIA_MAXBITS); /* XXX */
    return (patricia);
}


void
patricia_clear_tree (patricia_tree_t *patricia, void_fn_t func)
{
    assert (patricia);
    if (patricia->head) {

        patricia_node_t *Xstack[PATRICIA_MAXBITS+1];
        patricia_node_t **Xsp = Xstack;
        patricia_node_t *Xrn = patricia->head;

        while (Xrn) {
            patricia_node_t *l = Xrn->l;
            patricia_node_t *r = Xrn->r;

    	    if (Xrn->prefix) {
		if (Xrn->exts && func)
			func(Xrn->exts);
		patricia_deref_prefix (Xrn->prefix);
	    }
    	    free(Xrn);
	    patricia->num_active_node--;

            if (l) {
                if (r) {
                    *Xsp++ = r;
                }
                Xrn = l;
            } else if (r) {
                Xrn = r;
            } else if (Xsp != Xstack) {
                Xrn = *(--Xsp);
            } else {
                Xrn = (patricia_node_t *) 0;
            }
        }
    }
    assert (patricia->num_active_node == 0);
    /* free(patricia); */
}


void
patricia_destroy_tree (patricia_tree_t *patricia, void_fn_t func)
{
    patricia_clear_tree (patricia, func);
    free(patricia);
}


/*
 * if func is supplied, it will be called as func(node->prefix)
 */

void
patricia_process (patricia_tree_t *patricia, void_fn_t func)
{
    patricia_node_t *node;
    assert (func);

    PATRICIA_WALK (patricia->head, node) {
	func (node->prefix);
    } PATRICIA_WALK_END;
}

size_t
patricia_walk_inorder(patricia_node_t *node, void_fn_t func)
{
    size_t n = 0;
    assert(func);

    if (node->l) {
         n += patricia_walk_inorder(node->l, func);
    }

    if (node->prefix) {
	func(node->prefix);
	n++;
    }
	
    if (node->r) {
         n += patricia_walk_inorder(node->r, func);
    }

    return n;
}


patricia_node_t *
patricia_search_exact (patricia_tree_t *patricia, struct irc_in_addr *sin, unsigned char bitlen)
{
    patricia_node_t *node;
    u_char *addr;

    assert (patricia);
    assert (sin);
    assert (bitlen <= patricia->maxbits);

    if (patricia->head == NULL)
	return (NULL);

    node = patricia->head;
    addr = (u_char *)sin;

    while (node->bit < bitlen) {
	if (BIT_TEST (addr[node->bit >> 3], 0x80 >> (node->bit & 0x07)))
	    node = node->r;
	else
	    node = node->l;

	if (node == NULL)
	    return (NULL);
    }

    if (node->bit > bitlen || node->prefix == NULL)
	return (NULL);
    assert (node->bit == bitlen);
    assert (node->bit == node->prefix->bitlen);
    if (comp_with_mask (prefix_tochar (node->prefix), addr,	bitlen)) {
	return (node);
    }
    return (NULL);
}


/* if inclusive != 0, "best" may be the given prefix itself */
patricia_node_t *
patricia_search_best2 (patricia_tree_t *patricia, struct irc_in_addr *sin, unsigned char bitlen, int inclusive)
{
    patricia_node_t *node;
    patricia_node_t *stack[PATRICIA_MAXBITS + 1];
    u_char *addr;
    int cnt = 0;

    assert (patricia);
    assert (sin);
    assert (bitlen <= patricia->maxbits);

    if (patricia->head == NULL)
	return (NULL);

    node = patricia->head;
    addr = (u_char *)sin;

    while (node->bit < bitlen) {

	if (node->prefix) {
	    stack[cnt++] = node;
	}

	if (BIT_TEST (addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
	    node = node->r;
	}
	else {
	    node = node->l;
	}

	if (node == NULL)
	    break;
    }

    if (inclusive && node && node->prefix)
	stack[cnt++] = node;

    if (cnt <= 0)
	return (NULL);

    while (--cnt >= 0) {
	node = stack[cnt];
	if (comp_with_mask (prefix_tochar (node->prefix), 
			    addr,
			    node->prefix->bitlen)) {
	    return (node);
	}
    }
    return (NULL);
}


patricia_node_t *
patricia_search_best (patricia_tree_t *patricia, struct irc_in_addr *sin, unsigned char bitlen)
{
    return (patricia_search_best2 (patricia, sin, bitlen, 1));
}


patricia_node_t *
patricia_lookup (patricia_tree_t *patricia, prefix_t *prefix)
{
    patricia_node_t *node, *new_node, *parent, *glue;
    u_char *addr, *test_addr;
    u_int bitlen, check_bit, differ_bit;
    int i, j, r;

    assert (patricia);
    assert (prefix);
    assert (prefix->bitlen <= patricia->maxbits);

    /* if new trie, create the first node */
    if (patricia->head == NULL) {
	node = malloc(sizeof *node);
	memset(node->exts, 0, PATRICIA_MAXSLOTS * sizeof(void *));
	node->bit = prefix->bitlen;
	node->prefix = patricia_ref_prefix (prefix);
	node->parent = NULL;
	node->l = node->r = NULL;
	node->usercount = 0;
	patricia->head = node;
	patricia->num_active_node++;
	return (node);
    }

    addr = prefix_touchar (prefix);
    bitlen = prefix->bitlen;
    node = patricia->head;

    /* while ( bitlength of tree node < bitlength of node we're searching for || the node has no prefix */
    while (node->bit < bitlen || node->prefix == NULL) {
        /* check that we're not at the lowest leaf i.e. node->bit is less than max bits */
	if (node->bit < patricia->maxbits &&
	    (addr[node->bit >> 3]) & (0x80 >> (node->bit & 0x07))) {
	    if (node->r == NULL)
		break;
	    node = node->r;
	}
	else {
	    if (node->l == NULL)
		break;
	    node = node->l;
	}

	assert (node);
    }

    assert (node->prefix);

    test_addr = prefix_touchar (node->prefix);
    /* find the first bit different */
    check_bit = (node->bit < bitlen)? node->bit: bitlen;
    differ_bit = 0;
    for (i = 0; i*8 < check_bit; i++) {
	if ((r = (addr[i] ^ test_addr[i])) == 0) {
	    differ_bit = (i + 1) * 8;
	    continue;
	}
	/* I know the better way, but for now */
	for (j = 0; j < 8; j++) {
	    if ((r) & ((0x80 >> j)))
		break;
	}
	/* must be found */
	assert (j < 8);
	differ_bit = i * 8 + j;
	break;
    }
    if (differ_bit > check_bit)
	differ_bit = check_bit;


    parent = node->parent;
    while (parent && parent->bit >= differ_bit) {
	node = parent;
	parent = node->parent;
    }

    if (differ_bit == bitlen && node->bit == bitlen) {
	if (node->prefix) {
	    return (node);
	}
	node->prefix = patricia_ref_prefix (prefix);
	return (node);
    }

    new_node = malloc(sizeof *new_node);
    memset(new_node->exts, 0, PATRICIA_MAXSLOTS * sizeof(void *));
    new_node->bit = prefix->bitlen;
    new_node->prefix = patricia_ref_prefix (prefix);
    new_node->parent = NULL;
    new_node->l = new_node->r = NULL;
    new_node->usercount = 0;
    patricia->num_active_node++;

    if (node->bit == differ_bit) {
	new_node->parent = node;
	if (node->bit < patricia->maxbits &&
	    (addr[node->bit >> 3]) & (0x80 >> (node->bit & 0x07))) {
	    assert (node->r == NULL);
	    node->r = new_node;
	}
	else {
	    assert (node->l == NULL);
	    node->l = new_node;
	}
	return (new_node);
    }

    if (bitlen == differ_bit) {
	if (bitlen < patricia->maxbits &&
	    (test_addr[bitlen >> 3]) & (0x80 >> (bitlen & 0x07))) {
	    new_node->r = node;
	}
	else {
	    new_node->l = node;
	}
	new_node->parent = node->parent;
	if (node->parent == NULL) {
	    assert (patricia->head == node);
	    patricia->head = new_node;
	}
	else if (node->parent->r == node) {
	    node->parent->r = new_node;
	}
	else {
	    node->parent->l = new_node;
	}
	node->parent = new_node;
    }
    else {
        glue = malloc(sizeof *glue);
	memset(glue->exts, 0, PATRICIA_MAXSLOTS * sizeof(void *));
        glue->bit = differ_bit;
        glue->prefix = NULL;
        glue->parent = node->parent;
        glue->usercount = 0;
        patricia->num_active_node++;
	if (differ_bit < patricia->maxbits &&
	    (addr[differ_bit >> 3]) & (0x80 >> (differ_bit & 0x07))) {
	    glue->r = new_node;
	    glue->l = node;
	}
	else {
	    glue->r = node;
	    glue->l = new_node;
	}
	new_node->parent = glue;

	if (node->parent == NULL) {
	    assert (patricia->head == node);
	    patricia->head = glue;
	}
	else if (node->parent->r == node) {
	    node->parent->r = glue;
	}
	else {
	    node->parent->l = glue;
	}
	node->parent = glue;
    }
    return (new_node);
}

void
patricia_remove (patricia_tree_t *patricia, patricia_node_t *node)
{
    patricia_node_t *parent, *child;

    assert (patricia);
    assert (node);

    if (node->r && node->l) {	
	/* this might be a placeholder node -- have to check and make sure
	 * there is a prefix aossciated with it ! */
	if (node->prefix != NULL) 
	  patricia_deref_prefix (node->prefix);
	node->prefix = NULL;
	return;
    }

    if (node->r == NULL && node->l == NULL) {
	parent = node->parent;
	patricia_deref_prefix (node->prefix);
	free(node);
        patricia->num_active_node--;

	if (parent == NULL) {
	    assert (patricia->head == node);
	    patricia->head = NULL;
	    return;
	}

	if (parent->r == node) {
	    parent->r = NULL;
	    child = parent->l;
	}
	else {
	    assert (parent->l == node);
	    parent->l = NULL;
	    child = parent->r;
	}

	if (parent->prefix)
	    return;

	/* we need to remove parent too */

	if (parent->parent == NULL) {
	    assert (patricia->head == parent);
	    patricia->head = child;
	}
	else if (parent->parent->r == parent) {
	    parent->parent->r = child;
	}
	else {
	    assert (parent->parent->l == parent);
	    parent->parent->l = child;
	}
	child->parent = parent->parent;
	free(parent);
        patricia->num_active_node--;
	return;
    }

    if (node->r) {
	child = node->r;
    }
    else {
	assert (node->l);
	child = node->l;
    }
    parent = node->parent;
    child->parent = parent;

    patricia_deref_prefix (node->prefix);
    free(node);
    patricia->num_active_node--;

    if (parent == NULL) {
	assert (patricia->head == node);
	patricia->head = child;
	return;
    }

    if (parent->r == node) {
	parent->r = child;
    }
    else {
        assert (parent->l == node);
	parent->l = child;
    }
}

/* } */

patricia_node_t *
refnode(patricia_tree_t *tree, struct irc_in_addr *sin, int bitlen) {
  patricia_node_t *node;
  prefix_t *prefix;

  node = patricia_search_exact(tree, sin, bitlen);

  if (node == NULL) {
    prefix = patricia_new_prefix(sin, bitlen);
    node = patricia_lookup(tree, prefix);
    patricia_deref_prefix(prefix);
  } else if (node->prefix) {
    patricia_ref_prefix(node->prefix);
  }

  return node;
}

void
derefnode(patricia_tree_t *tree, patricia_node_t *node) {
  if (!node || !node->prefix)
    return;

  if (node->prefix->ref_count == 1) {
    patricia_remove(tree, node);
  } else
    patricia_deref_prefix(node->prefix);
}
