/* gtinput.c: Key input handling
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html
*/

#include "gtoption.h"
#include <wchar.h>
#include <wctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include "glk.h"
#include "glkterm.h"
#include "gtw_grid.h"
#include "gtw_buf.h"

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
static command_t *commands_always(glui32 key)
{
    static command_t cmdchangefocus = { gcmd_win_change_focus, 0 };
    static command_t cmdrefresh = { gcmd_win_refresh, 0 };

    switch (key) {
        case keycode_Tab: 
            return &cmdchangefocus;
        case '\014': /* ctrl-L */
            return &cmdrefresh;
    }
    
    return NULL;
}

/* Keys which are always meaningful in a text grid window. */
static command_t *commands_textgrid(glui32 key)
{
    return NULL;
}

/* Keys for char input in a text grid window. */
static command_t *commands_textgrid_char(glui32 key)
{
    static command_t cmdv = { gcmd_grid_accept_key, -1 };

    return &cmdv;
}

/* Keys for line input in a text grid window. */
static command_t *commands_textgrid_line(window_textgrid_t *dwin, glui32 key)
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

    if (key >= 32 && key < 256 && key != 127) 
        return &cmdinsert;

    switch (key) {
        case keycode_Return:
            return &cmdacceptline;
        case keycode_Left:
        case '\002': /* ctrl-B */
            return &cmdmoveleft;
        case keycode_Right:
        case '\006': /* ctrl-F */
            return &cmdmoveright;
        case keycode_Home:
        case '\001': /* ctrl-A */
            return &cmdmoveleftend;
        case keycode_End:
        case '\005': /* ctrl-E */
            return &cmdmoverightend;
        case keycode_Delete:
            return &cmddelete;
        case '\004': /* ctrl-D */
            return &cmddeletenext;
        case '\013': /* ctrl-K */
            return &cmdkillline;
        case '\025': /* ctrl-U */
            return &cmdkillinput;

        case '\033': /* escape */
        case keycode_Escape:
            if (dwin->intermkeys & 0x10000)
                return &cmdacceptlineterm;
            break;
        case keycode_Func1:
            if (dwin->intermkeys & (1<<1))
                return &cmdacceptlineterm;
            break;
        case keycode_Func2:
            if (dwin->intermkeys & (1<<2))
                return &cmdacceptlineterm;
            break;
        case keycode_Func3:
            if (dwin->intermkeys & (1<<3))
                return &cmdacceptlineterm;
            break;
        case keycode_Func4:
            if (dwin->intermkeys & (1<<4))
                return &cmdacceptlineterm;
            break;
        case keycode_Func5:
            if (dwin->intermkeys & (1<<5))
                return &cmdacceptlineterm;
            break;
        case keycode_Func6:
            if (dwin->intermkeys & (1<<6))
                return &cmdacceptlineterm;
            break;
        case keycode_Func7:
            if (dwin->intermkeys & (1<<7))
                return &cmdacceptlineterm;
            break;
        case keycode_Func8:
            if (dwin->intermkeys & (1<<8))
                return &cmdacceptlineterm;
            break;
        case keycode_Func9:
            if (dwin->intermkeys & (1<<9))
                return &cmdacceptlineterm;
            break;
        case keycode_Func10:
            if (dwin->intermkeys & (1<<10))
                return &cmdacceptlineterm;
            break;
        case keycode_Func11:
            if (dwin->intermkeys & (1<<11))
                return &cmdacceptlineterm;
            break;
        case keycode_Func12:
            if (dwin->intermkeys & (1<<12))
                return &cmdacceptlineterm;
            break;
    }
    
    /* Non-Latin-1 glyphs valid in this locale */
    if (key > 0xFF && GLIISPRINT(glui32_to_glichar(key)))
        return &cmdinsert;
    
    return NULL;
}

/* Keys which are always meaningful in a text buffer window. Note that
    these override character input, which means you can never type ctrl-Y
    or ctrl-V in a textbuffer, even though you can in a textgrid. The Glk
    API doesn't make this distinction. Damn. */
static command_t *commands_textbuffer(glui32 key)
{
    static command_t cmdscrolltotop = { gcmd_buffer_scroll, gcmd_UpEnd };
    static command_t cmdscrolltobottom = { gcmd_buffer_scroll, gcmd_DownEnd };
    static command_t cmdscrolluppage = { gcmd_buffer_scroll, gcmd_UpPage };
    static command_t cmdscrolldownpage = { gcmd_buffer_scroll, gcmd_DownPage };

    switch (key) {
        case keycode_Home:
            return &cmdscrolltotop;
        case keycode_End:
            return &cmdscrolltobottom;
        case keycode_PageUp:
        case '\031': /* ctrl-Y */
            return &cmdscrolluppage;
        case keycode_PageDown:
        case '\026': /* ctrl-V */
            return &cmdscrolldownpage;
    }
    return NULL;
}

