#ifndef HL1_X360_SHADED_H
#define HL1_X360_SHADED_H

#include <stdint.h>

/* Render an offline, solid isometric BMP from an ALREADY VALIDATED
   H360 v1 buffer. Does not use Xbox 360 GPU or Xenia display APIs.
   Return nonzero when a BMP is written to an available cache mount. */
int hl1_shaded_h360(const uint8_t *buffer,
                    uint32_t groups, uint32_t vertices,
                    uint32_t goff, uint32_t voff, uint32_t ioff);

#endif
