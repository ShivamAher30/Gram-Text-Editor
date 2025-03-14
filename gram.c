// Include Header Files

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>

// Defines
#define CTRL_KEY(k) ((k) & (0x1f))
#define GRAM_VERSION "1.0.0.0"
#define KILO_QUIT_TIMES 3
#define ABUF_INIT {NULL, 0}

enum editorKey
{
    BACKSPACE = 127,
    ARROW_LEFT = 2000,
    ARROW_RIGHT,
    ARROW_DOWN,
    ARROW_UP,
    PAGE_UP,
    PAGE_DOWN,
    DEL_KEY
};
enum editorHighlight
{
    HL_NORMAL = 0,
    HL_NUMBER,
    HL_KEYWORD1,
    HL_KEYWORD2,
};
// struct
typedef struct erow
{
    int size;
    int rsize;
    char *chars;
    char *render;
    unsigned char *hl;

} erow;
struct editorConfig
{
    int cx, cy;
    int numrows;
    erow *row;
    int rowoff;
    int coloff;
    char *filename;
    int screenrows;
    int screencolumns;
    struct termios originalstruct;
    char statusmsg[80];
    time_t status_msg_time;
    int dirty;
};
struct editorConfig E;

// buffer struct
struct abf
{
    char *b;
    int len;
};
// define functions
void editorinsertchar(int c);
void editorsave();
void editorsetstatusmsg(const char *fmt, ...);
void editordelchar();
void JumptoLine(int direction);
void editorUpdateSyntax(erow *row);
int editorSyntaxtocolor(int hl);

// append Buffer

void abappend(char *s, int len, struct abf *ptr)
{
    char *new = realloc(ptr->b, ptr->len + len);
    if (new == NULL)
        return;
    memcpy(&new[ptr->len], s, len);
    ptr->b = new;
    ptr->len += len;
}
void abFree(struct abf *ptr)
{
    // free(ptr);
}
//  terminal
int getwindowsize(int *row, int *col)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        return -1;
    }
    else
    {
        *row = ws.ws_row;
        *col = ws.ws_col;

        return 1;
    }
}
void die(const char *s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);

    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}
void disablerawMode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.originalstruct) == -1)
    {
        die("tcsetattr");
    }
}
void enablerawMode()
{
    struct termios raw = E.originalstruct;
    atexit(disablerawMode);
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cflag |= ~(CS8);
    raw.c_oflag &= ~(OPOST);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    {
        die("tcsetattr");
    }
}

int readkey()
{
    int nread;
    char c;
    while ((nread = (read(STDIN_FILENO, &c, 1))) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
        {
            die("read");
        }
    }
    if (c == '\x1b')
    {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';

        // To complete our low-level terminal code, we need to detect a few more special keypresses that use escape sequences, like the arrow keys did. We’ll start with the Page Up and Page Down keys. Page Up is sent as <esc>[5~ and Page Down is sent as <esc>[6~.

        if (seq[0] == '[')
        {
            if (seq[1] >= '0' && seq[1] <= '9')
            {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return '\x1b';
                if (seq[2] == '~')
                {
                    switch (seq[1])
                    {
                    case '5':
                        return PAGE_UP;
                        break;

                    case '6':
                        return PAGE_DOWN;
                    }
                }
            }
            else
            {
                switch (seq[1])
                {
                case 'A':
                    return ARROW_UP;
                    break;

                case 'B':
                    return ARROW_DOWN;
                    break;

                case 'C':
                    return ARROW_RIGHT;
                    break;

                case 'D':
                    return ARROW_LEFT;
                    break;

                default:
                    break;
                }
            }
        }
    }
    return c;
}

// Input
void editormovecursor(int c)
{
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy + 1];
    switch (c)
    {
    case ARROW_UP:
        if (E.cy != 0)
        {
            E.cy--;
        }
        break;
    case ARROW_LEFT:
        if (E.cx != 0)
            E.cx--;
        break;
    case ARROW_DOWN:
        if (E.cy <= E.numrows - 1)
            E.cy++;
        break;
    case ARROW_RIGHT:
        if ( E.cx-9 < row->size)
        {
            E.cx++;
        }
        else
        {
            E.cy++;
            E.cx = 0;
        }
        break;
    }
    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy + 1];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen)
    {
        E.cx = rowlen;
    }
}

