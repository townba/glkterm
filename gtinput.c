/* gtinput.c: Key input handling
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html
*/

#include "gtoption.h"
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "glk.h"
#include "glkterm.h"
#include "gtw_grid.h"
#include "gtw_buf.h"

/* TODO(townba): Down key dies in Bureaucracy form. */

typedef void (*command_fptr)(window_t *win, glui32);

typedef struct command_struct {
    command_fptr func;
    int arg;
} command_t;

/* The idea is that, depending on what kind of window has focus and
    whether it is set for line or character input, various keys are
    bound to various command_t objects. A command_t contains a function
    to call to handle the key, and an argument. (This allows one
    function to handle several variants of a command -- for example,
    gcmd_buffer_scroll() handles scrolling both up and down.) If the
    argument is -1, the function will be passed the actual key hit.
    (This allows a single function to handle a range of keys.) 
   Key values may be 0 to 255, or any of the special KEY_* values
    defined in curses.h. */

/* Keys which are always meaningful. */
static command_t *commands_always(const gkey_t *gkey)
{
    static command_t cmdchangefocus = { gcmd_win_change_focus, 0 };
    static command_t cmdrefresh = { gcmd_win_refresh, 0 };
    static command_t cmdresize = { gcmd_win_resize, 0 };

    switch (gkey->curses) {
        case '\t': 
            return &cmdchangefocus;
        case '\014': /* ctrl-L */
            return &cmdrefresh;
#ifdef KEY_RESIZE
        case KEY_RESIZE:
            return &cmdresize;
#endif
    }
    
    return NULL;
}

/* Keys which are always meaningful in a text grid window. */
static command_t *commands_textgrid(const gkey_t *gkey)
{
    return NULL;
}

/* Keys for char input in a text grid window. */
static command_t *commands_textgrid_char(const gkey_t *gkey)
{
    static command_t cmdv = { gcmd_grid_accept_key, -1 };

    return &cmdv;
}

/* Keys for line input in a text grid window. */
static command_t *commands_textgrid_line(window_textgrid_t *dwin, const gkey_t *gkey)
{
    static command_t cmdacceptline = { gcmd_grid_accept_line, 0 };
    static command_t cmdacceptlineterm = { gcmd_grid_accept_line, -1 };
    static command_t cmdinsert = { gcmd_grid_insert_key, -1 };
    static command_t cmdmoveleft = { gcmd_grid_move_cursor, gcmd_Left };
    static command_t cmdmoveright = { gcmd_grid_move_cursor, gcmd_Right };
    static command_t cmdmoveleftend = { gcmd_grid_move_cursor, gcmd_LeftEnd };
    static command_t cmdmoverightend = { gcmd_grid_move_cursor, gcmd_RightEnd };
    static command_t cmddelete = { gcmd_grid_delete, gcmd_Delete };
    static command_t cmddeletenext = { gcmd_grid_delete, gcmd_DeleteNext };
    static command_t cmdkillinput = { gcmd_grid_delete, gcmd_KillInput };
    static command_t cmdkillline = { gcmd_grid_delete, gcmd_KillLine };

    if ((gkey->curses >= 0x20 && gkey->curses < 0x7F) ||
        (gkey->curses >= 0xA0 && gkey->curses <= 0xFF)) {
        return &cmdinsert;
    }
    switch (gkey->curses) {
        case KEY_ENTER:
        case '\012': /* ctrl-J */
        case '\015': /* ctrl-M */
            return &cmdacceptline;
        case KEY_LEFT:
        case '\002': /* ctrl-B */
            return &cmdmoveleft;
        case KEY_RIGHT:
        case '\006': /* ctrl-F */
            return &cmdmoveright;
        case KEY_HOME:
        case '\001': /* ctrl-A */
            return &cmdmoveleftend;
        case KEY_END:
        case '\005': /* ctrl-E */
            return &cmdmoverightend;
        case '\177': /* delete */
        case '\010': /* backspace */
        case KEY_BACKSPACE:
        case KEY_DC:
            return &cmddelete;
        case '\004': /* ctrl-D */
            return &cmddeletenext;
        case '\013': /* ctrl-K */
            return &cmdkillline;
        case '\025': /* ctrl-U */
            return &cmdkillinput;

        case '\033': /* escape */
            if (dwin->intermkeys & 0x10000)
                return &cmdacceptlineterm;
            break;
#ifdef KEY_F
        case KEY_F(1):
            if (dwin->intermkeys & (1<<1))
                return &cmdacceptlineterm;
            break;
        case KEY_F(2):
            if (dwin->intermkeys & (1<<2))
                return &cmdacceptlineterm;
            break;
        case KEY_F(3):
            if (dwin->intermkeys & (1<<3))
                return &cmdacceptlineterm;
            break;
        case KEY_F(4):
            if (dwin->intermkeys & (1<<4))
                return &cmdacceptlineterm;
            break;
        case KEY_F(5):
            if (dwin->intermkeys & (1<<5))
                return &cmdacceptlineterm;
            break;
        case KEY_F(6):
            if (dwin->intermkeys & (1<<6))
                return &cmdacceptlineterm;
            break;
        case KEY_F(7):
            if (dwin->intermkeys & (1<<7))
                return &cmdacceptlineterm;
            break;
        case KEY_F(8):
            if (dwin->intermkeys & (1<<8))
                return &cmdacceptlineterm;
            break;
        case KEY_F(9):
            if (dwin->intermkeys & (1<<9))
                return &cmdacceptlineterm;
            break;
        case KEY_F(10):
            if (dwin->intermkeys & (1<<10))
                return &cmdacceptlineterm;
            break;
        case KEY_F(11):
            if (dwin->intermkeys & (1<<11))
                return &cmdacceptlineterm;
            break;
        case KEY_F(12):
            if (dwin->intermkeys & (1<<12))
                return &cmdacceptlineterm;
            break;
#endif /* KEY_F */
    }
    
    /* Non-Latin-1 glyphs valid in this locale */
    if (gkey->key32 > 0xFF && GLIISPRINT(glui32_to_glichar(gkey->key32)))
        return &cmdinsert;
    
    return NULL;
}

