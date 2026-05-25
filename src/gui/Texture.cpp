/*
 * PNG/JPEG bytes -> ID3D11ShaderResourceView via WIC.
 * WIC ships with Windows since XP SP3 -- no extra runtime needed.
 */
#include "Texture.hpp"

#include <windows.h>
#include <wincodec.h>
#include <objbase.h>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")

namespace rh::gui {

namespace {

bool wic_decode_to_bgra(const void *bytes, size_t len,
                        std::vector<unsigned char> &out_pixels,
                        UINT &width, UINT &height)
{
    IWICImagingFactory *factory = nullptr;
    if (CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&factory)) != S_OK || !factory) return false;

    IWICStream *stream = nullptr;
    if (factory->CreateStream(&stream) != S_OK || !stream) { factory->Release(); return false; }
    if (stream->InitializeFromMemory((BYTE *)const_cast<void *>(bytes), (DWORD)len) != S_OK) {
        stream->Release(); factory->Release(); return false;
    }

    IWICBitmapDecoder *decoder = nullptr;
    if (factory->CreateDecoderFromStream(stream, nullptr,
            WICDecodeMetadataCacheOnLoad, &decoder) != S_OK || !decoder) {
        stream->Release(); factory->Release(); return false;
    }

    IWICBitmapFrameDecode *frame = nullptr;
    if (decoder->GetFrame(0, &frame) != S_OK || !frame) {
        decoder->Release(); stream->Release(); factory->Release(); return false;
    }

    IWICFormatConverter *converter = nullptr;
    bool ok = false;
    if (factory->CreateFormatConverter(&converter) == S_OK && converter) {
        if (converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA,
                                  WICBitmapDitherTypeNone, nullptr, 0.0f,
                                  WICBitmapPaletteTypeMedianCut) == S_OK) {
            UINT w = 0, h = 0;
            if (converter->GetSize(&w, &h) == S_OK && w && h) {
                const UINT stride = w * 4;
                out_pixels.resize((size_t)stride * h);
                if (converter->CopyPixels(nullptr, stride,
                        (UINT)out_pixels.size(), out_pixels.data()) == S_OK) {
                    width  = w;
                    height = h;
                    ok = true;
                }
            }
        }
        converter->Release();
    }
    frame->Release();
    decoder->Release();
    stream->Release();
    factory->Release();
    return ok;
}

} /* anon */

ID3D11ShaderResourceView *create_texture_from_image_bytes(
    ID3D11Device *device, const void *bytes, size_t len,
    int *out_w, int *out_h)
{
    if (!device || !bytes || !len) return nullptr;

    std::vector<unsigned char> pixels;
    UINT w = 0, h = 0;
    if (!wic_decode_to_bgra(bytes, len, pixels, w, h)) return nullptr;

    D3D11_TEXTURE2D_DESC td = {};
    td.Width      = w;
    td.Height     = h;
    td.MipLevels  = 1;
    td.ArraySize  = 1;
    td.Format     = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage      = D3D11_USAGE_IMMUTABLE;
    td.BindFlags  = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem     = pixels.data();
    init.SysMemPitch = w * 4;

    ID3D11Texture2D *tex = nullptr;
    if (device->CreateTexture2D(&td, &init, &tex) != S_OK || !tex) return nullptr;

    D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
    sd.Format = td.Format;
    sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sd.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView *srv = nullptr;
    HRESULT hr = device->CreateShaderResourceView(tex, &sd, &srv);
    tex->Release();
    if (hr != S_OK) return nullptr;

    if (out_w) *out_w = (int)w;
    if (out_h) *out_h = (int)h;
    return srv;
}

} /* namespace rh::gui */