void editorKeyProcess()
{
    static int quit_times = 3;
    int c = readkey();

    // JumptoLine(c);
    switch (c)
    {
    case CTRL_KEY('a'):
        JumptoLine(1);
        break;
    case CTRL_KEY('d'):
        JumptoLine(0);
        break;
    case CTRL_KEY('s'):
        editorsave();
        break;
    case '\r':
    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
        editordelchar();

        break;
    case CTRL_KEY('t'):
        if (E.dirty && quit_times > 0)
        {
            editorsetstatusmsg("WARNING!!! File has unsaved changes. "
                               "Press Ctrl-T %d more times to quit.",
                               quit_times);
            quit_times--;
            return;
        }
        exit(0);

        break;
    case PAGE_UP:
    case PAGE_DOWN:
        // We create a code block with that pair of braces so that we’re allowed to declare the times variable. (You can’t declare variables directly inside a switch statement.)
        {
            if (c == PAGE_UP)
            {
                E.cy = E.rowoff;
            }
            else if (c == PAGE_DOWN)
            {
                E.cy = E.rowoff + E.screenrows - 1;
                if (E.cy > E.numrows)
                    E.cy = E.numrows;
            }
            int times = E.screenrows;
            while (times--)
            {
                editormovecursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
        }

    case ARROW_UP:
    case ARROW_LEFT:
    case ARROW_DOWN:
    case ARROW_RIGHT:
        editormovecursor(c);
        break;
    case CTRL_KEY('l'):
    case '\x1b':
        break;
    default:
        editorinsertchar(c);
        break;
    }
}

// Output
void scrolleditor()
{
    if (E.cy < E.rowoff)
    {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.screenrows + E.rowoff)
    {
        E.rowoff = E.cy - E.screenrows + 1;
    }
    if (E.cx < E.coloff)
    {
        E.coloff = E.cx;
    }
    if (E.cx >= E.coloff + E.screencolumns)
    {
        E.coloff = E.cx - E.screencolumns + 1;
    }
}
void editortildedraw(struct abf *ab)
{
    for (int i = 0; i < E.screenrows; i++)
    {
        int filerow = i + E.rowoff;
        if (filerow >= E.numrows)
        {

            if (i == E.screenrows / 3 && E.numrows == 0)
            {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome), "Gram Text Editor %s ", GRAM_VERSION);
                if (welcomelen > E.screencolumns)
                {
                    welcomelen = E.screencolumns;
                }
                int padding = (E.screencolumns - welcomelen) / 2;
                if (padding)
                {
                    abappend("~", 1, ab);
                    padding--;
                }
                while (padding--)
                {

                    abappend(" ", 1, ab);
                }
                abappend(welcome, welcomelen, ab);
                abappend("\x1b[K", 3, ab);
                abappend("\r\n", 2, ab);
            }

            else if (i < E.screenrows)
            {
                abappend("~", 1, ab);
                abappend("\x1b[K", 3, ab);
                abappend("\r\n", 2, ab);
            }
        }
        else
        {
            int len = E.row[filerow].size - E.coloff;
            if (len < 0)
                len = 0;

            if (len > E.screencolumns)
            {
                len = E.screencolumns;
            }
            if (i > 1)
            {
                char *linenum;
                linenum = malloc(sizeof(char) * 4);
                char value = i > E.cy ? i - E.cy - 1 : E.cy - i + 1;
                if (E.rowoff > 0)
                {
                    value -= E.rowoff;
                    if (value < 0)
                    {
                        value *= -1;
                    }
                }

                if (value >= 10)
                {

                    snprintf(linenum, 4, "+%d|", value % 10);
                }
                else
                {
                    snprintf(linenum, 4, " %d|", value);
                };

                abappend(linenum, 3, ab);
            }
            // char *linetoprint = &E.row[filerow].render[E.coloff];
            char *linetoprint = &E.row[filerow].chars[E.coloff];
            unsigned char *hl = &E.row[filerow].hl[E.coloff];
            int currentcolor = -1;
            for (int j = 0; j < len; j++)
            {

                if (hl[j] == HL_NORMAL)
                {
                    if (currentcolor != -1)
                    {
                        abappend("\x1b[39m", 5, ab);
                        currentcolor = -1;
                    }
                    abappend(&linetoprint[j], 1, ab);
                }
                else
                {
                    int color = editorSyntaxtocolor(hl[j]);
                    char buf[16];
                    if (color != currentcolor)
                    {
                        currentcolor = color;
                        int clen = snprintf(buf, 16, "\x1b[%dm", color);
                        // int clen = snprintf(buf, 16, "Thsiis%d", color);
                        abappend(buf, clen, ab);
                    }
                    abappend(&linetoprint[j], 1, ab);
                }
            }
            abappend("\x1b[39m", 5, ab);

            // abappend(&(linetoprint[E.coloff]), len, ab);

            abappend("\x1b[K", 3, ab);

            abappend("\r\n", 2, ab);
        }
    }
}
void editorUpdateRow(erow *row)
{
    free(row->render);
    row->render = malloc(row->size + 1);
    int j;
    int idx = 0;
    for (j = 0; j < row->size; j++)
    {
        row->render[idx++] = row->chars[j];
    }
    row->render[idx] = '\0';
    row->rsize = idx;
    editorUpdateSyntax(row);
}

