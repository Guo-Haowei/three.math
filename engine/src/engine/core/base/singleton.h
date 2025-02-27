#pragma once
#include "noncopyable.h"

namespace my {

template<typename T>
class Singleton : public NonCopyable {
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

public:
    Singleton() {
        DEV_ASSERT(s_singleton == nullptr);
        s_singleton = static_cast<T*>(this);
    }

    virtual ~Singleton() {
        DEV_ASSERT(s_singleton);
        s_singleton = nullptr;
    }

    static T& GetSingleton() {
        DEV_ASSERT(s_singleton);
        return *s_singleton;
    }

    static T* GetSingletonPtr() { return s_singleton; }

protected:
    inline static T* s_singleton;
};

}  // namespace my