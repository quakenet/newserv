/* H/G text editor */
#ifndef HED_H
#define HED_H

/* Maximum number of lines in buffer */
#define HED_BUFFER_LINES 16

#define HELPMOD_TEXT_DIR "./helpmod2/text"
#define HED_FILENAME_LENGTH 64

/* Forward declarations */
struct hchannel_struct;
struct huser_struct;

typedef enum
{
    HED_COMMAND,
    HED_INPUT_INSERT,
    HED_INPUT_APPEND
} hed_state;

typedef enum
{
    HED_ERR_NO_ERROR,
    HED_ERR_NO_COMMAND,
    HED_ERR_SYNTAX_ERROR,
    HED_ERR_UNKNOWN_COMMAND,
    HED_ERR_COMMAND_NOT_SUPPORTED,
    HED_ERR_INVALID_ARGUMENT,
    HED_ERR_LINE_TOO_LONG,
    HED_ERR_BUFFER_FULL,
    HED_ERR_CLIPBOARD_EMPTY,
    HED_ERR_UNSAVED
} hed_error;

typedef enum
{
    HED_FLAG_VERBOSE_ERRORS = 1 << 0,
    HED_FLAG_UNSAVED        = 1 << 1
} hed_flag;

#define HED_FLAGS_DEFAULT (0)

typedef struct helpmod_editor_line_struct
{
    char line[384];
    struct helpmod_editor_line_struct *next;
} hed_line;

typedef struct helpmod_editor_struct
{
    char filename[HED_FILENAME_LENGTH];
    hed_line buffers[HED_BUFFER_LINES];

    hed_line *start;
    hed_line *free_lines;
    hed_line *clipboard;

    int line;
    int flags;

    struct huser_struct *user;

    hed_state state;
    hed_error last_error;

    struct helpmod_editor_struct *next;
} helpmod_editor;

extern helpmod_editor *helpmod_editors;

helpmod_editor *hed_open(struct huser_struct *, const char *);
helpmod_editor *hed_close(helpmod_editor *);
helpmod_editor *hed_write(helpmod_editor *);

int hed_byte_count(helpmod_editor*);
int hed_line_count(helpmod_editor*);
int hed_is_valid_filename(const char *);

/* Get by filename */
helpmod_editor *hed_get(const char *);

void hed_command (struct huser_struct *sender, channel* returntype, char* ostr, int argc, char *argv[]);

const char *hed_error_text(hed_error);

char *hed_add_line(helpmod_editor *, int);
void hed_del_line(helpmod_editor *, int);
char *hed_get_line(helpmod_editor *, int);

void hed_clear_clipboard(helpmod_editor *);
void hed_paste(helpmod_editor *, int);

#endif
