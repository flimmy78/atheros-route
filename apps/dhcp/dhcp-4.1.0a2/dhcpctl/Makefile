# Makefile.in generated by automake 1.9.6 from Makefile.am.
# dhcpctl/Makefile.  Generated from Makefile.in by configure.

# Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
# 2003, 2004, 2005  Free Software Foundation, Inc.
# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.




srcdir = .
top_srcdir = ..


INCLUDES = -I. -I$(srcdir) -I$(top_srcdir) -I$(top_srcdir)/includes

COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CFLAGS)
LINK = $(CC) $(CFLAGS) -o $@

libdhcpctl_a_OBJECTS = dhcpctl.$(OBJEXT) callback.$(OBJEXT) \
	remote.$(OBJEXT)

all:	libdhcpctl.a

libdhcpctl.a: $(libdhcpctl_a_OBJECTS)
	-rm -f libdhcpctl.a
	$(AR) $(ARFLAGS) libdhcpctl.a $(libdhcpctl_a_OBJECTS) 
	$(RANLIB) libdhcpctl.a

.c.o:
	$(COMPILE) -c $<


clean: 
	-rm *.o
	-rm libdhcpctl.a


.PHONY: all clean 

# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
