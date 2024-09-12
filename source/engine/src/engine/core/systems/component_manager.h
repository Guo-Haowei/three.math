#pragma once
#include "core/io/archive.h"
#include "entity.h"

namespace my {
class Scene;
}

namespace my::ecs {

#define COMPONENT_MANAGER_ITERATOR_COMMON                                              \
public:                                                                                \
    self_type operator++(int) {                                                        \
        self_type tmp = *this;                                                         \
        ++m_index;                                                                     \
        return tmp;                                                                    \
    }                                                                                  \
    self_type operator--(int) {                                                        \
        self_type tmp = *this;                                                         \
        --m_index;                                                                     \
        return tmp;                                                                    \
    }                                                                                  \
    self_type& operator++() {                                                          \
        ++m_index;                                                                     \
        return *this;                                                                  \
    }                                                                                  \
    self_type& operator--() {                                                          \
        --m_index;                                                                     \
        return *this;                                                                  \
    }                                                                                  \
    bool operator==(const self_type& p_rhs) const { return m_index == p_rhs.m_index; } \
    bool operator!=(const self_type& p_rhs) const { return m_index != p_rhs.m_index; } \
    using _dummy_force_semi_colon = int

template<typename T>
class ComponentManagerIterator {
    using self_type = ComponentManagerIterator<T>;

public:
    ComponentManagerIterator(std::vector<Entity>& p_entity_array, std::vector<T>& p_component_array, size_t p_index)
        : m_entity_array(p_entity_array),
          m_component_array(p_component_array),
          m_index(p_index) {}

    COMPONENT_MANAGER_ITERATOR_COMMON;

    std::pair<Entity, T&> operator*() const {
        return std::make_pair(this->m_entity_array[this->m_index], std::ref(this->m_component_array[this->m_index]));
    }

private:
    std::vector<Entity>& m_entity_array;
    std::vector<T>& m_component_array;

    size_t m_index;
};

template<typename T>
class ComponentManagerConstIterator {
    using self_type = ComponentManagerConstIterator<T>;

public:
    ComponentManagerConstIterator(const std::vector<Entity>& p_entity_array, const std::vector<T>& p_component_array, size_t p_index)
        : m_entity_array(p_entity_array),
          m_component_array(p_component_array),
          m_index(p_index) {}

    COMPONENT_MANAGER_ITERATOR_COMMON;

    std::pair<Entity, const T&> operator*() const {
        return std::make_pair(this->m_entity_array[this->m_index], std::ref(this->m_component_array[this->m_index]));
    }

private:
    const std::vector<Entity>& m_entity_array;
    const std::vector<T>& m_component_array;

    size_t m_index;
};

class IComponentManager {
    IComponentManager(const IComponentManager&) = delete;
    IComponentManager& operator=(const IComponentManager&) = delete;

public:
    IComponentManager() = default;
    virtual ~IComponentManager() = default;
    virtual void clear() = 0;
    virtual void copy(const IComponentManager& p_other) = 0;
    virtual void merge(IComponentManager& p_other) = 0;
    virtual void remove(const Entity& p_entity) = 0;
    virtual bool contains(const Entity& p_entity) const = 0;
    virtual size_t getIndex(const Entity& p_entity) const = 0;
    virtual size_t getCount() const = 0;
    virtual Entity getEntity(size_t p_index) const = 0;

    virtual bool serialize(Archive& p_archive, uint32_t p_version) = 0;
};

template<typename T>
class ComponentManager final : public IComponentManager {
    using iter = ComponentManagerIterator<T>;
    using const_iter = ComponentManagerConstIterator<T>;

public:
    iter begin() { return iter(m_entity_array, m_component_array, 0); }
    iter end() { return iter(m_entity_array, m_component_array, m_component_array.size()); }
    const_iter begin() const { return const_iter(m_entity_array, m_component_array, 0); }
    const_iter end() const { return const_iter(m_entity_array, m_component_array, m_component_array.size()); }

    ComponentManager(size_t p_capacity = 0) { reserve(p_capacity); }

    void reserve(size_t p_capacity) {
        if (p_capacity) {
            m_component_array.reserve(p_capacity);
            m_entity_array.reserve(p_capacity);
            m_lookup.reserve(p_capacity);
        }
    }

    void clear() override {
        m_component_array.clear();
        m_entity_array.clear();
        m_lookup.clear();
    }

    void copy(const ComponentManager<T>& p_other) {
        clear();
        m_component_array = p_other.m_component_array;
        m_entity_array = p_other.m_entity_array;
        m_lookup = p_other.m_lookup;
    }

    void copy(const IComponentManager& p_other) override {
        copy((ComponentManager<T>&)p_other);
    }

