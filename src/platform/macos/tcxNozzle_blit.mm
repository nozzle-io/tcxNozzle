#import <Metal/Metal.h>
#include <cstdint>

namespace tcx::blit {

static id<MTLCommandQueue> get_shared_queue(id<MTLDevice> device) {
    static id<MTLCommandQueue> queue = nil;
    if (!queue) {
        queue = [device newCommandQueue];
    }
    return queue;
}

bool copy_gpu_texture(void *dst, const void *src, int width, int height, uint32_t nozzle_format) {
    if (!dst || !src || width <= 0 || height <= 0) {
        return false;
    }

    id<MTLTexture> dst_texture = (__bridge id<MTLTexture>)dst;
    id<MTLTexture> src_texture = (__bridge id<MTLTexture>)src;

    if (!dst_texture || !src_texture) {
        return false;
    }

    (void)nozzle_format;

    id<MTLDevice> device = [src_texture device];
    if (!device) {
        return false;
    }

    id<MTLCommandQueue> queue = get_shared_queue(device);
    if (!queue) {
        return false;
    }

    id<MTLCommandBuffer> command_buffer = [queue commandBuffer];
    if (!command_buffer) {
        return false;
    }

    id<MTLBlitCommandEncoder> encoder = [command_buffer blitCommandEncoder];
    if (!encoder) {
        return false;
    }

    [encoder copyFromTexture:src_texture
                 sourceSlice:0
                 sourceLevel:0
                sourceOrigin:MTLOriginMake(0, 0, 0)
                  sourceSize:MTLSizeMake(width, height, 1)
                   toTexture:dst_texture
            destinationSlice:0
            destinationLevel:0
           destinationOrigin:MTLOriginMake(0, 0, 0)];

    [encoder endEncoding];
    [command_buffer commit];
    [command_buffer waitUntilCompleted];

    return true;
}

} // namespace tcx::blit
