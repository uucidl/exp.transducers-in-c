#include "transducer_types.h"
#include "transducers.h"

#include "allocator.h"

struct Value reducer_identity(struct Reducer const *reducer,
                              struct Allocator *allocator)
{
        if (!reducer->identity) {
                return nullValue();
        }

        return reducer->identity(reducer, allocator);
}

struct Value reducer_complete(struct Reducer const *reducer,
                              struct Value result, struct Allocator *allocator)
{
        if (!reducer->complete) {
                return result;
        }

        return reducer->complete(reducer, result, allocator);
}

struct Value reducer_apply(struct Reducer const *reducer, struct Value input,
                           struct Value current, struct Allocator *allocator)
{
        return reducer->apply(reducer, input, current, allocator);
}

static struct Value idReducerApply(struct Reducer const *reducer,
                                   struct Value input, struct Value current,
                                   struct Allocator *allocator)
{
        return input;
}

struct Reducer *idReducer(struct Allocator *allocator)
{
        struct Reducer *result = allocator_alloc(allocator, sizeof *result);

        *result = (struct Reducer){
            .apply = idReducerApply,
        };

        return result;
}

/* return new reducer from input reducer */
struct Reducer *transducer_apply(struct Transducer *transducer,
                                 struct Reducer const *step,
                                 struct Allocator *allocator)
{
        return transducer->apply(transducer, step, allocator);
}

struct ChainedReducer
{
        struct Reducer super;
        struct Reducer const *step;
};

static struct Value chainedReducerIdentity(struct Reducer const *reducer,
                                           struct Allocator *allocator)
{
        struct ChainedReducer *self = (struct ChainedReducer *)reducer;
        return reducer_identity(self->step, allocator);
}

static struct Value chainedReducerComplete(struct Reducer const *reducer,
                                           struct Value result,
                                           struct Allocator *allocator)
{
        struct ChainedReducer *self = (struct ChainedReducer *)reducer;
        return reducer_complete(self->step, result, allocator);
}

static struct ChainedReducer chainedReducerMake(
    struct Reducer const *step,
    struct Value (*reducingFn)(struct Reducer const *, struct Value,
                               struct Value, struct Allocator *))
{
        struct ChainedReducer result = {.super = (struct Reducer){
                                            .identity = chainedReducerIdentity,
                                            .complete = chainedReducerComplete,
                                            .apply = reducingFn,
                                        },
                                        .step = step};

        return result;
}

struct FilteringTransducer
{
        struct Transducer super;
        bool (*predicate)(struct Value value, void *data);
        void *predicateData;
};

struct FilteringReducer
{
        struct ChainedReducer super;
        bool (*predicate)(struct Value value, void *data);
        void *predicateData;
};

static struct Value filteringReducerApply(struct Reducer const *reducer,
                                          struct Value const input,
                                          struct Value const current,
                                          struct Allocator *allocator)
{
        struct FilteringReducer *self = (struct FilteringReducer *)reducer;
        if (self->predicate(input, self->predicateData)) {
                return reducer_apply(self->super.step, input, current,
                                     allocator);
        }

        return current;
}

static struct Reducer *filteringTransducerApply(struct Transducer *transducer,
                                                struct Reducer const *step,
                                                struct Allocator *allocator)
{
        struct FilteringTransducer *self =
            (struct FilteringTransducer *)transducer;
        struct FilteringReducer *result =
            allocator_alloc(allocator, sizeof *result);

        result->predicate = self->predicate;
        result->predicateData = self->predicateData;
        result->super = chainedReducerMake(step, filteringReducerApply);

        return &result->super.super;
}

struct Transducer *
filteringTransducer(bool (*predicate)(struct Value value, void *data),
                    void *predicateData, struct Allocator *allocator)
{
        struct FilteringTransducer *transducer =
            allocator_alloc(allocator, sizeof *transducer);

        *transducer =
            (struct FilteringTransducer){.predicate = predicate,
                                         .predicateData = predicateData,
                                         .super = (struct Transducer){
                                             .apply = filteringTransducerApply,
                                         }};

        return &transducer->super;
}

struct MappingTransducer
{
        struct Transducer super;
        struct Reducer *reducer;
};

struct MappingReducer
{
        struct ChainedReducer super;
        struct Reducer const *reducer;
        struct Value reducerResult;
};

