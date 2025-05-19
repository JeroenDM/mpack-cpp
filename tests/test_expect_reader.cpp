#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <variant>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mpack_cpp/mpack_expect_reader.hpp"
#include "mpack_cpp/mpack_writer.hpp"
#include "mpack_cpp/mpack_macros.hpp"

using testing::ElementsAre;

// Use anonymous namespace so other tests can reuse some of the names defined here.
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
    MPACK_CPP_EXPECT_DEFINE(Website, compact, schema)
};

}  // namespace

TEST(mpack_expect_reader, website_example) {
    std::vector<char> buffer(BUFFER_SIZE);
    Website website{};
    website = {true, 0};
    auto size = mpack_cpp::WriteToMsgPack(website, buffer);
    buffer.resize(size);
    ASSERT_THAT(buffer, ElementsAre(0x82, 0xA7, 'c', 'o', 'm', 'p', 'a', 'c', 't', 0xC3,
                                    0xA6, 's', 'c', 'h', 'e', 'm', 'a', 0x00));

    website = {false, 3};
    mpack_cpp::expect::ReadFromMsgPack(website, buffer, size);
    EXPECT_EQ(website.compact, true);
    EXPECT_EQ(website.schema, 0);
    DebugPrintBuffer(buffer);
}

namespace {
struct Animal {
    std::string name;
    int age;
    MPACK_CPP_EXPECT_DEFINE(Animal, name, age)
};

struct Zoo {
    std::vector<Animal> animals;
    MPACK_CPP_EXPECT_DEFINE(Zoo, animals)
};
}  // namespace

TEST(mpack_expect_reader, nested) {
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

    bool success = mpack_cpp::expect::ReadFromMsgPack(after, buffer, n);
    ASSERT_TRUE(success);
    EXPECT_EQ(after.animals.size(), before.animals.size());
    for (std::size_t i = 0; i < after.animals.size(); ++i) {
        EXPECT_EQ(after.animals.at(i).name, before.animals.at(i).name);
        EXPECT_EQ(after.animals.at(i).age, before.animals.at(i).age);
    }
}

namespace {
struct WithVariant {
    std::variant<bool, double> choice;
    MPACK_CPP_EXPECT_DEFINE(WithVariant, choice)
};
}  // namespace

TEST(mpack_expect_reader, with_variant) {
    std::vector<char> buffer(BUFFER_SIZE);
    WithVariant before{false};
    WithVariant after{true};
    {
        auto n = mpack_cpp::WriteToMsgPack(before, buffer);
        EXPECT_EQ(n, 9);
        std::vector<char> trimmed{buffer.begin(),
                                  buffer.begin() + static_cast<std::ptrdiff_t>(n)};
        ASSERT_THAT(trimmed, ElementsAre(0x81, 0xA6, 'c', 'h', 'o', 'i', 'c', 'e', 0xC2));

        bool success = mpack_cpp::expect::ReadFromMsgPack(after, buffer, n);
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
        ASSERT_THAT(trimmed, ElementsAre(0x81, 0xA6, 'c', 'h', 'o', 'i', 'c', 'e', 0xC3));

        bool success = mpack_cpp::expect::ReadFromMsgPack(after, buffer, n);
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
        ASSERT_THAT(trimmed, ElementsAre(0x81, 0xA6, 'c', 'h', 'o', 'i', 'c', 'e', 0xCB,
                                         0x40, 0x09, 0x1E, 0xB8, 0x51, 0xEB, 0x85, 0x1F));

        bool success = mpack_cpp::expect::ReadFromMsgPack(after, buffer, n);
        EXPECT_TRUE(success);
        EXPECT_TRUE(std::holds_alternative<double>(after.choice));
        EXPECT_EQ(std::get<double>(before.choice), std::get<double>(after.choice));
    }
}

namespace {
struct WithPair {
    std::pair<std::string, bool> key_value;
    MPACK_CPP_EXPECT_DEFINE(WithPair, key_value)
};
}  // namespace

TEST(mpack_expect_reader, with_pair) {
    std::vector<char> buffer(BUFFER_SIZE);
    WithPair before{{"signal", false}};
    WithPair after{{"wrong", true}};

    auto n = mpack_cpp::WriteToMsgPack(before, buffer);
    EXPECT_EQ(n, 20);
    std::vector<char> trimmed{buffer.begin(),
                              buffer.begin() + static_cast<std::ptrdiff_t>(n)};
    ASSERT_THAT(trimmed, ElementsAre(0x81, 0xA9, 'k', 'e', 'y', '_', 'v', 'a', 'l', 'u', 'e',
                                     0x92, 0xA6, 's', 'i', 'g', 'n', 'a', 'l', 0xC2));

    bool success = mpack_cpp::expect::ReadFromMsgPack(after, buffer, n);
    EXPECT_TRUE(success);
    EXPECT_EQ(before.key_value.first, after.key_value.first);
    EXPECT_EQ(before.key_value.second, after.key_value.second);
}

TEST(mpack_expect_reader, just_pair) {
    std::vector<char> buffer(BUFFER_SIZE);
    std::pair<std::string, bool> before{"signal", false};
    std::pair<std::string, bool> after{"wrong", true};

    auto n = mpack_cpp::WriteToMsgPack(before, buffer);
    EXPECT_EQ(n, 9);
    std::vector<char> trimmed{buffer.begin(),
                              buffer.begin() + static_cast<std::ptrdiff_t>(n)};
    ASSERT_THAT(trimmed, ElementsAre(0x92, 0xA6, 's', 'i', 'g', 'n', 'a', 'l', 0xC2));

    bool success = mpack_cpp::expect::ReadFromMsgPack(after, buffer, n);
    EXPECT_TRUE(success);
    EXPECT_EQ(before.first, after.first);
    EXPECT_EQ(before.second, after.second);
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
        bool success = mpack_cpp::expect::ReadFromMsgPack(after, buffer, n);
        EXPECT_TRUE(success);
        EXPECT_EQ(before, after);
    }

    // Without offset, pass as pointer
    {
        std::fill(std::begin(buffer), std::end(buffer), 0);
        after.clear();
        auto n = mpack_cpp::WriteToMsgPack(before, buffer.data(), buffer.size());
        bool success = mpack_cpp::expect::ReadFromMsgPack(after, buffer.data(), n);
        EXPECT_TRUE(success);
        EXPECT_EQ(before, after);
    }

    // With offset.
    {
        std::fill(std::begin(buffer), std::end(buffer), 0);
        after.clear();
        auto n = mpack_cpp::WriteToMsgPack(before, buffer.data() + 4, buffer.size() - 4);
        bool success = mpack_cpp::expect::ReadFromMsgPack(after, buffer.data() + 4, n);
        EXPECT_TRUE(success);
        EXPECT_EQ(before, after);
    }
}
}  // namespace

TEST(mpack_expect_reader, try_different_buffer_overloads) {
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
        bool success = mpack_cpp::expect::ReadFromMsgPack(after, buffer_raw, n);
        EXPECT_TRUE(success);
        EXPECT_EQ(before, after);
    }

    {
        std::fill(std::begin(buffer_raw), std::end(buffer_raw), 0);
        after.clear();
        auto n = mpack_cpp::WriteToMsgPack(before, buffer_raw + 4, BUFFER_SIZE - 4);
        bool success = mpack_cpp::expect::ReadFromMsgPack(after, buffer_raw + 4, n);
        EXPECT_TRUE(success);
        EXPECT_EQ(before, after);
    }
}
