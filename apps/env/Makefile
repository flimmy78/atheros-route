#
# (C) Copyright 2002
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# See file CREDITS for list of people who contributed to this
# project.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA
#

SOURCES := crc32.c  fw_env.c  fw_env_main.c
HEADERS := fw_env.h

CPPFLAGS += -I ../../boot/u-boot/include
#CPPFLAGS += -I/home/changyongjin/yongjin/9344/boot/u-boot/include


CC=../../build/gcc-4.3.3/build_mips/staging_dir/usr/bin/mips-linux-uclibc-gcc
#CC=/home/changyongjin/9344/build/gcc-4.3.3/build_mips/staging_dir/usr/bin/mips-linux-uclibc-gcc

all:	fw_printenv

fw_printenv:	$(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) -DUSE_HOSTCC $(SOURCES) -o fw_printenv

clean:
	rm -f fw_setenv  fw_printenv crc32.c

crc32.c:
	ln -s ../../boot/u-boot/lib_generic/crc32.c crc32.c
#	ln -s /home/changyongjin/yongjin/9344/boot/u-boot/lib_generic/crc32.c crc32.c

#########################################################################

#.depend:	Makefile $(SOURCES)
#		$(CC) -M $(HOST_CFLAGS) $(CPPFLAGS) -DUSE_HOSTCC $(SOURCES) > $@

#sinclude .depend

#########################################################################
