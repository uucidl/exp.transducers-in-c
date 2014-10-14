#include "allocator.h"
#include "allocator_type.h"
#include "stream_types.h"
#include "transducer_types.h"
#include "transducers.h"
#include "value_stream_types.h"
#include "value_streams.h"
#include "values.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* 1. extensions to values & transducers */

static struct Value floatValue(float const f, struct Allocator * const allocator)
{
        float* result = allocator_alloc(allocator, sizeof *result);
        *result = f;
        return (struct Value) {
                .type_tag = TTAG_FLOAT,
                .element_size = sizeof *result,
                .address = result
        };
}

static float justFloat(struct Value value) {
        assert(value.type_tag == TTAG_FLOAT);
        return *((float*) value.address);
}

#define TTAG_IndexedValue (0xd4e1cf8d)

struct IndexedValue
{
        struct Value value;
        size_t index;
};

static size_t justIndex(struct Value value) {
        assert (value.type_tag == TTAG_IndexedValue);
        return ((struct IndexedValue*) value.address)->index;
}

static struct Value indexValue(struct Value value, size_t index, struct Allocator* allocator) {
        struct IndexedValue* result = allocator_alloc(allocator, sizeof *result);

        *result = (struct IndexedValue) {
                .value = value,
                .index = index,
        };

        return (struct Value) { TTAG_IndexedValue, sizeof *result, result, allocator };
}

static struct Value justValueOfIndexedValue(struct Value indexedValue) {
        assert (indexedValue.type_tag == TTAG_IndexedValue);
        return ((struct IndexedValue*) indexedValue.address)->value;
}


static struct Value transduceFloatArray(float* values, size_t valuesCount, struct Transducer* transducer, struct Allocator* allocator) {
        struct Reducer* reducer = transducer_apply(transducer, idReducer(allocator), allocator);
        struct Value result = reducer_identity(reducer, allocator);
        for (size_t i = 0; i < valuesCount; i++) {
                struct Value value = { TTAG_FLOAT, sizeof values[i], &values[i], 0 };
                result = reducer_apply(reducer, value, result, allocator);
        }

        return result;
}

/* 2. streams of values */

static
struct Value nextValueVSR(struct ValueStreamRange* range)
{
        static struct Value zero;

        while (range->error == S_NoError) {
                if (range->cursor < range->end) {
                        void const* valueAddress = range->cursor;
                        range->cursor += range->element_size;
                        return (struct Value) {
                                .type_tag = range->type_tag,
                                        .element_size = range->element_size,
                                        .address = valueAddress,
                                        };
                }
                range->next(range);
        }

        return zero;
}

static
struct Value reduceStream(struct ValueStreamRange* range, struct Reducer* reducer, struct Allocator * allocator)
{
        struct Value element;
        struct Value result = reducer_identity(reducer, allocator);
        while ((element = nextValueVSR(range), range->error == S_NoError)) {
                result = reducer_apply(reducer, element, result, allocator);
        }
        return result;
}

/* 3. reducers */

static
struct Value accumulateFloat(
        struct Value const input,
        struct Value const current,
        struct Allocator * const allocator
        )
{
        assert (input.type_tag == TTAG_FLOAT);
        assert (current.type_tag == TTAG_FLOAT);

        float const* inputFloat = input.address;
        float const* currentFloat = current.address;

        return floatValue(*inputFloat + *currentFloat, allocator);
}

static
struct Value accumulateFloatIdentity(struct Reducer const* reducer, struct Allocator* allocator)
{
        static float zero = 0.0f;
        return (struct Value) {
                .type_tag = TTAG_FLOAT,
                .element_size = sizeof zero,
                .address = &zero,
        };
}

static
struct Value accumulateFloatApply(struct Reducer const* reducer,
                                  struct Value const input,
                                  struct Value const current,
                                  struct Allocator* allocator)
{
        return accumulateFloat(input, current, allocator);
}

static
struct Value printReducerIdentity(struct Reducer const* reducer, struct Allocator* allocator)
{
        return nullValue();
}

static void printValue(struct Value value) {
        if (value.type_tag == TTAG_FLOAT) {
                printf("%f", *((float*)value.address));
        } else if (value.type_tag == TTAG_IndexedValue) {
                printf("(%zu ", justIndex(value));
                printValue(justValueOfIndexedValue(value));
                printf(")");

        } else {
                printf("?");
        }
}

static struct Value printReducerApply(struct Reducer const* reducer, struct Value input, struct Value current, struct Allocator* allocator) {
        if (current.type_tag != 0) {
                printf(", ");
        }

        printValue(input);

        return input;
}

static struct Reducer* printReducer(struct Allocator* allocator) {
        struct Reducer* result =
                allocator_alloc(allocator, sizeof *result);

        *result = (struct Reducer) {
                .identity = printReducerIdentity,
                .apply = printReducerApply,
        };

        return result;
}

static bool positiveFloatsOnly(struct Value value, void* data) {
        (void) data;
        return value.type_tag == TTAG_FLOAT && *((float*) value.address) > 0.0f;
}

