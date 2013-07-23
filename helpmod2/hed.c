#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <ctype.h>

#include "../localuser/localuserchannel.h"

#include "hed.h"
#include "huser.h"
#include "hchannel.h"
#include "helpmod.h"
#include "hgen.h"

int hed_is_valid_filename(const char *fname)
{
    int i;
    if (strlen(fname) < 1 || strlen(fname) > (64 - 1))
	return 0;

    for (i=0;i < 64 && fname[i] != '\0';i++)
	if (!isalnum(fname[i]))
            return 0;

    return 1;
}

helpmod_editor *hed_open(huser *husr, const char *fname)
{
    helpmod_editor *editor;
    hed_line *ptr;
    char fname_buffer[128];
    FILE *file;
    int i;

    if (hed_get(fname) != NULL)
        return hed_get(fname);

    if (!hed_is_valid_filename(fname))
	return NULL;

    sprintf(fname_buffer, HELPMOD_TEXT_DIR"/%s" ,fname);
    file = fopen(fname_buffer, "rwt");
    if (file == NULL)
	return NULL;

    editor = (helpmod_editor*)malloc(sizeof (helpmod_editor));

    editor->user = husr;
    strcpy(editor->filename, fname);
    editor->line = 0;
    editor->flags = 0;

    editor->state = HED_COMMAND;
    editor->last_error = HED_ERR_NO_ERROR;

    editor->start = NULL;
    editor->free_lines = &editor->buffers[0];
    editor->clipboard = NULL;

    for (ptr = editor->free_lines,i = 1;i < HED_BUFFER_LINES;i++,ptr = ptr->next)
    {
	ptr->next = &editor->buffers[i];
        ptr->line[0] = '\0';
    }
    ptr->next = NULL;

    for(i = 0;!feof(file) && i < HED_BUFFER_LINES;i++)
    {
        char *str = hed_add_line(editor, i);
	if (!fgets(str, 384, file))
	{ /* If the input ran out, the last line added has to be removed */
	    hed_del_line(editor, i);
            hed_clear_clipboard(editor);
            break;
	}
	strchr(str, '\n')[0] = '\0';
    }

    fclose(file);

    /* Clear flags */
    editor->flags = HED_FLAGS_DEFAULT;

    editor->next = helpmod_editors;
    helpmod_editors = editor;

    return editor;
}

helpmod_editor *hed_write(helpmod_editor *editor)
{
    FILE *file;
    char fname_buffer[128];
    hed_line *ptr;

    sprintf(fname_buffer, HELPMOD_TEXT_DIR"/%s" ,editor->filename);
    if ((file = fopen(fname_buffer, "wt")) == NULL)
    {
        Error("helpmod", ERR_ERROR, "hed: could not open file: %s", fname_buffer);
        return editor;
    }

    for (ptr = editor->start;ptr != NULL;ptr = ptr->next)
        fprintf(file, "%s\n", ptr->line);

    fclose(file);
    return editor;
}

helpmod_editor *hed_close(helpmod_editor *editor)
{
    helpmod_editor **ptr = &helpmod_editors;

    if (editor == NULL)
        return NULL;

    for (;*ptr;ptr = &(*ptr)->next)
	if (*ptr == editor)
	{
	    helpmod_editor *tmp = editor->next;
	    editor->user->editor = NULL;

	    free (editor);
	    *ptr = tmp;
	    break;
	}
    return NULL;
}

int hed_byte_count(helpmod_editor *editor)
{
    hed_line *ptr = editor->start;
    int count = 0;

    for (;ptr && ptr->line[0] != '\0';ptr = ptr->next)
	count += strlen(ptr->line);

    return count;
}

int hed_line_count(helpmod_editor *editor)
{
    hed_line *ptr = editor->start;
    int count = 0;

    for (;ptr && ptr != NULL;ptr = ptr->next)
	count++;

    return count;
}

/* Get by filename */
helpmod_editor *hed_get(const char *fname)
{
    helpmod_editor *tmp = helpmod_editors;
    for (;tmp != NULL;tmp = tmp->next)
	if (!strcmp(tmp->filename, fname))
	    return tmp;
    return NULL;
}

