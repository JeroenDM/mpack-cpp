#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <vector>

#include "mpack.h"

struct Animal {
    int32_t age;
};

struct Zoo {
    int32_t visitors;
    std::vector<Animal> animals;
};

std::ostream& operator<<(std::ostream& os, const Zoo& zoo) {
    os << "visitors: " << zoo.visitors << "\n";
    os << "animals:";
    for (const auto& a : zoo.animals) {
        os << "animal: " << a.age << ", ";
    }
    os << "\n";
    return os;
}

template <typename T>
using ReaderF = std::function<T(mpack_node_t)>;

template <typename T>
T ReadField(mpack_node_t node, const char* key, ReaderF<T> reader_f) {
    return reader_f(mpack_node_map_cstr(node, key));
}

template <typename El>
std::vector<El> ReadArray(mpack_node_t node, const char* key, ReaderF<El> reader_f) {
    std::vector<El> out;
    auto value_node = mpack_node_map_cstr(node, key);
    out.resize(mpack_node_array_length(value_node));
    for (std::size_t i{0}; i < out.size(); ++i) {
        auto nested_node = mpack_node_array_at(value_node, i);
        out[i] = reader_f(nested_node);
    }
    return out;
}

Animal ReadAnimal(mpack_node_t node) {
    Animal a;
    a.age = ReadField<int32_t>(node, "age", mpack_node_i32);
    return a;
}

Zoo ReadZoo(mpack_node_t root) {
    Zoo z;
    z.visitors = ReadField<int32_t>(root, "visitors", mpack_node_i32);
    z.animals = ReadArray<Animal>(root, "animals", ReadAnimal);
    return z;
}

template <typename T>
struct ReadStep {
    mpack_node_t node;
    const char* key;
    ReaderF<T> reader_f;
};

void WriteZoo(const Zoo&, mpack_writer_t* writer) {
    mpack_build_map(writer);
    mpack_write_cstr(writer, "visitors");
    mpack_write_i32(writer, 32);

    mpack_write_cstr(writer, "animals");
    mpack_build_array(writer);

    mpack_build_map(writer);
    mpack_write_cstr(writer, "age");
    mpack_write_i32(writer, 2);
    mpack_complete_map(writer);

    mpack_build_map(writer);
    mpack_write_cstr(writer, "age");
    mpack_write_i32(writer, 5);
    mpack_complete_map(writer);

    mpack_complete_array(writer);

    mpack_complete_map(writer);
}

int main() {
    char* data;
    size_t size;
    mpack_writer_t writer;
    mpack_writer_init_growable(&writer, &data, &size);

    WriteZoo({}, &writer);

    // finish writing
    if (mpack_writer_destroy(&writer) != mpack_ok) {
        fprintf(stderr, "An error occurred encoding the data!\n");
        return EXIT_FAILURE;
    }

    mpack_tree_t tree;
    mpack_tree_init_data(&tree, data, size);
    mpack_tree_parse(&tree);
    mpack_node_t root = mpack_tree_root(&tree);

    Zoo zoo2 = ReadZoo(root);
    std::cout << zoo2 << "\n";

    // use the data
    // mpack_print_data_to_stdout(data, size);

    free(data);

    return 0;
}