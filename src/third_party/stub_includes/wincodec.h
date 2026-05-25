/*
 * Minimal hand-rolled wincodec.h for MinGW.org, which ships no WIC headers.
 *
 * Declares ONLY what src/gui/Texture.cpp touches: enough of the WIC COM
 * interfaces to decode an in-memory PNG/JPEG to 32bpp PBGRA.  Interfaces are
 * laid out so every method we call sits at its real vtable slot -- methods we
 * never call are kept as correctly-ordered placeholders (signature is
 * irrelevant to a vtable slot, only declaration order is).  Do NOT call a
 * placeholder; its argument list is a stub.
 *
 * C++ / non-CINTERFACE only (Texture.cpp is the sole consumer).
 */
#ifndef RH_WINCODEC_H
#define RH_WINCODEC_H

#include <windows.h>    /* GUID (no standalone guiddef.h on MinGW.org) */
#include <unknwn.h>     /* IUnknown      */
#include <objidl.h>     /* IStream       */

#if !defined(__cplusplus) || defined(CINTERFACE)
#error "this minimal wincodec.h stub supports C++ COM (non-CINTERFACE) only"
#endif

/* --- enums / typedefs --------------------------------------------------- */

typedef GUID        WICPixelFormatGUID;
typedef const GUID &REFWICPixelFormatGUID;

typedef struct WICRect {
    INT X;
    INT Y;
    INT Width;
    INT Height;
} WICRect;

typedef enum WICDecodeOptions {
    WICDecodeMetadataCacheOnDemand = 0,
    WICDecodeMetadataCacheOnLoad   = 0x1
} WICDecodeOptions;

typedef enum WICBitmapDitherType {
    WICBitmapDitherTypeNone     = 0,
    WICBitmapDitherTypeSolid    = 0,
    WICBitmapDitherTypeOrdered4x4 = 0x1
} WICBitmapDitherType;

typedef enum WICBitmapPaletteType {
    WICBitmapPaletteTypeCustom    = 0,
    WICBitmapPaletteTypeMedianCut = 0x1
} WICBitmapPaletteType;

/* Forward decls for interfaces referenced only as opaque pointers. */
struct IWICPalette;
struct IWICBitmapFrameDecode;
struct IWICFormatConverter;
struct IWICBitmapDecoder;
struct IWICStream;

/* --- IWICBitmapSource --------------------------------------------------- */
struct IWICBitmapSource : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetSize(UINT *puiWidth, UINT *puiHeight) = 0;   /* 1 */
    virtual HRESULT STDMETHODCALLTYPE GetPixelFormat(WICPixelFormatGUID *pPixelFormat) = 0; /* 2 */
    virtual HRESULT STDMETHODCALLTYPE GetResolution(double *pDpiX, double *pDpiY) = 0; /* 3 */
    virtual HRESULT STDMETHODCALLTYPE CopyPalette(IWICPalette *pIPalette) = 0;         /* 4 */
    virtual HRESULT STDMETHODCALLTYPE CopyPixels(const WICRect *prc, UINT cbStride,
                                                 UINT cbBufferSize, BYTE *pbBuffer) = 0; /* 5 */
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IWICBitmapSource, 0x00000120, 0xa8f2, 0x4877, 0xba,0x0a, 0xfd,0x2b,0x66,0x45,0xfb,0x94)
#endif

/* --- IWICBitmapFrameDecode (used only as an IWICBitmapSource) ----------- */
struct IWICBitmapFrameDecode : public IWICBitmapSource {
    /* own methods unused; left out -- only the IWICBitmapSource base is touched */
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IWICBitmapFrameDecode, 0x3b16811b, 0x6a43, 0x4ec9, 0xa8,0x13, 0x3d,0x93,0x0c,0x13,0xb9,0x40)
#endif

/* --- IWICFormatConverter ------------------------------------------------ */
struct IWICFormatConverter : public IWICBitmapSource {
    virtual HRESULT STDMETHODCALLTYPE Initialize(IWICBitmapSource *pISource,
                                                 REFWICPixelFormatGUID dstFormat,
                                                 WICBitmapDitherType dither,
                                                 IWICPalette *pIPalette,
                                                 double alphaThresholdPercent,
                                                 WICBitmapPaletteType paletteTranslate) = 0; /* 1 */
    virtual HRESULT STDMETHODCALLTYPE CanConvert(void) = 0;                                   /* 2 (placeholder) */
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IWICFormatConverter, 0x00000301, 0xa8f2, 0x4877, 0xba,0x0a, 0xfd,0x2b,0x66,0x45,0xfb,0x94)
#endif

