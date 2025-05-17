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

