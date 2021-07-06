#define _XOPEN_SOURCE_EXTENDED /* ncursesw *wch* and *wstr* functions */
#include <curses.h>
#include "gtoption.h"
#include "glk.h"
#include "glkterm.h"

#define REPLACEMENT_CHAR ((wchar_t)0xFFFD)

int gli_curses_addch_uni(glui32 ch)
{
    if (ch > WCHAR_MAX) ch = REPLACEMENT_CHAR;
    wchar_t wch = ch;
    return addnwstr(&wch, 1);
}

int gli_curses_addstr(const char *str)
{
    return addstr(str);
}

glui32 gli_curses_getch_uni()
{
    wint_t wint;
    if (get_wch(&wint) == ERR) return ERR;
    return wint;
}

int gli_curses_mvaddch_uni(int y, int x, glui32 ch)
{
    if (ch > WCHAR_MAX) ch = REPLACEMENT_CHAR;
    return mvaddch(y, x, ch);
}
