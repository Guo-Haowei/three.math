#include "engine/assets/asset_loader.h"
#include "engine/scene/scene.h"

namespace tinygltf {
class Model;
struct Animation;
struct Mesh;
}  // namespace tinygltf

namespace my {

class LoaderTinyGLTF : public IAssetLoader {
public:
    using IAssetLoader::IAssetLoader;

    static std::unique_ptr<IAssetLoader> CreateLoader(const IAsset::Meta& p_meta) {
        return std::make_unique<LoaderTinyGLTF>(p_meta);
    }

    auto Load() -> Result<IAsset*> override;

protected:
    void ProcessNode(int p_node_index, ecs::Entity p_parent);
    void ProcessMesh(const tinygltf::Mesh& p_gltf_mesh, int p_id);
    void ProcessAnimation(const tinygltf::Animation& p_gltf_anim, int p_id);

    std::unordered_map<int, ecs::Entity> m_entityMap;
    std::shared_ptr<tinygltf::Model> m_model;
    Scene* m_scene;
};

}  // namespace my