/* Keys which are always meaningful in a text buffer window. Note that
    these override character input, which means you can never type ctrl-Y
    or ctrl-V in a textbuffer, even though you can in a textgrid. The Glk
    API doesn't make this distinction. Damn. */
static command_t *commands_textbuffer(const gkey_t *gkey)
{
    static command_t cmdscrolltotop = { gcmd_buffer_scroll, gcmd_UpEnd };
    static command_t cmdscrolltobottom = { gcmd_buffer_scroll, gcmd_DownEnd };
    static command_t cmdscrolluppage = { gcmd_buffer_scroll, gcmd_UpPage };
    static command_t cmdscrolldownpage = { gcmd_buffer_scroll, gcmd_DownPage };

    switch (gkey->curses) {
        case KEY_HOME:
            return &cmdscrolltotop;
        case KEY_END:
            return &cmdscrolltobottom;
        case KEY_PPAGE:
        case '\031': /* ctrl-Y */
            return &cmdscrolluppage;
        case KEY_NPAGE:
        case '\026': /* ctrl-V */
            return &cmdscrolldownpage;
    }
    return NULL;
}

/* Keys for "hit any key to page" mode. */
static command_t *commands_textbuffer_paging(const gkey_t *gkey)
{
    static command_t cmdscrolldownpage = { gcmd_buffer_scroll, gcmd_DownPage };

    return &cmdscrolldownpage;
}

/* Keys for char input in a text buffer window. */
static command_t *commands_textbuffer_char(const gkey_t *gkey)
{
    static command_t cmdv = { gcmd_buffer_accept_key, -1 };

    return &cmdv;
}

