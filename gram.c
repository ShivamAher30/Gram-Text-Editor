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

// Defines
#define CTRL_KEY(k) ((k) & (0x1f))
#define GRAM_VERSION "1.0.0.0"
// struct
struct editorConfig
{
    int cx, cy;

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

char readkey()
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
    return c;
}

// Input
void editormovecursor(char c)
{
    switch (c)
    {
    case 'w':
        E.cy--;
        break;
    case 'a':
        E.cx--;
        break;
    case 's':
        E.cy++;
        break;
    case 'd':
        E.cx++;
        break;
    }
}
void editorKeyProcess()
{
    char c = readkey();
    switch (c)
    {
    case 'q':
        exit(0);

        break;
    case 'w':
    case 'a':
    case 's':
    case 'd':
        editormovecursor(c);
        break;
    }
}

// Output
void editordrawtilde(struct abf *ab)
{
    for (int i = 0; i < E.screenrows; i++)
    {
        if (i == E.screenrows / 3)
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
}
#define ABUF_INIT {NULL, 0}

void editorrefreshscreen()
{
    struct abf ab = ABUF_INIT;
    abappend("\x1b[?25l", 6, &ab);
    // The following commmand is used to reposition the cursor at top of the terminal
    abappend("\x1b[H", 3, &ab);
    editordrawtilde(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abappend(buf, strlen(buf), &ab);

    abappend("\x1b[?25h", 6, &ab);
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

// init

void initeditor()
{
    E.cx = 0;
    E.cy = 0;

    if (getwindowsize(&E.screenrows, &E.screencolumns) == -1)
    {
        die("Getwindowsize");
    }
}

int main()
{
    enablerawMode();
    initeditor();

    char c;
    while (1)
    {
        editorrefreshscreen();
        editorKeyProcess();
    }
}
