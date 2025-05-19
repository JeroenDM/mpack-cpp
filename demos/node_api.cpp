#include <array>
#include <cassert>
#include <iostream>
#include <type_traits>

#include "mpack.h"
#include "mpack_cpp/mpack_writer.hpp"

constexpr std::size_t BUFFER_SIZE{1024};

struct TreeVisitor {
    mpack_node_t node;

    void operator()(bool& out) { out = mpack_node_bool(node); }
    void operator()(std::uint64_t& out) { out = mpack_node_u64(node); }

    void operator()(std::string& out) {
        out.resize(mpack_node_strlen(node) + 1);
        mpack_node_copy_cstr(node, out.data(), out.size());
        out.resize(mpack_node_strlen(node));
    }

    template <typename T, typename Allocator>
    void operator()(std::vector<T, Allocator>& out) {
        out.resize(mpack_node_array_length(node));
        for (std::size_t i{0}; i < out.size(); ++i) {
            auto nested_node = mpack_node_array_at(node, i);
            TreeVisitor{nested_node}(out[i]);
        }
    }
};

template <typename T>
void ReadField(mpack_node_t node, const char* key, T& out) {
    auto value_node = mpack_node_map_cstr(node, key);
    TreeVisitor{value_node}(out);
}

struct Group {
    std::string name;
    std::vector<std::pair<std::string, std::variant<bool, double>>> skills;

    void to_message_pack(mpack_writer_t& writer) const {
        mpack_cpp::WriteField(writer, "GroupName", name);
        mpack_cpp::WriteField(writer, "Skills", skills);
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
    std::vector<std::uint64_t> groups;
    // std::vector<Group> groups;
    // Label label;

    void to_message_pack(mpack_writer_t& writer) const {
        mpack_cpp::WriteField(writer, "Name", name);
        mpack_cpp::WriteField(writer, "Time", time);
        mpack_cpp::WriteField(writer, "Groups", groups);

        // auto data = static_cast<char>(label);
        // mpack_cpp::WriteExtField(writer, "Status", 0x01, std::array{data});
    }

    void from_message_pack_node(mpack_node_t node) {
        ReadField(node, "Name", name);
        ReadField(node, "Time", time);
        ReadField(node, "Groups", groups);
    }
};

std::ostream& operator<<(std::ostream& os, const ComplexData& d) {
    os << "Name: " << d.name << "\n";
    os << "Time: " << d.time << "\n";
    for (const auto& g : d.groups) {
        os << g << ", ";
    }
    os << "\n";
    return os;
}

int main() {
    std::vector<char> buffer(BUFFER_SIZE);
    ComplexData before{
        "far-away-land", 1234, {1, 2, 3},
        // {Group{
        //      "forest",
        //      {{"CanTalk", false}, {"Size", 14.0}},
        //  },
        //  Group{
        //      "sea",
        //      {{"CanTalk", true}, {"IsWet", true}, {"Size", -9.2}},
        //  }},
        // Label::NEVER,
    };

    auto n = mpack_cpp::WriteToMsgPack(before, buffer);

    std::cout << "n: " << n << std::endl;
    mpack_print(buffer.data(), n);

    std::array<mpack_node_data_t, 200> pool;

    mpack_tree_t tree;
    // mpack_tree_init_data(&tree, buffer.data(), n);
    mpack_tree_init_pool(&tree, buffer.data(), n, pool.data(), pool.size());
    mpack_tree_parse(&tree);
    mpack_node_t root = mpack_tree_root(&tree);

    if (tree.error != mpack_ok) {
        std::cout << "ERROR: " << mpack_error_to_string(tree.error) << std::endl;
    } else {
        std::cout << "SUCCES\n";
    }

    ComplexData after;
    after.from_message_pack_node(root);
    std::cout << "--- after ---\n" << after << std::endl;

    // std::string name(30, '\0');
    // mpack_node_copy_cstr(mpack_node_map_cstr(root, "Name"), name.data(), name.size());
    // name.resize(std::strlen(name.data()));

    // // auto t = mpack_node_u64(mpack_node_map_cstr(root, "Time"));
    // std::uint64_t t{};
    // ReadField(root, "Time", t);

    // auto n2 = mpack_node_map_cstr_optional(root, "NotInData");
    // assert(mpack_node_type(n2) == mpack_type_missing);
    // std::cout << "name: " << name.c_str() << "\ntime: " << t << std::endl;

    // auto groups = mpack_node_map_cstr(root, "Groups");
    // auto num_groups = mpack_node_array_length(groups);
    // for (size_t i{0}; i < num_groups; ++i) {
    //     auto g = mpack_node_array_at(groups, i);
    //     std::string g_name(30, '\0');
    //     mpack_node_copy_cstr(mpack_node_map_cstr(g, "GroupName"), g_name.data(),
    //                          g_name.size());
    //     std::cout << i << ": " << g_name.c_str() << std::endl;

    //     auto skills = mpack_node_map_cstr(g, "Skills");
    //     auto num_skills = mpack_node_array_length(skills);
    //     std::string s_name(30, '\0');
    //     for (size_t j{0}; j < num_skills; ++j) {
    //         auto el = mpack_node_array_at(skills, j);
    //         assert(mpack_node_array_length(el) == 2);

    //         auto first = mpack_node_array_at(el, 0);
    //         mpack_node_copy_cstr(first, s_name.data(), s_name.size());
    //         s_name.resize(std::strlen(s_name.data()));

    //         auto second = mpack_node_array_at(el, 1);
    //         if (mpack_node_type(second) == mpack_type_bool) {
    //             auto b = mpack_node_bool(second) ? "true" : "false";
    //             std::cout << "size: " << s_name.size() << std::endl;
    //             std::cout << "\t" << j << ": " << s_name << ", " << b << std::endl;
    //         } else if (mpack_node_type(second) == mpack_type_double) {
    //             auto d = mpack_node_double(second);
    //             std::cout << "\t" << j << ": " << s_name << ", " << d << std::endl;
    //         }
    //     }
    // }

    mpack_tree_destroy(&tree);

    return 0;
}
