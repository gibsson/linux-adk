
V ?= 0

ifeq ($V, 1)
E =
P = @true
else
E = @
P = @echo
endif

SHELL = /bin/sh

srcdir		= src
objdir		= obj
prefix		= $(DESTDIR)/usr/local
exec_prefix	= $(DESTDIR)/${prefix}
bindir		= $(exec_prefix)/bin

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

OBJ 		= $(objdir)/accessory.o \
			  $(objdir)/hid.o \
			  $(objdir)/linux-adk.o

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

.PHONY: clean
clean:
	$P '  RM       TARGET'
	$E rm -f $(TARGET)
	$P '  RM       OBJS'
	$E rm -rf $(objdir) 

install:
	$P '  MKDIRS   '
	$E $(MKDIR) $(bindir)
	$P '  INSTALL  $(TARGET)'
	$E $(INSTALL) $(TARGET) $(bindir)

uninstall:
	$P '  UNINSTALL'
	$E rm -f $(bindir)/$(TARGET)

