#ifndef HELPMOD_ALIAS_C
#define HELPMOD_ALIAS_C

#include "../lib/sstring.h"
#include "helpmod_entries.h"
#include <string.h>
#include <ctype.h>

typedef struct alias_tree_struct
{
    sstring* name;
    helpmod_entry state;
    struct alias_tree_struct* left;
    struct alias_tree_struct* right;
}* alias_tree;

extern alias_tree aliases;

int helpmod_alias_count(alias_tree);
void helpmod_init_alias(alias_tree*);
void helpmod_clear_aliases(alias_tree*);
helpmod_entry helpmod_get_alias(char*);
void helpmod_add_alias(char*, helpmod_entry);

#endif