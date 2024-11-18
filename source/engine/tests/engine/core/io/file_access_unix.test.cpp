#include "core/io/file_access_unix.h"

namespace my {

TEST(file_access_unix, open_read_fail) {
    FileAccess::MakeDefault<FileAccessUnix>(FileAccess::ACCESS_FILESYSTEM);

    auto err = FileAccess::Open("file_access_unix_open_read_fail", FileAccess::READ).error();
    EXPECT_EQ(err.GetValue(), ERR_FILE_NOT_FOUND);
    EXPECT_EQ(err.GetMessage(), "error code: ERR_FILE_NOT_FOUND");
}

TEST(file_access_unix, open_write_fail) {
    FileAccess::MakeDefault<FileAccessUnix>(FileAccess::ACCESS_FILESYSTEM);
    const std::string FILE_NAME = "file_access_unix_open_write_fail";

    std::filesystem::create_directory(FILE_NAME);
    auto err = FileAccess::Open(FILE_NAME, FileAccess::WRITE).error();
    EXPECT_EQ(err.GetValue(), ERR_FILE_CANT_OPEN);

    ASSERT_TRUE(std::filesystem::remove_all(FILE_NAME));
}

TEST(file_access_unix, open_write_success) {
    FileAccess::MakeDefault<FileAccessUnix>(FileAccess::ACCESS_FILESYSTEM);
    const std::string FILE_NAME = "file_access_unix_open_write_success";

    {
        auto f = FileAccess::Open(FILE_NAME, FileAccess::WRITE).value();
        ASSERT_TRUE(f->IsOpen());
        // should call f->close() in destructor
    }

    ASSERT_TRUE(std::filesystem::remove(FILE_NAME));
}

TEST(file_access_unix, write_read_buffer) {
    FileAccess::MakeDefault<FileAccessUnix>(FileAccess::ACCESS_FILESYSTEM);
    const std::string FILE_NAME = "file_access_unix_write_read_buffer";
    const std::string STRING = "abcdefg";

    {
        auto f = FileAccess::Open(FILE_NAME, FileAccess::WRITE).value();
        ASSERT_TRUE(f->IsOpen());
        ASSERT_TRUE(f->WriteBuffer(STRING.data(), STRING.length()));
    }
    {
        auto f = FileAccess::Open(FILE_NAME, FileAccess::READ).value();
        ASSERT_TRUE(f->IsOpen());

        char buffer[128]{ 0 };
        ASSERT_TRUE(f->ReadBuffer(buffer, STRING.length()));

        EXPECT_EQ(std::string(buffer), STRING);
    }
}

}  // namespace my
