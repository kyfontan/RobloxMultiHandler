/*
 * Tiny shim for the old MinGW.org Win32 headers (XP-era).
 * Defines a handful of Vista/7+ symbols ImGui's Win32 backend uses.
 * Safe to include even on modern toolchains.
 */
#ifndef RH_WIN_COMPAT_H
#define RH_WIN_COMPAT_H

#include <windows.h>

/*
 * MinGW-w64's <windows.h> transitively drags in <stdint.h>/<stdio.h>; the
 * leaner MinGW.org one does not.  Pull them in here (this header is force-
 * included into every C++ TU) so sources relying on that transitivity --
 * intptr_t, snprintf -- keep building.
 */
#include <stdint.h>
#include <stdio.h>

/*
 * DEFINE_ENUM_FLAG_OPERATORS: MSVC/MinGW-w64 winnt.h provides it; MinGW.org
 * does not, and the WIDL d3d11sdklayers stub uses it.  Standard expansion.
 */
#ifndef DEFINE_ENUM_FLAG_OPERATORS
#ifdef __cplusplus
#define DEFINE_ENUM_FLAG_OPERATORS(ENUMTYPE) \
extern "C++" { \
  inline ENUMTYPE  operator |  (ENUMTYPE a, ENUMTYPE b) { return ENUMTYPE(((int)a) | ((int)b)); } \
  inline ENUMTYPE &operator |= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((int &)a) |= ((int)b)); } \
  inline ENUMTYPE  operator &  (ENUMTYPE a, ENUMTYPE b) { return ENUMTYPE(((int)a) & ((int)b)); } \
  inline ENUMTYPE &operator &= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((int &)a) &= ((int)b)); } \
  inline ENUMTYPE  operator ~  (ENUMTYPE a) { return ENUMTYPE(~((int)a)); } \
  inline ENUMTYPE  operator ^  (ENUMTYPE a, ENUMTYPE b) { return ENUMTYPE(((int)a) ^ ((int)b)); } \
  inline ENUMTYPE &operator ^= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((int &)a) ^= ((int)b)); } \
}
#else
#define DEFINE_ENUM_FLAG_OPERATORS(ENUMTYPE)
#endif
#endif

/* DXGI error HRESULTs missing from the MinGW.org / stub dxgi headers. */
#ifndef DXGI_ERROR_UNSUPPORTED
#define DXGI_ERROR_UNSUPPORTED ((HRESULT)0x887A0004L)
#endif

/*
 * The WIDL-generated DX stubs tag their anonymous unions/structs with the
 * __C89_NAMELESS* helper macros (MinGW-w64 _mingw.h).  MinGW.org defines none
 * of them, so map them to GNU anonymous aggregates (__extension__ silences
 * the -pedantic noise).  Must precede any d3d10/d3d11 stub include.
 */
#ifndef __C89_NAMELESS
#define __C89_NAMELESS __extension__
#define __C89_NAMELESSSTRUCTNAME
#define __C89_NAMELESSSTRUCTNAME1
#define __C89_NAMELESSSTRUCTNAME2
#define __C89_NAMELESSSTRUCTNAME3
#define __C89_NAMELESSSTRUCTNAME4
#define __C89_NAMELESSSTRUCTNAME5
#define __C89_NAMELESSUNIONNAME
#define __C89_NAMELESSUNIONNAME1
#define __C89_NAMELESSUNIONNAME2
#define __C89_NAMELESSUNIONNAME3
#define __C89_NAMELESSUNIONNAME4
#define __C89_NAMELESSUNIONNAME5
#define __C89_NAMELESSUNIONNAME6
#define __C89_NAMELESSUNIONNAME7
#define __C89_NAMELESSUNIONNAME8
#endif

/* __uuidof / IID_PPV_ARGS machinery the stubs need (see header). */
#include "mingw_uuid_compat.h"

#ifndef TME_NONCLIENT
#define TME_NONCLIENT 0x00000010
#endif

#ifndef XBUTTON1
#define XBUTTON1 0x0001
#endif
#ifndef XBUTTON2
#define XBUTTON2 0x0002
#endif

#ifndef GET_XBUTTON_WPARAM
#define GET_XBUTTON_WPARAM(wParam) (HIWORD(wParam))
#endif

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif

#ifndef WM_NCMOUSEHOVER
#define WM_NCMOUSEHOVER 0x02A0
#endif

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

/* Old headers ship OSVERSIONINFOEXW but not the RTL_ alias. */
typedef OSVERSIONINFOEXW RTL_OSVERSIONINFOEXW;

/* Declared in user32.dll since Vista. */
#ifdef __cplusplus
extern "C" {
#endif
WINUSERAPI BOOL WINAPI SetProcessDPIAware(VOID);
#ifdef __cplusplus
}
#endif

#endif