void editordrawstatusbar(struct abf *ab)
{
    abappend("\x1b[7m", 4, ab);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.filename ? E.filename : "[No File Opened]", E.numrows, E.dirty ? "[MODIFIED]" : " ");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.numrows);

    if (len > E.screencolumns)
        len = E.screencolumns;
    abappend(status, len, ab);
    while (len < E.screencolumns)
    {
        if (rlen + len >= E.screencolumns)
        {
            abappend(rstatus, rlen, ab);
            break;
        }
        else
        {

            abappend(" ", 1, ab);
            len++;
        }
    }

    abappend("\x1b[m", 3, ab);
    abappend("\r\n", 2, ab);
}
void editormessagebar(struct abf *ab)
{
    abappend("\x1b[K", 3, ab);
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencolumns)
    {
        msglen = E.screencolumns;
    }
    if (msglen && time(NULL) - E.status_msg_time < 5)
    {
        abappend(E.statusmsg, msglen, ab);
    }
}
void editorrefreshscreen()
{
    scrolleditor();
    struct abf ab = ABUF_INIT;
    abappend("\x1b[?25l", 6, &ab);
    // The following commmand is used to reposition the cursor at top of the terminal
    abappend("\x1b[H", 3, &ab);
    editortildedraw(&ab);
    editordrawstatusbar(&ab);
    editormessagebar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.cx - E.coloff) + 1);
    abappend(buf, strlen(buf), &ab);

    abappend("\x1b[?25h", 6, &ab);
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}
void editorsetstatusmsg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);

    va_end(ap);
    E.status_msg_time = time(NULL);
}

// Row Operations
// void editorInsertRow
void rowAppend(char *s, ssize_t len)
{
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    int at = E.numrows;
    E.row[at].size = len + 1;

    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.row[at].rsize = 0;
    E.row[at].hl = NULL;
    E.row[at].render = NULL;
    E.numrows++;
    editorUpdateRow(&E.row[at]);
}
void editorfreerow(erow *row)
{
    free(row->chars);
    free(row->render);
    free(row->hl);
}
void editordelrow(int at)

{
    if (at < 0 || at > E.numrows)
    {
        return;
    }
    erow *row = &E.row[at];
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow *) * (E.numrows - at - 1));
    E.numrows--;
    E.dirty++;
}
void editorrowappendstring(erow *row, char *s, size_t len)
{
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}
void editorRowInsertChar(erow *row, int at, int c)
{
    if (at < 0 || at > row->size)
    {
        at = row->size;
    }
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->chars[at] = c;
    row->size++;
}
void editorinsertchar(int c)
{
    if (E.cy == E.numrows)
    {
        rowAppend("", 0);
    }
    editorRowInsertChar(&E.row[E.cy + 1], E.cx - 3, c);
    E.cx++;
    E.dirty++;
}
void editorrowdelchar(erow *row, int at)
{
    if (at < 0 || at > row->size)
    {
        return;
    }
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty--;
}

void editordelchar()

{
    if (E.cy == E.numrows)
    {
        return;
    }
    if (E.cx == 0 && E.cy == 0)
    {
        return;
    }
    erow *row = &E.row[E.cy + 1];
    if (E.cx > row->size)
    {
        return;
    }
    if (E.cx > 0)
    {
        E.cx--;
        editorrowdelchar(row, E.cx);
    }
    else
    {
        E.cx = E.row[E.cy].size;
        editorrowappendstring(&E.row[E.cy], row->chars, row->size);
        editordelrow(E.cy + 1);
        E.cy--;
    }
}

