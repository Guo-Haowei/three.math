#include "core/base/rid_owner.h"
#include "core/framework/graphics_manager.h"

// @TODO: fix
struct OpenGLMeshBuffers : public my::MeshBuffers {
    uint32_t vao = 0;
    uint32_t ebo = 0;
    uint32_t vbos[6] = { 0 };
};

namespace my {

class OpenGLGraphicsManager : public GraphicsManager {
public:
    OpenGLGraphicsManager() : GraphicsManager("OpenGLGraphicsManager", Backend::OPENGL) {}

    bool initialize_internal() final;
    void finalize() final;

    void set_render_target(const Subpass* p_subpass, int p_index, int p_mip_level) final;
    void clear(const Subpass* p_subpass, uint32_t p_flags, float* p_clear_color) final;
    void set_viewport(const Viewport& p_viewport) final;

    void set_mesh(const MeshBuffers* p_mesh) final;
    void draw_elements(uint32_t p_count, uint32_t offset) final;

    std::shared_ptr<UniformBufferBase> create_uniform_buffer(int p_slot, size_t p_capacity) final;
    void update_uniform_buffer(const UniformBufferBase* p_buffer, const void* p_data, size_t p_size) final;

    std::shared_ptr<Texture> create_texture(const TextureDesc& p_texture_desc, const SamplerDesc& p_sampler_desc) final;
    std::shared_ptr<Subpass> create_subpass(const SubpassDesc& p_desc) final;

    // @TODO: refactor this
    void fill_material_constant_buffer(const MaterialComponent* material, MaterialConstantBuffer& cb) final;

protected:
    void on_scene_change(const Scene& p_scene) final;
    void set_pipeline_state_impl(PipelineStateName p_name) final;
    void render() final;

    void createGpuResources();
    void destroyGpuResources();

    // @TODO: rename
    RIDAllocator<OpenGLMeshBuffers> m_meshes;
};

}  // namespace my