static struct Value mappingReducerComplete(struct Reducer const *reducer,
                                           struct Value result,
                                           struct Allocator *allocator)
{
        struct MappingReducer *self = (struct MappingReducer *)reducer;
        return reducer_complete(
            self->super.step,
            reducer_complete(self->reducer, result, allocator), allocator);
}

static struct Value mappingReducerApply(struct Reducer const *reducer,
                                        struct Value input,
                                        struct Value current,
                                        struct Allocator *allocator)
{
        struct MappingReducer *self = (struct MappingReducer *)reducer;

        self->reducerResult =
            reducer_apply(self->reducer, input, self->reducerResult, allocator);

        return reducer_apply(self->super.step, self->reducerResult, current,
                             allocator);
}

static struct Reducer *newMappingReducer(struct Reducer const *reducer,
                                         struct Reducer const *step,
                                         struct Allocator *allocator)
{
        struct MappingReducer *result =
            allocator_alloc(allocator, sizeof *result);

        *result = (struct MappingReducer){
            .super = chainedReducerMake(step, mappingReducerApply),
            reducer,
            reducer_identity(reducer, allocator),
        };

        result->super.super.complete = mappingReducerComplete;

        return &result->super.super;
}

static struct Reducer *mappingTransducerApply(struct Transducer *transducer,
                                              struct Reducer const *step,
                                              struct Allocator *allocator)
{
        struct MappingTransducer *self = (struct MappingTransducer *)transducer;

        return newMappingReducer(self->reducer, step, allocator);
}

struct Transducer *mappingTransducer(struct Reducer *reducer,
                                     struct Allocator *allocator)
{
        struct MappingTransducer *result =
            allocator_alloc(allocator, sizeof *result);

        result->super = (struct Transducer){
            mappingTransducerApply,
        };

        result->reducer = reducer;

        return &result->super;
}

struct MappingFnInput
{
        struct Value (*mapperFn)(struct Value, void *data);
        void *mapperData;
};

struct MappingFnReducer
{
        struct ChainedReducer super;
        struct MappingFnInput input;
};

struct MappingFnTransducer
{
        struct Transducer super;
        struct MappingFnInput input;
};

static struct Value mappingFnReducerApply(struct Reducer const *reducer,
                                          struct Value input,
                                          struct Value current,
                                          struct Allocator *allocator)
{
        struct MappingFnReducer *self = (struct MappingFnReducer *)reducer;

        return reducer_apply(
            self->super.step,
            self->input.mapperFn(input, self->input.mapperData), current,
            allocator);
}

static struct Reducer *mappingFnTransducerApply(struct Transducer *transducer,
                                                struct Reducer const *step,
                                                struct Allocator *allocator)
{
        struct MappingFnTransducer *self =
            (struct MappingFnTransducer *)transducer;

        struct MappingFnReducer *result =
            allocator_alloc(allocator, sizeof *result);
        result->super = chainedReducerMake(step, mappingFnReducerApply);
        result->input = self->input;

        return &result->super.super;
}

struct Transducer *
mappingFnTransducer(struct Value (*mapperFn)(struct Value, void *data),
                    void *mapperData, struct Allocator *allocator)
{
        struct MappingFnTransducer *result =
            allocator_alloc(allocator, sizeof *result);

        result->super = (struct Transducer){
            mappingFnTransducerApply,
        };

        result->input = (struct MappingFnInput){
            .mapperFn = mapperFn, .mapperData = mapperData,
        };

        return &result->super;
}

struct ComposingTransducer
{
        struct Transducer super;
        struct Transducer **transducers;
        size_t transducersCount;
};

static struct Reducer *composingTransducerApply(struct Transducer *transducer,
                                                struct Reducer const *step,
                                                struct Allocator *allocator)
{
        struct ComposingTransducer *self =
            (struct ComposingTransducer *)transducer;
        struct Reducer *x = (struct Reducer *)step;
        for (size_t i = 0; i < self->transducersCount; i++) {
                size_t idx = self->transducersCount - 1 - i;
                x = transducer_apply(self->transducers[idx], x, allocator);
        }

        return x;
}

struct Transducer *composingTransducer(struct Transducer **transducers,
                                       size_t transducerCount,
                                       struct Allocator *allocator)
{
        struct ComposingTransducer *result =
            allocator_alloc(allocator, sizeof *result);

        result->super = (struct Transducer){
            composingTransducerApply,
        };

        result->transducers = transducers;
        result->transducersCount = transducerCount;

        return &result->super;
}