/* Keys for line input in a text buffer window. */
static command_t *commands_textbuffer_line(window_textbuffer_t *dwin, const gkey_t *gkey)
{
    static command_t cmdacceptline = { gcmd_buffer_accept_line, 0 };
    static command_t cmdacceptlineterm = { gcmd_buffer_accept_line, -1 };
    static command_t cmdinsert = { gcmd_buffer_insert_key, -1 };
    static command_t cmdmoveleft = { gcmd_buffer_move_cursor, gcmd_Left };
    static command_t cmdmoveright = { gcmd_buffer_move_cursor, gcmd_Right };
    static command_t cmdmoveleftend = { gcmd_buffer_move_cursor, gcmd_LeftEnd };
    static command_t cmdmoverightend = { gcmd_buffer_move_cursor, gcmd_RightEnd };
    static command_t cmddelete = { gcmd_buffer_delete, gcmd_Delete };
    static command_t cmddeletenext = { gcmd_buffer_delete, gcmd_DeleteNext };
    static command_t cmdkillinput = { gcmd_buffer_delete, gcmd_KillInput };
    static command_t cmdkillline = { gcmd_buffer_delete, gcmd_KillLine };
    static command_t cmdhistoryprev = { gcmd_buffer_history, gcmd_Up };
    static command_t cmdhistorynext = { gcmd_buffer_history, gcmd_Down };

    if ((gkey->curses >= 0x20 && gkey->curses < 0x7F) ||
        (gkey->curses >= 0xA0 && gkey->curses <= 0xFF)) {
        return &cmdinsert;
    }
    switch (gkey->curses) {
        case KEY_ENTER:
        case '\012': /* ctrl-J */
        case '\015': /* ctrl-M */
            return &cmdacceptline;
        case KEY_LEFT:
        case '\002': /* ctrl-B */
            return &cmdmoveleft;
        case KEY_RIGHT:
        case '\006': /* ctrl-F */
            return &cmdmoveright;
        case KEY_HOME:
        case '\001': /* ctrl-A */
            return &cmdmoveleftend;
        case KEY_END:
        case '\005': /* ctrl-E */
            return &cmdmoverightend;
        case '\177': /* delete */
        case '\010': /* backspace */
        case KEY_BACKSPACE:
        case KEY_DC:
            return &cmddelete;
        case '\004': /* ctrl-D */
            return &cmddeletenext;
        case '\013': /* ctrl-K */
            return &cmdkillline;
        case '\025': /* ctrl-U */
            return &cmdkillinput;
        case KEY_UP:
        case '\020': /* ctrl-P */
            return &cmdhistoryprev;
        case KEY_DOWN:
        case '\016': /* ctrl-N */
            return &cmdhistorynext;

        case '\033': /* escape */
            if (dwin->intermkeys & 0x10000)
                return &cmdacceptlineterm;
            break;
#ifdef KEY_F
        case KEY_F(1):
            if (dwin->intermkeys & (1<<1))
                return &cmdacceptlineterm;
            break;
        case KEY_F(2):
            if (dwin->intermkeys & (1<<2))
                return &cmdacceptlineterm;
            break;
        case KEY_F(3):
            if (dwin->intermkeys & (1<<3))
                return &cmdacceptlineterm;
            break;
        case KEY_F(4):
            if (dwin->intermkeys & (1<<4))
                return &cmdacceptlineterm;
            break;
        case KEY_F(5):
            if (dwin->intermkeys & (1<<5))
                return &cmdacceptlineterm;
            break;
        case KEY_F(6):
            if (dwin->intermkeys & (1<<6))
                return &cmdacceptlineterm;
            break;
        case KEY_F(7):
            if (dwin->intermkeys & (1<<7))
                return &cmdacceptlineterm;
            break;
        case KEY_F(8):
            if (dwin->intermkeys & (1<<8))
                return &cmdacceptlineterm;
            break;
        case KEY_F(9):
            if (dwin->intermkeys & (1<<9))
                return &cmdacceptlineterm;
            break;
        case KEY_F(10):
            if (dwin->intermkeys & (1<<10))
                return &cmdacceptlineterm;
            break;
        case KEY_F(11):
            if (dwin->intermkeys & (1<<11))
                return &cmdacceptlineterm;
            break;
        case KEY_F(12):
            if (dwin->intermkeys & (1<<12))
                return &cmdacceptlineterm;
            break;
#endif /* KEY_F */
    }
    
    if (gkey->key32 > 0xFF && GLIISPRINT(glui32_to_glichar(gkey->key32)))
        return &cmdinsert;
    
    return NULL;
}

