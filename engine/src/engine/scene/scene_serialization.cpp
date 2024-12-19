#include "scene_serialization.h"

#include <fstream>

#include "engine/core/framework/asset_registry.h"
#include "engine/core/io/archive.h"
#include "engine/core/io/file_access.h"
#include "engine/core/string/string_utils.h"
#include "engine/scene/scene.h"
#include "engine/systems/serialization/serialization.inl"

namespace my {

#define SCENE_DBG_LOG(...) LOG_VERBOSE(__VA_ARGS__)

#pragma region VERSION_HISTORY
// SCENE_VERSION history
// version 1: initial version
// version 2: don't serialize scene.m_bound
// version 3: light component atten
// version 4: light component flags
// version 5: add validation
// version 6: add collider component
// version 7: add enabled to material
// version 8: add particle emitter
// version 9: add ParticleEmitterComponent.gravity
// version 10: add ForceFieldComponent
// version 11: add ScriptFieldComponent
// version 12: add CameraComponent
// version 13: add SoftBodyComponent
// version 14: modify RigidBodyComponent
// version 15: add predefined shadow region to lights
// version 16: change scene binary representation
#pragma endregion VERSION_HISTORY
static constexpr uint32_t SCENE_VERSION = 16;
static constexpr char SCENE_MAGIC[] = "xBScene";
static constexpr char SCENE_GUARD_MESSAGE[] = "Should see this message";
static constexpr uint64_t HAS_NEXT_FLAG = 6368519827137030510;
static constexpr char BIN_GUARD_MAGIC[] = "SEETHIS";

Result<void> SaveSceneBinary(const std::string& p_path, Scene& p_scene) {
    Archive archive;
    if (auto res = archive.OpenWrite(p_path); !res) {
        return HBN_ERROR(res.error());
    }

    archive << SCENE_MAGIC;
    archive << SCENE_VERSION;
    archive << ecs::Entity::GetSeed();
    archive << p_scene.m_root;

    archive << SCENE_GUARD_MESSAGE;

    for (const auto& it : p_scene.GetLibraryEntries()) {
        archive << HAS_NEXT_FLAG;
        archive << it.first;  // write name
        it.second.m_manager->Serialize(archive, SCENE_VERSION);
    }
    archive << uint64_t(0);
    return Result<void>();
}

Result<void> LoadSceneBinary(const std::string& p_path, Scene& p_scene) {
    Archive archive;
    if (auto res = archive.OpenRead(p_path); !res) {
        return HBN_ERROR(res.error());
    }

    char magic[sizeof(SCENE_MAGIC)]{ 0 };
    if (!archive.Read(magic) || !StringUtils::StringEqual(magic, SCENE_MAGIC)) {
        return HBN_ERROR(ErrorCode::ERR_FILE_CORRUPT, "file corrupted, magic is not '{}'", SCENE_MAGIC);
    }

    uint32_t version;
    if (!archive.Read(version) || version > SCENE_VERSION) {
        return HBN_ERROR(ErrorCode::ERR_FILE_CORRUPT, "incorrect scene version {}, current version is {}", version, SCENE_VERSION);
    }

    SCENE_DBG_LOG("loading scene '{}', version: {}", p_path, version);

    uint32_t seed = ecs::Entity::MAX_ID;
    if (!archive.Read(seed)) {
        return HBN_ERROR(ErrorCode::ERR_FILE_CORRUPT, "failed to read seed");
    }

    ecs::Entity::SetSeed(seed);

    archive >> p_scene.m_root;

    char guard_message[sizeof(SCENE_GUARD_MESSAGE)]{ 0 };
    archive >> guard_message;
    if (!StringUtils::StringEqual(guard_message, SCENE_GUARD_MESSAGE)) {
        return HBN_ERROR(ErrorCode::ERR_FILE_CORRUPT);
    }

    for (;;) {
        uint64_t has_next = 0;
        archive >> has_next;
        if (has_next != HAS_NEXT_FLAG) {
            return Result<void>();
        }

        std::string key;
        archive >> key;

        SCENE_DBG_LOG("Loading Component {}", key);

        auto it = p_scene.GetLibraryEntries().find(key);
        if (it == p_scene.GetLibraryEntries().end()) {
            return HBN_ERROR(ErrorCode::ERR_FILE_CORRUPT, "entry '{}' not found", key);
        }
        if (!it->second.m_manager->Serialize(archive, version)) {
            return HBN_ERROR(ErrorCode::ERR_FILE_CORRUPT, "failed to serialize '{}'", key);
        }
    }
}

template<typename T>
void DumpAny(YAML::Emitter& p_out, const T& p_value) {
    p_out << p_value;
}

template<typename T>
[[nodiscard]] bool UndumpAny(const YAML::Node& p_node, T& p_value) {
    p_value = p_node.as<T>();
    return true;
}

template<typename T>
concept EnumType = std::is_enum_v<T>;

template<typename T>
concept ContainerType = requires(T t) {
    typename T::iterator;
    { t.begin() } -> std::same_as<typename T::iterator>;
    { t.end() } -> std::same_as<typename T::iterator>;
};

static_assert(ContainerType<std::vector<int>>);
static_assert(ContainerType<int> == false);

template<EnumType T>
void DumpAny(YAML::Emitter& p_out, const T& p_value) {
    p_out << std::to_underlying(p_value);
}

template<EnumType T>
[[nodiscard]] bool UndumpAny(const YAML::Node& p_node, T& p_value) {
    static_assert(sizeof(T) <= sizeof(uint32_t));
    p_value = static_cast<T>(p_node.as<uint32_t>());
    return true;
}
//
// template<ContainerType T>
// void DumpAny(YAML::Emitter& p_output, const T& p_array, Archive&) {
//}

#define DUMP_BEGIN(EMITTER, BINARY) \
    auto& _out_ = EMITTER;          \
    ((void)(BINARY));               \
    constexpr bool _true_ = true
#define DUMP_END() return _true_

#define DUMP_KEY_VALUE(KEY, VALUE) DumpKeyValuePair(_out_, KEY, VALUE)
#define DUMP_KEY(KEY)              YAML::Key << (KEY) << YAML::Value

#define UNDUMP_BEGIN(NODE, BINARY) \
    auto& _node_ = NODE;           \
    ((void)(BINARY));              \
    bool _ok_ = true
#define UNDUMP_END() return _ok_

#define UNDUMP_KEY_VALUE(KEY, VALUE) _ok_ = _ok_ && UndumpKeyValuePair(_node_, KEY, VALUE)

template<>
void DumpAny(YAML::Emitter& p_out, const ecs::Entity& p_value) {
    p_out << p_value.GetId();
}

template<>
[[nodiscard]] bool UndumpAny(const YAML::Node& p_node, ecs::Entity& p_value) {
    p_value = ecs::Entity(p_node.as<uint32_t>());
    return true;
}

template<>
void DumpAny(YAML::Emitter& p_out, const Degree& p_value) {
    p_out << p_value.GetDegree();
}

template<>
[[nodiscard]] bool UndumpAny(const YAML::Node& p_node, Degree& p_value) {
    p_value = Degree(p_node.as<float>());
    return true;
}

template<>
void DumpAny(YAML::Emitter& p_out, const Vector3f& p_value) {
    p_out.SetSeqFormat(YAML::Flow);
    p_out << YAML::BeginSeq;
    p_out << p_value.x;
    p_out << p_value.y;
    p_out << p_value.z;
    p_out << YAML::EndSeq;
}

template<>
[[nodiscard]] bool UndumpAny(const YAML::Node& p_node, Vector3f& p_value) {
    if (!p_node || !(p_node.IsSequence() && p_node.size() == 3)) {
        return false;
    }

    p_value.x = p_node[0].as<float>();
    p_value.y = p_node[1].as<float>();
    p_value.z = p_node[2].as<float>();
    return true;
}

template<>
void DumpAny(YAML::Emitter& p_out, const Vector4f& p_value) {
    p_out.SetSeqFormat(YAML::Flow);
    p_out << YAML::BeginSeq;
    p_out << p_value.x;
    p_out << p_value.y;
    p_out << p_value.z;
    p_out << p_value.w;
    p_out << YAML::EndSeq;
}

template<>
[[nodiscard]] bool UndumpAny(const YAML::Node& p_node, Vector4f& p_value) {
    if (!p_node || !(p_node.IsSequence() && p_node.size() == 4)) {
        return false;
    }

    p_value.x = p_node[0].as<float>();
    p_value.y = p_node[1].as<float>();
    p_value.z = p_node[2].as<float>();
    p_value.w = p_node[3].as<float>();
    return true;
}

template<>
void DumpAny(YAML::Emitter& p_out, const AABB& p_value) {
    p_out << YAML::BeginMap;
    p_out << DUMP_KEY("min");
    DumpAny<Vector3f>(p_out, p_value.GetMin());
    p_out << DUMP_KEY("max");
    DumpAny<Vector3f>(p_out, p_value.GetMax());
    p_out << YAML::EndMap;
}

template<>
[[nodiscard]] bool UndumpAny(const YAML::Node& p_node, AABB& p_value) {
    if (!p_node || !p_node.IsMap()) {
        return false;
    }

    Vector3f min, max;
    bool ok = UndumpAny(p_node["min"], min);
    ok = ok && UndumpAny(p_node["max"], max);
    if (!ok) {
        return false;
    }

    p_value = AABB(min, max);
    return true;
}

// @TODO: proper error handling
template<typename T>
void DumpVectorBinary(YAML::Emitter& p_out, const std::vector<T>& p_value, FileAccess* p_binary) {
    const size_t size_in_byte = sizeof(T) * p_value.size();
    DEV_ASSERT(size_in_byte);
    const size_t size = p_value.size();
    p_binary->Write(BIN_GUARD_MAGIC);
    p_binary->Write(size);
    const auto offset = p_binary->Tell();
    DEV_ASSERT(offset > 0);
    p_binary->WriteBuffer(p_value.data(), size_in_byte);

    p_out << YAML::BeginMap;
    p_out << DUMP_KEY("offset") << offset;
    p_out << DUMP_KEY("length") << size_in_byte;
    p_out << YAML::EndMap;
}

template<typename T>
bool UndumpVectorBinary(const YAML::Node& p_node, std::vector<T>& p_value, FileAccess* p_binary) {
    constexpr size_t internal_offset = sizeof(BIN_GUARD_MAGIC) + sizeof(size_t);
    // can have undefind value
    if (!p_node) {
        return true;
    }
    ERR_FAIL_COND_V(!p_node.IsMap(), false);
    size_t offset = 0;
    size_t length = 0;
    ERR_FAIL_COND_V(!UndumpAny(p_node["offset"], offset), false);
    ERR_FAIL_COND_V(!UndumpAny(p_node["length"], length), false);
    ERR_FAIL_COND_V(length == 0, false);
    ERR_FAIL_COND_V(length % sizeof(T) != 0, false);
    ERR_FAIL_COND_V(offset < internal_offset, false);

    const int seek = p_binary->Seek((long)(offset - internal_offset));
    ERR_FAIL_COND_V(seek != 0, false);

    char magic[sizeof(BIN_GUARD_MAGIC)];
    // @TODO: small string optimization for StringEqual
    if (!p_binary->Read(magic) || !StringUtils::StringEqual(magic, BIN_GUARD_MAGIC)) {
        return false;
    }
    size_t read_length = 0;
    if (!p_binary->Read(read_length) || read_length == length) {
        return false;
    }
    p_value.resize(length / sizeof(T));
    p_binary->ReadBuffer(p_value.data(), length);
    return true;
}

template<typename T>
void DumpKeyValuePair(YAML::Emitter& p_out, const char* p_key, const T& p_value) {
    p_out << YAML::Key << p_key << YAML::Value;
    DumpAny(p_out, p_value);
}

template<typename T>
[[nodiscard]] bool UndumpKeyValuePair(const YAML::Node& p_node, const char* p_key, T& p_value) {
    const auto& node = p_node[p_key];
    if (node) {
        return UndumpAny(node, p_value);
    }
    return false;
}

template<Serializable T>
void DumpComponent(YAML::Emitter& p_out,
                   const char* p_name,
                   ecs::Entity p_entity,
                   const Scene& p_scene,
                   FileAccess* p_bianary) {

    const T* component = p_scene.GetComponent<T>(p_entity);
    if (component) {
        p_out << DUMP_KEY(p_name);
        serialize::SerializeYamlContext context;
        context.file = p_bianary;
        serialize::SerializeYaml(p_out, *component, context);
    }
}

template<Serializable T>
static Result<void> LoadComponent(const YAML::Node& p_node,
                                  const char* p_key,
                                  ecs::Entity p_id,
                                  uint32_t p_version,
                                  Scene& p_scene,
                                  FileAccess* p_binary) {
    const auto& node = p_node[p_key];
    if (!node.IsDefined()) {
        return Result<void>();
    }

    if (!node.IsMap()) {
        return HBN_ERROR(ErrorCode::ERR_PARSE_ERROR, "entity {} has invalid '{}'", p_id.GetId(), p_key);
    }

    // @TODO: reserve
    T& component = p_scene.Create<T>(p_id);
    if (!component.Undump(node, p_binary, p_version)) {
        return HBN_ERROR(ErrorCode::ERR_PARSE_ERROR, "entity {} has invalid '{}'", p_id.GetId(), p_key);
    }

    return Result<void>();
}

Result<void> LoadSceneText(const std::string& p_path, Scene& p_scene) {
    unused(p_scene);

    auto res = FileAccess::Open(p_path, FileAccess::READ);
    if (!res) {
        return HBN_ERROR(res.error());
    }

    auto file = *res;
    const size_t size = file->GetLength();

    std::string buffer;
    buffer.resize(size);
    if (auto read = file->ReadBuffer(buffer.data(), size); read != size) {
        return HBN_ERROR(ErrorCode::ERR_FILE_CANT_READ, "failed to read {} bytes from '{}'", size, p_path);
    }

    file->Close();
    const auto node = YAML::Load(buffer);

    YAML::Emitter out;
    auto version = node["version"].as<uint32_t>();
    if (version > SCENE_VERSION) {
        return HBN_ERROR(ErrorCode::ERR_FILE_CORRUPT, "incorrect version {}", version);
    }
    auto seed = node["seed"].as<uint32_t>();
    ecs::Entity::SetSeed(seed);

    ecs::Entity root(node["root"].as<uint32_t>());
    p_scene.m_root = root;

    auto binary_file = node["binary"].as<std::string>();
    res = FileAccess::Open(binary_file, FileAccess::READ);
    if (!res) {
        return HBN_ERROR(res.error());
    }

    file = *res;
    const auto& entities = node["entities"];
    if (!entities.IsSequence()) {
        return HBN_ERROR(ErrorCode::ERR_FILE_CORRUPT, "invalid format");
    }

    for (const auto& entity : entities) {
        if (!entity.IsMap()) {
            return HBN_ERROR(ErrorCode::ERR_FILE_CORRUPT, "invalid format");
        }

        // ecs::Entity id(entity["id"].as<uint32_t>());

        //#define REGISTER_COMPONENT(a, ...)                                                  \
//    do {                                                                            \
//        auto res2 = LoadComponent<a>(entity, #a, id, version, p_scene, file.get()); \
//        if (!res2) { return HBN_ERROR(res2.error()); }                              \
//    } while (0);
        //        REGISTER_COMPONENT_LIST
        // #undef REGISTER_COMPONENT
    }

    return Result<void>();
}

Result<void> SaveSceneText(const std::string& p_path, const Scene& p_scene) {
    // @TODO: remove this
    NameComponent::RegisterClass();
    HierarchyComponent::RegisterClass();
    TransformComponent::RegisterClass();

    std::unordered_set<uint32_t> entity_set;

    for (const auto& it : p_scene.GetLibraryEntries()) {
        auto& manager = it.second.m_manager;
        for (auto entity : manager->GetEntityArray()) {
            entity_set.insert(entity.GetId());
        }
    }

    std::vector<uint32_t> entity_array(entity_set.begin(), entity_set.end());
    std::sort(entity_array.begin(), entity_array.end());

    auto binary_path = std::format("{}{}", p_path, ".bin");
    Archive archive;
    if (auto res = archive.OpenWrite(binary_path); !res) {
        return HBN_ERROR(res.error());
    }

    YAML::Emitter out;
    out << YAML::BeginMap;
    out << DUMP_KEY("version") << SCENE_VERSION;
    out << DUMP_KEY("seed") << ecs::Entity::GetSeed();
    out << DUMP_KEY("root") << p_scene.m_root.GetId();
    out << DUMP_KEY("binary") << binary_path;

    out << DUMP_KEY("entities");
    out << YAML::BeginSeq;

    bool ok = true;
    ok = ok && archive.Write(SCENE_MAGIC);
    ok = ok && archive.Write(SCENE_VERSION);
    ok = ok && archive.Write(SCENE_GUARD_MESSAGE);
    if (!ok) {
        return HBN_ERROR(ErrorCode::ERR_FILE_CANT_WRITE, "failed to save file '{}'", p_path);
    }

    for (auto id : entity_array) {
        ecs::Entity entity{ id };

        out << YAML::BeginMap;
        out << DUMP_KEY("id") << id;

        out.SetSeqFormat(YAML::Flow);

        DumpComponent<NameComponent>(out, "NameComponent", entity, p_scene, archive.GetFileAccess().get());
        DumpComponent<HierarchyComponent>(out, "HierarchyComponent", entity, p_scene, archive.GetFileAccess().get());
        DumpComponent<TransformComponent>(out, "TransformComponent", entity, p_scene, archive.GetFileAccess().get());

        // #define REGISTER_COMPONENT(a, ...) DumpComponent<a>(out, #a, entity, p_scene, archive.GetFileAccess().get());
        //         REGISTER_COMPONENT_LIST
        // #undef REGISTER_COMPONENT

        out.SetSeqFormat(YAML::Block);

        out << YAML::EndMap;
    }

    out << YAML::EndSeq;
    out << YAML::EndMap;

    auto res = FileAccess::Open(p_path, FileAccess::WRITE);
    if (!res) {
        return HBN_ERROR(res.error());
    }

    auto yaml_file = *res;
    const char* result = out.c_str();
    yaml_file->WriteBuffer(result, strlen(result));
    return Result<void>();
}

#pragma region SCENE_COMPONENT_SERIALIZATION
void NameComponent::Serialize(Archive& p_archive, uint32_t p_version) {
    unused(p_version);

    if (p_archive.IsWriteMode()) {
        p_archive << m_name;
    } else {
        p_archive >> m_name;
    }
}

bool NameComponent::Dump(YAML::Emitter& p_out, FileAccess* p_binary, uint32_t p_version) const {
    unused(p_version);

    DUMP_BEGIN(p_out, p_binary);
    DUMP_KEY_VALUE("name", m_name);
    DUMP_END();
}

bool NameComponent::Undump(const YAML::Node& p_node, FileAccess* p_binary, uint32_t p_version) {
    unused(p_version);

    UNDUMP_BEGIN(p_node, p_binary);
    UNDUMP_KEY_VALUE("name", m_name);
    UNDUMP_END();
}

void HierarchyComponent::Serialize(Archive& p_archive, uint32_t) {
    if (p_archive.IsWriteMode()) {
        p_archive << m_parentId;
    } else {
        p_archive >> m_parentId;
    }
}

void AnimationComponent::Serialize(Archive& p_archive, uint32_t) {
    if (p_archive.IsWriteMode()) {
        p_archive << flags;
        p_archive << start;
        p_archive << end;
        p_archive << timer;
        p_archive << amount;
        p_archive << speed;
        p_archive << channels;

        uint64_t num_samplers = samplers.size();
        p_archive << num_samplers;
        for (uint64_t i = 0; i < num_samplers; ++i) {
            p_archive << samplers[i].keyframeTmes;
            p_archive << samplers[i].keyframeData;
        }
    } else {
        p_archive >> flags;
        p_archive >> start;
        p_archive >> end;
        p_archive >> timer;
        p_archive >> amount;
        p_archive >> speed;
        p_archive >> channels;

        uint64_t num_samplers = 0;
        p_archive >> num_samplers;
        samplers.resize(num_samplers);
        for (uint64_t i = 0; i < num_samplers; ++i) {
            p_archive >> samplers[i].keyframeTmes;
            p_archive >> samplers[i].keyframeData;
        }
    }
}

bool AnimationComponent::Dump(YAML::Emitter& p_out, FileAccess* p_binary, uint32_t p_version) const {
    unused(p_version);
    CRASH_NOW();

    DUMP_BEGIN(p_out, p_binary);
    DUMP_KEY_VALUE("flags", flags);
    DUMP_KEY_VALUE("start", start);
    DUMP_KEY_VALUE("end", end);
    DUMP_KEY_VALUE("timer", timer);
    DUMP_KEY_VALUE("amount", amount);
    DUMP_KEY_VALUE("speed", speed);

    DUMP_END();
}

bool AnimationComponent::Undump(const YAML::Node& p_node, FileAccess* p_binary, uint32_t p_version) {
    unused(p_version);
    CRASH_NOW();

    UNDUMP_BEGIN(p_node, p_binary);
    UNDUMP_KEY_VALUE("flags", flags);
    UNDUMP_KEY_VALUE("start", start);
    UNDUMP_KEY_VALUE("end", end);
    UNDUMP_KEY_VALUE("timer", timer);
    UNDUMP_KEY_VALUE("amount", amount);
    UNDUMP_KEY_VALUE("speed", speed);
    UNDUMP_END();
}

void TransformComponent::Serialize(Archive& p_archive, uint32_t) {
    if (p_archive.IsWriteMode()) {
        p_archive << m_flags;
        p_archive << m_scale;
        p_archive << m_translation;
        p_archive << m_rotation;
    } else {
        p_archive >> m_flags;
        p_archive >> m_scale;
        p_archive >> m_translation;
        p_archive >> m_rotation;
        SetDirty();
    }
}

void MeshComponent::Serialize(Archive& p_archive, uint32_t) {
    if (p_archive.IsWriteMode()) {
        p_archive << flags;
        p_archive << indices;
        p_archive << positions;
        p_archive << normals;
        p_archive << tangents;
        p_archive << texcoords_0;
        p_archive << texcoords_1;
        p_archive << joints_0;
        p_archive << weights_0;
        p_archive << color_0;
        p_archive << subsets;
        p_archive << armatureId;
    } else {
        p_archive >> flags;
        p_archive >> indices;
        p_archive >> positions;
        p_archive >> normals;
        p_archive >> tangents;
        p_archive >> texcoords_0;
        p_archive >> texcoords_1;
        p_archive >> joints_0;
        p_archive >> weights_0;
        p_archive >> color_0;
        p_archive >> subsets;
        p_archive >> armatureId;

        CreateRenderData();
    }
}

bool MeshComponent::Dump(YAML::Emitter& p_out, FileAccess* p_binary, uint32_t p_version) const {
    unused(p_version);

    DUMP_BEGIN(p_out, p_binary);
    DUMP_KEY_VALUE("flags", flags);
    DUMP_KEY_VALUE("armature_id", armatureId);

    p_out << DUMP_KEY("subsets");
    p_out.SetSeqFormat(YAML::Block);
    p_out << YAML::BeginSeq;
    for (const auto& subset : subsets) {
        p_out << YAML::BeginMap;
        DUMP_KEY_VALUE("material_id", subset.material_id);
        DUMP_KEY_VALUE("index_count", subset.index_count);
        DUMP_KEY_VALUE("index_offset", subset.index_offset);
        DUMP_KEY_VALUE("local_bound", subset.local_bound);
        p_out << YAML::EndMap;
    }
    p_out << YAML::EndSeq;
    p_out.SetSeqFormat(YAML::Flow);

#define DUMP_VEC_HELPER(a)                        \
    do {                                          \
        if (!a.empty()) {                         \
            p_out << DUMP_KEY(#a);                \
            DumpVectorBinary(p_out, a, p_binary); \
        }                                         \
    } while (0)
    DUMP_VEC_HELPER(indices);
    DUMP_VEC_HELPER(positions);
    DUMP_VEC_HELPER(normals);
    DUMP_VEC_HELPER(tangents);
    DUMP_VEC_HELPER(texcoords_0);
    DUMP_VEC_HELPER(texcoords_1);
    DUMP_VEC_HELPER(joints_0);
    DUMP_VEC_HELPER(weights_0);
    DUMP_VEC_HELPER(color_0);
#undef DUMP_VEC_HELPER

    DUMP_END();
}

bool MeshComponent::Undump(const YAML::Node& p_node, FileAccess* p_binary, uint32_t p_version) {
    unused(p_version);

    UNDUMP_BEGIN(p_node, p_binary);
    UNDUMP_KEY_VALUE("flags", flags);
    UNDUMP_KEY_VALUE("armature_id", armatureId);

    const auto& node = p_node["subsets"];
    if (!(node && node.IsSequence())) {
        return false;
    }

    const size_t subset_count = node.size();
    subsets.resize(subset_count);

    for (size_t i = 0; i < subset_count; ++i) {
        const auto& subset = node[i];
        DEV_ASSERT(subset && subset.IsMap());
        bool ok = UndumpKeyValuePair(subset, "material_id", subsets[i].material_id);
        ok = ok && UndumpKeyValuePair(subset, "index_count", subsets[i].index_count);
        ok = ok && UndumpKeyValuePair(subset, "index_offset", subsets[i].index_offset);
        ok = ok && UndumpKeyValuePair(subset, "local_bound", subsets[i].local_bound);
        if (!ok) {
            return false;
        }
    }

#define UNDUMP_VEC_HELPER(a) ERR_FAIL_COND_V(!UndumpVectorBinary(p_node[#a], a, p_binary), false);
    UNDUMP_VEC_HELPER(indices);
    UNDUMP_VEC_HELPER(positions);
    UNDUMP_VEC_HELPER(normals);
    UNDUMP_VEC_HELPER(tangents);
    UNDUMP_VEC_HELPER(texcoords_0);
    UNDUMP_VEC_HELPER(texcoords_1);
    UNDUMP_VEC_HELPER(joints_0);
    UNDUMP_VEC_HELPER(weights_0);
    UNDUMP_VEC_HELPER(color_0);
#undef UNDUMP_VEC_HELPER

    CreateRenderData();
    return true;
}

void MaterialComponent::Serialize(Archive& p_archive, uint32_t p_version) {
    unused(p_version);

    if (p_archive.IsWriteMode()) {
        p_archive << metallic;
        p_archive << roughness;
        p_archive << emissive;
        p_archive << baseColor;
        for (int i = 0; i < TEXTURE_MAX; ++i) {
            p_archive << textures[i].enabled;
            p_archive << textures[i].path;
        }
    } else {
        p_archive >> metallic;
        p_archive >> roughness;
        p_archive >> emissive;
        p_archive >> baseColor;
        for (int i = 0; i < TEXTURE_MAX; ++i) {
            p_archive >> textures[i].enabled;
            std::string& path = textures[i].path;
            p_archive >> path;

            // request image
            if (!path.empty()) {
                AssetRegistry::GetSingleton().RequestAssetAsync(path);
            }
        }
    }
}

bool MaterialComponent::Dump(YAML::Emitter& p_out, FileAccess* p_binary, uint32_t p_version) const {
    unused(p_version);

    DUMP_BEGIN(p_out, p_binary);
    DUMP_KEY_VALUE("metallic", metallic);
    DUMP_KEY_VALUE("roughness", roughness);
    DUMP_KEY_VALUE("emssive", emissive);
    DUMP_KEY_VALUE("baseColor", baseColor);

    // @TODO: textures can be undefined
    p_out << DUMP_KEY("textures");
    p_out << YAML::BeginSeq;
    for (int i = 0; i < TEXTURE_MAX; ++i) {
        p_out << YAML::BeginMap << YAML::Value;
        DUMP_KEY_VALUE("enabled", textures[i].enabled);
        DUMP_KEY_VALUE("path", textures[i].path);
        p_out << YAML::EndMap;
    }
    p_out << YAML::EndSeq;

    DUMP_END();
}

bool MaterialComponent::Undump(const YAML::Node& p_node, FileAccess* p_binary, uint32_t p_version) {
    unused(p_version);

    UNDUMP_BEGIN(p_node, p_binary);
    UNDUMP_KEY_VALUE("metallic", metallic);
    UNDUMP_KEY_VALUE("roughness", roughness);
    UNDUMP_KEY_VALUE("emssive", emissive);
    UNDUMP_KEY_VALUE("baseColor", baseColor);

    const auto& node = p_node["textures"];
    if (node.IsDefined()) {
        if (node.IsSequence() && node.size() == TEXTURE_MAX) {
            for (size_t i = 0; i < node.size(); ++i) {
                const auto& map = node[i];
                textures[i].enabled = map["enabled"].as<bool>();
                textures[i].path = map["path"].as<std::string>();
            }
        } else {
            return false;
        }
    }

    UNDUMP_END();
}

void LightComponent::Serialize(Archive& p_archive, uint32_t p_version) {
    unused(p_version);

    if (p_archive.IsWriteMode()) {
        p_archive << m_flags;
        p_archive << m_type;
        p_archive << m_atten;
        p_archive << m_shadowRegion;
    } else {
        p_archive >> m_flags;
        p_archive >> m_type;
        p_archive >> m_atten;
        if (p_version > 14) {
            p_archive >> m_shadowRegion;
        } else {
            m_flags &= ~(SHADOW_REGION);
        }

        m_flags |= DIRTY;
    }
}

bool LightComponent::Dump(YAML::Emitter& p_out, FileAccess* p_binary, uint32_t p_version) const {
    unused(p_version);

    DUMP_BEGIN(p_out, p_binary);
    DUMP_KEY_VALUE("flags", m_flags);
    DUMP_KEY_VALUE("type", m_type);
    LOG_WARN("TODO: shadow region");

    p_out << DUMP_KEY("atten");
    p_out << YAML::BeginMap;
    DUMP_KEY_VALUE("constant", m_atten.constant);
    DUMP_KEY_VALUE("linear", m_atten.linear);
    DUMP_KEY_VALUE("quadratic", m_atten.quadratic);
    p_out << YAML::EndMap;

    DUMP_END();
}

bool LightComponent::Undump(const YAML::Node& p_node, FileAccess* p_binary, uint32_t p_version) {
    unused(p_version);
    unused(p_binary);

    m_flags = p_node["flags"].as<uint32_t>();
    m_type = p_node["type"].as<int>();
    LOG_WARN("TODO: shadow region");

    const auto& atten = p_node["atten"];
    if (atten.IsDefined() && atten.IsMap()) {
        // @TODO: fix typo
        m_atten.constant = atten["contant"].as<float>();
        m_atten.linear = atten["linear"].as<float>();
        m_atten.quadratic = atten["quadratic"].as<float>();
    }

    return true;
}

WARNING_PUSH()
WARNING_DISABLE(4100, "-Wunused-parameter")

void ArmatureComponent::Serialize(Archive& p_archive, uint32_t) {
    if (p_archive.IsWriteMode()) {
        p_archive << flags;
        p_archive << boneCollection;
        p_archive << inverseBindMatrices;
    } else {
        p_archive >> flags;
        p_archive >> boneCollection;
        p_archive >> inverseBindMatrices;
    }
}

bool ArmatureComponent::Dump(YAML::Emitter& p_out, FileAccess* p_binary, uint32_t p_version) const {
    CRASH_NOW();
    return true;
}

bool ArmatureComponent::Undump(const YAML::Node& p_node, FileAccess* p_binary, uint32_t p_version) {
    CRASH_NOW();
    return true;
}

void ObjectComponent::Serialize(Archive& p_archive, uint32_t) {
    if (p_archive.IsWriteMode()) {
        p_archive << flags;
        p_archive << meshId;
    } else {
        p_archive >> flags;
        p_archive >> meshId;
    }
}

bool ObjectComponent::Dump(YAML::Emitter& p_out, FileAccess* p_binary, uint32_t p_version) const {
    DUMP_BEGIN(p_out, p_binary);
    DUMP_KEY_VALUE("flags", flags);
    DUMP_KEY_VALUE("mesh_id", meshId);
    DUMP_END();
}

bool ObjectComponent::Undump(const YAML::Node& p_node, FileAccess* p_binary, uint32_t p_version) {
    UNDUMP_BEGIN(p_node, p_binary);
    UNDUMP_KEY_VALUE("flags", flags);
    UNDUMP_KEY_VALUE("mesh_id", meshId);
    UNDUMP_END();
}

void PerspectiveCameraComponent::Serialize(Archive& p_archive, uint32_t) {
    if (p_archive.IsWriteMode()) {
        p_archive << m_flags;
        p_archive << m_near;
        p_archive << m_far;
        p_archive << m_fovy;
        p_archive << m_width;
        p_archive << m_height;
        p_archive << m_pitch;
        p_archive << m_yaw;
        p_archive << m_position;
    } else {
        p_archive >> m_flags;
        p_archive >> m_near;
        p_archive >> m_far;
        p_archive >> m_fovy;
        p_archive >> m_width;
        p_archive >> m_height;
        p_archive >> m_pitch;
        p_archive >> m_yaw;
        p_archive >> m_position;

        SetDirty();
    }
}

bool PerspectiveCameraComponent::Dump(YAML::Emitter& p_out, FileAccess* p_binary, uint32_t p_version) const {
    DUMP_BEGIN(p_out, p_binary);
    DUMP_KEY_VALUE("flags", m_flags);
    DUMP_KEY_VALUE("near", m_near);
    DUMP_KEY_VALUE("far", m_far);
    DUMP_KEY_VALUE("fovy", m_fovy);
    DUMP_KEY_VALUE("width", m_width);
    DUMP_KEY_VALUE("height", m_height);
    DUMP_KEY_VALUE("pitch", m_pitch);
    DUMP_KEY_VALUE("yaw", m_yaw);
    DUMP_KEY_VALUE("position", m_position);
    DUMP_END();
}

bool PerspectiveCameraComponent::Undump(const YAML::Node& p_node, FileAccess* p_binary, uint32_t p_version) {
    UNDUMP_BEGIN(p_node, p_binary);
    UNDUMP_KEY_VALUE("flags", m_flags);
    UNDUMP_KEY_VALUE("near", m_near);
    UNDUMP_KEY_VALUE("far", m_far);
    UNDUMP_KEY_VALUE("fovy", m_fovy);
    UNDUMP_KEY_VALUE("width", m_width);
    UNDUMP_KEY_VALUE("height", m_height);
    UNDUMP_KEY_VALUE("pitch", m_pitch);
    UNDUMP_KEY_VALUE("yaw", m_yaw);
    UNDUMP_KEY_VALUE("position", m_position);

    SetDirty();
    UNDUMP_END();
}

void LuaScriptComponent::Serialize(Archive& p_archive, uint32_t p_version) {
    if (p_archive.IsWriteMode()) {
        p_archive << m_path;
    } else {
        std::string path;
        p_archive >> path;
        SetScript(path);
    }
}

void NativeScriptComponent::Serialize(Archive& p_archive, uint32_t p_version) {
    CRASH_NOW();
}

void CollisionObjectBase::Serialize(Archive& p_archive, uint32_t p_version) {
    if (p_archive.IsWriteMode()) {
        p_archive << collisionType;
        p_archive << collisionMask;
    } else {
        p_archive >> collisionType;
        p_archive >> collisionMask;
    }
}

void RigidBodyComponent::Serialize(Archive& p_archive, uint32_t p_version) {
    CollisionObjectBase::Serialize(p_archive, p_version);

    if (p_archive.IsWriteMode()) {
        p_archive << shape;
        p_archive << objectType;
        p_archive << param;
        p_archive << mass;
    } else {
        p_archive >> shape;
        p_archive >> objectType;
        p_archive >> param;
        p_archive >> mass;
    }
}

void ClothComponent::Serialize(Archive& p_archive, uint32_t p_version) {
    CollisionObjectBase::Serialize(p_archive, p_version);

    CRASH_NOW();
    if (p_archive.IsWriteMode()) {
    } else {
    }
}

void EnvironmentComponent::Serialize(Archive& p_archive, uint32_t p_version) {
    CRASH_NOW();
}
#pragma endregion SCENE_COMPONENT_SERIALIZATION
WARNING_POP()

}  // namespace my