// file i/o
void editoropen(char *filename)
{
    free(E.filename);
    E.filename = strdup(filename);

    FILE *fl = fopen(filename, "r");
    if (!fl)
        die("fopen");
    char *line = NULL;
    ssize_t linecap = 0;
    ssize_t linelen = 0;
    while ((linelen = getline(&line, &linecap, fl)) != -1)
    {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
        {
            linelen--;
        }
        rowAppend(line, linelen);
    }

    free(line);
    fclose(fl);
}
char *editorRowsToString(int *buflen)
{
    int totlen = 0;
    for (int i = 0; i < E.numrows; i++)
    {
        totlen += E.row[i].size;
    }
    char *buf = malloc(totlen);
    char *p = buf;
    for (int i = 0; i < E.numrows; i++)
    {
        memcpy(p, E.row[i].chars, E.row[i].size - 1);
        p += E.row[i].size - 1;
        *p = '\n'; // Add a newline after each row
        p++;
    }
    *buflen = totlen;
    return buf;
}
// Jump to Line
void JumptoLine(int direction)
{

    editorsetstatusmsg("%s", direction == 1 ? "jump up to " : "jump down to");

    // if direction is 1 jump upwards else downwards
    int num = readkey();
    if (num >= 48 && num <= 57)
    {
        if (num == 'e')
        {
            return;
        }
        num -= 48;
        if (direction == 0 && (E.cy + num) % E.screenrows < E.screenrows)
        {
            editorsetstatusmsg("+%d", num);
            E.cy += num;
            return;
        }
        if (direction == 1 && E.cy - num > 0)
        {
            editorsetstatusmsg("-%d", num);
            E.cy -= num;
        }
        else
        {
            editorsetstatusmsg("Invalid Line Position");
        }
    }
}
void editorsave()
{
    if (E.filename == NULL)
        return;
    int len;
    char *buf = editorRowsToString(&len);

    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1)
    {
        if (ftruncate(fd, len) != -1)
        {
            if (write(fd, buf, len) == len)
            {
                close(fd);
                free(buf);
                E.dirty = 0;
                editorsetstatusmsg("%d-bytes saved to file !!!- %s ", len, E.filename);

                E.status_msg_time = time(NULL);

                return;
            }
        }
        close(fd);
    }
    free(buf);
}

// Syntax Highlighting
int is_separator(int c)
{
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}
void editorUpdateSyntax(erow *row)
{
    row->hl = realloc(row->hl, row->rsize);
    memset(row->hl, HL_NORMAL, row->rsize);
    int i = 0;
    char *keywords[] = {
        "#define",
        "switch",
        "if",
        "while",
        "for",
        "break",
        "continue",
        "return",
        "else",
        "struct",
        "union",
        "typedef",
        "static",
        "enum",
        "class",
        "case",
        "int|",
        "long|",
        "double|",
        "float|",
        "char|",
        "unsigned|",
        "signed|",
        "void|",
        "#include",
        NULL};

    int prev_sep = 1;
    while (i < row->size)
    {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;
        if (isdigit(c) && (prev_sep || prev_hl == HL_NUMBER))
        {
            row->hl[i] = HL_NUMBER;
            i++;
            prev_sep = 0;
            continue;
        }
        if (prev_sep)
        {
            int j;
            for (j = 0; keywords[j]; j++)
            {
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen - 1] == '|';
                if (kw2)
                    klen--;
                if (!strncmp(&row->render[i], keywords[j], klen) &&
                    is_separator(row->render[i + klen]))
                {
                    memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                    i += klen;
                    break;
                }
            }
            if (keywords[j] != NULL)
            {
                prev_sep = 0;
                continue;
            }
        }
        prev_sep = is_separator(c);
        i++;
    }
}
int editorSyntaxtocolor(int hl)
{
    switch (hl)
    {
    case HL_NUMBER:
        return 31;
        break;
    case HL_KEYWORD1:
        return 33;
    case HL_KEYWORD2:
        return 32;

    default:
        return 37;
        break;
    }
}

// init

void initeditor()
{
    E.dirty = 0;
    E.cx = 0;
    E.cy = 1;
    rowAppend(" ", 1);
    E.row[0].hl =NULL; 
    E.numrows = 1;
    E.row = NULL;
    E.rowoff = 0;
    E.coloff = 0;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.status_msg_time = 0;

    if (getwindowsize(&E.screenrows, &E.screencolumns) == -1)
    {
        die("Getwindowsize");
    }
    E.screenrows -= 1;
}

int main(int argc, char *argv[])
{
    enablerawMode();
    initeditor();
    if (argc > 1)
    {
        editoropen(argv[1]);
    }
    editorsetstatusmsg("Welcome to the code Editor");

    char c;
    while (1)
    {
        editorrefreshscreen();
        editorKeyProcess();
    }
}