#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <variant>
#include <vector>

#include "gtest/gtest.h"
#include "mpack_cpp/mpack_reader.hpp"
#include "mpack_cpp/mpack_writer.hpp"

namespace {
constexpr std::size_t BUFFER_SIZE{1024};

struct WithOptional {
    int always;
    std::optional<int> sometimes;
    int last;

    void to_message_pack(mpack_cpp::WriteCtx& writer) const {
        mpack_cpp::WriteField(writer, "Always", always);
        mpack_cpp::WriteOptionalField(writer, "Sometimes", sometimes);
        mpack_cpp::WriteField(writer, "Last", last);
    }

    void from_message_pack(mpack_cpp::ReadCtx& reader) {
        mpack_cpp::ReadField(reader, "Always", always);
        mpack_cpp::ReadOptionalField(reader, "Sometimes", sometimes);
        mpack_cpp::ReadField(reader, "Last", last);
    }
};

struct WithOptionalAtEnd {
    int always;
    std::optional<int> sometimes;

    void to_message_pack(mpack_cpp::WriteCtx& writer) const {
        mpack_cpp::WriteField(writer, "Always", always);
        mpack_cpp::WriteOptionalField(writer, "Sometimes", sometimes);
    }

    void from_message_pack(mpack_cpp::ReadCtx& reader) {
        mpack_cpp::ReadField(reader, "Always", always);
        mpack_cpp::ReadOptionalField(reader, "Sometimes", sometimes);
    }
};
}  // namespace

TEST(read_write_optional, with_optional) {
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

TEST(read_write_optional, with_optional_at_end) {
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
struct OptionalOnlyMember {
    std::optional<int> short_name;

    void to_message_pack(mpack_cpp::WriteCtx& writer) const {
        mpack_cpp::WriteOptionalField(writer, "long_name", short_name);
    }

    void from_message_pack(mpack_cpp::ReadCtx& reader) {
        mpack_cpp::ReadOptionalField(reader, "long_name", short_name);
    }
};
}  // namespace

TEST(read_write_optional, option_field_only_member) {
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

    void to_message_pack(mpack_cpp::WriteCtx& writer) const {
        mpack_cpp::WriteOptionalField(writer, k_long_name, short_name);
    }

    void from_message_pack(mpack_cpp::ReadCtx& reader) {
        mpack_cpp::ReadOptionalField(reader, k_long_name, short_name);
    }
};
}  // namespace

TEST(read_write_optional, option_field_large_key) {
    std::vector<char> buffer(BUFFER_SIZE);
    OptionalLargeName before{};
    OptionalLargeName after{};

    before = {3};
    after = {0};
    {
        auto n = mpack_cpp::WriteToMsgPack(before, buffer);
        bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
        EXPECT_TRUE(success);
        EXPECT_EQ(before.short_name, after.short_name);
    }
}