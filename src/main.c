#include "allocator.h"
#include "allocator_type.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

enum TypeTags
{
        TTAG_FLOAT,
};

struct Value
{
        int type_tag;
        void* address;
};

static struct Value floatValue(float const f, struct Allocator * const allocator)
{
        float* result = allocator_alloc(allocator, sizeof *result);
        *result = f;
        return (struct Value) { .type_tag = TTAG_FLOAT, .address = result };
}

/* reducer example */
static
struct Value accumulateFloat(
        struct Value const input,
        struct Value const current,
        struct Allocator * const allocator
        )
{
        assert (input.type_tag == TTAG_FLOAT);
        assert (current.type_tag == TTAG_FLOAT);

        float* inputFloat = input.address;
        float* currentFloat = current.address;

        return floatValue(*inputFloat + *currentFloat, allocator);
}

typedef struct Value (*ReducerFn)(struct Value input, struct Value current, struct Allocator * allocator);

static
void* stdlib_alloc(struct Allocator * const allocator, size_t size)
{
        return malloc(size);
}

static
void stdlib_free(struct Allocator * const allocator, void* ptr)
{
        free(ptr);
}

int main (int argc, char** argv)
{
        struct Allocator heapAllocator = {
                .alloc = stdlib_alloc,
                .free = stdlib_free,
        };

        struct Value result = accumulateFloat(floatValue(1.0f, &heapAllocator), floatValue(3.0f, &heapAllocator), &heapAllocator);

        printf("result is: %f", *((float*)result.address));

        return 0;
}
