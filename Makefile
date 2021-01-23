# Unix Makefile for GlkTerm library

# This generates two files. One, of course, is libglkterm.a -- the library
# itself. The other is Make.glkterm; this is a snippet of Makefile code
# which locates the glkterm library and associated libraries (such as
# ncurses.)
#
# When you install glkterm, you must put libglkterm.a in the lib directory,
# and glk.h, glkstart.h, and Make.glkterm in the include directory.

# Pick a C compiler.
#CC = cc
CC = gcc -ansi

# You may need to set directories to pick up the ncursesw library.
INCLUDEDIRS = $(shell pkg-config --cflags ncursesw)
#LIBDIRS = -L/usr/5lib 
LIBS = $(shell pkg-config --libs ncursesw)

# Use this LIBS line instead of the above if you do not have ncursesw
# available. For example, on a standard Mac OSX setup. (You will
# probably also have to set LOCAL_NCURSESW in CFLAGS or in gtoption.h.)
#LIBS = -lncurses

OPTIONS = -O -Wall -Werror

CFLAGS = $(OPTIONS) $(INCLUDEDIRS)

GLKLIB = libglkterm.a

GLKTERM_OBJS = \
  main.o gtevent.o gtfref.o gtgestal.o gtinput.o \
  gtmessag.o gtmessin.o gtmisc.o gtstream.o gtstyle.o \
  gtw_blnk.o gtw_buf.o gtw_grid.o gtw_pair.o gtwindow.o \
  gtschan.o gtblorb.o cgunicod.o cgdate.o gi_dispa.o gi_blorb.o \
  gtncursesw.o

GLKTERM_HEADERS = \
  glkterm.h gtoption.h gtw_blnk.h gtw_buf.h \
  gtw_grid.h gtw_pair.h gi_dispa.h

all: $(GLKLIB) Make.glkterm

cgunicod.o: cgunigen.c

$(GLKLIB): $(GLKTERM_OBJS)
	ar r $(GLKLIB) $(GLKTERM_OBJS)
	ranlib $(GLKLIB)

Make.glkterm:
	echo LINKLIBS = $(LIBDIRS) $(LIBS) > Make.glkterm
	echo GLKLIB = -lglkterm >> Make.glkterm

$(GLKTERM_OBJS): glk.h $(GLKTERM_HEADERS)

clean:
	rm -f *~ *.o $(GLKLIB) Make.glkterm
