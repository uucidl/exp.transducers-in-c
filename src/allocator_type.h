#pragma once

#include <stddef.h>

struct Allocator
{
        void *(*alloc)(struct Allocator *self, size_t size);
        void (*free)(struct Allocator *self, void *ptr);
};
