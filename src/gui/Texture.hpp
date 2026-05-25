#pragma once
#include <d3d11.h>
#include <stddef.h>

namespace rh::gui {

/* Decode PNG/JPEG bytes via WIC and upload as an SRV. Returns nullptr on failure.
   Caller releases the SRV when done. width/height are optional out-params. */
ID3D11ShaderResourceView *create_texture_from_image_bytes(
    ID3D11Device  *device,
    const void    *bytes,
    size_t         len,
    int           *out_w = nullptr,
    int           *out_h = nullptr);

} /* namespace rh::gui */
