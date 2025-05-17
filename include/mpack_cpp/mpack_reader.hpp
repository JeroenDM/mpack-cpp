#ifndef MPACK_CPP__MPACK_READER_HPP_
#define MPACK_CPP__MPACK_READER_HPP_

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "mpack.h"  //  NOLINT

namespace mpack_cpp {

namespace internal {

template <typename T, typename... Args>
void read_and_assign(std::variant<Args...>& variant, T (*reader_func)(mpack_reader_t*),
                     mpack_reader_t* reader) {
    T value = reader_func(reader);
    if constexpr ((std::is_same_v<T, Args> || ...)) {
        variant = value;
    } else {
        mpack_reader_flag_error(reader, mpack_error_unsupported);
    }
}

struct ReadVisitor {
    mpack_reader_t& reader;

    void operator()(bool& out) { out = mpack_expect_bool(&reader); }

    void operator()(float& out) { out = mpack_expect_float(&reader); }
    void operator()(double& out) { out = mpack_expect_double(&reader); }

    void operator()(std::uint8_t& out) { out = mpack_expect_u8(&reader); }
    void operator()(std::uint16_t& out) { out = mpack_expect_u16(&reader); }
    void operator()(std::uint32_t& out) { out = mpack_expect_u32(&reader); }
    void operator()(std::uint64_t& out) { out = mpack_expect_u64(&reader); }

    void operator()(std::int8_t& out) { out = mpack_expect_i8(&reader); }
    void operator()(std::int16_t& out) { out = mpack_expect_i16(&reader); }
    void operator()(std::int32_t& out) { out = mpack_expect_i32(&reader); }
    void operator()(std::int64_t& out) { out = mpack_expect_i64(&reader); }

    void operator()(std::string& value) {
        uint32_t length = mpack_expect_str(&reader);
        value.resize(static_cast<std::size_t>(length));
        mpack_read_cstr(&reader, value.data(), 100, value.size());
        mpack_done_str(&reader);
    }

    template <typename ElemT>
    void operator()(std::vector<ElemT>& vec) {
        std::size_t count = mpack_expect_array_max(&reader, 100);
        vec.resize(count);
        for (std::size_t i{0}; i < count; ++i) {
            (*this)(vec[i]);
        }
        mpack_done_array(&reader);
    }

    template <typename FirstT, typename SecondT>
    void operator()(std::pair<FirstT, SecondT>& pair) {
        mpack_expect_array_match(&reader, 2);
        (*this)(pair.first);
        (*this)(pair.second);
        mpack_done_array(&reader);
    }

    template <typename... Args>
    void operator()(std::variant<Args...>& variant) {
        auto tag = mpack_peek_tag(&reader);
        switch (tag.type) {
            case mpack_type_bool:
                read_and_assign<bool>(variant, mpack_expect_bool, &reader);
                break;
            case mpack_type_double:
                read_and_assign<double>(variant, mpack_expect_double, &reader);
                break;
            case mpack_type_uint:
                read_and_assign<uint64_t>(variant, mpack_expect_u64, &reader);
                break;
            default:
                mpack_reader_flag_error(&reader, mpack_error_unsupported);
        }
    }
    /**
     * @brief Generic fallback template for deserializing objects from a
     * MessagePack reader.
     *
     * For most generic template, when no specialization is found for the type
     * being deserialized, It uses the `from_message_pack` method of the type
     * `T` to deserialize the object. This method allows nested objects to be
     * deserialized as well.
     *
     * @tparam T The type of the object to deserialize.
     * @param value The object to populate with deserialized data.
     */

    // template<typename T, std::enable_if<has_from_message_pack_v<T>, int> = 0>
    template <typename T>
    void operator()(T& value) {
        mpack_expect_map_max(&reader, 30);
        value.from_message_pack(reader);
        mpack_done_map(&reader);
    }
};

}  // namespace internal

template <typename T>
void ReadField(mpack_reader_t& reader, const char* key, T&& value) {
    mpack_expect_cstr_match(&reader, key);
    internal::ReadVisitor{reader}(std::forward<T>(value));
}

template <std::size_t N>
void ReadExtField(mpack_reader_t& reader, const char* key, std::int8_t& type,
                  std::array<char, N>& data) {
    mpack_expect_cstr_match(&reader, key);
    auto n =
        mpack_expect_ext_max(&reader, &type, static_cast<std::uint32_t>(data.size()));
    if (n != data.size()) {
        mpack_reader_flag_error(&reader, mpack_error_data);
    } else {
        mpack_read_bytes(&reader, data.data(), data.size());
        mpack_done_ext(&reader);
    }
}

namespace internal {
inline bool IsFixStr(const char c) {
    // Check for pattern 101xxxxx.
    return (static_cast<std::uint8_t>(c) >> 5) == 5;
}

inline std::uint8_t GetFixStrLength(const char c) {
    // Extract last 5 bits as uint8.
    return static_cast<std::uint8_t>(c) & static_cast<uint8_t>(~0xe0);
}
}  // namespace internal

template <typename T>
void ReadOptionalField(mpack_reader_t& reader, const char* key, T&& value) {
    value = std::nullopt;
    if (!internal::IsFixStr(reader.data[0])) {
        return;
    }

    auto length = internal::GetFixStrLength(reader.data[0]);
    const std::string key_s(key, strlen(key));
    const std::string next_key_s(reader.data + 1, length);
    if (key_s != next_key_s) {
        return;
    }

    value.emplace();
    ReadField(reader, key, value.value());
}

template <typename T>
bool ReadFromMsgPack(T& data, const char* buffer_start, std::size_t msg_size) {
    mpack_reader_t reader;
    mpack_reader_init_data(&reader, buffer_start, msg_size);
    internal::ReadVisitor{reader}(data);
    auto err = mpack_reader_destroy(&reader);
    if (err != mpack_ok) {
        fprintf(stderr, "An error occurred decoding the data!\n");
        fprintf(stderr, "%s!\n", mpack_error_to_string(err));
        return false;
    } else {
        return true;
    }
}

template <typename T>
bool ReadFromMsgPack(T& msg, const std::uint8_t* buffer_start, std::size_t msg_size) {
    return ReadFromMsgPack(msg, reinterpret_cast<const char*>(buffer_start), msg_size);
}

template <typename T>
bool ReadFromMsgPack(T& msg, const std::vector<char>& buffer, std::size_t msg_size) {
    return ReadFromMsgPack(msg, buffer.data(), msg_size);
}

template <typename T>
bool ReadFromMsgPack(T& msg, const std::vector<std::uint8_t>& buffer,
                     std::size_t msg_size) {
    return ReadFromMsgPack(msg, reinterpret_cast<const char*>(buffer.data()), msg_size);
}

}  // namespace mpack_cpp

#endif  //  MPACK_CPP__MPACK_READER_HPP_