void hed_command (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    helpmod_editor *editor = sender->editor;
    assert (editor != NULL);

    if (editor->state == HED_COMMAND)
    {
	int i, start = editor->line + 1, stop = editor->line + 1, target = editor->line + 1, nread;
        char command;
	char buffer[128];

	if (argc == 0)
	{
	    editor->last_error = HED_ERR_NO_COMMAND;
	    goto ERROR;
	}

	if (strlen(argv[0]) > 64)
	{
	    editor->last_error = HED_ERR_SYNTAX_ERROR;
	    goto ERROR;
	}

	if (!strcmp(argv[0], "wq"))
	{
	    hed_write(editor);
	    helpmod_reply(sender, returntype, "Ged: %d", hed_byte_count(editor));
	    hed_close(editor);
            return;
	}

	{ /* Handle replacements */
	    int replaces = 3, j;
	    for (i=0,j=0;argv[0][i] != '\0' && replaces;i++)
	    {
                replaces--;
		switch (argv[0][i])
		{
		case '.':
		    j+=sprintf(&buffer[j], "%d", editor->line + 1);
		    break;
		case '$':
		    j+=sprintf(&buffer[j], "%d", hed_line_count(editor) + 0);
		    break;
		case '-':
		case '^':
		    j+=sprintf(&buffer[j], "%d", editor->line + 0);
		    break;
		case '+':
		    j+=sprintf(&buffer[j], "%d", editor->line + 2);
		    break;
		default:
		    buffer[j++] = argv[0][i];
                    replaces++;
		}
	    }
            buffer[j] = '\0';
	}

	/* parse the command */
	if (strchr(buffer, ',') != NULL)
	{
	    if (sscanf(buffer, "%d,%d%c%d%n", &start, &stop, &command, &target, &nread) == 4 && (nread == strlen(argv[0])));
	    else if (sscanf(buffer, "%d,%d%c%n", &start, &stop, &command, &nread) == 3 && (nread == strlen(argv[0])));
	    else
	    {
		editor->last_error = HED_ERR_SYNTAX_ERROR;
		goto ERROR;
	    }
	}
	else if (strnumcount(buffer) > 0)
	{
	    if (sscanf(buffer, "%d%n", &start, &nread) == 1 && nread == strlen(argv[0]))
		command = 'p';
	    else if (sscanf(buffer, "%d%c%n", &start, &command, &nread) == 2 && nread == strlen(argv[0]));
	    else if (sscanf(buffer, "%d%c%d%n", &start, &command, &target, &nread) == 3 && nread == strlen(argv[0]));
	    else
	    {
		editor->last_error = HED_ERR_SYNTAX_ERROR;
		goto ERROR;
	    }
	    stop = start;
	}
	else
	{ 
	    if (sscanf(buffer, "%c%n", &command, &nread) == 1 && nread == strlen(argv[0]));
	    else
	    {
		editor->last_error = HED_ERR_SYNTAX_ERROR;
		goto ERROR;
	    }
	}

        /* Change to internal presentation */
	start--;
        stop--;
	target--;

        /* Handle the command */
	switch (command)
	{
	case 'a': /* Append */
	    if (hed_line_count(editor) == HED_BUFFER_LINES)
	    {
                editor->last_error = HED_ERR_BUFFER_FULL;
		goto ERROR;
	    }
	    if (start < 0 || stop > hed_line_count(editor) || start > stop)
	    {
		editor->last_error = HED_ERR_INVALID_ARGUMENT;
		goto ERROR;
	    }
	    editor->state = HED_INPUT_APPEND;
            break;
	case 'i': /* Insert */
	    if (hed_line_count(editor) == HED_BUFFER_LINES)
	    {
                editor->last_error = HED_ERR_BUFFER_FULL;
		goto ERROR;
	    }
	    if (start < 0 || stop >= hed_line_count(editor) || start > stop)
	    {
		editor->last_error = HED_ERR_INVALID_ARGUMENT;
		goto ERROR;
	    }
	    editor->state = HED_INPUT_INSERT;
            break;
	case 'c': /* Replace */
	    if (start < 0 || stop >= hed_line_count(editor) || start > stop)
	    {
		editor->last_error = HED_ERR_INVALID_ARGUMENT;
		goto ERROR;
	    }

	    hed_del_line(editor, start);
            editor->state = HED_INPUT_INSERT;

	    break;
	case 'd': /* Delete */
	    if (start < 0 || stop >= hed_line_count(editor) || start > stop)
	    {
		editor->last_error = HED_ERR_INVALID_ARGUMENT;
		goto ERROR;
	    }

	    for (i = 0;i <= stop - start;i++)
		hed_del_line(editor, start);

	    break;
	case 'e': /* Open file for editing */
	case 'E': /* Uncondititional e */
            editor->last_error = HED_ERR_COMMAND_NOT_SUPPORTED;
	    goto ERROR;
	    break;
	case 'f': /* View (or set) the filename */
	    helpmod_reply(sender, returntype, "Ged: %s", editor->filename);
	    break;
	case 'h': /* Last error */
	    helpmod_reply(sender, returntype, "Ged: %s", hed_error_text(editor->last_error));
	    break;
	case 'H':
            editor->flags ^= HED_FLAG_VERBOSE_ERRORS;
            break;
	case 'j': /* Join lines */
	    if ((start < 0 || start >= hed_line_count(editor)) ||
		(stop < 0 || stop >= hed_line_count(editor)) ||
                start == stop)
	    {
		editor->last_error = HED_ERR_INVALID_ARGUMENT;
		goto ERROR;
	    }

	    if (strlen(hed_get_line(editor, start)) + strlen(hed_get_line(editor, stop)) > 384 - 2)
	    {
		editor->last_error = HED_ERR_LINE_TOO_LONG;
		goto ERROR;
	    }

	    strcat(hed_get_line(editor, start), hed_get_line(editor, stop));
            hed_del_line(editor, stop);

            break;
	case 'l': /* Print */
	case 'p':
	case 'n': /* Print with line numbers */
	    if (start < 0 || stop >= hed_line_count(editor))
	    {
		editor->last_error = HED_ERR_INVALID_ARGUMENT;
		goto ERROR;
	    }

	    for (i=start;i <= stop;i++)
		if (command == 'n')
		    helpmod_reply(sender, returntype, "Ged: %-8d %s", i+1, hed_get_line(editor, i));
		else
		    helpmod_reply(sender, returntype, "Ged: %s", hed_get_line(editor, i));
	    break;
	case 'm': /* Move */
	    if (start < 0 || stop >= hed_line_count(editor) || start > stop)
	    {
		editor->last_error = HED_ERR_INVALID_ARGUMENT;
		goto ERROR;
	    }
	    if (target < 0 || target >= hed_line_count(editor) - (stop - start) ||
		(target >= start && target <= stop))
	    {
		editor->last_error = HED_ERR_INVALID_ARGUMENT;
		goto ERROR;
	    }
	    if (target > start)
                target-= (stop - start);

	    for (i = 0;i <= stop - start;i++)
		hed_del_line(editor, start);

            hed_paste(editor, target);
            break;
	case 'q': /* Quit */
	    if (editor->flags & HED_FLAG_UNSAVED)
	    {
		editor->flags &=~HED_FLAG_UNSAVED;
                editor->last_error = HED_ERR_UNSAVED;
                goto ERROR;
	    }
	    else
	    {
		hed_close(editor);
		return;
	    }
	    return;
	case 'Q': /* Unconditional quit */
	    hed_close(editor);
	    return;
	case 'r': /* Read from file */
	    editor->last_error = HED_ERR_COMMAND_NOT_SUPPORTED;
	    goto ERROR;
	    break;
	case 'w': /* Write file */
	    hed_write(editor);
            editor->flags &=~HED_FLAG_UNSAVED;
	    helpmod_reply(sender, returntype, "Ged: %d", hed_byte_count(editor));
	    break;
	case 'x': /* Paste */
	    if (start < 0 || start >= hed_line_count(editor))
	    {
                editor->last_error = HED_ERR_INVALID_ARGUMENT;
		goto ERROR;
	    }
	    if (editor->clipboard == NULL)
	    {
                editor->last_error = HED_ERR_CLIPBOARD_EMPTY;
		goto ERROR;
	    }
            hed_paste(editor, start);
	    break;
	case '=': /* Current line */
	    helpmod_reply(sender, returntype, "Ged: %d", editor->line + 1);
            break;
	default:
            editor->last_error = HED_ERR_UNKNOWN_COMMAND;
	    goto ERROR;
	}

        editor->line = start;
    }
    else
    { /* HED_INPUT */
	char *str;

	if (argc == 0)
	    ostr[0] = '\0';

	/* return to command mode */
	if (!strcmp(ostr, "."))
	{
            editor->state = HED_COMMAND;
	    return;
	}

	if (hed_line_count(editor) >= HED_BUFFER_LINES)
	{
            editor->last_error = HED_ERR_BUFFER_FULL;
	    editor->state = HED_COMMAND;
	    goto ERROR;
	}

	if (strlen(ostr) > 384 - 2)
	{
            editor->last_error = HED_ERR_LINE_TOO_LONG;
	    goto ERROR;
	}

	switch (editor->state)
	{
	case HED_INPUT_INSERT:
	    str = hed_add_line(editor, editor->line);
            strcpy(str, ostr);
	    break;
	case HED_INPUT_APPEND:
            /* the last line is handled specially */
	    if (editor->line == HED_BUFFER_LINES - 1)
                editor->line--;
	    str = hed_add_line(editor, editor->line + 1);
            strcpy(str, ostr);
	    break;
	case HED_COMMAND:
	default:
            break;
	}

        editor->line++;
    }
    editor->last_error = HED_ERR_NO_ERROR;
    return;
ERROR:
    if (editor->flags & HED_FLAG_VERBOSE_ERRORS)
	helpmod_reply(sender, returntype, "Ged: %s", hed_error_text(editor->last_error));
    else
	helpmod_reply(sender, returntype, "Ged: ?");
}

