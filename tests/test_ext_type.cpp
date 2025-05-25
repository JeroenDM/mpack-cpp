#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <stdexcept>
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
enum class Level : std::uint8_t { DEBUG, WARN, INFO };

/** a contrived example thats overly complicated and does nothing,
 * all to check the extension type.
 */
struct WithExtField {
    std::variant<Status, Level> ext;

    void to_message_pack(mpack_writer_t& writer) const {
        if (std::holds_alternative<Status>(ext)) {
            auto data = static_cast<char>(std::get<Status>(ext));
            mpack_cpp::WriteExtField(writer, "Ext", 33, std::array{data});
        } else if (std::holds_alternative<Level>(ext)) {
            auto data = static_cast<char>(std::get<Level>(ext));
            mpack_cpp::WriteExtField(writer, "Ext", 99, std::array{data});
        } else {
            throw std::runtime_error("Unsupported variant type.");
        }
    }

    void from_message_pack(mpack_node_t& reader) {
        std::int8_t type{0};
        std::array<char, 1> data{0};
        mpack_cpp::ReadExtField(reader, "Ext", type, data);
        if (type == 33) {
            ext = static_cast<Status>(data[0]);
        } else if (type == 99) {
            ext = static_cast<Level>(data[0]);
        } else {
            throw std::runtime_error("Unsupported extension type.");
        }
    }
};
}  // namespace

TEST(mpack_cpp_to_msgpack_and_back, with_ext_field_status) {
    std::vector<char> buffer(BUFFER_SIZE);
    WithExtField before{Status::SOME};
    WithExtField after{Level::DEBUG};

    auto n = mpack_cpp::WriteToMsgPack(before, buffer);
    EXPECT_EQ(n, 8);
    std::vector<char> trimmed{buffer.begin(),
                              buffer.begin() + static_cast<std::ptrdiff_t>(n)};
    ASSERT_THAT(trimmed, ElementsAre(0x81, 0xA3, 'E', 'x', 't', 0xD4, 33, 0x02));

    bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
    EXPECT_TRUE(success);
    EXPECT_EQ(before.ext, after.ext);
}

TEST(mpack_cpp_to_msgpack_and_back, with_ext_field_level) {
    std::vector<char> buffer(BUFFER_SIZE);
    WithExtField before{Level::INFO};
    WithExtField after{Status::NONE};

    auto n = mpack_cpp::WriteToMsgPack(before, buffer);
    EXPECT_EQ(n, 8);
    std::vector<char> trimmed{buffer.begin(),
                              buffer.begin() + static_cast<std::ptrdiff_t>(n)};
    ASSERT_THAT(trimmed, ElementsAre(0x81, 0xA3, 'E', 'x', 't', 0xD4, 99, 0x02));

    bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
    EXPECT_TRUE(success);
    EXPECT_EQ(before.ext, after.ext);
}