/* Check to see if key is bound to anything in the given window.
    First check for char or line input bindings, then general
    bindings. */
static command_t *commands_window(window_t *win, const gkey_t *gkey)
{
    command_t *cmd = NULL;
    
    switch (win->type) {
        case wintype_TextGrid: {
            window_textgrid_t *dwin = win->data;
            cmd = commands_textgrid(gkey);
            if (!cmd) {
                if (win->line_request)
                    cmd = commands_textgrid_line(dwin, gkey);
                else if (win->char_request)
                    cmd = commands_textgrid_char(gkey);
            }
            }
            break;
        case wintype_TextBuffer: {
            window_textbuffer_t *dwin = win->data;
            cmd = commands_textbuffer(gkey);
            if (!cmd) {
                if (dwin->lastseenline < dwin->numlines - dwin->height) {
                    cmd = commands_textbuffer_paging(gkey);
                }
                if (!cmd) {
                    if (win->line_request)
                        cmd = commands_textbuffer_line(dwin, gkey);
                    else if (win->char_request)
                        cmd = commands_textbuffer_char(gkey);
                }
            }
            }
            break;
    }
    
    return cmd;
}

/* Return a string describing a given key. This (sometimes) uses a
    static buffer, which is overwritten with each call. */
static glichar *key_to_name(const gkey_t *gkey)
{
    static glichar kbuf[32];

    if (gkey->curses >= 0x20 && gkey->curses < 0x7F) {
        kbuf[0] = gkey->curses;
        kbuf[1] = '\0';
        return kbuf;
    }
    else if (gkey->curses >= 0xA0 && gkey->curses <= 0xFF) {
        /* TODO(townba): Output the actual character, if possible. */
        static const char digits[] = "0123456789ABCDEF";
        kbuf[0] = '0';
        kbuf[1] = 'x';
        kbuf[2] = digits[(gkey->curses >> 4) & 0xF];
        kbuf[3] = digits[gkey->curses & 0xF];
        kbuf[4] = '\0';
        return kbuf;
    }

    switch (gkey->curses) {
        case '\t':
            return GLITEXT("tab");
        case '\033':
            return GLITEXT("escape");
        case KEY_DOWN:
            return GLITEXT("down-arrow");
        case KEY_UP:
            return GLITEXT("up-arrow");
        case KEY_LEFT:
            return GLITEXT("left-arrow");
        case KEY_RIGHT:
            return GLITEXT("right-arrow");
        case KEY_HOME:
            return GLITEXT("home");
        case '\010': /* backspace */
        case KEY_BACKSPACE:
            return GLITEXT("backspace");
        case '\177': /* delete */
        case KEY_DC:
            return GLITEXT("delete-char");
        case KEY_IC:
            return GLITEXT("insert-char");
        case KEY_NPAGE:
            return GLITEXT("page-down");
        case KEY_PPAGE:
            return GLITEXT("page-up");
        case KEY_ENTER:
            return GLITEXT("enter");
        case KEY_END:
            return GLITEXT("end");
        case KEY_HELP:
            return GLITEXT("help");
#ifdef KEY_F
        case KEY_F(1):
            return GLITEXT("func-1");
        case KEY_F(2):
            return GLITEXT("func-2");
        case KEY_F(3):
            return GLITEXT("func-3");
        case KEY_F(4):
            return GLITEXT("func-4");
        case KEY_F(5):
            return GLITEXT("func-5");
        case KEY_F(6):
            return GLITEXT("func-6");
        case KEY_F(7):
            return GLITEXT("func-7");
        case KEY_F(8):
            return GLITEXT("func-8");
        case KEY_F(9):
            return GLITEXT("func-9");
        case KEY_F(10):
            return GLITEXT("func-10");
        case KEY_F(11):
            return GLITEXT("func-11");
        case KEY_F(12):
            return GLITEXT("func-12");
#endif /* KEY_F */
#ifdef KEY_RESIZE
        case KEY_RESIZE:
            return GLITEXT("resize");
#endif
    }

    if (gkey->curses >= 0 && gkey->curses < 0x20) {
        GLISNPRINTF(kbuf, sizeof(kbuf)/sizeof(kbuf[0]), GLITEXT("ctrl-%c"), '@' + gkey->curses);
        return kbuf;
    }

    return GLITEXT("unknown-key");
}

