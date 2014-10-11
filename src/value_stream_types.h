#pragma once

#include "stream_types.h"

#include <stddef.h>

struct ValueStreamRange
{
        int type_tag;
        size_t element_size;
        uint8_t const *start;
        uint8_t const *end;
        uint8_t const *cursor;
        enum StreamErrorCode error;
        enum StreamErrorCode (*next)(struct ValueStreamRange *);
};
