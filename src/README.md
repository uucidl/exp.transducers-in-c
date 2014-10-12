# transducers in C

Some remarks about limits of the approach:

Making closures is not C's forte, and a new type must be created for
each new function signature. It all boils down to pairing a function
pointer and a data pointer.

Since functions/closures are not first class, inlining closures is not
possible in the general case. Some compilers manage to do so in cases
where the function pointer is clearly leading to an implementation,
however we're convinced this would not fly in any random transducer
combination.

The transducer approach also relies on a type signature with
polymorphic types at either end: `input -fn-> output`

This, again is not quite sympathetic to the C model.

Closures also need some bookeeping and memory allocation. This is easy
to track using a garbage collector. Without, we have to deal with some
notion of process boundaries/ownership. This implementation does not
yet deal with this.
