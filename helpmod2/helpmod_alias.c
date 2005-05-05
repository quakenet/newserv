#include "helpmod_alias.h"

#include "hgen.h"

int helpmod_alias_count(alias_tree node)
{
    if (node == NULL)
	return 0;
    else
	return 1 + helpmod_alias_count(node->left) + helpmod_alias_count(node->right);
}

void helpmod_init_alias(alias_tree* node)
{
    *node = (alias_tree)malloc(sizeof(struct alias_tree_struct));
    (*node)->left = NULL;
    (*node)->right = NULL;
    (*node)->name = NULL;
    (*node)->state = NULL;
}
void helpmod_clear_aliases(alias_tree* node)
{
    if (!*node)
	return;
    helpmod_clear_aliases(&(*node)->left);
    helpmod_clear_aliases(&(*node)->right);
    freesstring((*node)->name);
    free(*node);
    *node = NULL;
}
helpmod_entry helpmod_get_alias(char* search)
{
    int val;
    alias_tree tmp = aliases;
    while (tmp)
    {
	val = ci_strcmp(tmp->name->content, search);
	if (!val)
	    return tmp->state;
	if (val < 0)
	    tmp = tmp->left;
	else
	    tmp = tmp->right;
    }
    return NULL;
}

void helpmod_add_alias(char* name, helpmod_entry state)
{
    int val;
    alias_tree* tmp = &aliases;
    while (*tmp)
    {
	val = strcmp((*tmp)->name->content, name);
	if (!val)
            return; /* duplicate, let's not do anything, silly person to create duplicates in the file.. */
	if (val < 0)
	    tmp = &(*tmp)->left;
	else
            tmp = &(*tmp)->right;
    }
    helpmod_init_alias(tmp);
    (*tmp)->state = state;
    (*tmp)->name = getsstring(name,strlen(name));
    for (val=0;val < strlen(name);val++)
	if (isspace((*tmp)->name->content[val]))
	{
	    (*tmp)->name->content[val] = 0x00;
	    break;
	}
}
