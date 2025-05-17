#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <variant>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mpack_cpp/mpack_reader.hpp"
#include "mpack_cpp/mpack_writer.hpp"

using testing::ElementsAre;

namespace {

constexpr std::size_t BUFFER_SIZE{1024};

inline void DebugPrintBuffer(const std::vector<char>& buffer) {
    for (std::size_t i = 0; buffer[i] != '\0'; i++) {
        printf("0x%02X,", (unsigned char)buffer[i]);
    }
    printf("\n");

    for (std::size_t i = 0; buffer[i] != '\0'; i++) {
        printf("'%c',", (unsigned char)buffer[i]);
    }
    printf("\n");
}

struct Website {
    bool compact;
    std::uint8_t schema;

    void to_message_pack(mpack_writer_t& writer) const {
        mpack_cpp::WriteField(writer, "compact", compact);
        mpack_cpp::WriteField(writer, "schema", schema);
    }

    void from_message_pack(mpack_reader_t& reader) {
        mpack_cpp::ReadField(reader, "compact", compact);
        mpack_cpp::ReadField(reader, "schema", schema);
    }
};

}  // namespace

TEST(mpack_cpp, website_example) {
    std::vector<char> buffer(BUFFER_SIZE);
    Website website{};
    website = {true, 0};
    auto size = mpack_cpp::WriteToMsgPack(website, buffer);
    buffer.resize(size);
    ASSERT_THAT(buffer, ElementsAre(0x82, 0xA7, 'c', 'o', 'm', 'p', 'a', 'c', 't', 0xC3,
                                    0xA6, 's', 'c', 'h', 'e', 'm', 'a', 0x00));

    website = {false, 3};
    mpack_cpp::ReadFromMsgPack(website, buffer, size);
    EXPECT_EQ(website.compact, true);
    EXPECT_EQ(website.schema, 0);
    DebugPrintBuffer(buffer);
}

namespace {

struct Animal {
    std::string name;
    int age;

    void to_message_pack(mpack_writer_t& writer) const {
        mpack_cpp::WriteField(writer, "name", name);
        mpack_cpp::WriteField(writer, "age", age);
    }

    void from_message_pack(mpack_reader_t& reader) {
        mpack_cpp::ReadField(reader, "name", name);
        mpack_cpp::ReadField(reader, "age", age);
    }
};

struct Zoo {
    std::vector<Animal> animals;

    void to_message_pack(mpack_writer_t& writer) const {
        mpack_cpp::WriteField(writer, "animals", animals);
    }

    void from_message_pack(mpack_reader_t& reader) {
        mpack_cpp::ReadField(reader, "animals", animals);
    }
};

}  // namespace

TEST(mpack_cpp_to_msgpack_and_back, nested) {
    std::vector<char> buffer(BUFFER_SIZE);
    Zoo before{{Animal{"dog", 11}, Animal{"cat", 5}}};
    Zoo after{};
    after.animals.resize(2);
    after.animals.at(0).name.reserve(10);
    after.animals.at(1).name.reserve(10);

    auto n = mpack_cpp::WriteToMsgPack(before, buffer);
    EXPECT_EQ(n, 40);
    std::vector<char> trimmed{buffer.begin(),
                              buffer.begin() + static_cast<std::ptrdiff_t>(n)};
    ASSERT_THAT(trimmed, testing::ElementsAre(
                             0x81, 0xA7, 0x61, 0x6E, 0x69, 0x6D, 0x61, 0x6C, 0x73, 0x92,
                             0x82, 0xA4, 0x6E, 0x61, 0x6D, 0x65, 0xA3, 0x64, 0x6F, 0x67,
                             0xA3, 0x61, 0x67, 0x65, 0x0B, 0x82, 0xA4, 0x6E, 0x61, 0x6D,
                             0x65, 0xA3, 0x63, 0x61, 0x74, 0xA3, 0x61, 0x67, 0x65, 0x05));

    bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
    ASSERT_TRUE(success);
    EXPECT_EQ(after.animals.size(), before.animals.size());
    for (std::size_t i = 0; i < after.animals.size(); ++i) {
        EXPECT_EQ(after.animals.at(i).name, before.animals.at(i).name);
        EXPECT_EQ(after.animals.at(i).age, before.animals.at(i).age);
    }
}

namespace {

enum class Status : std::uint8_t { NONE, ONE, SOME };

struct WithExtField {
    Status status;

    void to_message_pack(mpack_writer_t& writer) const {
        auto data = static_cast<char>(status);
        mpack_cpp::WriteExtField(writer, "Status", 0x2a, std::array{data});
    }

