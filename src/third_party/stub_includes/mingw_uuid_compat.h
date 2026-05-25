/*
 * __uuidof / IID_PPV_ARGS shim for old MinGW.org (GCC 6.3, no MinGW-w64 CRT).
 *
 * The WIDL-generated DX/WIC stubs in this directory already carry
 *     #ifdef __CRT_UUID_DECL
 *         __CRT_UUID_DECL(Interface, 0x..., ...)
 *     #endif
 * blocks -- they were produced for MinGW-w64, whose CRT defines that macro
 * plus the __mingw_uuidof<> machinery that __uuidof / IID_PPV_ARGS rely on.
 * MinGW.org ships none of it, so those blocks are skipped and IID_PPV_ARGS is
 * undefined.  We provide exactly the MinGW-w64 mechanism, so every stub's
 * __CRT_UUID_DECL activates and __uuidof / IID_PPV_ARGS resolve per-interface.
 *
 * MUST be included before any d3d11.h / dxgi.h / wincodec.h stub.  win_compat.h
 * (force-included via -include) pulls it in, which guarantees that ordering.
 */
#ifndef RH_MINGW_UUID_COMPAT_H
#define RH_MINGW_UUID_COMPAT_H

#ifdef __cplusplus

#include <windows.h>   /* GUID, IID (MinGW.org has no standalone guiddef.h) */
#include <unknwn.h>    /* IUnknown (for the IID_PPV_ARGS_Helper static_cast) */

extern "C++" {
    template<typename T> const GUID &__mingw_uuidof();
}

#ifndef __CRT_UUID_DECL
#define __CRT_UUID_DECL(type, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)      \
    extern "C++" {                                                           \
        template<> inline const GUID &__mingw_uuidof<type>() {               \
            static const IID __uuid_inst =                                   \
                { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } };           \
            return __uuid_inst;                                              \
        }                                                                    \
        template<> inline const GUID &__mingw_uuidof<type *>() {             \
            return __mingw_uuidof<type>();                                   \
        }                                                                    \
    }
#endif

#ifndef __uuidof
#define __uuidof(expr) __mingw_uuidof<__typeof__(expr)>()
#endif

#ifndef IID_PPV_ARGS
extern "C++" {
    template<typename T> void **IID_PPV_ARGS_Helper(T **pp) {
        (void)static_cast<IUnknown *>(*pp);   /* type-check: T must be a COM iface */
        return reinterpret_cast<void **>(pp);
    }
}
#define IID_PPV_ARGS(ppType) __uuidof(**(ppType)), IID_PPV_ARGS_Helper(ppType)
#endif

#endif /* __cplusplus */

#endif /* RH_MINGW_UUID_COMPAT_H */
