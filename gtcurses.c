#define _XOPEN_SOURCE_EXTENDED /* ncursesw *wch* and *wstr* functions */
#include <curses.h>
#include "gtoption.h"
#include "glk.h"
#include "glkterm.h"

int gli_curses_addch(const unsigned ch)
{
    wchar_t wch = ch;
    return addnwstr(&wch, 1);
}

int gli_curses_addstr(const char *str)
{
    return addstr(str);
}

int gli_curses_mvaddch(int y, int x, const unsigned ch)
{
    return mvaddch(y, x, ch);
}
