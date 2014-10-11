#include "allocator.h"
#include "allocator_type.h"
#include "stream_types.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

enum TypeTags
{
        TTAG_NULL,
        TTAG_FLOAT,
};

struct Value
{
        int type_tag;
        size_t element_size;
        void const* address;
};

struct Reducer;

// reducer closure
struct Reducer
{
        struct Value (*zero)(struct Reducer const *reducer);
        struct Value (*apply)(struct Reducer const *reducer, struct Value input, struct Value current);
        struct Allocator* allocator;
};

static
struct Value reducer_zero(struct Reducer const* reducer)
{
        return reducer->zero(reducer);
}

static
struct Value reducer_apply(struct Reducer const* reducer, struct Value input, struct Value current) {
        return reducer->apply(reducer, input, current);
}

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
struct Value accumulateFloatZero(struct Reducer const* reducer)
{
        static float zero = 0.0f;
        return (struct Value) {
                .type_tag = TTAG_FLOAT,
                .element_size = sizeof zero,
                .address = &zero,
        };
}

static
struct Value accumulateFloatApply(struct Reducer const* reducer, struct Value const input, struct Value const current)
{
        return accumulateFloat(input, current, reducer->allocator);
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
        struct Value result = reducer_zero(reducer);
        while ((element = nextValueVSR(range), range->error == S_NoError)) {
                result = reducer_apply(reducer, element, result);
        }
        return result;
}

/* transducers */

struct Transducer
{
        struct Reducer* (*apply)(struct Transducer *transducer, struct Reducer const* step, struct Allocator* allocator);
};

/* return new reducer from input reducer */
static struct Reducer* transducer_apply(struct Transducer* transducer, struct Reducer const *step, struct Allocator *allocator) {
        return transducer->apply(transducer, step, allocator);
}

struct FilteringTransducer {
        struct Transducer super;
        bool (*predicate)(struct Value value);
};

struct FilteringReducer {
        struct Reducer super;
        struct Reducer const* step;
        bool (*predicate)(struct Value value);
};

static
struct Value filteringReducerZero(struct Reducer const* reducer)
{
        return (struct Value) {0, 0, 0};
}

static
struct Value filteringReducerApply(struct Reducer const* reducer, struct Value const input, struct Value const current) {
        struct FilteringReducer* self = (struct FilteringReducer*) reducer;
        if (self->predicate(input)) {
                return reducer_apply(self->step, input, current);
        }

        return current;
}

static struct Reducer* filteringTransducerApply(struct Transducer *transducer, struct Reducer const* step, struct Allocator* allocator) {
        struct FilteringTransducer* self = (struct FilteringTransducer*) transducer;
        struct FilteringReducer* result = allocator_alloc(allocator, sizeof *result);

        result->step = step;
        result->predicate = self->predicate;
        result->super = (struct Reducer) {
                .zero = filteringReducerZero,
                .apply = filteringReducerApply,
                .allocator = allocator
        };

        return &result->super;
}

static struct Transducer* filteringTransducer(bool (*predicate)(struct Value value), struct Allocator* allocator) {
        struct FilteringTransducer* transducer = allocator_alloc(allocator, sizeof *transducer);

        transducer->predicate = predicate;
        transducer->super = (struct Transducer) {
                .apply = filteringTransducerApply,
        };

        return &transducer->super;
}

struct ComposingTransducer
{
        struct Transducer super;
        struct Transducer** transducers;
        size_t transducersCount;
};

struct ComposingReducer
{
        struct Reducer super;
        struct Reducer** reducers;
        struct Value* reducersResult;
        size_t reducersCount;
};

static struct Value composingReducerZero(struct Reducer const* reducer) {
        return (struct Value) {0, 0, 0};
}

static struct Value composingReducerApply(struct Reducer const* reducer, struct Value input, struct Value current) {
        struct ComposingReducer* self = (struct ComposingReducer*) reducer;

        return reducer_apply(self->reducers[0],
                             input, current);
}

