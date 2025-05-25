#include <cstdint>
#include <cstdio>
#include <variant>
#include <vector>

#include "gtest/gtest.h"
#include "mpack_cpp/mpack_reader.hpp"
#include "mpack_cpp/mpack_writer.hpp"

namespace {

constexpr std::size_t BUFFER_SIZE{1024};

struct Group {
    std::string name;
    std::vector<std::pair<std::string, std::variant<bool, double>>> skills;

    void to_message_pack(mpack_writer_t& writer) const {
        mpack_cpp::WriteField(writer, "GroupName", name);
        mpack_cpp::WriteField(writer, "Skills", skills);
    }

    void from_message_pack(mpack_node_t& node) {
        mpack_cpp::ReadField(node, "GroupName", name);
        mpack_cpp::ReadField(node, "Skills", skills);
    }
};

enum class Label : std::uint8_t {
    DONE,
    TODO,
    NEVER,
};

struct ComplexData {
    std::string name;
    std::uint64_t time;
    std::vector<Group> groups;
    Label label;

    void to_message_pack(mpack_writer_t& writer) const {
        mpack_cpp::WriteField(writer, "Name", name);
        mpack_cpp::WriteField(writer, "Time", time);
        mpack_cpp::WriteField(writer, "Groups", groups);

        auto data = static_cast<char>(label);
        mpack_cpp::WriteExtField(writer, "Status", 0x01, std::array{data});
    }

    void from_message_pack(mpack_node_t& reader) {
        mpack_cpp::ReadField(reader, "Name", name);
        mpack_cpp::ReadField(reader, "Time", time);
        mpack_cpp::ReadField(reader, "Groups", groups);

        std::int8_t type;
        std::array<char, 1> data;
        mpack_cpp::ReadExtField(reader, "Status", type, data);
        assert(type == 0x01);
        label = static_cast<Label>(data[0]);
    }
};

}  // namespace

TEST(mpack_cpp_to_msgpack_and_back, complex_struct) {
    std::vector<char> buffer(BUFFER_SIZE);
    ComplexData before{
        "far-away-land",
        1234,
        {Group{
             "forest",
             {{"CanTalk", false}, {"Size", 14.0}},
         },
         Group{
             "sea",
             {{"CanTalk", false}, {"IsWet", true}, {"Size", -9.2}},
         }},
        Label::NEVER,
    };
    ComplexData after{};
    after.groups.resize(2);
    after.groups.at(0).name.reserve(20);
    after.groups.at(0).skills.reserve(5);
    after.groups.at(1).name.reserve(20);
    after.groups.at(1).skills.reserve(5);
    after.label = Label::DONE;

    auto n = mpack_cpp::WriteToMsgPack(before, buffer);
    EXPECT_EQ(n, 153);

    bool success = mpack_cpp::ReadFromMsgPack(after, buffer.data(), n);
    ASSERT_TRUE(success);
    EXPECT_EQ(before.groups.size(), after.groups.size());
    for (std::size_t i = 0; i < after.groups.size(); ++i) {
        EXPECT_EQ(before.groups.at(i).name, after.groups.at(i).name);
        EXPECT_EQ(before.groups.at(i).skills.size(), after.groups.at(i).skills.size());
        for (std::size_t j = 0; j < after.groups.at(i).skills.size(); ++j) {
            EXPECT_EQ(before.groups.at(i).skills.at(j).first,
                      after.groups.at(i).skills.at(j).first);
            EXPECT_EQ(before.groups.at(i).skills.at(j).second,
                      after.groups.at(i).skills.at(j).second);
        }
    }
    EXPECT_EQ(before.label, after.label);
}