    void from_message_pack(mpack_reader_t& reader) {
        std::int8_t type;
        std::array<char, 1> data;
        mpack_cpp::ReadExtField(reader, "Status", type, data);
        assert(type == 0x2a);
        status = static_cast<Status>(data[0]);
    }
};

}  // namespace

TEST(mpack_cpp_to_msgpack_and_back, with_ext_field) {
    std::vector<char> buffer(BUFFER_SIZE);
    WithExtField before{Status::SOME};
    WithExtField after{Status::NONE};

    auto n = mpack_cpp::WriteToMsgPack(before, buffer);
    EXPECT_EQ(n, 11);
    std::vector<char> trimmed{buffer.begin(),
                              buffer.begin() + static_cast<std::ptrdiff_t>(n)};
    ASSERT_THAT(trimmed, ElementsAre(0x81, 0xA6, 0x53, 0x74, 0x61, 0x74, 0x75, 0x73, 0xD4,
                                     0x2A, 0x02));

    bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
    EXPECT_TRUE(success);
    EXPECT_EQ(before.status, after.status);
}

namespace {

struct WithVariant {
    std::variant<bool, double> choice;

    void to_message_pack(mpack_writer_t& writer) const {
        mpack_cpp::WriteField(writer, "Choice", choice);
    }

    void from_message_pack(mpack_reader_t& reader) {
        mpack_cpp::ReadField(reader, "Choice", choice);
    }
};

}  // namespace

TEST(mpack_cpp_to_msgpack_and_back, with_variant) {
    std::vector<char> buffer(BUFFER_SIZE);
    WithVariant before{false};
    WithVariant after{true};
    {
        auto n = mpack_cpp::WriteToMsgPack(before, buffer);
        EXPECT_EQ(n, 9);
        std::vector<char> trimmed{buffer.begin(),
                                  buffer.begin() + static_cast<std::ptrdiff_t>(n)};
        ASSERT_THAT(trimmed, ElementsAre(0x81, 0xA6, 'C', 'h', 'o', 'i', 'c', 'e', 0xC2));

        bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
        EXPECT_TRUE(success);
        EXPECT_TRUE(std::holds_alternative<bool>(after.choice));
        EXPECT_EQ(std::get<bool>(before.choice), std::get<bool>(after.choice));
    }
    before.choice = true;
    after.choice = false;
    {
        auto n = mpack_cpp::WriteToMsgPack(before, buffer);
        EXPECT_EQ(n, 9);
        std::vector<char> trimmed{buffer.begin(),
                                  buffer.begin() + static_cast<std::ptrdiff_t>(n)};
        ASSERT_THAT(trimmed, ElementsAre(0x81, 0xA6, 'C', 'h', 'o', 'i', 'c', 'e', 0xC3));

        bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
        EXPECT_TRUE(success);
        EXPECT_TRUE(std::holds_alternative<bool>(after.choice));
        EXPECT_EQ(std::get<bool>(before.choice), std::get<bool>(after.choice));
    }
    before.choice = 3.14;
    after.choice = false;
    {
        auto n = mpack_cpp::WriteToMsgPack(before, buffer);
        EXPECT_EQ(n, 17);
        std::vector<char> trimmed{buffer.begin(),
                                  buffer.begin() + static_cast<std::ptrdiff_t>(n)};
        ASSERT_THAT(trimmed, ElementsAre(0x81, 0xA6, 'C', 'h', 'o', 'i', 'c', 'e', 0xCB,
                                         0x40, 0x09, 0x1E, 0xB8, 0x51, 0xEB, 0x85, 0x1F));

        bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
        EXPECT_TRUE(success);
        EXPECT_TRUE(std::holds_alternative<double>(after.choice));
        EXPECT_EQ(std::get<double>(before.choice), std::get<double>(after.choice));
    }
}

namespace {

struct WithPair {
    std::pair<std::string, bool> key_value;

    void to_message_pack(mpack_writer_t& writer) const {
        mpack_cpp::WriteField(writer, "KeyValue", key_value);
    }

    void from_message_pack(mpack_reader_t& reader) {
        mpack_cpp::ReadField(reader, "KeyValue", key_value);
    }
};

}  // namespace

