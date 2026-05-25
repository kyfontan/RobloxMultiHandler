/*
 * Stub <dwmapi.h> for old MinGW headers that lack it.
 * Only declares what Dear ImGui's Win32 backend touches.
 * All functions are no-op stubs returning success; we never call
 * the Dwm helpers anyway (the only consumer is
 * ImGui_ImplWin32_EnableAlphaCompositing(), which we don't invoke).
 */
#ifndef _DWMAPI_H_STUB_
#define _DWMAPI_H_STUB_

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DWM_BB_ENABLE
#define DWM_BB_ENABLE 0x00000001
#endif
#ifndef DWM_BB_BLURREGION
#define DWM_BB_BLURREGION 0x00000002
#endif
#ifndef DWM_BB_TRANSITIONONMAXIMIZED
#define DWM_BB_TRANSITIONONMAXIMIZED 0x00000004
#endif

typedef struct _DWM_BLURBEHIND {
    DWORD dwFlags;
    BOOL  fEnable;
    HRGN  hRgnBlur;
    BOOL  fTransitionOnMaximized;
} DWM_BLURBEHIND, *PDWM_BLURBEHIND;

static __inline HRESULT WINAPI DwmIsCompositionEnabled(BOOL *p) {
    if (p) *p = TRUE; return S_OK;
}
static __inline HRESULT WINAPI DwmGetColorizationColor(DWORD *c, BOOL *o) {
    if (c) *c = 0; if (o) *o = FALSE; return S_OK;
}
static __inline HRESULT WINAPI DwmEnableBlurBehindWindow(HWND h, const DWM_BLURBEHIND *b) {
    (void)h; (void)b; return S_OK;
}

#ifdef __cplusplus
}
#endif

#endif
