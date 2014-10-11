#pragma once

#include <stddef.h> /* for size_t */

struct Allocator;

void *allocator_alloc(struct Allocator *allocator, size_t size);
void allocator_free(struct Allocator *allocator, void *ptr);