/* Keys for "hit any key to page" mode. */
static command_t *commands_textbuffer_paging(glui32 key)
{
    static command_t cmdscrolldownpage = { gcmd_buffer_scroll, gcmd_DownPage };

    return &cmdscrolldownpage;
}

/* Keys for char input in a text buffer window. */
static command_t *commands_textbuffer_char(glui32 key)
{
    static command_t cmdv = { gcmd_buffer_accept_key, -1 };

    return &cmdv;
}

/* Keys for line input in a text buffer window. */
static command_t *commands_textbuffer_line(window_textbuffer_t *dwin, glui32 key)
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

    if (key >= 32 && key < 256 && key != '\177') 
        return &cmdinsert;

    switch (key) {
        case keycode_Return:
            return &cmdacceptline;
        case keycode_Left:
        case '\002': /* ctrl-B */
            return &cmdmoveleft;
        case keycode_Right:
        case '\006': /* ctrl-F */
            return &cmdmoveright;
        case keycode_Home:
        case '\001': /* ctrl-A */
            return &cmdmoveleftend;
        case keycode_End:
        case '\005': /* ctrl-E */
            return &cmdmoverightend;
        case keycode_Delete:
            return &cmddelete;
        case '\004': /* ctrl-D */
            return &cmddeletenext;
        case '\013': /* ctrl-K */
            return &cmdkillline;
        case '\025': /* ctrl-U */
            return &cmdkillinput;
        case keycode_Up:
        case '\020': /* ctrl-P */
            return &cmdhistoryprev;
        case keycode_Down:
        case '\016': /* ctrl-N */
            return &cmdhistorynext;

        case '\033': /* escape */
        case keycode_Escape:
            if (dwin->intermkeys & 0x10000)
                return &cmdacceptlineterm;
            break;
        case keycode_Func1:
            if (dwin->intermkeys & (1<<1))
                return &cmdacceptlineterm;
            break;
        case keycode_Func2:
            if (dwin->intermkeys & (1<<2))
                return &cmdacceptlineterm;
            break;
        case keycode_Func3:
            if (dwin->intermkeys & (1<<3))
                return &cmdacceptlineterm;
            break;
        case keycode_Func4:
            if (dwin->intermkeys & (1<<4))
                return &cmdacceptlineterm;
            break;
        case keycode_Func5:
            if (dwin->intermkeys & (1<<5))
                return &cmdacceptlineterm;
            break;
        case keycode_Func6:
            if (dwin->intermkeys & (1<<6))
                return &cmdacceptlineterm;
            break;
        case keycode_Func7:
            if (dwin->intermkeys & (1<<7))
                return &cmdacceptlineterm;
            break;
        case keycode_Func8:
            if (dwin->intermkeys & (1<<8))
                return &cmdacceptlineterm;
            break;
        case keycode_Func9:
            if (dwin->intermkeys & (1<<9))
                return &cmdacceptlineterm;
            break;
        case keycode_Func10:
            if (dwin->intermkeys & (1<<10))
                return &cmdacceptlineterm;
            break;
        case keycode_Func11:
            if (dwin->intermkeys & (1<<11))
                return &cmdacceptlineterm;
            break;
        case keycode_Func12:
            if (dwin->intermkeys & (1<<12))
                return &cmdacceptlineterm;
            break;
    }
    
    if (key > 0xFF && GLIISPRINT(glui32_to_glichar(key)))
        return &cmdinsert;
    
    return NULL;
}

/* Check to see if key is bound to anything in the given window.
    First check for char or line input bindings, then general
    bindings. */
static command_t *commands_window(window_t *win, glui32 key)
{
    command_t *cmd = NULL;
    
    switch (win->type) {
        case wintype_TextGrid: {
            window_textgrid_t *dwin = win->data;
            cmd = commands_textgrid(key);
            if (!cmd) {
                if (win->line_request)
                    cmd = commands_textgrid_line(dwin, key);
                else if (win->char_request)
                    cmd = commands_textgrid_char(key);
            }
            }
            break;
        case wintype_TextBuffer: {
            window_textbuffer_t *dwin = win->data;
            cmd = commands_textbuffer(key);
            if (!cmd) {
                if (dwin->lastseenline < dwin->numlines - dwin->height) {
                    cmd = commands_textbuffer_paging(key);
                }
                if (!cmd) {
                    if (win->line_request)
                        cmd = commands_textbuffer_line(dwin, key);
                    else if (win->char_request)
                        cmd = commands_textbuffer_char(key);
                }
            }
            }
            break;
    }
    
    return cmd;
}

