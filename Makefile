# Makefile for WindowLab

# Comment out to remove shape support (for X11R5 or just a tiny bin)
DEFINES += -DSHAPE

# Set this to the hardcoded location of all files if it's not /
PREFIX = /usr/local

# Set this to the directory, below PREFIX, where man pages 
# are expected. Below this directory, the target "install"
# will put "windowlab.1x" in section "man1".
MANBASE = /man

# Set this to the location of the X installation you want to compile against
XROOT = /usr/X11R6

# Some flexibility for configuration location
CONFPREFIX = $(PREFIX)
CONFDIR = /etc/X11/windowlab

# Set this to the location of the global configuration files
SYSCONFDIR = $(CONFPREFIX)$(CONFDIR)

# Information about the location of the menurc file
ifndef MENURC
MENURC = $(SYSCONFDIR)/windowlab.menurc
endif

DEFINES += -DDEF_MENURC="\"$(MENURC)\""

# Freetype support required (requires XFree86 4.0.2 or later)

# Uncomment for debugging info (abandon all hope, ye who enter here)
#DEFINES += -DDEBUG

# --------------------------------------------------------------------

CC = clang
CXX = clang++
ifndef CFLAGS
CFLAGS = -g -O0
endif
ifndef CXXFLAGS
CXXFLAGS = -g -O0 -std=c++17
endif
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)$(MANBASE)/man1
CFGDIR = $(SYSCONFDIR)
INCLUDES = -I$(XROOT)/include `pkg-config --cflags xft` $(EXTRA_INC)
LDPATH = -L$(XROOT)/lib
LDFLAGS = -std=c++17
#LDFLAGS = -m32
LIBS = -lX11 -lXext `pkg-config --libs xft` $(EXTRA_LIBS)

PROG = windowlab
MANPAGE = windowlab.1x
OBJS = main.o events.o client.o new.o manage.o misc.o taskbar.o menufile.o
HEADERS = windowlab.h

all: $(PROG)

$(PROG): $(OBJS)
	$(CXX) $(OBJS) $(LDPATH) $(LIBS) $(LDFLAGS) -o $@

$(OBJS): %.o: %.cc $(HEADERS)
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

install: all
	mkdir -p $(BINDIR) && install -m 755 -s $(PROG) $(BINDIR)
	mkdir -p $(MANDIR) && install -m 644 $(MANPAGE) $(MANDIR) && gzip -9vfn $(MANDIR)/$(MANPAGE)
	mkdir -p $(CFGDIR) && cp -i windowlab.menurc $(CFGDIR)/windowlab.menurc && chmod 644 $(CFGDIR)/windowlab.menurc

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all install clean
