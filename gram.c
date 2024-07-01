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
// define functions
void editorinsertchar(int c);
void editorsave();
// struct
typedef struct erow
{
    int size;
    int rsize;
    char *chars;
    char *render;

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
};
struct editorConfig E;

// buffer struct
struct abf
{
    char *b;
    int len;
};
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
        if (row && E.cx < row->size)
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
    int c = readkey();
    switch (c)
    {
    case CTRL_KEY('s'):
        editorsave();
        break;
    case '\r':
    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
        break;
    case 'q':
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
void editordrawtilde(struct abf *ab)
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

            abappend(&E.row[filerow].chars[E.coloff], len, ab);
            abappend("\x1b[K", 3, ab);

            abappend("\r\n", 2, ab);
        }
    }
}
#define ABUF_INIT {NULL, 0}
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
}

void editordrawstatusbar(struct abf *ab)
{
    abappend("\x1b[7m", 4, ab);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines ", E.filename ? E.filename : "[No File Opened]", E.numrows);
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
    if (msglen && time(NULL) - E.status_msg_time < 7.5)
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
    editordrawtilde(&ab);
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
void rowAppend(char *s, ssize_t len)
{
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    int at = E.numrows;
    E.row[at].size = len + 1;

    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    E.numrows++;
    editorUpdateRow(&E.row[at]);
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
    editorRowInsertChar(&E.row[E.cy + 1], E.cx, c);
    E.cx++;
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

// char *editorRowsToString(int *buflen)
// {
//     int totlen = 0;
//     for (int i = 0; i < E.numrows; i++)
//     {
//         totlen += E.row[i].size + 1;
//     }
//     char *p = malloc(sizeof(char) * totlen);
//     char *buf = p;
//     int index = 0;
//     for (int i = 0; i < E.numrows; i++)
//     {
//         memcpy(&buf[index], E.row[i].chars, E.row[i].size);
//         buf[index + E.row[i].size] = '\n';

//         index += E.row[i].size + 1;
//     }
//     *buflen = totlen;
//     return p;
// }
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
                return;
            }
        }
        close(fd);
    }
    free(buf);
}
// init

void initeditor()
{
    E.cx = 0;
    ;
    E.cy = 0;
    E.numrows = 0;
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