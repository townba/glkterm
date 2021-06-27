#include "gtoption.h"
#include "glk.h"
#include "glkterm.h"

#ifdef LOCAL_NCURSESW

#include <curses.h>
#include <stdlib.h>
#include <wchar.h>
#include <memory.h>
/* only needed for local_* prototypes */
#include "glk.h"

int local_addnstr(const glichar *wstr, int n)
{
#ifdef OPT_WIDE_CHARACTERS
    int i;
    int status = OK;
    size_t size = 0;
    char *buffer = (char *)calloc(MB_CUR_MAX + 1, sizeof(char));
    mbstate_t state;
    
    for (i = 0; status != ERR && i < n && wstr[i] != '\0'; ++i) {
        memset(&state, '\0', sizeof(state));
        size = wcrtomb(buffer, wstr[i], &state);
        if (size == (size_t) -1) {
            status = ERR;
        }
	else {
            addnstr(buffer, size);
        }
    }
    
    free(buffer);
    return status;
#else
    return addnstr(wstr, n);
#endif
}

int local_addstr(const glichar *wstr)
{
    return local_addnstr(wstr, GLISTRLEN(wstr));
}

int local_get_wch(wint_t *ch)
{
#ifdef OPT_WIDE_CHARACTERS
    int i;
    int status = 0;
    char *buffer = (char *)calloc(MB_CUR_MAX + 1, sizeof(char));
    mbstate_t state;
    
    for (i = 0; status != ERR && i < MB_CUR_MAX; ++i) {
        status = getch();
        if (status == ERR) {
            break;
        }
        if ((unsigned)status >= 0x100) {
            /* returned a function key */
            *ch = status;
            status = KEY_CODE_YES;
            free(buffer);
            return status;
        }
        buffer[i] = status;
        memset(&state, '\0', sizeof(state));
        status = mbrlen(buffer, i + 1, &state);
        switch (status) {
            case -2: /* continue reading */
                status = (i + 1 < MB_CUR_MAX) ? OK : ERR;
                break;
            case -1: /* abort */
                status = ERR;
                break;
            default: /* got a character */
                memset(&state, '\0', sizeof(state));
                status = mbrtowc((wchar_t *)ch, buffer, i + 1, &state);
                status = OK;
                /* This is just to break the loop */
                i = MB_CUR_MAX;
                break;
        }
        
    }

    free(buffer);
    return status;
#else
    int ret = getch();
    if (ret != ERR) {
        *ch = ret;
        if ((unsigned)ret >= 0x100) {
            /* returned a function key */
            ret = KEY_CODE_YES;
        }
        else {
            ret = OK;
        }
    }
    return ret;
#endif
}

int local_mvaddnstr(int y, int x, const glichar *wstr, int n)
{
    move(y, x);
    return local_addnstr(wstr, n);
}

#else /* LOCAL_NCURSESW */

#define _XOPEN_SOURCE_EXTENDED /* ncursesw *wch* and *wstr* functions */
#include <curses.h>

int local_addnstr(const glichar *wstr, int n)
{
#ifdef OPT_WIDE_CHARACTERS
    return addnwstr(wstr, n);
#else
    return addnstr(wstr, n);
#endif
}

int local_addstr(const glichar *wstr)
{
#ifdef OPT_WIDE_CHARACTERS
    return addwstr(wstr);
#else
    return addstr(wstr);
#endif
}

int local_get_wch(wint_t *ch)
{
#ifdef OPT_WIDE_CHARACTERS
    return get_wch(ch);
#else
    int ret = getch();
    if (ret != ERR) {
        *ch = ret;
        if ((unsigned)ret >= 0x100) {
            /* returned a function key */
            ret = KEY_CODE_YES;
        }
        else {
            ret = OK;
        }
    }
    return ret;
#endif
}

int local_mvaddnstr(int y, int x, const glichar *wstr, int n)
{
#ifdef OPT_WIDE_CHARACTERS
    return mvaddnwstr(y, x, wstr, n);
#else
    return mvaddnstr(y, x, wstr, n);
#endif
}

#endif /* LOCAL_NCURSESW */
