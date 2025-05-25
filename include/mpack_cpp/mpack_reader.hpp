#ifndef MPACK_CPP__MPACK_READER_HPP_
#define MPACK_CPP__MPACK_READER_HPP_

#include <algorithm>
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
void read_and_assign(std::variant<Args...>& variant, T (*node_func)(mpack_node_t),
                     mpack_node_t node) {
    T value = node_func(node);
    if constexpr ((std::is_same_v<T, Args> || ...)) {
        variant = value;
    } else {
        assert(false);
        // mpack_reader_flag_error(node, mpack_error_unsupported);
    }
}

/** Main type selection visitor for decoding values.
 *
 * To allow for costum allocation strategies for the destination type,
 * the following overloads are generic with respect to their allocator type:
 *   - `std::basic_string` (std::string, std::pmr::string, ...)
 *   - `std::vector`
 *
 * TODO(jeroedm)?
 *  - std::map
 *  - std::unordered_map
 *
 */
struct ReadVisitor {
    mpack_node_t node;

    void operator()(bool& out) { out = mpack_node_bool(node); }

    void operator()(float& out) { out = mpack_node_float(node); }
    void operator()(double& out) { out = mpack_node_double(node); }

    // Should we use the generic mpack_expect_int here to make it more flexible?
    void operator()(std::uint8_t& out) { out = mpack_node_u8(node); }
    void operator()(std::uint16_t& out) { out = mpack_node_u16(node); }
    void operator()(std::uint32_t& out) { out = mpack_node_u32(node); }
    void operator()(std::uint64_t& out) { out = mpack_node_u64(node); }

    void operator()(std::int8_t& out) { out = mpack_node_i8(node); }
    void operator()(std::int16_t& out) { out = mpack_node_i16(node); }
    void operator()(std::int32_t& out) { out = mpack_node_i32(node); }
    void operator()(std::int64_t& out) { out = mpack_node_i64(node); }

    /** Decode any kind of string with support for custom allocators
     * (e.g std::pmr::vector).
     */
    template <typename CharT, typename Traits, typename Allocator>
    void operator()(std::basic_string<CharT, Traits, Allocator>& out) {
        out.resize(mpack_node_strlen(node) + 1);
        mpack_node_copy_cstr(node, out.data(), out.size());
        out.resize(mpack_node_strlen(node));
    }

    /** Decode any kind of vector with support for custom allocators
     * (e.g. std::pmr::vector),
     */
    template <typename T, typename Allocator>
    void operator()(std::vector<T, Allocator>& out) {
        out.resize(mpack_node_array_length(node));
        for (std::size_t i{0}; i < out.size(); ++i) {
            auto nested_node = mpack_node_array_at(node, i);
            ReadVisitor{nested_node}(out[i]);
        }
    }

    /** Decode a pair from a fixed length array, two element, MessagePack array. */
    template <typename FirstT, typename SecondT>
    void operator()(std::pair<FirstT, SecondT>& pair) {
        if (2 == mpack_node_array_length(node)) {
            ReadVisitor{mpack_node_array_at(node, 0)}(pair.first);
            ReadVisitor{mpack_node_array_at(node, 1)}(pair.second);
        } else {
            assert(false);
            // mpack_node_flag_error(mpack_error_type);
        }
    }

    template <typename... Args>
    void operator()(std::variant<Args...>& variant) {
        switch (mpack_node_type(node)) {
            case mpack_type_bool:
                read_and_assign<bool>(variant, mpack_node_bool, node);
                break;
            case mpack_type_double:
                read_and_assign<double>(variant, mpack_node_double, node);
                break;
            case mpack_type_uint:
                read_and_assign<uint64_t>(variant, mpack_node_u64, node);
                break;
            default:
                mpack_node_flag_error(node, mpack_error_unsupported);
        }
    }
    /**
     * Generic fallback template for deserializing objects from a
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
        std::size_t n = mpack_node_map_count(node);
        if (n > 0) {
            value.from_message_pack(node);
        }
    }
};

}  // namespace internal

/** Generic key-value decoder for 'simple' types.
 *
 * For 'complex' types use the corresponding specialized version:
 *   - `std::optional`: `ReadOptionalField`
 *   - Extension types: `ReadExtField`
 */
template <typename T>
void ReadField(mpack_node_t node, const char* key, T&& out) {
    auto value_node = mpack_node_map_cstr(node, key);
    internal::ReadVisitor{value_node}(std::forward<T>(out));
}

template <std::size_t N>
void ReadExtField(mpack_node_t& node, const char* key, std::int8_t& type,
                  std::array<char, N>& data) {
    auto ext_node = mpack_node_map_cstr(node, key);
    type = mpack_node_exttype(ext_node);
    auto n = mpack_node_data_len(ext_node);
    if (n == data.size()) {
        std::copy_n(mpack_node_data(ext_node), data.size(), data.begin());
    } else {
        assert(false);
        // TODO(jeroendm) report error
    }
}

/** Decode optional fields.  */
template <typename T>
void ReadOptionalField(mpack_node_t& node, const char* key, T&& out) {
    if (mpack_node_map_contains_cstr(node, key)) {
        out.emplace();
        auto opt_node = mpack_node_map_cstr(node, key);
        internal::ReadVisitor{opt_node}(out.value());
    } else {
        out = std::nullopt;
    }
}

template <typename T>
bool ReadFromMsgPack(T& data, const char* buffer_start, std::size_t msg_size) {
    mpack_tree_t tree;
    mpack_tree_init_data(&tree, buffer_start, msg_size);
    // mpack_tree_init_pool(&tree, buffer.data(), n, pool.data(), pool.size());
    mpack_tree_parse(&tree);
    mpack_node_t root = mpack_tree_root(&tree);
    internal::ReadVisitor{root}(data);
    auto err = mpack_tree_destroy(&tree);
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
