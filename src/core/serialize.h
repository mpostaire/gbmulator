#pragma once

#define SERIALIZED_SIZE_FUNCTION_DECL(name) size_t name##_serialized_length(gb_t *gb)
#define SERIALIZER_FUNCTION_DECL(name) uint8_t *name##_serialize(gb_t *gb, size_t *size)
#define UNSERIALIZER_FUNCTION_DECL(name) void name##_unserialize(gb_t *gb, uint8_t *buf)

#define SERIALIZE_FUNCTION_DECLS(name)   \
    SERIALIZED_SIZE_FUNCTION_DECL(name); \
    SERIALIZER_FUNCTION_DECL(name);      \
    UNSERIALIZER_FUNCTION_DECL(name);

#define SERIALIZED_SIZE_FUNCTION(type, name, ...) \
    SERIALIZED_SIZE_FUNCTION_DECL(name) {         \
        size_t length = 0;                        \
        type *tmp = gb->name;                    \
        __VA_ARGS__;                              \
        return length;                            \
    }

#define SERIALIZER_FUNCTION(type, name, ...)   \
    SERIALIZER_FUNCTION_DECL(name) {           \
        *size = name##_serialized_length(gb); \
        uint8_t *buf = xmalloc(*size);          \
        size_t offset = 0;                     \
        type *tmp = gb->name;                 \
        __VA_ARGS__;                           \
        return buf;                            \
    }

#define UNSERIALIZER_FUNCTION(type, name, ...) \
    UNSERIALIZER_FUNCTION_DECL(name) {         \
        type *tmp = gb->name;                 \
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

// serialization macros for the special cases of the mmu:

#define SERIALIZED_LENGTH_COND_LITERAL(member, cond, size_literal_true, size_literal_else) \
    length += cond ? size_literal_true : size_literal_else

#define SERIALIZE_COND_LITERAL(member, cond, size_literal_true, size_literal_else) \
    do {                                                                           \
        size_t sz = cond ? size_literal_true : size_literal_else;                  \
        memcpy(buf + offset, &(tmp->member), sz);                                  \
        offset += sz;                                                              \
    } while (0)

#define UNSERIALIZE_COND_LITERAL(member, cond, size_literal_true, size_literal_else) \
    do {                                                                             \
        size_t sz = cond ? size_literal_true : size_literal_else;                    \
        memcpy(&(tmp->member), buf + offset, sz);                                    \
        offset += sz;                                                                \
    } while (0)

#define SERIALIZED_LENGTH_FROM_MEMBER(member, size_member, multiplier) \
    length += tmp->size_member * (multiplier)

#define SERIALIZE_FROM_MEMBER(member, size_member, multiplier) \
    do {                                                       \
        size_t sz = tmp->size_member * (multiplier);           \
        memcpy(buf + offset, &(tmp->member), sz);              \
        offset += sz;                                          \
    } while (0)

#define UNSERIALIZE_FROM_MEMBER(member, size_member, multiplier) \
    do {                                                         \
        size_t sz = tmp->size_member * (multiplier);             \
        memcpy(&(tmp->member), buf + offset, sz);                \
        offset += sz;                                            \
    } while (0)

#define SERIALIZED_LENGTH_IF_CGB(member) \
    length += gb->mode == GB_MODE_CGB ? sizeof(tmp->member) : 0

#define SERIALIZE_IF_CGB(member)     \
    do {                             \
        if (gb->mode == GB_MODE_CGB) \
            SERIALIZE(member);       \
    } while (0)

#define UNSERIALIZE_IF_CGB(member)   \
    do {                             \
        if (gb->mode == GB_MODE_CGB) \
            UNSERIALIZE(member);     \
    } while (0)
