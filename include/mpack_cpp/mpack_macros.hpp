#ifndef MPACK_CPP__MPACK_MACROS_HPP_
#define MPACK_CPP__MPACK_MACROS_HPP_

#include "boost/preprocessor.hpp"

// Each macro takes 3 parameters (r, data, elem) as required by BOOST_PP_SEQ_FOR_EACH
#define MPACK_WRITE_FIELD_OP(r, writer, field) \
    mpack_cpp::WriteField(writer, BOOST_PP_STRINGIZE(field), field);

#define MPACK_READ_FIELD_OP(r, node, field) \
    mpack_cpp::ReadField(node, BOOST_PP_STRINGIZE(field), field);

#define MPACK_EXPECT_READ_FIELD_OP(r, reader, field) \
    mpack_cpp::expect::ReadField(reader, BOOST_PP_STRINGIZE(field), field);

#define MPACK_CPP_DEFINE(Type, ...)                                  \
    void to_message_pack(mpack_writer_t& writer) const {             \
        BOOST_PP_SEQ_FOR_EACH(MPACK_WRITE_FIELD_OP, writer,          \
                              BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \
    }                                                                \
    void from_message_pack(mpack_node_t& node) {                     \
        BOOST_PP_SEQ_FOR_EACH(MPACK_READ_FIELD_OP, node,             \
                              BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \
    }

#define MPACK_CPP_EXPECT_DEFINE(Type, ...)                           \
    void to_message_pack(mpack_writer_t& writer) const {             \
        BOOST_PP_SEQ_FOR_EACH(MPACK_WRITE_FIELD_OP, writer,          \
                              BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \
    }                                                                \
    void from_message_pack(mpack_reader_t& reader) {                 \
        BOOST_PP_SEQ_FOR_EACH(MPACK_EXPECT_READ_FIELD_OP, reader,    \
                              BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \
    }

#endif