    void merge(ComponentManager<T>& p_other) {
        const size_t reserved = getCount() + p_other.getCount();
        m_component_array.reserve(reserved);
        m_entity_array.reserve(reserved);
        m_lookup.reserve(reserved);

        for (size_t i = 0; i < p_other.getCount(); ++i) {
            Entity entity = p_other.m_entity_array[i];
            DEV_ASSERT(!contains(entity));
            m_entity_array.push_back(entity);
            m_lookup[entity] = m_component_array.size();
            m_component_array.push_back(std::move(p_other.m_component_array[i]));
        }

        p_other.clear();
    }

    void merge(IComponentManager& p_other) override {
        merge((ComponentManager<T>&)p_other);
    }

    void remove(const Entity& p_entity) override {
        auto it = m_lookup.find(p_entity);
        if (it == m_lookup.end()) {
            return;
        }

        CRASH_NOW_MSG("TODO: make block invalid, instead of erase it");
        size_t index = it->second;
        m_lookup.erase(it);
        DEV_ASSERT_INDEX(index, m_entity_array.size());
        m_entity_array.erase(m_entity_array.begin() + index);
        m_component_array.erase(m_component_array.begin() + index);
    }

    bool contains(const Entity& p_entity) const override {
        if (m_lookup.empty()) {
            return false;
        }
        return m_lookup.find(p_entity) != m_lookup.end();
    }

    inline T& getComponent(size_t p_index) {
        DEV_ASSERT(p_index < m_component_array.size());
        return m_component_array[p_index];
    }

    T* getComponent(const Entity& p_entity) {
        if (!p_entity.isValid() || m_lookup.empty()) {
            return nullptr;
        }

        auto it = m_lookup.find(p_entity);

        if (it == m_lookup.end()) {
            return nullptr;
        }

        return &m_component_array[it->second];
    }

    size_t getIndex(const Entity& p_entity) const override {
        if (m_lookup.empty()) {
            return Entity::INVALID_INDEX;
        }

        const auto it = m_lookup.find(p_entity);
        if (it == m_lookup.end()) {
            return Entity::INVALID_INDEX;
        }

        return it->second;
    }

    size_t getCount() const override { return m_component_array.size(); }

    Entity getEntity(size_t p_index) const override {
        DEV_ASSERT(p_index < m_entity_array.size());
        return m_entity_array[p_index];
    }

    T& create(const Entity& p_entity) {
        DEV_ASSERT(p_entity.isValid());

        const size_t componentCount = m_component_array.size();
        DEV_ASSERT(m_lookup.find(p_entity) == m_lookup.end());
        DEV_ASSERT(m_entity_array.size() == componentCount);
        DEV_ASSERT(m_lookup.size() == componentCount);

        m_lookup[p_entity] = componentCount;
        m_component_array.emplace_back();
        m_entity_array.push_back(p_entity);
        return m_component_array.back();
    }

    const T& operator[](size_t p_index) const { return getComponent(p_index); }

    T& operator[](size_t p_index) { return getComponent(p_index); }

    bool serialize(Archive& p_archive, uint32_t p_version) override {
        constexpr uint64_t magic = 7165065861825654388llu;
        size_t count;
        if (p_archive.isWriteMode()) {
            p_archive << magic;
            count = static_cast<uint32_t>(m_component_array.size());
            p_archive << count;
            for (auto& component : m_component_array) {
                component.serialize(p_archive, p_version);
            }
            for (auto& entity : m_entity_array) {
                entity.serialize(p_archive);
            }
        } else {
            uint64_t read_magic;
            p_archive >> read_magic;
            if (read_magic != magic) {
                return false;
            }

            clear();
            p_archive >> count;
            m_component_array.resize(count);
            m_entity_array.resize(count);
            for (size_t i = 0; i < count; ++i) {
                m_component_array[i].serialize(p_archive, p_version);
            }
            for (size_t i = 0; i < count; ++i) {
                m_entity_array[i].serialize(p_archive);
                m_lookup[m_entity_array[i]] = i;
            }
        }

        return true;
    }

private:
    std::vector<T> m_component_array;
    std::vector<Entity> m_entity_array;
    std::unordered_map<Entity, size_t> m_lookup;
};

class ComponentLibrary {
public:
    struct LibraryEntry {
        std::unique_ptr<IComponentManager> m_manager = nullptr;
        uint64_t m_version = 0;
    };

    template<typename T>
    inline ComponentManager<T>& registerManager(const std::string& p_name, uint64_t p_version = 0) {
        DEV_ASSERT(m_entries.find(p_name) == m_entries.end());
        m_entries[p_name].m_manager = std::make_unique<ComponentManager<T>>();
        m_entries[p_name].m_version = p_version;
        return static_cast<ComponentManager<T>&>(*(m_entries[p_name].m_manager));
    }

private:
    std::unordered_map<std::string, LibraryEntry> m_entries;

    friend class Scene;
};

}  // namespace my::ecs
