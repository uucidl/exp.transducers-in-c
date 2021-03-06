#pragma once

struct Allocator;
struct Reducer;
struct Transducer;

#include "values.h"

#include <stdbool.h>

/// establishes the initial reducing state
struct Value reducer_identity(struct Reducer const *reducer,
                              struct Allocator *allocator);

/// produce a final value and/or flush state.
struct Value reducer_complete(struct Reducer const *reducer,
                              struct Value result, struct Allocator *allocator);

/// reduction function
struct Value reducer_apply(struct Reducer const *reducer, struct Value input,
                           struct Value current, struct Allocator *allocator);

struct Reducer *idReducer(struct Allocator *allocator);

struct Reducer *transducer_apply(struct Transducer *transducer,
                                 struct Reducer const *step,
                                 struct Allocator *allocator);

struct Transducer *
filteringTransducer(bool (*predicate)(struct Value value, void *data),
                    void *predicateData, struct Allocator *allocator);

struct Transducer *mappingTransducer(struct Reducer *reducer,
                                     struct Allocator *allocator);

struct Transducer *composingTransducer(struct Transducer **transducers,
                                       size_t transducerCount,
                                       struct Allocator *allocator);

struct Transducer *
mappingFnTransducer(struct Value (*mapperFn)(struct Value, void *data),
                    void *mapperData, struct Allocator *allocator);
