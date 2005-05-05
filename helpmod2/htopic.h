/* H topic system */

#ifndef HTOPIC_H
#define HTOPIC_H

typedef struct htopic_struct
{
    sstring *entry;
    struct htopic_struct *next;
} htopic;

htopic *htopic_add(htopic**, const char*, int);
htopic *htopic_del(htopic**, htopic*);
void htopic_del_all(htopic**);

htopic *htopic_get(htopic*, int);

const char *htopic_construct(htopic*);

int htopic_count(htopic*);
int htopic_len(htopic*);

#endif
