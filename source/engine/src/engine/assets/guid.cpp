#include "guid.h"

#include <objbase.h>

#include "core/string/string_builder.h"
#include "core/string/string_utils.h"

namespace my {

static_assert(sizeof(Guid) == sizeof(GUID));

Guid::Guid() {
    memset(m_data, 0, sizeof(Guid));
}

Guid::Guid(const uint8_t* p_buffer) {
    memcpy(m_data, p_buffer, sizeof(Guid));
}

Guid Guid::Create() {
    Guid result;
    GUID guid;

    ::CoCreateGuid(&guid);
    memcpy(&result, &guid, sizeof(Guid));
    return result;
}

Result<Guid> Guid::Parse(const char* p_start, int p_length) {
    if (p_length != 35 /* 16 x 2 + 3 */) {
        return HBN_ERROR(ErrorCode::ERR_PARSE_ERROR, "invalid GUID {}", std::string(p_start, p_length));
    }

    Guid guid;
    size_t i = 0;
    int buffer_index = 0;
    do {
        char c = p_start[i];
        if (buffer_index == 4 || buffer_index == 6 || buffer_index == 8) {
            if (c != '-') {
                return HBN_ERROR(ErrorCode::ERR_PARSE_ERROR, "invalid GUID {}", std::string(p_start, p_length));
            }

            ++i;  // skip '-'
        }

        const char high = StringUtils::HexToInt(p_start[i]);
        const char low = StringUtils::HexToInt(p_start[i + 1]);
        if (low < 0 || high < 0) {
            return HBN_ERROR(ErrorCode::ERR_PARSE_ERROR, "invalid GUID {}", std::string(p_start, p_length));
        }

        guid.m_data[buffer_index++] = (high << 4) | (low);
        i += 2;
    } while (i < p_length);

    return guid;
}

std::string Guid::ToString() const {
    // @TODO: fixed string builder
    StringStreamBuilder builder;
    int i = 0;
    while (i < array_length(m_data)) {
        builder.Append(std::format("{:02X}", m_data[i++]));
        if (i == 4 || i == 6 || i == 8) {
            builder.Append('-');
        }
    }

    return builder.ToString();
}

}  // namespace my
