# Copyright (C) 1992 - 2001 Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 1, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#

CLEANFILES = *.log *.sum site.bak setval.tmp site.exp

.SUFFIXES:	
clean:
	-rm -f demangle trace-fork trace-clone trace-exec trace-exec1
	-rm -f time-record-tt time-record-ttt time-record-T
	-rm -f attach-process count-record
	-rm -f print-instruction-pointer
	-rm -f *.o *.so 
	-rm -f *.ltrace
	-rm -f $(CLEANFILES)
distclean: clean


.PHONY: $(RECURSIVE_TARGETS) check  clean distclean realclean

# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
