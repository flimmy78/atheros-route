/*
 *  TAP-Win32 -- A kernel driver to provide virtual tap device functionality
 *               on Windows.  Originally derived from the CIPE-Win32
 *               project by Damion K. Wilson, with extensive modifications by
 *               James Yonan. IPv6 support added by Hexago.
 *
 *  All source code which derives from the CIPE-Win32 project is
 *  Copyright (C) Damion K. Wilson, 2003, and is released under the
 *  GPL version 2 (see below).
 *
 *  All other source code is Copyright (C) James Yonan, 2003,
 *  and is released under the GPL version 2 (see below).
 *
 *  Except the IPv6 support which is Copyright (C) Hexago, 2004,
 *  and is released under the GPL version 2 (see below).
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program (see the file COPYING included with this
 *  distribution); if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef HEXDUMP_DEFINED
#define HEXDUMP_DEFINED

#ifdef __cplusplus
extern "C" {
#endif

//=====================================================================================
//                                   Debug Routines
//=====================================================================================

#ifndef NDIS_MINIPORT_DRIVER
#   include <stdio.h>
#   include <ctype.h>
#   include <windows.h>
#   include <winnt.h>
#   include <memory.h>

#   ifndef DEBUGP
#      define DEBUGP(fmt) { DbgMessage fmt; }
#   endif

    extern void (*DbgMessage)(char *p_Format, ...);

    void DisplayDebugString (char *p_Format, ...);
#endif

//===================================================================================
//                              Reporting / Debugging
//===================================================================================
#define IfPrint(c) (c >= 32 && c < 127 ? c : '.')

void HexDump (unsigned char *p_Buffer, unsigned long p_Size);

#ifdef __cplusplus
}
#endif

#endif
