#ifndef __BUTTON_H
#define __BUTTON_H

#include <lcom/lcf.h>
#include "graphics.h"

typedef int (*button_action)();

typedef enum button_state {
    BUTTON_NORMAL,
    BUTTON_HOVERING,
    BUTTON_PRESSING,
    BUTTON_PRESSING_NOT_HOVERING
} button_state;

typedef struct button_t {
    uint16_t x, y, width, height;
    button_state state;
    button_action action;
} button_t;

int new_button(button_t *button, uint16_t x, uint16_t y, uint16_t width, uint16_t height, button_action action);
bool button_is_hovering(button_t button, uint16_t x, uint16_t y);
int button_draw(frame_buffer_t buf, button_t button);
int button_update_state(button_t *button, bool hovering, bool lb, bool rb);

// Triangle buttons will probably not be used

// typedef struct triangle_button {
//     uint16_t x, y, width, height;
//     button_state state;
//     xpm_image_t icon;
// } triangle_button;

// bool is_hovering_tb(triangle_button tb, uint16_t mouse_x, uint16_t mouse_y);
// int tb_draw(triangle_button tb);

#endif /* __BUTTON_H */
