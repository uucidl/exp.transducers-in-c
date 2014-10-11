struct Allocator;
struct Reducer;
struct Transducer;

#include "values.h"

#include <stdbool.h>

extern struct Value reducer_zero(struct Reducer const *reducer,
                                 struct Allocator *allocator);

extern struct Value reducer_apply(struct Reducer const *reducer,
                                  struct Value input, struct Value current,
                                  struct Allocator *allocator);

extern struct Reducer *idReducer(struct Allocator *allocator);

extern struct Reducer *transducer_apply(struct Transducer *transducer,
                                        struct Reducer const *step,
                                        struct Allocator *allocator);

extern struct Transducer *
filteringTransducer(bool (*predicate)(struct Value value),
                    struct Allocator *allocator);

extern struct Transducer *mappingTransducer(struct Reducer *reducer,
                                            struct Allocator *allocator);

extern struct Transducer *composingTransducer(struct Transducer **transducers,
                                              size_t transducerCount,
                                              struct Allocator *allocator);
