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

static struct Value transduceFloatArray(float* values, size_t valuesCount, struct Transducer* transducer, struct Allocator* allocator) {
        struct Reducer* reducer = transducer_apply(transducer, idReducer(allocator), allocator);
        struct Value result = reducer_zero(reducer, allocator);
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
        struct Value result = reducer_zero(reducer, allocator);
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
struct Value accumulateFloatZero(struct Reducer const* reducer, struct Allocator* allocator)
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
struct Value printReducerZero(struct Reducer const* reducer, struct Allocator* allocator)
{
        return nullValue();
}

static struct Value printReducerApply(struct Reducer const* reducer, struct Value input, struct Value current, struct Allocator* allocator) {
        if (current.type_tag != 0) {
                printf(", ");
        }

        if (input.type_tag == TTAG_FLOAT) {
                printf("%f", *((float*)input.address));
        } else {
                printf("?");
        }

        return input;
}

static struct Reducer* printReducer(struct Allocator* allocator) {
        struct Reducer* result =
                allocator_alloc(allocator, sizeof *result);

        *result = (struct Reducer) {
                .zero = printReducerZero,
                .apply = printReducerApply,
        };

        return result;
}

static bool positiveFloatsOnly(struct Value value, void* data) {
        (void) data;
        return value.type_tag == TTAG_FLOAT && *((float*) value.address) > 0.0f;
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
                .zero = accumulateFloatZero,
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

        return 0;
}
