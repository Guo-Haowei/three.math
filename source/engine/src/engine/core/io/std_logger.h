#pragma once
#include "core/io/logger.h"

namespace my {

class StdLogger : public ILogger {
public:
    void Print(LogLevel p_level, std::string_view p_message) override;

private:
    std::mutex m_consoleMutex;
};

class DebugConsoleLogger : public ILogger {
public:
    void Print(LogLevel p_level, std::string_view p_message) override;
};

}  // namespace my
