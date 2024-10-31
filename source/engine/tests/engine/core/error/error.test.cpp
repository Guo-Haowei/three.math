#include "engine/core/error/error.h"

namespace my {

TEST(error, constructor_no_string) {
    constexpr size_t LINE_NUMBER = __LINE__;
    auto err = VCT_ERROR(100).error();
    EXPECT_EQ(err.line, LINE_NUMBER + 1);
    EXPECT_EQ(err.GetValue(), 100);
    EXPECT_EQ(err.GetMessage(), "");
}

TEST(error, constructor_with_format) {
    constexpr size_t LINE_NUMBER = __LINE__;
    auto err = VCT_ERROR(10.0f, "({}={}={})", 1, 2, 3).error();
    EXPECT_EQ(err.line, LINE_NUMBER + 1);
    EXPECT_EQ(err.GetValue(), 10.0f);
    EXPECT_EQ(err.GetMessage(), "(1=2=3)");
}

}  // namespace my
