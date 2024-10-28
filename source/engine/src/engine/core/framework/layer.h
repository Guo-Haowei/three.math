#pragma once
#include "core/base/noncopyable.h"

namespace my {

class Layer : public NonCopyable {
public:
    Layer(const std::string& p_name = "Layer") : m_name(p_name) {}

    virtual void Attach() = 0;
    virtual void Render() = 0;
    virtual void Update(float p_elapsedTime) = 0;

    const std::string& GetName() const { return m_name; }

protected:
    std::string m_name;
};

}  // namespace my