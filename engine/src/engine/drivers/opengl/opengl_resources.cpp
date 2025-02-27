#include "opengl_resources.h"

#include "opengl_helpers.h"

namespace my {

void OpenGlBuffer::Clear() {
    if (handle) {
        glDeleteBuffers(1, &handle);
        handle = 0;
    }
}

void OpenGlGpuTexture::Clear() {
    if (handle) {
        glDeleteTextures(1, &handle);
        handle = 0;
        residentHandle = 0;
    }
}

void OpenGlFramebuffer::Clear() {
    if (handle) {
        glDeleteFramebuffers(1, &handle);
        handle = 0;
    }
}

void OpenGlUniformBuffer::Clear() {
    if (handle) {
        glDeleteBuffers(1, &handle);
        handle = 0;
    }
}

void OpenGlStructuredBuffer::Clear() {
    if (handle) {
        glDeleteBuffers(1, &handle);
        handle = 0;
    }
}

}  // namespace my