/* Return a string describing a given key. This (sometimes) uses a
    static buffer, which is overwritten with each call. */
static glichar *key_to_name(glui32 key)
{
    static glichar kbuf[32];
    
    if (key >= 32 && key < 256) {
        if (key == 127) {
            return GLITEXT("delete");
        }
        kbuf[0] = key;
        kbuf[1] = '\0';
        return kbuf;
    }

    switch (key) {
        case '\t':
            return GLITEXT("tab");
        case '\033':
            return GLITEXT("escape");
        case keycode_Down:
            return GLITEXT("down-arrow");
        case keycode_Up:
            return GLITEXT("up-arrow");
        case keycode_Left:
            return GLITEXT("left-arrow");
        case keycode_Right:
            return GLITEXT("right-arrow");
        case keycode_Home:
            return GLITEXT("home");
        /* Now that we don't have access to the raw code, glk can't
           distinguish between Backspace and Delete.
        case KEY_BACKSPACE:
            return GLITEXT("backspace");
        */
        case keycode_Delete:
            return GLITEXT("delete-char");
        /* Now that we don't have access to the raw code, glk can't
           detect IC
        case KEY_IC:
            return GLITEXT("insert-char");
        */
        case keycode_PageDown:
            return GLITEXT("page-down");
        case keycode_PageUp:
            return GLITEXT("page-up");
        case keycode_Return:
            return GLITEXT("enter");
        case keycode_End:
            return GLITEXT("end");
        /* Now that we don't have access to the raw code, glk can't
           detect HELP
        case KEY_HELP:
            return GLITEXT("help");
        */
        case keycode_Func1:
            return GLITEXT("func-1");
        case keycode_Func2:
            return GLITEXT("func-2");
        case keycode_Func3:
            return GLITEXT("func-3");
        case keycode_Func4:
            return GLITEXT("func-4");
        case keycode_Func5:
            return GLITEXT("func-5");
        case keycode_Func6:
            return GLITEXT("func-6");
        case keycode_Func7:
            return GLITEXT("func-7");
        case keycode_Func8:
            return GLITEXT("func-8");
        case keycode_Func9:
            return GLITEXT("func-9");
        case keycode_Func10:
            return GLITEXT("func-10");
        case keycode_Func11:
            return GLITEXT("func-11");
        case keycode_Func12:
            return GLITEXT("func-12");
    }

    if (key >= 0 && key < 32) {
        GLISNPRINTF(kbuf, 32, GLITEXT("ctrl-%c"), '@'+key);
        return kbuf;
    }

    return GLITEXT("unknown-key");
}

/* Valid Latin-1 input values */
int gli_good_latin_key(glui32 key)
{
    return (key == 0x0A) || (key >= 0x20 && key < 0x7F) ||
        (key >= 0xA0 && key < 0x110000) ||
        (key > (0xFFFFFFFF - keycode_MAXVAL));
}

/* Allowed key codes */
int gli_legal_keycode(glui32 key)
{
    switch(key) {
        case keycode_Left:
            return has_key(glui32_to_glichar(KEY_LEFT));
        case keycode_Right:
            return has_key(glui32_to_glichar(KEY_RIGHT));
        case keycode_Up:
            return has_key(glui32_to_glichar(KEY_UP));
        case keycode_Down:
            return has_key(glui32_to_glichar(KEY_DOWN));
        case keycode_Return:
            return TRUE;
        case keycode_Delete:
            return has_key(glui32_to_glichar(KEY_BACKSPACE)) || has_key(glui32_to_glichar(KEY_DC));
        case keycode_Escape:
            return TRUE;
        case keycode_Tab:
            return TRUE;
        case keycode_Unknown:
            return FALSE;
        case keycode_PageUp:
            return has_key(glui32_to_glichar(KEY_PPAGE));
        case keycode_PageDown:
            return has_key(glui32_to_glichar(KEY_NPAGE));
        case keycode_Home:
            return has_key(glui32_to_glichar(KEY_HOME));
        case keycode_End:
            return has_key(glui32_to_glichar(KEY_END));
        case keycode_Func1:
            return has_key(glui32_to_glichar(KEY_F(1)));
        case keycode_Func2:
            return has_key(glui32_to_glichar(KEY_F(2)));
        case keycode_Func3:
            return has_key(glui32_to_glichar(KEY_F(3)));
        case keycode_Func4:
            return has_key(glui32_to_glichar(KEY_F(4)));
        case keycode_Func5:
            return has_key(glui32_to_glichar(KEY_F(5)));
        case keycode_Func6:
            return has_key(glui32_to_glichar(KEY_F(6)));
        case keycode_Func7:
            return has_key(glui32_to_glichar(KEY_F(7)));
        case keycode_Func8:
            return has_key(glui32_to_glichar(KEY_F(8)));
        case keycode_Func9:
            return has_key(glui32_to_glichar(KEY_F(9)));
        case keycode_Func10:
            return has_key(glui32_to_glichar(KEY_F(10)));
        case keycode_Func11:
            return has_key(glui32_to_glichar(KEY_F(11)));
        case keycode_Func12:
            return has_key(glui32_to_glichar(KEY_F(12)));
        default:
            return FALSE;
    }

}

