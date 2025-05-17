#ifndef MPACK_CPP__MPACK_WRITER_HPP_
#define MPACK_CPP__MPACK_WRITER_HPP_

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "mpack.h"  //  NOLINT

namespace mpack_cpp {
namespace internal {

struct WriteVisitor {
    mpack_writer_t& writer;

    void operator()(bool value) { mpack_write_bool(&writer, value); }

    void operator()(float value) { mpack_write_float(&writer, value); }
    void operator()(double value) { mpack_write_double(&writer, value); }

    void operator()(std::uint8_t value) { mpack_write_u8(&writer, value); }
    void operator()(std::uint16_t value) { mpack_write_u16(&writer, value); }
    void operator()(std::uint32_t value) { mpack_write_u32(&writer, value); }
    void operator()(std::uint64_t value) { mpack_write_u64(&writer, value); }

    void operator()(std::int8_t value) { mpack_write_i8(&writer, value); }
    void operator()(std::int16_t value) { mpack_write_i16(&writer, value); }
    void operator()(std::int32_t value) { mpack_write_i32(&writer, value); }
    void operator()(std::int64_t value) { mpack_write_i64(&writer, value); }

    void operator()(const std::string& value) {
        mpack_write_utf8(&writer, value.c_str(),
                         static_cast<std::uint32_t>(value.size()));
    }

    template <typename ElemT>
    void operator()(const std::vector<ElemT>& vec) {
        mpack_start_array(&writer, static_cast<std::uint32_t>(vec.size()));
        for (const auto& elem : vec) {
            // Recursively process each element in the vector.
            (*this)(elem);
        }
        mpack_finish_array(&writer);
    }

    template <typename First, typename Second>
    void operator()(const std::pair<First, Second>& pair) {
        mpack_start_array(&writer, 2);
        (*this)(pair.first);
        (*this)(pair.second);
        mpack_finish_array(&writer);
    }

    template <typename... Args>
    void operator()(const std::variant<Args...>& variant) {
        std::visit(*this, variant);
    }

    /** @brief   Recursively process custom types. */
    template <typename T>
    void operator()(const T& value) {
        // The method 'to_message_pack' calls 'AddField' which will
        // call into this visitor again recursively.
        mpack_build_map(&writer);
        value.to_message_pack(writer);
        mpack_complete_map(&writer);
    }
};

}  // namespace internal

/** @brief Add a basic generic field to the given mpack writer. */
template <typename T>
void WriteField(mpack_writer_t& writer, const char* key, T&& value) {
    mpack_write_cstr(&writer, key);
    internal::WriteVisitor{writer}(std::forward<T>(value));
}

template <typename T>
void WriteOptionalField(mpack_writer_t& writer, const char* key, T&& value) {
    if (value.has_value()) {
        WriteField(writer, key, value.value());
    }
}

/** @brief Add an extension type field to the given mpack writer. */
template <std::size_t N>
void WriteExtField(mpack_writer_t& writer, const char* key, std::int8_t type,
                   const std::array<char, N>& data) {
    mpack_write_cstr(&writer, key);
    mpack_start_ext(&writer, type, N);
    mpack_write_bytes(&writer, data.data(), data.size());
    mpack_finish_ext(&writer);
}

template <typename T>
std::size_t WriteToMsgPack(const T& data, char* buffer_start, std::size_t buffer_size) {
    mpack_writer_t writer;
    mpack_writer_init(&writer, buffer_start, buffer_size);
    internal::WriteVisitor{writer}(data);
    std::size_t n = mpack_writer_buffer_used(&writer);

    auto err = mpack_writer_destroy(&writer);
    if (err != mpack_ok) {
        fprintf(stderr, "An error occurred encoding the data!\n");
        fprintf(stderr, "%s!\n", mpack_error_to_string(err));
        return 0;
    } else {
        return n;
    }
}

template <typename T>
std::size_t WriteToMsgPack(const T& msg, std::uint8_t* buffer_start,
                           std::size_t buffer_size) {
    return WriteToMsgPack(msg, reinterpret_cast<char*>(buffer_start), buffer_size);
}

template <typename T, typename ByteT>
std::size_t WriteToMsgPack(const T& msg, std::vector<ByteT>& buffer) {
    return WriteToMsgPack(msg, buffer.data(), buffer.size());
}

}  // namespace mpack_cpp

#endif  //  MPACK_CPP__MPACK_WRITER_HPP_
