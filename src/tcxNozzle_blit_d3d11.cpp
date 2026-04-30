#include <d3d11.h>
#include <cstdint>

namespace tcx::blit {

bool copy_gpu_texture(void *dst, const void *src, int width, int height, uint32_t nozzle_format) {
    (void)width;
    (void)height;
    (void)nozzle_format;

    if (!dst || !src) {
        return false;
    }

    auto *dst_texture = static_cast<ID3D11Texture2D *>(dst);
    auto *src_texture = static_cast<ID3D11Texture2D *>(const_cast<void *>(src));

    ID3D11Device *device = nullptr;
    src_texture->GetDevice(&device);
    if (!device) {
        return false;
    }

    ID3D11DeviceContext *context = nullptr;
    device->GetImmediateContext(&context);
    if (!context) {
        device->Release();
        return false;
    }

    context->CopySubresourceRegion(dst_texture, 0, 0, 0, 0, src_texture, 0, nullptr);

    device->Release();

    return true;
}

} // namespace tcx::blit