static struct Reducer* composingTransducerApply(struct Transducer* transducer, struct Reducer const* step, struct Allocator* allocator) {
        struct ComposingTransducer* self = (struct ComposingTransducer*) transducer;
        struct ComposingReducer* result = allocator_alloc(allocator, sizeof *result);

        result->reducersCount = self->transducersCount;
        result->reducers = allocator_alloc(allocator, result->reducersCount * sizeof result->reducers[0]);
        result->reducersResult = allocator_alloc(allocator, result->reducersCount * sizeof result->reducersResult[0]);

        struct Reducer const * x = step;
        for (size_t i = 0; i < self->transducersCount; i++) {
                size_t idx = self->transducersCount - 1 - i;
                result->reducers[idx] = transducer_apply(self->transducers[idx], x, allocator);
                result->reducersResult[idx] = reducer_zero(result->reducers[idx]);
                x = result->reducers[idx];
        }

        result->super = (struct Reducer) {
                composingReducerZero,
                composingReducerApply,
                allocator,
        };

        return &result->super;
}

static struct Transducer* composingTransducer(struct Transducer** transducers, size_t transducerCount, struct Allocator* allocator) {
        struct ComposingTransducer* result = allocator_alloc(allocator, sizeof *result);

        result->super = (struct Transducer) {
                composingTransducerApply,
        };

        result->transducers = transducers;
        result->transducersCount = transducerCount;

        return &result->super;
}

struct ReducingTransducer
{
        struct Transducer super;
        struct Reducer* reducer;
};

struct ReducingReducer
{
        struct Reducer super;
        struct Reducer *reducer;
        struct Value reducerResult;
        struct Reducer const* step;
};

static
struct Value reducingReducerZero(struct Reducer const* reducer)
{
        struct ReducingReducer* self = (struct ReducingReducer*) reducer;
        return reducer_zero(self->step);
}

static
struct Value reducingReducerApply(struct Reducer const* reducer, struct Value input, struct Value current) {
        struct ReducingReducer* self = (struct ReducingReducer*) reducer;

        self->reducerResult = reducer_apply(self->reducer, input, self->reducerResult);

        return reducer_apply(self->step, self->reducerResult, current);
}

static
struct Reducer* reducingTransducerApply(struct Transducer* transducer, struct Reducer const* step, struct Allocator* allocator) {
        struct ReducingTransducer* self = (struct ReducingTransducer*) transducer;
        struct ReducingReducer* result = allocator_alloc(allocator, sizeof *result);

        result->reducer = self->reducer;
        result->reducerResult = reducer_zero(result->reducer);
        result->step = step;
        result->super = (struct Reducer) {
                reducingReducerZero,
                reducingReducerApply,
                allocator,
        };

        return &result->super;
}

static
struct Transducer* reducingTransducer(struct Reducer* reducer, struct Allocator* allocator)
{
        struct ReducingTransducer* result = allocator_alloc(allocator, sizeof *result);

        result->super = (struct Transducer) {
                reducingTransducerApply,
        };

        result->reducer = reducer;

        return &result->super;
}

static
struct Value printReducerZero(struct Reducer const* reducer)
{
        return (struct Value) {0, 0, 0};
}

static struct Value printReducerApply(struct Reducer const* reducer, struct Value input, struct Value current) {
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
                .allocator = allocator,
        };

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
                struct Value result = accumulateFloat(floatValue(1.0f, &heapAllocator), floatValue(3.0f, &heapAllocator), &heapAllocator);

                printf("result is: %f; expected: 4.0\n", *((float*)result.address));
        }

        printf("2. process array as stream\n");
        {
                float values[] = { 1.0, 2.0, 3.0, 4.0 };
                struct ValueStreamRange valuesRange;
                struct Reducer accumulator = {
                        .zero = accumulateFloatZero,
                        .apply = accumulateFloatApply,
                        .allocator = &heapAllocator,
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
                        .allocator = &heapAllocator,
                };
                struct Transducer* processSteps[] = {
                        filteringTransducer(positiveFloatsOnly, &heapAllocator),
                        reducingTransducer(&accumulator, &heapAllocator),
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

        }

        return 0;
}
