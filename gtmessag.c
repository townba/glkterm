/* gtmessag.c: The message line at the bottom of the screen
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html
*/

#include "gtoption.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include "glk.h"
#include "glkterm.h"

/* Nothing fancy here. We store a string, and print it on the bottom line.
    If pref_messageline is FALSE, none of these functions do anything. */

static glichar *msgbuf = NULL;
static int msgbuflen = 0;
static int msgbuf_size = 0;

void gli_msgline_warning(glichar *msg)
{
    const glichar prefix[] = GLITEXT("Glk library error: ");
    size_t prefixlen, msglen;
    glichar *buf = NULL;
    
    if (!pref_messageline)
        return;
        
    prefixlen = sizeof(prefix)/sizeof(prefix[0]);
    msglen = GLISTRLEN(msg);
    buf = calloc(prefixlen + msglen, sizeof(glichar));
    if (!buf)
        return;
    GLISTRCPY(buf, prefix);
    GLISTRCAT(buf, msg);
    gli_msgline(buf);
    free(buf);
}

/* TODO(townba): Get this working with wide characters. */
/*#if defined(__clang__) || defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif*/
void gli_msgline_warningf(const glichar *format, ...)
{
    va_list args;
    int n = 0;
    size_t siz = 256;
    glichar *buf = NULL;
    for (;;) {
        buf = malloc(siz * sizeof(glichar));
        if (!buf) {
            gli_msgline_warning((glichar *)format);
            return;
        }
        va_start(args, format);
#ifdef OPT_WIDE_CHARACTERS
        n = vswprintf(buf, siz, format, args);
#else
        n = vsnprintf(buf, siz, format, args);
#endif
        va_end(args);
        if (n < 0 || (size_t)n < siz) {
            gli_msgline_warning(buf);
            free(buf);
            return;
        }
        siz = (size_t)n + 1;
        free(buf);
    }
}

void gli_msgline(glichar *msg)
{
    int len;
    
    if (!pref_messageline)
        return;
        
    if (!msg) 
        msg = GLITEXT("");
    
    len = GLISTRLEN(msg);
    if (!msgbuf) {
        msgbuf_size = len+80;
        msgbuf = (glichar *)malloc(msgbuf_size * sizeof(glichar));
    }
    else if (len+1 > msgbuf_size) {
        while (len+1 > msgbuf_size)
            msgbuf_size *= 2;
        msgbuf = (glichar *)realloc(msgbuf, msgbuf_size * sizeof(glichar));
    }

    if (!msgbuf)
        return;
    
    GLISTRCPY(msgbuf, msg);
    msgbuflen = len;
    
    gli_msgline_redraw();
}

void gli_msgline_redraw()
{
    if (!pref_messageline || !msgbuf)
        return;
        
    if (msgbuflen == 0) {
        move(content_box.bottom, 0);
        clrtoeol();
    }
    else {
        int ix, len;
        
        move(content_box.bottom, 0);
        addch(' ');
        addch(' ');
        attron(A_REVERSE);
        if (msgbuflen > content_box.right-3)
            len = content_box.right-3;
        else
            len = msgbuflen;
        for (ix=0; ix<len; ix++) {
            addch(msgbuf[ix]);
        }
        attrset(0);
        clrtoeol();
    }
}
