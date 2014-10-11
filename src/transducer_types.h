struct Allocator;

// reducer closure
struct Reducer
{
        struct Value (*zero)(struct Reducer const *reducer,
                             struct Allocator *allocator);

        struct Value (*apply)(struct Reducer const *reducer, struct Value input,
                              struct Value current,
                              struct Allocator *allocator);
};

/* transducers */

struct Transducer
{
        struct Reducer *(*apply)(struct Transducer *transducer,
                                 struct Reducer const *step,
                                 struct Allocator *allocator);
};
