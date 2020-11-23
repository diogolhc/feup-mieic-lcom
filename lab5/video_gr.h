#ifndef __VIDEO_GR_H
#define __VIDEO_GR_H

#include <lcom/lcf.h>

int (vg_draw_pattern)(uint8_t no_rectangles, uint32_t first, uint8_t step);
int (vg_draw_sprite)(xpm_image_t img, uint16_t x, uint16_t y);

#endif /* __VIDEO_GR_H */