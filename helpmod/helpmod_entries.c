#include "helpmod_entries.h"
#include "helpmod_alias.h"
#include "../core/error.h"

struct helpmod_parsed_line_struct helpmod_parsed_line;

int helpmod_valid_selection(helpmod_entry state, int selection)
{
    if (selection == -1)
	return ((int)state->parent);
    return (selection >= 0 && selection < state->option_count);
}

helpmod_entry helpmod_make_selection(helpmod_entry state, int selection)
{
    if (selection == -1)
	return state->parent;
    return state->options[selection];
}

void helpmod_init_entry(helpmod_entry * entry)
{
    *entry = (helpmod_entry)malloc(sizeof(**entry));
    (*entry)->option_count = (*entry)->text_lines = 0;
    (*entry)->text = NULL;
    (*entry)->options = NULL;
    (*entry)->parent = NULL;
    (*entry)->description = NULL;
}

void helpmod_clear_entries(helpmod_entry state)
{
    int i;
    if (state == NULL)
	return;
    if (state->description != NULL)
        freesstring(state->description);
    for (i=0;i<state->text_lines;i++)
	freesstring(state->text[i]);
    if (state->text_lines)
        free(state->text);
    for (i=0;i<state->option_count;i++)
    {
	helpmod_clear_entries(state->options[i]);
	free(state->options[i]);
    }
    if (state->option_count)
	free(state->options);
}

void helpmod_parse_line(FILE* input)
{
    int i, slen;
    char str[512] = "";
    /**str = '\0'; */
    fgets(str,511,input);
    helpmod_parsed_line.line_type = HM_LINE_NORMAL;
    if (*str >= '0' && *str <= '0')
    {
	sscanf(str, "%2d", &helpmod_parsed_line.depth);
	if (str[2] == '*')
	{
            helpmod_parsed_line.line_type = HM_LINE_TEXT;
	    strcpy(helpmod_parsed_line.text, str + 3);
	}
	else if (str[2] == '$')
	{
	    helpmod_parsed_line.line_type = HM_LINE_ALIAS;
            strcpy(helpmod_parsed_line.text, str + 3);
	}
	else
	    strcpy(helpmod_parsed_line.text, str + 2);
        return;
    }
    for(i=0,slen=strlen(str);i<slen;i++)
	if (str[i] == '*')
	{
	    helpmod_parsed_line.line_type = HM_LINE_TEXT;
	    i++;
	    break;
	}
	else if (str[i] == '$')
	{
	    helpmod_parsed_line.line_type = HM_LINE_ALIAS;
	    i++;
	    break;
	}
	else if (str[i] != ' ')
	    break;

    helpmod_parsed_line.depth = i;
    strcpy(helpmod_parsed_line.text, str + i);
}

void helpmod_clear_all_entries(void)
{
    helpmod_clear_entries(helpmod_base);
    free(helpmod_base);
    helpmod_base = NULL;
}

void helpmod_load_entries(char* setting_file)
{
    FILE *main_input;
    FILE *tmp_input;
    char buffer[512];
    int i;
    if (NULL == setting_file)
	setting_file = "helpmod/default";
    if (NULL == (main_input = fopen(setting_file, "rt")))
    {
	Error("helpmod", ERR_WARNING, "Default database not found, service will be unavailable until a valid database is loaded\n");
        return;
    }
    while (!feof(main_input))
    {
	fgets(buffer,511,main_input);
	if (feof(main_input))
	    return;
	if (*buffer == '$')
	{
	    helpmod_base->options = (helpmod_entry*)realloc(helpmod_base->options, sizeof(helpmod_entry) * ++helpmod_base->option_count);
            helpmod_base->options[helpmod_base->option_count-1] = NULL;
	    /* remove the \n, it's not wanted */
	    for (i=0;i<strlen(buffer+1);i++)
		if ((buffer+1)[i] == '\n')
		{
		    (buffer+1)[i] = 0x00;
		    break;
		}
	    tmp_input = fopen(buffer+1, "r");
	    if (tmp_input == NULL)
	    {
		Error("helpmod", ERR_ERROR, "File %s specified in %s not found\n",buffer+1, setting_file);
		return;
	    }
            helpmod_parse_line(tmp_input);
	    helpmod_load_entry(&helpmod_base->options[helpmod_base->option_count-1], tmp_input, 0, helpmod_base);
            fclose(tmp_input);
	}
	else
	{
	    helpmod_base->text = (sstring**)realloc(helpmod_base->text, sizeof(sstring*) * ++helpmod_base->text_lines);
	    helpmod_base->text[helpmod_base->text_lines-1] = getsstring(buffer, strlen(buffer));
	}
    }
    fclose(main_input);
}

void helpmod_load_entry(helpmod_entry* base, FILE* input, int depth, helpmod_entry parent)
{
    if (feof(input))
    {
	*base = NULL;
	helpmod_parsed_line.depth = 0;
	return;
    }

    if (helpmod_parsed_line.depth < depth)
    {
	*base = NULL;
	return;
    }

    helpmod_init_entry(base);
    (*base)->parent = parent;
    (*base)->description = getsstring(helpmod_parsed_line.text, strlen(helpmod_parsed_line.text));
    depth++;

    helpmod_parse_line(input);
    while (helpmod_parsed_line.line_type == HM_LINE_ALIAS)
    {
        helpmod_add_alias(helpmod_parsed_line.text, *base); /* create an alias */
        helpmod_parse_line(input);
    }
    while (helpmod_parsed_line.line_type == HM_LINE_TEXT)
    {
	if (feof(input))
            return;
	(*base)->text = (sstring**)realloc((*base)->text, sizeof(sstring*) * ++(*base)->text_lines);
	(*base)->text[(*base)->text_lines - 1] = getsstring(helpmod_parsed_line.text, strlen(helpmod_parsed_line.text));
        helpmod_parse_line(input);
    }
    while (helpmod_parsed_line.depth == depth)
    {
	
	(*base)->options = (helpmod_entry*)realloc((*base)->options, sizeof(helpmod_entry) * ++(*base)->option_count);
	helpmod_load_entry(&(*base)->options[(*base)->option_count-1],input,depth, *base);
    }
    return;
}

long helpmod_entry_count(helpmod_entry base)
{
    int i;
    long counter = 0;
    if (base == NULL)
	return 0;
    if (base->options == NULL)
	return 0;
    for (i=0;i<base->option_count;i++)
	counter+=helpmod_entry_count(base->options[i]);
    return i+counter;
}