/* --- IWICBitmapDecoder -------------------------------------------------- */
struct IWICBitmapDecoder : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE QueryCapability(void) = 0;        /* 1  placeholder */
    virtual HRESULT STDMETHODCALLTYPE Initialize(void) = 0;            /* 2  placeholder */
    virtual HRESULT STDMETHODCALLTYPE GetContainerFormat(void) = 0;    /* 3  placeholder */
    virtual HRESULT STDMETHODCALLTYPE GetDecoderInfo(void) = 0;        /* 4  placeholder */
    virtual HRESULT STDMETHODCALLTYPE CopyPalette(void) = 0;           /* 5  placeholder */
    virtual HRESULT STDMETHODCALLTYPE GetMetadataQueryReader(void) = 0;/* 6  placeholder */
    virtual HRESULT STDMETHODCALLTYPE GetPreview(void) = 0;            /* 7  placeholder */
    virtual HRESULT STDMETHODCALLTYPE GetColorContexts(void) = 0;      /* 8  placeholder */
    virtual HRESULT STDMETHODCALLTYPE GetThumbnail(void) = 0;          /* 9  placeholder */
    virtual HRESULT STDMETHODCALLTYPE GetFrameCount(void) = 0;         /* 10 placeholder */
    virtual HRESULT STDMETHODCALLTYPE GetFrame(UINT index,
                                               IWICBitmapFrameDecode **ppIBitmapFrame) = 0; /* 11 */
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IWICBitmapDecoder, 0x9edde9e7, 0x8dee, 0x47ea, 0x99,0xdf, 0xe6,0xfa,0xf2,0xed,0x44,0xbf)
#endif

/* --- IWICStream (extends IStream) --------------------------------------- */
struct IWICStream : public IStream {
    virtual HRESULT STDMETHODCALLTYPE InitializeFromIStream(void) = 0;   /* 1 placeholder */
    virtual HRESULT STDMETHODCALLTYPE InitializeFromFilename(void) = 0;  /* 2 placeholder */
    virtual HRESULT STDMETHODCALLTYPE InitializeFromMemory(BYTE *pbBuffer,
                                                           DWORD cbBufferSize) = 0; /* 3 */
    virtual HRESULT STDMETHODCALLTYPE InitializeFromIStreamRegion(void) = 0; /* 4 placeholder */
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IWICStream, 0x135ff860, 0x22b7, 0x4ddf, 0xb0,0xf6, 0x21,0x8f,0x4f,0x29,0x9a,0x43)
#endif

/* --- IWICImagingFactory ------------------------------------------------- */
struct IWICImagingFactory : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateDecoderFromFilename(void) = 0;    /* 1  placeholder */
    virtual HRESULT STDMETHODCALLTYPE CreateDecoderFromStream(IStream *pIStream,
                                                              const GUID *pguidVendor,
                                                              WICDecodeOptions metadataOptions,
                                                              IWICBitmapDecoder **ppIDecoder) = 0; /* 2 */
    virtual HRESULT STDMETHODCALLTYPE CreateDecoderFromFileHandle(void) = 0;  /* 3  placeholder */
    virtual HRESULT STDMETHODCALLTYPE CreateComponentInfo(void) = 0;          /* 4  placeholder */
    virtual HRESULT STDMETHODCALLTYPE CreateDecoder(void) = 0;                /* 5  placeholder */
    virtual HRESULT STDMETHODCALLTYPE CreateEncoder(void) = 0;                /* 6  placeholder */
    virtual HRESULT STDMETHODCALLTYPE CreatePalette(void) = 0;                /* 7  placeholder */
    virtual HRESULT STDMETHODCALLTYPE CreateFormatConverter(
                                          IWICFormatConverter **ppIFormatConverter) = 0;        /* 8 */
    virtual HRESULT STDMETHODCALLTYPE CreateBitmapScaler(void) = 0;           /* 9  placeholder */
    virtual HRESULT STDMETHODCALLTYPE CreateBitmapClipper(void) = 0;          /* 10 placeholder */
    virtual HRESULT STDMETHODCALLTYPE CreateBitmapFlipRotator(void) = 0;      /* 11 placeholder */
    virtual HRESULT STDMETHODCALLTYPE CreateStream(IWICStream **ppIWICStream) = 0;               /* 12 */
    /* remaining factory methods (CreateColorContext .. CreateQueryWriterFromReader) unused */
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IWICImagingFactory, 0xec5ec8a9, 0xc395, 0x4314, 0x9c,0x77, 0x54,0xd7,0xa9,0x35,0xff,0x70)
#endif

/* --- GUID constants used by Texture.cpp (single TU -> internal linkage) -- */
static const CLSID CLSID_WICImagingFactory =
    { 0xcacaf262, 0x9370, 0x4615, { 0xa1,0x3b, 0x9f,0x55,0x39,0xda,0x4c,0x0a } };

static const GUID GUID_WICPixelFormat32bppPBGRA =
    { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1,0x85, 0x3d,0x77,0x76,0x8d,0xc9,0x10 } };

#endif /* RH_WINCODEC_H */