TEST(mpack_cpp_to_msgpack_and_back, with_pair) {
    std::vector<char> buffer(BUFFER_SIZE);
    WithPair before{{"signal", false}};
    WithPair after{{"wrong", true}};

    auto n = mpack_cpp::WriteToMsgPack(before, buffer);
    EXPECT_EQ(n, 19);
    std::vector<char> trimmed{buffer.begin(),
                              buffer.begin() + static_cast<std::ptrdiff_t>(n)};
    ASSERT_THAT(trimmed, ElementsAre(0x81, 0xA8, 'K', 'e', 'y', 'V', 'a', 'l', 'u', 'e',
                                     0x92, 0xA6, 's', 'i', 'g', 'n', 'a', 'l', 0xC2));

    bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
    EXPECT_TRUE(success);
    EXPECT_EQ(before.key_value.first, after.key_value.first);
    EXPECT_EQ(before.key_value.second, after.key_value.second);
}

TEST(mpack_cpp_to_msgpack_and_back, just_pair) {
    std::vector<char> buffer(BUFFER_SIZE);
    std::pair<std::string, bool> before{"signal", false};
    std::pair<std::string, bool> after{"wrong", true};

    auto n = mpack_cpp::WriteToMsgPack(before, buffer);
    EXPECT_EQ(n, 9);
    std::vector<char> trimmed{buffer.begin(),
                              buffer.begin() + static_cast<std::ptrdiff_t>(n)};
    ASSERT_THAT(trimmed, ElementsAre(0x92, 0xA6, 's', 'i', 'g', 'n', 'a', 'l', 0xC2));

    bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
    EXPECT_TRUE(success);
    EXPECT_EQ(before.first, after.first);
    EXPECT_EQ(before.second, after.second);
}

namespace {

struct WithOptional {
    int always;
    std::optional<int> sometimes;
    int last;

    void to_message_pack(mpack_writer_t& writer) const {
        mpack_cpp::WriteField(writer, "Always", always);
        mpack_cpp::WriteOptionalField(writer, "Sometimes", sometimes);
        mpack_cpp::WriteField(writer, "Last", last);
    }

    void from_message_pack(mpack_reader_t& reader) {
        mpack_cpp::ReadField(reader, "Always", always);
        mpack_cpp::ReadOptionalField(reader, "Sometimes", sometimes);
        mpack_cpp::ReadField(reader, "Last", last);
    }
};

struct WithOptionalAtEnd {
    int always;
    std::optional<int> sometimes;

    void to_message_pack(mpack_writer_t& writer) const {
        mpack_cpp::WriteField(writer, "Always", always);
        mpack_cpp::WriteOptionalField(writer, "Sometimes", sometimes);
    }

    void from_message_pack(mpack_reader_t& reader) {
        mpack_cpp::ReadField(reader, "Always", always);
        mpack_cpp::ReadOptionalField(reader, "Sometimes", sometimes);
    }
};

}  // namespace

TEST(mpack_cpp_to_msgpack_and_back, with_optional) {
    std::vector<char> buffer(BUFFER_SIZE);
    WithOptional before{};
    WithOptional after{};

    before = {3, std::nullopt, 5};
    after = {7, 8, 8};
    {
        auto n = mpack_cpp::WriteToMsgPack(before, buffer);
        bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
        EXPECT_TRUE(success);
        EXPECT_EQ(before.always, after.always);
        EXPECT_EQ(before.sometimes, after.sometimes);
        EXPECT_EQ(before.last, after.last);
    }

    before = {3, 4, 5};
    after = {7, 8, 9};
    {
        auto n = mpack_cpp::WriteToMsgPack(before, buffer);
        bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
        EXPECT_TRUE(success);
        EXPECT_EQ(before.always, after.always);
        EXPECT_EQ(before.sometimes, after.sometimes);
        EXPECT_EQ(before.last, after.last);
    }
}

TEST(mpack_cpp_to_msgpack_and_back, with_optional_at_end) {
    std::vector<char> buffer(BUFFER_SIZE);
    WithOptionalAtEnd before{};
    WithOptionalAtEnd after{};

    before = {3, std::nullopt};
    after = {7, 8};
    {
        auto n = mpack_cpp::WriteToMsgPack(before, buffer);
        bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
        EXPECT_TRUE(success);
        EXPECT_EQ(before.always, after.always);
        EXPECT_EQ(before.sometimes, after.sometimes);
    }

    before = {3, 4};
    after = {7, 8};
    {
        auto n = mpack_cpp::WriteToMsgPack(before, buffer);
        bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
        EXPECT_TRUE(success);
        EXPECT_EQ(before.always, after.always);
        EXPECT_EQ(before.sometimes, after.sometimes);
    }
}

