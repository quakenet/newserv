#ifndef HELPMOD_ENTRIES_H
#define HELPMOD_ENTRIES_H

#include "../lib/sstring.h"
#include <stdio.h>
#include <stdlib.h>

typedef enum
{
    HM_LINE_NORMAL,
    HM_LINE_ALIAS,
    HM_LINE_TEXT
} HM_line_type;

typedef struct helpmod_entry_struct
{
    struct helpmod_entry_struct* parent;
    unsigned short int option_count;
    unsigned short int text_lines;

    sstring* description;
    sstring** text;
    struct helpmod_entry_struct** options;
} *helpmod_entry;

extern struct helpmod_parsed_line_struct
{
    char text[512];
    int depth;
    HM_line_type line_type;
} helpmod_parsed_line;

extern helpmod_entry helpmod_base;

int helpmod_valid_selection(helpmod_entry, int);
helpmod_entry helpmod_make_selection(helpmod_entry, int);
void helpmod_init_entries(void);
void helpmod_init_entry(helpmod_entry*);
void helpmod_clear_all_entries(void);
void helpmod_load_entries(char*);
void helpmod_load_entry(helpmod_entry*, FILE*, int, helpmod_entry);
void helpmod_parse_line(FILE*);
void helpmod_clear_entries(helpmod_entry);
long helpmod_entry_count(helpmod_entry);

#endif
