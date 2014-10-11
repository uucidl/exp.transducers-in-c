#pragma once

struct Allocator;
struct Reducer;
struct Transducer;

#include "values.h"

#include <stdbool.h>

struct Value reducer_zero(struct Reducer const *reducer,
                          struct Allocator *allocator);

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

struct Transducer *
fnMappingTransducer(struct Value (*mapper)(struct Value value, void *data),
                    void *mapperData, struct Allocator *allocator);

struct Transducer *composingTransducer(struct Transducer **transducers,
                                       size_t transducerCount,
                                       struct Allocator *allocator);