const char *hed_error_text(hed_error err)
{
    switch (err)
    {
    case HED_ERR_NO_ERROR:
        return "No error";
    case HED_ERR_NO_COMMAND:
	return "No command given";
    case HED_ERR_SYNTAX_ERROR:
	return "Syntax error";
    case HED_ERR_UNKNOWN_COMMAND:
	return "Unknown command";
    case HED_ERR_COMMAND_NOT_SUPPORTED:
	return "Unsupported command";
    case HED_ERR_INVALID_ARGUMENT:
	return "Invalid argument";
    case HED_ERR_LINE_TOO_LONG:
	return "Line exceeds 384 characters";
    case HED_ERR_BUFFER_FULL:
	return "Buffer full";
    case HED_ERR_CLIPBOARD_EMPTY:
	return "Nothing to paste";
    case HED_ERR_UNSAVED:
        return "File unsaved, q again to quit";
    default:
        return "Error, please contact strutsi";
    }
}

char* hed_add_line(helpmod_editor *editor, int position)
{
    hed_line **pos, *tmp;
    hed_clear_clipboard(editor);

    assert(position >= 0 && position < HED_BUFFER_LINES);
    assert(hed_line_count(editor) < HED_BUFFER_LINES);
    assert(editor->free_lines != NULL);

    editor->flags |= HED_FLAG_UNSAVED;

    for (pos = &editor->start;*pos != NULL && position;pos = &(*pos)->next, position--);

    tmp = editor->free_lines;
    editor->free_lines = editor->free_lines->next;

    tmp->next = *pos;
    tmp->line[0] = '\0';

    *pos = tmp;

    return tmp->line;
}

