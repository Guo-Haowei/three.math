#include "opengl_resources.h"

#include "opengl_prerequisites.h"

namespace my {

void OpenGlGpuTexture::Clear() {
    if (handle) {
        glDeleteTextures(1, &handle);
        handle = 0;
        residentHandle = 0;
    }
}
void OpenGlSubpass::Clear() {
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
