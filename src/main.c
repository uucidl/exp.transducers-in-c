#include "allocator.h"
#include "allocator_type.h"
#include "stream_types.h"
#include "transducer_types.h"
#include "transducers.h"
#include "values.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

struct ValueStreamRange
{
        int type_tag;
        size_t element_size;
        uint8_t const *start;
        uint8_t const *end;
        uint8_t const *cursor;
        enum StreamErrorCode error;
        enum StreamErrorCode (*next)(struct ValueStreamRange*);
};

static enum StreamErrorCode zerosVSRNext(struct ValueStreamRange* range)
{
        static uint8_t const zeros[256] = {0};

        range->type_tag = 0;
        range->element_size = 1;
        range->start = zeros;
        range->cursor = zeros;
        range->end = zeros + sizeof(zeros);

        return range->error;
}

static enum StreamErrorCode failVSR(struct ValueStreamRange* range, enum StreamErrorCode error)
{
        range->error = error;
        range->next = zerosVSRNext;
        return range->next(range);
}

static enum StreamErrorCode floatArrayNext(struct ValueStreamRange* range)
{
        return failVSR(range, S_ReadPastEnd);
}

static
void floatArrayVSR(struct ValueStreamRange* range, float const* values, size_t count)
{
        range->type_tag = TTAG_FLOAT;
        range->element_size = sizeof(float);
        range->start = (uint8_t*) values;
        range->cursor = (uint8_t*) values;
        range->end = (uint8_t*) (values + count);
        range->error = S_NoError;
        range->next = floatArrayNext;
}

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

static struct Value transduceFloatArray(float* values, size_t valuesCount, struct Transducer* transducer, struct Allocator* allocator) {
        struct Reducer* reducer = transducer_apply(transducer, idReducer(allocator), allocator);
        struct Value result = reducer_zero(reducer, allocator);
        for (size_t i = 0; i < valuesCount; i++) {
                struct Value value = { TTAG_FLOAT, sizeof values[i], &values[i], 0 };
                result = reducer_apply(reducer, value, result, allocator);
        }

        return result;
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

static bool positiveFloatsOnly(struct Value value) {
        return value.type_tag == TTAG_FLOAT && *((float*) value.address) > 0.0f;
}

int main (int argc, char** argv)
{
        struct Allocator heapAllocator = {
                .alloc = stdlib_alloc,
                .free = stdlib_free,
        };

        printf("1. individual test\n");
        {
                struct Value a = floatValue(1.0f, &heapAllocator);
                struct Value b = floatValue(3.0f, &heapAllocator);
                struct Value result = accumulateFloat(a, b, &heapAllocator);

                printf("result is: %f; expected: 4.0\n", *((float*)result.address));

                freeValue(&a), freeValue(&b), freeValue(&result);
        }

        printf("2. process array as stream\n");
        {
                float values[] = { 1.0, 2.0, 3.0, 4.0 };
                struct ValueStreamRange valuesRange;
                struct Reducer accumulator = {
                        .zero = accumulateFloatZero,
                        .apply = accumulateFloatApply,
                };
                floatArrayVSR(&valuesRange, values, sizeof values / sizeof values[0]);

                struct Value result = reduceStream(&valuesRange, &accumulator, &heapAllocator);
                printf("result is: %f; expected: 10.0\n", *((float*) result.address));
        }

        printf("3. filter out all negative floats and accumulate\n");
        {
                float values[] = { -1.0f, 1.0f, -2.0f, 2.0f, 3.0f, -3.0f, 4.0f, -4.0f };
                struct ValueStreamRange valuesRange;
                struct Reducer accumulator = {
                        .zero = accumulateFloatZero,
                        .apply = accumulateFloatApply,
                };
                struct Transducer* processSteps[] = {
                        filteringTransducer(positiveFloatsOnly, &heapAllocator),
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

                float resultFloat = -1.0000;
                if (result.type_tag == TTAG_FLOAT) {
                        resultFloat = *((float*) result.address);
                }
                printf("result is: %f ; expected: 10.0\n", resultFloat);

                printf("transduce it again with transduceFloatArray\n");
                {
                        struct Value result = transduceFloatArray(values, sizeof values / sizeof values[0], process, &heapAllocator);
                        assert(result.type_tag == TTAG_FLOAT);
                        float resultFloat = *((float*) result.address);
                        printf("result is: %f ; expected: 10.0\n", resultFloat);
                }
        }

        return 0;
}
