#pragma once

enum TypeTags {
        TTAG_NULL,
        TTAG_FLOAT,
};

struct Value
{
        int type_tag;
        size_t element_size;
        void const *address;
        struct Allocator *allocator;
};

static inline struct Value nullValue()
{
        return (struct Value) { .type_tag = TTAG_NULL };
}
