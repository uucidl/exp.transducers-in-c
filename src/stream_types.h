/**
 * @file
 * Buffered stream I/O
 */

#include <stdint.h>

/**
 * state of a stream
 */
enum StreamErrorCode {
        /// no error has occured
        S_NoError,
        /// the consumer attempted to read past the end
        S_ReadPastEnd,
};

/**
 * Buffer-centric I/O.
 *
 * A stream shared between a producer and consumer is represented by this
 * structure.
 *
 * When the consumer wants a next range of data, it calls next() on
 * the struct to obtain the next available one.
 *
 * @see http://fgiesen.wordpress.com/2011/11/21/buffer-centric-io/ for original
 *idea
 */
struct StreamRange
{
        /**
         * start of buffer.
         */
        uint8_t const *start;

        /**
         * one byte past the end of buffer.
         *
         * so that:
         * - size = end - start
         * - end >= start
         */
        uint8_t const *end;

        /**
         * interesting point in the buffer
         *
         * start <= cursor <= end
         */
        uint8_t const *cursor;

        /**
         * initialized to BSEC_NoError
         */
        enum StreamErrorCode error;

        /**
         * Refill function, called when more data is needed by the consumer.
         *
         * - pre-condition: cursor == end
         * - post-condition: start == cursor < end
         */
        enum StreamErrorCode (*next)(struct StreamRange *);
};
