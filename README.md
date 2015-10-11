Exploration: tranducers in C
============================

Implementing tranducers in C.

Transducers have been introduced by Rich Hickey in his Strange Loop talk in 2014.

see https://www.youtube.com/watch?v=6mTbuzafcII

[Read here about the implementation](./src/README.md)

Status
------

Abandoned. I came out convinced that I would use transducers in Clojure however found them not a very good fit for a non-dynamic language.

see the [ChangeLog](./ChangeLog) file for changes.

Licensing
---------

MIT. Look for the [LICENSE](./LICENSE) file.

Using
-----

[test.sh](./test.sh)

Contributing
------------

Requirement: clang-format

use [test.sh](./test.sh) to run the unit tests.

use [pre-commit.sh](./pre-commit.sh) to ensure the code is well formatted (uses clang-format)
