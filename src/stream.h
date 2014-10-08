#include <stdint.h>
#include <string.h>

struct StreamRange;

void stream_of_zeros(struct StreamRange *range);
void stream_on_memory(struct StreamRange *range, uint8_t const *mem,
                      size_t size);