struct Value indexingReducerIdentity(struct Reducer const *reducer, struct Allocator *allocator) {
        return nullValue();
}

struct Value indexingReducerApply(struct Reducer const *reducer, struct Value input,
                           struct Value current, struct Allocator *allocator) {
        size_t index;
        if (current.type_tag == 0) {
                index = 0;
        } else {
                index = 1 + justIndex(current);
        }

        return indexValue(input, index, allocator);
}

struct Reducer* indexingReducer(struct Allocator* allocator) {
        struct Reducer* result = allocator_alloc(allocator, sizeof *result);

        *result = (struct Reducer) {
                indexingReducerIdentity,
                indexingReducerApply,
        };

        return result;
}

struct Range
{
        size_t start;
        size_t end;
};

static bool isIndexInRange(struct Value value, void* userData) {
        struct Range* range = userData;

        size_t index = justIndex(value);

        return index >= range->start && index < range->end;
}

struct Value invertFloat(struct Value value, void *userData)
{
        struct Allocator* allocator = userData;
        return floatValue(-justFloat(value), allocator);
}

/* main program */

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

        static struct Reducer accumulator = {
                .identity = accumulateFloatIdentity,
                .apply = accumulateFloatApply,
        };

        printf("1. individual test\n");
        {
                struct Value a = floatValue(1.0f, &heapAllocator);
                struct Value b = floatValue(3.0f, &heapAllocator);
                struct Value result = accumulateFloat(a, b, &heapAllocator);

                printf("result is: %f; expected: 4.0\n", justFloat(result));

                freeValue(&a), freeValue(&b), freeValue(&result);
        }

        printf("2. process array as stream\n");
        {
                float values[] = { 1.0, 2.0, 3.0, 4.0 };
                struct ValueStreamRange valuesRange;
                floatArrayVSR(&valuesRange, values, sizeof values / sizeof values[0]);

                struct Value result = reduceStream(&valuesRange, &accumulator, &heapAllocator);
                printf("result is: %f; expected: 10.0\n", justFloat(result));
        }

        printf("3. filter out all negative floats and accumulate\n");
        {
                float values[] = { -1.0f, 1.0f, -2.0f, 2.0f, 3.0f, -3.0f, 4.0f, -4.0f };
                struct ValueStreamRange valuesRange;
                struct Transducer* processSteps[] = {
                        filteringTransducer(positiveFloatsOnly, NULL, &heapAllocator),
                        mappingTransducer(&accumulator, &heapAllocator),
                };
                struct Transducer* process = composingTransducer(
                        processSteps,
                        sizeof processSteps / sizeof processSteps[0],
                        &heapAllocator);


                printf("input: ");
                {
                        floatArrayVSR(&valuesRange, values, sizeof values / sizeof values[0]);
                        reduceStream(&valuesRange, printReducer(&heapAllocator), &heapAllocator);
                }

                printf("\nprint-out: ");
                struct Value result;
                {
                        struct Reducer* reducer = transducer_apply(process, printReducer(&heapAllocator), &heapAllocator);

                        floatArrayVSR(&valuesRange, values, sizeof values / sizeof values[0]);
                        result = reduceStream(&valuesRange, reducer, &heapAllocator);
                        printf("\n");
                }

                printf("result is: %f ; expected: 10.0\n", justFloat(result));

                printf("transduce it again with transduceFloatArray\n");
                {
                        struct Value result = transduceFloatArray(values, sizeof values / sizeof values[0], process, &heapAllocator);
                        printf("result is: %f ; expected: 10.0\n", justFloat(result));
                }
        }

        printf ("4. demonstrate that a processing can stop earlier\n");
        {
                struct Range range = {
                        .start = 0,
                        .end = 4,
                };
                struct Transducer* processSteps[] = {
                        mappingFnTransducer(invertFloat, &heapAllocator, &heapAllocator),
                        filteringTransducer(positiveFloatsOnly, NULL, &heapAllocator),
                        mappingTransducer(indexingReducer(&heapAllocator), &heapAllocator),
                        filteringTransducer(isIndexInRange, &range, &heapAllocator),
                        mappingTransducer(printReducer(&heapAllocator), &heapAllocator),
//                        fnMappingTransducer(unwrapIndexedValue, NULL, &heapAllocator),
//                        mappingTransducer(&accumulator, &heapAllocator),
                };
                struct Transducer* process = composingTransducer(
                        processSteps,
                        sizeof processSteps / sizeof processSteps[0],
                        &heapAllocator);

                float values[] = { -3.0, -5.0, 1.0, 2.0, 3.0, 4.0, -5.0, -6.0, -7.0 };
                {
                        struct Value result = transduceFloatArray(values, sizeof values / sizeof values[0], process, &heapAllocator);
                        printf("\n");
                        if (result.type_tag == TTAG_FLOAT) {
                                printf("result is: %f ; expected 10.0\n", justFloat(result));
                        }
                }
        }

        return 0;
}
