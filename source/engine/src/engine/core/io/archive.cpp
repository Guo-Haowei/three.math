#include "archive.h"

#include "core/io/print.h"

namespace my {

auto Archive::OpenMode(const std::string& p_path, bool p_write_mode) -> std::expected<void, ErrorRef> {
    m_path = p_path;
    m_isWriteMode = p_write_mode;
    if (m_isWriteMode) {
        m_path += ".tmp";
    }

    auto result = FileAccess::Open(m_path, p_write_mode ? FileAccess::WRITE : FileAccess::READ);
    if (!result) {
        return HBN_ERROR(result.error());
    }

    m_file = *result;
    return std::expected<void, ErrorRef>();
}

void Archive::Close() {
    if (!m_file) {
        return;
    }

    m_file.reset();

    if (m_isWriteMode) {
        namespace fs = std::filesystem;

        fs::path final_path{ m_path };
        final_path.replace_extension();
        if (fs::exists(final_path)) {
            fs::remove(final_path);
        }
        fs::rename(m_path, final_path);
    }
}

bool Archive::IsWriteMode() const {
    DEV_ASSERT(m_file);
    return m_isWriteMode;
}

bool Archive::Write(const void* p_data, size_t p_size) {
    DEV_ASSERT(m_file && m_isWriteMode);
    return m_file->WriteBuffer(p_data, p_size);
}

bool Archive::Read(void* p_data, size_t p_size) {
    DEV_ASSERT(m_file && !m_isWriteMode);
    return m_file->ReadBuffer(p_data, p_size);
}

Archive& Archive::WriteString(const char* p_data, size_t p_length) {
    Write(p_length);
    Write(p_data, p_length);
    return *this;
}

}  // namespace my