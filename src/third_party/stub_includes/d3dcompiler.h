/*
 * Minimal <d3dcompiler.h> stub for old MinGW: just declares D3DCompile.
 * ID3DBlob / D3D_SHADER_MACRO / ID3DInclude come from <d3dcommon.h>.
 */
#ifndef RH_D3DCOMPILER_H_STUB
#define RH_D3DCOMPILER_H_STUB

#include <d3dcommon.h>

#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI D3DCompile(
    LPCVOID                 pSrcData,
    SIZE_T                  SrcDataSize,
    LPCSTR                  pSourceName,
    const D3D_SHADER_MACRO *pDefines,
    ID3DInclude            *pInclude,
    LPCSTR                  pEntrypoint,
    LPCSTR                  pTarget,
    UINT                    Flags1,
    UINT                    Flags2,
    ID3DBlob              **ppCode,
    ID3DBlob              **ppErrorMsgs);

#ifdef __cplusplus
}
#endif

#endif
