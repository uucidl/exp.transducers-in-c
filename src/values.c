#include "values.h"

#include "allocator.h"

void freeValue(struct Value* value)
{
        allocator_free(value->allocator, (void*) value->address);
        value->address = NULL;
        value->allocator = NULL;
}
