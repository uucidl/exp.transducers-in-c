#include "stream_types.h"
#include "value_stream_types.h"
#include "values.h"

static enum StreamErrorCode zerosVSRNext(struct ValueStreamRange* range)
{
        static uint8_t const zeros[256] = {0};

        range->type_tag = 0;
        range->element_size = 1;
        range->start = zeros;
        range->cursor = zeros;
        range->end = zeros + sizeof(zeros);

        return range->error;
}

static enum StreamErrorCode failVSR(struct ValueStreamRange* range, enum StreamErrorCode error)
{
        range->error = error;
        range->next = zerosVSRNext;
        return range->next(range);
}

static enum StreamErrorCode floatArrayNext(struct ValueStreamRange* range)
{
        return failVSR(range, S_ReadPastEnd);
}

void floatArrayVSR(struct ValueStreamRange* range, float const* values, size_t count)
{
        range->type_tag = TTAG_FLOAT;
        range->element_size = sizeof(float);
        range->start = (uint8_t*) values;
        range->cursor = (uint8_t*) values;
        range->end = (uint8_t*) (values + count);
        range->error = S_NoError;
        range->next = floatArrayNext;
}
