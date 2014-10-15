#include "stream_types.h"
#include "stream.h"

static enum StreamErrorCode next_zeros(struct StreamRange *range)
{
        static uint8_t const zeros[256] = {0};

        range->start = zeros;
        range->cursor = zeros;
        range->end = zeros + sizeof(zeros);

        return range->error;
}

void stream_of_zeros(struct StreamRange *range)
{
        range->next = next_zeros;
        range->error = S_NoError;

        range->next(range);
}

static enum StreamErrorCode fail(struct StreamRange *range,
                                 enum StreamErrorCode error)
{
        range->error = error;
        range->next = next_zeros;

        return range->next(range);
}

static enum StreamErrorCode next_on_memory_buffer(struct StreamRange *range)
{
        return fail(range, S_ReadPastEnd);
}

void stream_on_memory(struct StreamRange *range, uint8_t const *mem,
                      size_t const size)
{
        range->start = mem;
        range->cursor = mem;
        range->end = range->start + size;
        range->error = S_NoError;
        range->next = next_on_memory_buffer;
}
