#include "file_path.h"

namespace my {

FilePath::FilePath(const std::string& p_path) : m_path(p_path) {
    prettify();
}

FilePath::FilePath(std::string_view p_path) : m_path(p_path) {
    prettify();
}

FilePath::FilePath(const char* p_path) : m_path(p_path) {
    prettify();
}

const char* FilePath::extensionCStr() const {
    const char* p = strrchr(m_path.c_str(), '.');
    p = p ? p : "";
    return p;
}

void FilePath::prettify() {
    for (char& c : m_path) {
        if (c == '\\') {
            c = '/';
        }
    }
}

FilePath FilePath::concat(std::string_view p_path) const {
    std::string path = m_path;
    path.push_back('/');
    path.append(p_path);
    return FilePath{ path };
}

}  // namespace my
