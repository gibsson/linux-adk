
V ?= 0

ifeq ($V, 1)
E =
P = @true
else
E = @
P = @echo
endif

SHELL = /bin/sh


top_srcdir	= .
srcdir		= src
objdir		= obj
prefix		= $(DESTDIR)/usr/local
exec_prefix	= $(DESTDIR)/${prefix}
bindir		= $(exec_prefix)/bin
docdir		= $(prefix)/share/doc/linux-adk

CC			= $(CROSS_COMPILE)gcc
INSTALL		= install
MKDIR		= mkdir -p

LIBS		=  -lusb-1.0 -lasound -lpthread
CFLAGS		+= -g -O0
LDFLAGS 	+= 
CPPFLAGS	+= 

ARCH		?= $(ARCH_x86_64)
ARCH_CFLAGS	?= $(CFLAGS_x86_64)

CFLAGS		+= -Isrc -I/usr/include/libusb-1.0
CFLAGS		+= -Wall -Wextra -Wno-char-subscripts -Wno-unused-parameter -Wno-format
CFLAGS		+= $(ARCH_CFLAGS)

OBJ 		= $(objdir)/linux-adk.o \
			  $(objdir)/accessory.o

TARGET		= linux-adk

all: $(objdir) $(TARGET)

$(TARGET): $(OBJ)
	$P '  LD       $@'
	$E $(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(objdir):
	$E mkdir $(objdir)

$(objdir)/%.o: $(srcdir)/%.c
	$P '  CC       $@'
	$E $(CC) $(CFLAGS) -c -o $@ $^

.PHONY: doc
doc:
	$P '  DOC      '
	$E doxygen doxygen.cfg

.PHONY: clean
clean:
	$P '  RM       TARGET'
	$E rm -f $(TARGET)
	$P '  RM       OBJS'
	$E rm -rf $(objdir) 

.PHONY: distclean
distclean: clean
	$P '  RM       Makefile'
	$E rm -f Makefile
	$P '  RM       doc'
	$E rm -fr doc
	$P '  RM       config.*'
	$E rm -f src/config.h config.status config.cache config.log
	$P '  RM       cache'
	$E rm -fr autom4te.cache

install:
	$P '  MKDIRS   '
	$E $(MKDIR) $(bindir)
	$E $(MKDIR) $(mandir)
	$E $(MKDIR) $(docdir)
	$P '  INSTALL  $(TARGET)'
	$E $(INSTALL) $(TARGET) $(bindir)
	$P '  INSTALL  README'
	$E $(INSTALL) README $(docdir)

uninstall:
	$P '  UNINSTALL'
	$E rm -f $(bindir)/$(TARGET)
	$E rm -f $(docdir)/README

