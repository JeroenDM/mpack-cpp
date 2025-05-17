# mpack-cpp
C++17 wrapper around message pack parsing library mpack

## design goals

- Parse json-like data, with key-value pairs.
- Nice interface:
  - Minimal code to parse data to/from msgpack.
  - Specify field name different from member name in parsed struct.
- Option to pass allocator when reading from message pack to struct. 
