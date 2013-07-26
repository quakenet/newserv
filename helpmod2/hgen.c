#include <ctype.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hgen.h"

int ci_strcmp(const char *str1, const char *str2)
{
    while (*str1)
    {
        if (tolower(*str1) != tolower(*str2))
            return tolower(*str1) - tolower(*str2);
        str1++;
        str2++;
    }
    return tolower(*str1) - tolower(*str2);
}

/* can't even remember what this does, but it works */
static char *rstrchr(const char * str, char chr)
{
    while (*str)
        if (tolower(*str) == tolower(chr))
            return (char*)str;
        else
            str++;
    return NULL;
}

int strregexp(const char * str, const char * pattern)
{
    if (!pattern)
        return 1;
    while (*pattern && *str)
    {
        if (*pattern == '?')
            pattern++, str++;
        else if (*pattern == '*')
        {
            while (*(++pattern) == '*');
            if (!*pattern)
                return 1;
            if (*pattern == '?')
            {
                while (*str)
                    if (strregexp(str++, pattern))
                        return 1;
            }
            else
            {
                while ((str = rstrchr(str, *pattern)))
                    if (strregexp(str++, pattern))
                        return 1;
            }
            return 0;
        }
        else if (tolower(*pattern) != tolower(*str))
            return 0;
        else
            pattern++, str++;
    }

    while(*pattern == '*')
        pattern++;

    if (!*pattern && !*str)
        return 1;
    else
        return 0;
}

#define TIME_PRINT(elem,marker)\
if (elem)\
{\
    buf+=sprintf(buf, " %d%s", elem, marker);\
}\

/* This implementation might look a little evil but it does work */
const char *helpmod_strtime(int total_seconds)
{
    static int buffer_index = 0;
    static char buffers[3][64];

    char *buf = buffers[buffer_index];
    char *buffer = buf;

    int years, months, days, hours, minutes, seconds;

    buffer_index = (buffer_index+1) % 3;

    /* trivial case */
    if (!total_seconds)
        return "0s";

    if (total_seconds < 0)
    {
	*buf = '-';
	buf++;
        total_seconds = -total_seconds;
    }

    years = total_seconds / HDEF_y;
    total_seconds %= HDEF_y;

    months = total_seconds / HDEF_M;
    total_seconds %= HDEF_M;

    days = total_seconds / HDEF_d;
    total_seconds %= HDEF_d;

    hours = total_seconds / HDEF_h;
    total_seconds %= HDEF_h;

    minutes = total_seconds / HDEF_m;
    total_seconds %= HDEF_m;

    seconds = total_seconds;

    TIME_PRINT(years, "y");
    TIME_PRINT(months, "M");
    TIME_PRINT(days, "d");
    TIME_PRINT(hours, "h");
    TIME_PRINT(minutes, "m");
    TIME_PRINT(seconds, "s");

    if (*buffer != '-')
	return buffer+1;
    else
        return buffer;
}

int helpmod_read_strtime(const char *str)
{
    int sum = 0, tmp_sum, tmp;

    while (*str)
    {
        if (!sscanf(str, "%d%n", &tmp_sum, &tmp))
            return -1;
        str+=tmp;

        switch (*str)
        {
        case 's':
            break;
        case 'm':
            tmp_sum*=HDEF_m;
            break;
        case 'h':
            tmp_sum*=HDEF_h;
            break;
        case 'd':
            tmp_sum*=HDEF_d;
            break;
        case 'w':
            tmp_sum*=HDEF_w;
            break;
        case 'M':
            tmp_sum*=HDEF_M;
            break;
        case 'y':
            tmp_sum*=HDEF_y;
            break;
        default: /* includes '\0' */
            return -1;
        }

        str++;
        /* sanity checks */
        if (tmp_sum > 10 * HDEF_y)
            return -1;
        sum+=tmp_sum;
        if (sum > 10 * HDEF_y)
            return -1;
    }
    return sum;
}

int hword_count(const char *ptr)
{
    int wordc = 0;

    while (*ptr)
    {
        while (isspace(*ptr) && *ptr)
            ptr++;
        if (*ptr)
            wordc++;
        while (!isspace(*ptr) && *ptr)
            ptr++;
    }

    return wordc;
}

int helpmod_is_lame_line(const char *line)
{
    const char lamechars[] = {(char)2, (char)3, (char)22, (char)31, (char)0};
    if (strpbrk(line, lamechars) != NULL)
        return 1;
    else
	return 0;
}

int strislower(const char *str)
{
    for (;*str;str++)
        if (isupper(*str))
            return 0;
    return 1;
}

int strisupper(const char *str)
{
    for (;*str;str++)
        if (islower(*str))
            return 0;
    return 1;
}

int strisalpha(const char *str)
{
    for (;*str;str++)
        if (!isalpha(*str))
            return 0;
    return 1;
}

int strnumcount(const char *str)
{
    int count = 0;
    for (;*str;str++)
        if (isdigit(*str))
            count++;
    return count;
}

float helpmod_percentage(int larger, int smaller)
{
    if (larger == 0)
	return 0.0;

    return 100.0 * ((float)smaller / (float)larger);
}

int helpmod_select(const char *str, const char **strs, int *enums, int count)
{
    int i;

    if (count == 0)
	return -1;

    for (i = 0;i < count;i++)
	if (!ci_strcmp(strs[i], str))
            return enums[i];

    return -1;
}