/* Valid Latin-1 input values */
/* TODO(townba): Rethink. */
int gli_good_latin_key(glui32 key32)
{
    return (key32 == 0x0A) || (key32 >= 0x20 && key32 < 0x7F) ||
        (key32 >= 0xA0 && key32 < 0x110000) ||
        (key32 > (0xFFFFFFFF - keycode_MAXVAL));
}

/* Allowed key codes */
/* TODO(townba): Rethink. */
int gli_legal_keycode(glui32 key32)
{
    switch (key32) {
        case keycode_Left:
            return has_key(KEY_LEFT);
        case keycode_Right:
            return has_key(KEY_RIGHT);
        case keycode_Up:
            return has_key(KEY_UP);
        case keycode_Down:
            return has_key(KEY_DOWN);
        case keycode_Return:
            return TRUE;
        case keycode_Delete:
            return has_key(KEY_BACKSPACE) || has_key(KEY_DC);
        case keycode_Escape:
            return TRUE;
        case keycode_Tab:
            return TRUE;
        case keycode_Unknown:
            return FALSE;
        case keycode_PageUp:
            return has_key(KEY_PPAGE);
        case keycode_PageDown:
            return has_key(KEY_NPAGE);
        case keycode_Home:
            return has_key(KEY_HOME);
        case keycode_End:
            return has_key(KEY_END);
        case keycode_Func1:
            return has_key(KEY_F(1));
        case keycode_Func2:
            return has_key(KEY_F(2));
        case keycode_Func3:
            return has_key(KEY_F(3));
        case keycode_Func4:
            return has_key(KEY_F(4));
        case keycode_Func5:
            return has_key(KEY_F(5));
        case keycode_Func6:
            return has_key(KEY_F(6));
        case keycode_Func7:
            return has_key(KEY_F(7));
        case keycode_Func8:
            return has_key(KEY_F(8));
        case keycode_Func9:
            return has_key(KEY_F(9));
        case keycode_Func10:
            return has_key(KEY_F(10));
        case keycode_Func11:
            return has_key(KEY_F(11));
        case keycode_Func12:
            return has_key(KEY_F(12));
        default:
            return FALSE;
    }
}

glui32 gli_input_from_native(glui32 key32)
{
    /* This is where illegal values get filtered out from character input */
    if (!gli_good_latin_key(key32) || (key32 >= 0x100 && !(gli_legal_keycode(key32) || GLIISPRINT(glui32_to_glichar(key32)))))
        return keycode_Unknown;
    else
        return key32;
}