namespace {

template <typename T>
void TestBufferOnVector(std::vector<T>& buffer) {
    // We only test with one data type for the thing we decode/encode, and std::string.
    std::string before{"hello"};
    std::string after;

    // Without offset.
    {
        std::fill(std::begin(buffer), std::end(buffer), 0);
        after.clear();
        auto n = mpack_cpp::WriteToMsgPack(before, buffer);
        bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
        EXPECT_TRUE(success);
        EXPECT_EQ(before, after);
    }

    // Without offset, pass as pointer
    {
        std::fill(std::begin(buffer), std::end(buffer), 0);
        after.clear();
        auto n = mpack_cpp::WriteToMsgPack(before, buffer.data(), buffer.size());
        bool success = mpack_cpp::ReadFromMsgPack(after, buffer.data(), n);
        EXPECT_TRUE(success);
        EXPECT_EQ(before, after);
    }

    // With offset.
    {
        std::fill(std::begin(buffer), std::end(buffer), 0);
        after.clear();
        auto n = mpack_cpp::WriteToMsgPack(before, buffer.data() + 4, buffer.size() - 4);
        bool success = mpack_cpp::ReadFromMsgPack(after, buffer.data() + 4, n);
        EXPECT_TRUE(success);
        EXPECT_EQ(before, after);
    }
}

}  // namespace

TEST(mpack_cpp_to_msgpack_and_back, try_different_buffer_overloads) {
    // Test all overloads for reading and writing with different buffers.

    // Vector buffer.
    std::vector<char> buffer_char(BUFFER_SIZE);
    TestBufferOnVector(buffer_char);
    std::vector<uint8_t> buffer_uint8(BUFFER_SIZE);
    TestBufferOnVector(buffer_uint8);

    // c-style array buffer.
    std::string before{"hello"};
    std::string after;
    std::uint8_t buffer_raw[BUFFER_SIZE];
    {
        std::fill(std::begin(buffer_raw), std::end(buffer_raw), 0);
        after.clear();
        auto n = mpack_cpp::WriteToMsgPack(before, buffer_raw, BUFFER_SIZE);
        bool success = mpack_cpp::ReadFromMsgPack(after, buffer_raw, n);
        EXPECT_TRUE(success);
        EXPECT_EQ(before, after);
    }

    {
        std::fill(std::begin(buffer_raw), std::end(buffer_raw), 0);
        after.clear();
        auto n = mpack_cpp::WriteToMsgPack(before, buffer_raw + 4, BUFFER_SIZE - 4);
        bool success = mpack_cpp::ReadFromMsgPack(after, buffer_raw + 4, n);
        EXPECT_TRUE(success);
        EXPECT_EQ(before, after);
    }
}

namespace {
struct OptionalOnlyMember {
    std::optional<int> short_name;

    void to_message_pack(mpack_writer_t& writer) const {
        mpack_cpp::WriteOptionalField(writer, "long_name", short_name);
    }

    void from_message_pack(mpack_reader_t& reader) {
        mpack_cpp::ReadOptionalField(reader, "long_name", short_name);
    }
};
}  // namespace

TEST(mpack_cpp, option_field_only_member) {
    std::vector<char> buffer(BUFFER_SIZE);
    OptionalOnlyMember before{};
    OptionalOnlyMember after{};

    before = {3};
    after = {0};
    {
        auto n = mpack_cpp::WriteToMsgPack(before, buffer);
        bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
        EXPECT_TRUE(success);
        EXPECT_EQ(before.short_name, after.short_name);
    }

    before = {std::nullopt};
    // after = {0}; // TODO(jeroendm) this does not work, we must initialize to
    // std::nullopt...
    after = {std::nullopt};
    {
        auto n = mpack_cpp::WriteToMsgPack(before, buffer);
        bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
        EXPECT_TRUE(success);
        EXPECT_EQ(before.short_name, after.short_name);
    }
}

namespace {
// A key of more than 31 bytes triggers the bug for #1.
constexpr const char k_long_name[] = "01234567890123456789012345678901";

struct OptionalLargeName {
    std::optional<int> short_name;

    void to_message_pack(mpack_writer_t& writer) const {
        mpack_cpp::WriteOptionalField(writer, k_long_name, short_name);
    }

    void from_message_pack(mpack_reader_t& reader) {
        mpack_cpp::ReadOptionalField(reader, k_long_name, short_name);
    }
};
}  // namespace

TEST(mpack_cpp, option_field_large_key) {
    std::vector<char> buffer(BUFFER_SIZE);
    OptionalLargeName before{};
    OptionalLargeName after{};

    before = {3};
    after = {0};
    {
        auto n = mpack_cpp::WriteToMsgPack(before, buffer);
        bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
        EXPECT_FALSE(success);
        EXPECT_NE(before.short_name, after.short_name);
    }
}