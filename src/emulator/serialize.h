#pragma once

#define SERIALIZED_SIZE_FUNCTION_DECL(name) size_t name##_serialized_length(emulator_t *emu)
#define SERIALIZER_FUNCTION_DECL(name) byte_t *name##_serialize(emulator_t *emu, size_t *size)
#define UNSERIALIZER_FUNCTION_DECL(name) void name##_unserialize(emulator_t *emu, byte_t *buf)

#define SERIALIZE_FUNCTION_DECLS(name)   \
    SERIALIZED_SIZE_FUNCTION_DECL(name); \
    SERIALIZER_FUNCTION_DECL(name);      \
    UNSERIALIZER_FUNCTION_DECL(name);

#define SERIALIZED_SIZE_FUNCTION(type, name, ...) \
    SERIALIZED_SIZE_FUNCTION_DECL(name) {         \
        size_t length = 0;                        \
        type *tmp = emu->name;                    \
        __VA_ARGS__;                              \
        return length;                            \
    }

#define SERIALIZER_FUNCTION(type, name, ...)   \
    SERIALIZER_FUNCTION_DECL(name) {           \
        *size = name##_serialized_length(emu); \
        byte_t *buf = xmalloc(*size);          \
        size_t offset = 0;                     \
        type *tmp = emu->name;                 \
        __VA_ARGS__;                           \
        return buf;                            \
    }

#define UNSERIALIZER_FUNCTION(type, name, ...) \
    UNSERIALIZER_FUNCTION_DECL(name) {         \
        type *tmp = emu->name;                 \
        size_t offset = 0;                     \
        __VA_ARGS__;                           \
    }

#define SERIALIZED_LENGTH(member) length += sizeof(tmp->member)

#define SERIALIZED_LENGTH_ARRAY_OF_STRUCTS(array, member) \
    length += sizeof(tmp->array[0].member) * (sizeof(tmp->array) / sizeof(tmp->array[0]));

#define SERIALIZE(member)                                          \
    do {                                                           \
        memcpy(buf + offset, &(tmp->member), sizeof(tmp->member)); \
        offset += sizeof(tmp->member);                             \
    } while (0)

#define SERIALIZE_ARRAY_OF_STRUCTS(array, member)                                    \
    for (size_t i = 0; i < sizeof(tmp->array) / sizeof(tmp->array[0]); i++) {        \
        memcpy(buf + offset, &(tmp->array[i].member), sizeof(tmp->array[i].member)); \
        offset += sizeof(tmp->array[i].member);                                      \
    }

#define UNSERIALIZE(member)                                        \
    do {                                                           \
        memcpy(&(tmp->member), buf + offset, sizeof(tmp->member)); \
        offset += sizeof(tmp->member);                             \
    } while (0)

#define UNSERIALIZE_ARRAY_OF_STRUCTS(array, member)                                  \
    for (size_t i = 0; i < sizeof(tmp->array) / sizeof(tmp->array[0]); i++) {        \
        memcpy(&(tmp->array[i].member), buf + offset, sizeof(tmp->array[i].member)); \
        offset += sizeof(tmp->array[i].member);                                      \
    }