int gli_get_key(gkey_t *gkey)
{
    int status = local_get_wch(&gkey->curses);
    if (status == ERR)
        return FALSE;
    gkey->function = (status == KEY_CODE_YES);
    if (gkey->function) {
        switch (gkey->curses) {
            case KEY_DOWN:
                gkey->key32 = keycode_Down;
                break;
            case KEY_UP:
                gkey->key32 = keycode_Up;
                break;
            case KEY_LEFT:
                gkey->key32 = keycode_Left;
                break;
            case KEY_RIGHT:
                gkey->key32 = keycode_Right;
                break;
            case KEY_HOME:
                gkey->key32 = keycode_Home;
                break;
            case KEY_BACKSPACE:
            case KEY_DC:
                gkey->key32 = keycode_Delete;
                break;
            case KEY_NPAGE:
                gkey->key32 = keycode_PageDown;
                break;
            case KEY_PPAGE:
                gkey->key32 = keycode_PageUp;
                break;
            case KEY_ENTER:
                gkey->key32 = keycode_Return;
                break;
            case KEY_END:
                gkey->key32 = keycode_End;
                break;
            case KEY_F(1):
                gkey->key32 = keycode_Func1;
                break;
            case KEY_F(2):
                gkey->key32 = keycode_Func2;
                break;
            case KEY_F(3):
                gkey->key32 = keycode_Func3;
                break;
            case KEY_F(4):
                gkey->key32 = keycode_Func4;
                break;
            case KEY_F(5):
                gkey->key32 = keycode_Func5;
                break;
            case KEY_F(6):
                gkey->key32 = keycode_Func6;
                break;
            case KEY_F(7):
                gkey->key32 = keycode_Func7;
                break;
            case KEY_F(8):
                gkey->key32 = keycode_Func8;
                break;
            case KEY_F(9):
                gkey->key32 = keycode_Func9;
                break;
            case KEY_F(10):
                gkey->key32 = keycode_Func10;
                break;
            case KEY_F(11):
                gkey->key32 = keycode_Func11;
                break;
            case KEY_F(12):
                gkey->key32 = keycode_Func12;
                break;
            default:
                gkey->key32 = keycode_Unknown;
                break;
        }
    }
    else {
        /* TODO(townba): Clean this up. */
        switch (gkey->curses) {
            case '\t': 
                gkey->key32 = keycode_Tab;
                break;
            case '\033':
                gkey->key32 = keycode_Escape;
                break;
            case '\177': /* delete */
            case '\010': /* backspace */
                gkey->key32 = keycode_Delete;
                break;
            case '\012': /* ctrl-J */
            case '\015': /* ctrl-M */
                gkey->key32 = keycode_Return;
                break;
            default:
                if (gkey->curses > 0x100 && !GLIISPRINT(gkey->curses))
                    gkey->key32 = keycode_Unknown;
                else
                    gkey->key32 = glichar_to_glui32(gkey->curses);
                break;
        }
    }

    return TRUE;
}

/* Handle a keystroke. This is called from glk_select() whenever a
    key is hit. */
void gli_input_handle_key(const gkey_t *gkey)
{
    command_t *cmd = NULL;
    window_t *win = NULL;

    /* First, see if the key has a general binding. */
    if (!cmd) {
        cmd = commands_always(gkey);
        if (cmd)
            win = NULL;
    }

    /* If not, see if the key is bound in the focus window. */
    if (!cmd && gli_focuswin) {
        cmd = commands_window(gli_focuswin, gkey);
        if (cmd)
            win = gli_focuswin;
    }
    
    /* If not, see if there's some other window which has a binding for
        the key; if so, set the focus there. */
    if (!cmd && gli_rootwin) {
        window_t *altwin = gli_focuswin;
        command_t *altcmd = NULL;
        do {
            altwin = gli_window_iterate_treeorder(altwin);
            if (altwin && altwin->type != wintype_Pair) {
                altcmd = commands_window(altwin, gkey);
                if (altcmd)
                    break;
            }
        } while (altwin != gli_focuswin);
        if (altwin != gli_focuswin && altcmd) {
            cmd = altcmd;
            win = altwin;
            gli_focuswin = win; /* set the focus */
        }
    }
    
    if (cmd) {
        /* We found a binding. Run it. */
        glui32 arg;
        if (cmd->arg == -1) {
            arg = gkey->key32;
        }
        else {
            arg = cmd->arg;
        }
        (*cmd->func)(win, arg);
    }
    else {
        glichar buf[256];
        GLISTRCPY(buf, GLITEXT("The key <"));
        GLISTRCAT(buf, key_to_name(gkey));
        GLISTRCAT(buf, GLITEXT("> is not currently defined."));
        gli_msgline(buf);
    }
}

/* Pick a window which might want input. This is called at the beginning
    of glk_select(). */
void gli_input_guess_focus()
{
    window_t *altwin;
    
    if (gli_focuswin 
        && (gli_focuswin->line_request || gli_focuswin->char_request)) {
        return;
    }
    
    altwin = gli_focuswin;
    do {
        altwin = gli_window_iterate_treeorder(altwin);
        if (altwin 
            && (altwin->line_request || altwin->char_request)) {
            break;
        }
    } while (altwin != gli_focuswin);
    
    if (gli_focuswin != altwin)
        gli_focuswin = altwin;
}

