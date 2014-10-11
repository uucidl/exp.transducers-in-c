#include "transducer_types.h"
#include "transducers.h"

#include "allocator.h"

struct Value reducer_zero(struct Reducer const* reducer, struct Allocator* allocator)
{
        return reducer->zero(reducer, allocator);
}

struct Value reducer_apply(struct Reducer const* reducer, struct Value input, struct Value current, struct Allocator* allocator) {
        return reducer->apply(reducer, input, current, allocator);
}

static
struct Value idReducerZero(struct Reducer const* reducer, struct Allocator* allocator) {
        return nullValue();
}

static
struct Value idReducerApply(struct Reducer const* reducer, struct Value input, struct Value current, struct Allocator* allocator) {
        return input;
}

struct Reducer* idReducer(struct Allocator* allocator) {
        struct Reducer* result = allocator_alloc(allocator, sizeof *result);

        *result = (struct Reducer) {
                idReducerZero,
                idReducerApply,
        };

        return result;
}

/* return new reducer from input reducer */
struct Reducer* transducer_apply(struct Transducer* transducer, struct Reducer const *step, struct Allocator *allocator) {
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
struct Value filteringReducerZero(struct Reducer const* reducer, struct Allocator* allocator)
{
        return nullValue();
}

static
struct Value filteringReducerApply(struct Reducer const* reducer, struct Value const input, struct Value const current, struct Allocator* allocator) {
        struct FilteringReducer* self = (struct FilteringReducer*) reducer;
        if (self->predicate(input)) {
                return reducer_apply(self->step, input, current, allocator);
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
        };

        return &result->super;
}

struct Transducer* filteringTransducer(bool (*predicate)(struct Value value), struct Allocator* allocator) {
        struct FilteringTransducer* transducer = allocator_alloc(allocator, sizeof *transducer);

        transducer->predicate = predicate;
        transducer->super = (struct Transducer) {
                .apply = filteringTransducerApply,
        };

        return &transducer->super;
}

struct MappingTransducer
{
        struct Transducer super;
        struct Reducer* reducer;
};

struct MappingReducer
{
        struct Reducer super;
        struct Reducer const *reducer;
        struct Value reducerResult;
        struct Reducer const* step;
};

static
struct Value mappingReducerZero(struct Reducer const* reducer, struct Allocator* allocator)
{
        struct MappingReducer* self = (struct MappingReducer*) reducer;
        return reducer_zero(self->step, allocator);
}

static
struct Value mappingReducerApply(struct Reducer const* reducer, struct Value input, struct Value current, struct Allocator* allocator) {
        struct MappingReducer* self = (struct MappingReducer*) reducer;

        self->reducerResult = reducer_apply(self->reducer, input, self->reducerResult, allocator);

        return reducer_apply(self->step, self->reducerResult, current, allocator);
}

static
struct Reducer* newMappingReducer(struct Reducer const* reducer, struct Reducer const* step, struct Allocator* allocator) {
        struct MappingReducer* result = allocator_alloc(allocator, sizeof *result);

        *result = (struct MappingReducer) {
                (struct Reducer) {
                        mappingReducerZero,
                        mappingReducerApply,
                },
                reducer,
                reducer_zero(reducer, allocator),
                step,
        };

        return &result->super;
}

static
struct Reducer* mappingTransducerApply(struct Transducer* transducer, struct Reducer const* step, struct Allocator* allocator) {
        struct MappingTransducer* self = (struct MappingTransducer*) transducer;

        return newMappingReducer(self->reducer, step, allocator);
}

struct Transducer* mappingTransducer(struct Reducer* reducer, struct Allocator* allocator)
{
        struct MappingTransducer* result = allocator_alloc(allocator, sizeof *result);

        result->super = (struct Transducer) {
                mappingTransducerApply,
        };

        result->reducer = reducer;

        return &result->super;
}

struct ComposingTransducer
{
        struct Transducer super;
        struct Transducer** transducers;
        size_t transducersCount;
};

static struct Reducer* composingTransducerApply(struct Transducer* transducer, struct Reducer const* step, struct Allocator* allocator) {
        struct ComposingTransducer* self = (struct ComposingTransducer*) transducer;
        struct Reducer * x = (struct Reducer*) step;
        for (size_t i = 0; i < self->transducersCount; i++) {
                size_t idx = self->transducersCount - 1 - i;
                x = transducer_apply(self->transducers[idx], x, allocator);
        }

        return x;
}

struct Transducer* composingTransducer(struct Transducer** transducers, size_t transducerCount, struct Allocator* allocator) {
        struct ComposingTransducer* result = allocator_alloc(allocator, sizeof *result);

        result->super = (struct Transducer) {
                composingTransducerApply,
        };

        result->transducers = transducers;
        result->transducersCount = transducerCount;

        return &result->super;
}
