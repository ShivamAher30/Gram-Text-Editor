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

// Defines
#define CTRL_KEY(k) ((k) & (0x1f))
#define GRAM_VERSION "1.0.0.0"

enum editorKey
{
    ARROW_LEFT = 2000,
    ARROW_RIGHT,
    ARROW_DOWN,
    ARROW_UP,
    PAGE_UP,
    PAGE_DOWN,
    DEL_KEY
};

// struct
typedef struct erow
{
    int size;
    char *chars;

} erow;
struct editorConfig
{
    int cx, cy;
    int numrows;
    erow *row;
    int rowoff;
    int screenrows;
    int screencolumns;
    struct termios originalstruct;
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
        if (E.cx - 1 < E.screencolumns)
            E.cx++;
        break;
    }
}
void editorKeyProcess()
{
    int c = readkey();
    switch (c)
    {
    case 'q':
        exit(0);

        break;
    case PAGE_DOWN:
    case PAGE_UP:
        // We create a code block with that pair of braces so that we’re allowed to declare the times variable. (You can’t declare variables directly inside a switch statement.)
        {
            int time = E.screenrows;
            while (time--)
            {
                editormovecursor(c == PAGE_DOWN ? ARROW_DOWN : ARROW_UP);
            }
        }
        break;
    case ARROW_UP:
    case ARROW_LEFT:
    case ARROW_DOWN:
    case ARROW_RIGHT:
        editormovecursor(c);
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

            else if (i < E.screenrows - 1)
            {
                abappend("~", 1, ab);
                abappend("\x1b[K", 3, ab);
                abappend("\r\n", 2, ab);
            }
            else
            {
                abappend("~", 1, ab);
            }
        }
        else
        {
            int len = E.row[filerow].size;
            if (len > E.screencolumns)
            {
                len = E.screencolumns;
            }
            abappend(E.row[filerow].chars, len, ab);
            abappend("\x1b[K", 3, ab);
            abappend("\r\n", 2, ab);
        }
    }
}
#define ABUF_INIT {NULL, 0}

void editorrefreshscreen()
{
    scrolleditor();
    struct abf ab = ABUF_INIT;
    abappend("\x1b[?25l", 6, &ab);
    // The following commmand is used to reposition the cursor at top of the terminal
    abappend("\x1b[H", 3, &ab);
    editordrawtilde(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", 0 , 0);
    abappend(buf, strlen(buf), &ab);

    abappend("\x1b[?25h", 6, &ab);
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}
// Row Operations
void rowAppend(char *s, int len)
{
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    int at = E.numrows;
    E.row[at].size = len;

    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.numrows++;
}
// file i/o
void editoropen(char *filename)
{
    FILE *fl = fopen(filename, "r");
    if (!fl)
        die("fopen");
    char *line = NULL;
    ssize_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fl)) != -1)
    {
        while (linelen > 0 && line[linelen - 1] == '\n' || line[linelen - 1] == '\r')
        {
            linelen--;
        }
        rowAppend(line, linelen);
    }

    free(line);
    fclose(fl);
}

// init

void initeditor()
{
    E.cx = 0;
    E.cy = 0;
    E.numrows = 0;
    E.row = NULL;
    E.rowoff = 0;

    if (getwindowsize(&E.screenrows, &E.screencolumns) == -1)
    {
        die("Getwindowsize");
    }
}

int main(int argc, char *argv[])
{
    enablerawMode();
    initeditor();
    if (argc > 1)
    {
        editoropen(argv[1]);
    }

    char c;
    while (1)
    {
        editorrefreshscreen();
        editorKeyProcess();
    }
}
