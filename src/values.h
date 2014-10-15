#pragma once

struct Allocator;

#include <stddef.h>
#include <stdint.h>

enum TypeTags {
        TTAG_NULL,
        TTAG_FLOAT,
};

struct Value
{
        uint32_t type_tag;
        size_t element_size;
        void const *address;
        struct Allocator *allocator;
};

static inline struct Value nullValue()
{
        return (struct Value){.type_tag = TTAG_NULL};
}

void freeValue(struct Value *value);