void hed_del_line(helpmod_editor *editor, int position)
{
    hed_line **ptr, **ptr_free, *tmp;
    assert(position >= 0 && position < hed_line_count(editor));

    editor->flags |= HED_FLAG_UNSAVED;

    for (ptr = &editor->start;position;ptr = &(*ptr)->next, position--);

    tmp = *ptr;
    *ptr = (*ptr)->next;

    for (ptr_free = &editor->clipboard;*ptr_free;ptr_free = &(*ptr_free)->next);

    tmp->next = NULL;
    *ptr_free = tmp;
}

char* hed_get_line(helpmod_editor *editor, int position)
{
    hed_line *ptr;
    assert(position >= 0 &&  position < hed_line_count(editor));

    for (ptr = editor->start;position--;ptr = ptr->next);
    return ptr->line;
}

void hed_clear_clipboard(helpmod_editor *editor)
{
    hed_line **ptr;
    for (ptr = &editor->clipboard;*ptr != NULL;ptr = &(*ptr)->next);
    (*ptr) = editor->free_lines;
    editor->free_lines = editor->clipboard;
    editor->clipboard = NULL;
}

void hed_paste(helpmod_editor *editor, int position)
{
    hed_line **ptr, **ptr_clipboard;

    assert(position >= 0 && position <= hed_line_count(editor));
    editor->flags |= HED_FLAG_UNSAVED;

    for (ptr = &editor->start;position;ptr = &(*ptr)->next, position--);
    for (ptr_clipboard = &editor->clipboard;*ptr_clipboard != NULL;ptr_clipboard = &(*ptr_clipboard)->next);

    *ptr_clipboard = *ptr;
    *ptr = editor->clipboard;

    editor->clipboard = NULL;
}