glui32 gli_input_from_native(glui32 key)
{
    /* This is where illegal values get filtered out from character input */
    if (!gli_good_latin_key(key) || (key >= 0x100 && !(gli_legal_keycode(key) || GLIISPRINT(glui32_to_glichar(key)))))
        return keycode_Unknown;
    else
        return key;
}

glui32 gli_translate_key(int status, wint_t key)
{
    glui32 arg = 0;

    /* convert from curses.h key codes to Glk, if necessary. */
    if ( status == KEY_CODE_YES ) {
        switch (key) {
            case KEY_DOWN:
                arg = keycode_Down;
                break;
            case KEY_UP:
                arg = keycode_Up;
                break;
            case KEY_LEFT:
                arg = keycode_Left;
                break;
            case KEY_RIGHT:
                arg = keycode_Right;
                break;
            case KEY_HOME:
                arg = keycode_Home;
                break;
            case KEY_BACKSPACE:
            case KEY_DC:
                arg = keycode_Delete;
                break;
            case KEY_NPAGE:
                arg = keycode_PageDown;
                break;
            case KEY_PPAGE:
                arg = keycode_PageUp;
                break;
            case KEY_ENTER:
                arg = keycode_Return;
                break;
            case KEY_END:
                arg = keycode_End;
                break;
            case KEY_F(1):
                arg = keycode_Func1;
                break;
            case KEY_F(2):
                arg = keycode_Func2;
                break;
            case KEY_F(3):
                arg = keycode_Func3;
                break;
            case KEY_F(4):
                arg = keycode_Func4;
                break;
            case KEY_F(5):
                arg = keycode_Func5;
                break;
            case KEY_F(6):
                arg = keycode_Func6;
                break;
            case KEY_F(7):
                arg = keycode_Func7;
                break;
            case KEY_F(8):
                arg = keycode_Func8;
                break;
            case KEY_F(9):
                arg = keycode_Func9;
                break;
            case KEY_F(10):
                arg = keycode_Func10;
                break;
            case KEY_F(11):
                arg = keycode_Func11;
                break;
            case KEY_F(12):
                arg = keycode_Func12;
                break;
            default:
                  arg = keycode_Unknown;
                  break;
        }
    }
    else {
        switch (key) {
            case '\t': 
                arg = keycode_Tab;
                break;
            case '\033':
                arg = keycode_Escape;
                break;
            case '\177': /* delete */
            case '\010': /* backspace */
                arg = keycode_Delete;
                break;
            case '\012': /* ctrl-J */
            case '\015': /* ctrl-M */
                arg = keycode_Return;
                break;
            default:
                if ( key > 0x100 && ! GLIISPRINT(key) ) {
                    arg = keycode_Unknown;
                }
                else {
                    arg = glichar_to_glui32(key);
                }
                break;
        }
    }

    return arg;
}

int gli_get_key(glui32 *key)
{
    wint_t keycode;
    int status;
    
    status = local_get_wch(&keycode);
    
    *key = gli_translate_key(status, keycode);
    
    return status;
}

/* Handle a keystroke. This is called from glk_select() whenever a
    key is hit. */
void gli_input_handle_key(glui32 key)
{
    command_t *cmd = NULL;
    window_t *win = NULL;

    /* First, see if the key has a general binding. */
    if (!cmd) {
        cmd = commands_always(key);
        if (cmd)
            win = NULL;
    }

    /* If not, see if the key is bound in the focus window. */
    if (!cmd && gli_focuswin) {
        cmd = commands_window(gli_focuswin, key);
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
                altcmd = commands_window(altwin, key);
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
            arg = key;
        }
        else {
            arg = cmd->arg;
        }
        (*cmd->func)(win, arg);
    }
    else {
        glichar buf[256];
        glichar *kbuf = key_to_name(key);
        GLISNPRINTF(buf, 256, GLITEXT("The key <%s> is not currently defined."), kbuf);
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

