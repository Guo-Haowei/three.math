#pragma once

namespace my {

class Application;

class Module {
public:
    Module(std::string_view name) : m_initialized(false), m_name(name) {}
    virtual ~Module() = default;

    virtual auto Initialize() -> Result<void> = 0;
    virtual void Finalize() = 0;

    std::string_view GetName() const { return m_name; }

protected:
    bool m_initialized;
    std::string_view m_name;
    Application* m_app;
    friend class Application;
};

}  // namespace my
