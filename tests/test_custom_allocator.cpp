#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory_resource>
#include <optional>
#include <variant>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mpack_cpp/mpack_reader.hpp"
#include "mpack_cpp/mpack_writer.hpp"

namespace {

constexpr std::size_t BUFFER_SIZE{1024};

struct Animal {
    std::pmr::string name;
    int age;

    Animal(std::pmr::memory_resource* alloc = std::pmr::get_default_resource())
        : name(alloc), age{} {}
    Animal(std::string_view name_, int age_,
           std::pmr::memory_resource* alloc = std::pmr::get_default_resource())
        : name(name_, alloc), age{age_} {}

    void to_message_pack(mpack_cpp::WriteCtx& writer) const {
        mpack_cpp::WriteField(writer, "name", name);
        mpack_cpp::WriteField(writer, "age", age);
    }

    void from_message_pack(mpack_cpp::ReadCtx& reader) {
        mpack_cpp::ReadField(reader, "name", name);
        mpack_cpp::ReadField(reader, "age", age);
    }
};

struct Zoo {
    std::pmr::vector<Animal> animals;

    Zoo(std::pmr::memory_resource* alloc = std::pmr::get_default_resource())
        : animals(alloc) {}
    Zoo(std::initializer_list<Animal> animals_,
        std::pmr::memory_resource* alloc = std::pmr::get_default_resource())
        : animals(animals_, alloc) {}

    void to_message_pack(mpack_cpp::WriteCtx& writer) const {
        mpack_cpp::WriteField(writer, "animals", animals);
    }

    void from_message_pack(mpack_cpp::ReadCtx& reader) {
        mpack_cpp::ReadField(reader, "animals", animals);
    }
};

}  // namespace

TEST(custom_allocator, string_and_vector) {
    std::array<std::uint8_t, 1000> arena_buffer{0};
    std::pmr::monotonic_buffer_resource arena{arena_buffer.data(), arena_buffer.size(),
                                              std::pmr::null_memory_resource()};
    std::pmr::set_default_resource(&arena);

    std::vector<char> buffer(BUFFER_SIZE);

    Zoo before{{Animal{"dog", 11}, Animal{"cat", 5}}};
    auto n = mpack_cpp::WriteToMsgPack(before, buffer);

    EXPECT_EQ(n, 40);
    std::vector<char> trimmed{buffer.begin(),
                              buffer.begin() + static_cast<std::ptrdiff_t>(n)};
    ASSERT_THAT(trimmed, testing::ElementsAre(
                             0x81, 0xA7, 0x61, 0x6E, 0x69, 0x6D, 0x61, 0x6C, 0x73, 0x92,
                             0x82, 0xA4, 0x6E, 0x61, 0x6D, 0x65, 0xA3, 0x64, 0x6F, 0x67,
                             0xA3, 0x61, 0x67, 0x65, 0x0B, 0x82, 0xA4, 0x6E, 0x61, 0x6D,
                             0x65, 0xA3, 0x63, 0x61, 0x74, 0xA3, 0x61, 0x67, 0x65, 0x05));

    Zoo after{};
    after.animals.resize(2);
    // after.animals.at(0).name = std::pmr::string(alloc);
    // after.animals.at(1).name = std::pmr::string(alloc);

    bool success = mpack_cpp::ReadFromMsgPack(after, buffer, n);
    ASSERT_TRUE(success);
    EXPECT_EQ(after.animals.size(), before.animals.size());
    for (std::size_t i = 0; i < after.animals.size(); ++i) {
        EXPECT_EQ(after.animals.at(i).name, before.animals.at(i).name);
        EXPECT_EQ(after.animals.at(i).age, before.animals.at(i).age);
    }
}