#ifndef __DISPATCHER_H
#define __DISPATCHER_H

#include <lcom/lcf.h>
#include "keyboard.h"
#include "textbox.h"

int dispatcher_bind_buttons(size_t number_of_buttons, ...);
int dispatcher_bind_text_box(text_box_t *text_box); // TODO se chegarmos a ter mais, fazer semelhante à dos botões
int dispatch_mouse_packet(struct packet p);
int dispatch_keyboard_event(kbd_event_t s);
int dispatch_timer_tick();
int draw_frame();
int event_new_stroke(bool primary_button);
int event_new_atom(uint16_t x, uint16_t y);
int event_new_game();
int event_end_round();
int event_undo();
int event_redo();
bool should_end();

#endif /* __DISPATCHER_H */
