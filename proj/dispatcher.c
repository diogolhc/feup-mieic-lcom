#include <lcom/lcf.h>
#include <stdarg.h>
#include "dispatcher.h"
#include "kbc.h"
#include "keyboard.h"
#include "mouse.h"
#include "video_gr.h"
#include "rtc.h"
#include "canvas.h"
#include "cursor.h"
#include "font.h"
#include "game.h"
#include "text_box.h"
#include "button.h"
#include "menu.h"
#include "protocol.h"
#include "queue.h"

#define EVENTS_TO_HANDLE_CAPACITY 16 // starting capacity of events_to_handle queue

static bool end = false;

static queue_t *events_to_handle;
static bool bound_canvas = false;
static size_t num_listening_buttons = 0;
static button_t **listening_buttons = NULL;
static size_t num_listening_text_boxes = 0;
static text_box_t **listening_text_boxes = NULL;

typedef enum player_state {
    NOT_READY,
    READY,
    RANDOM_NUMBER_SENT
} player_state_t;

static player_state_t this_player_state = NOT_READY;
static int this_player_random_number = 0;
static player_state_t other_player_state = NOT_READY;
static int other_player_random_number = 0;

int dispatcher_init() {
    events_to_handle = new_queue(sizeof(event_t), EVENTS_TO_HANDLE_CAPACITY);
    if (events_to_handle == NULL)
        return 1;
    
    return 0;
}

void dispatcher_exit() {
    dispatcher_reset_bindings();
    delete_queue(events_to_handle);
}

int dispatcher_bind_buttons(size_t number_of_buttons, ...) {
    if (listening_buttons != NULL)
        free(listening_buttons);

    num_listening_buttons = number_of_buttons;
    if (number_of_buttons == 0) {
        listening_buttons = NULL;
    } else {
        listening_buttons = malloc(number_of_buttons * sizeof(button_t*));
        if (listening_buttons == NULL)
            return 1;
    }
    
    va_list ap;
    va_start(ap, number_of_buttons);
    for (size_t i = 0; i < number_of_buttons; i++) {
        listening_buttons[i] = va_arg(ap, button_t*);
    }
    va_end(ap);

    return 0;
}

int dispatcher_bind_text_boxes(size_t number_of_text_boxes, ...) {
    if (listening_text_boxes != NULL)
        free(listening_text_boxes);

    num_listening_text_boxes = number_of_text_boxes;
    if (number_of_text_boxes == 0) {
        listening_text_boxes = NULL;
    } else {
        listening_text_boxes = malloc(number_of_text_boxes * sizeof(text_box_t*));
        if (listening_text_boxes == NULL)
            return 1;
    }
    
    va_list ap;
    va_start(ap, number_of_text_boxes);
    for (size_t i = 0; i < number_of_text_boxes; i++) {
        listening_text_boxes[i] = va_arg(ap, text_box_t*);
    }
    va_end(ap);

    return 0;
}

int dispatcher_bind_canvas(bool is_to_bind) {
    bound_canvas = is_to_bind;
    if (!is_to_bind) {
        if (canvas_update_state(false, false, false) != OK)
            return 1;
    }

    return 0;
}

int dispatcher_reset_bindings() {
    if (dispatcher_bind_buttons(0) != OK)
        return 1;
    if (dispatcher_bind_text_boxes(0) != OK)
        return 1;
    if (dispatcher_bind_canvas(false) != OK)
        return 1;
    return 0;
}

int dispatcher_queue_event(event_t event) {
    if (queue_push(events_to_handle, &event) != OK)
        return 1;
    
    return 0;
}

static void dispatch_mouse_packet() {
    struct packet p;
    if (mouse_retrieve_packet(&p) != OK) {
        printf("Failed to retrieve mouse packet\n");
        return;
    }

    if (p.x_ov || p.y_ov) {
        printf("Mouse displacement overflow\n");
        return;
    }

    cursor_move(p.delta_x, p.delta_y);
    cursor_update_buttons(p.lb, p.rb);
    
    if (handle_update_cursor_state() != OK) {
        printf("Failed to update cursor state\n");
    }
}

int handle_update_cursor_state() {
    int16_t x = cursor_get_x();
    int16_t y = cursor_get_y();
    bool lb = cursor_get_lb();
    bool rb = cursor_get_rb();

    bool hovering = false;
    cursor_set_state(CURSOR_ARROW);

    for (size_t i = 0; i < num_listening_buttons; i++) {
        button_t *button = listening_buttons[i];
        if (!hovering && button_is_hovering(button, x, y)) {
            hovering = true;
            if (button_update_state(button, true, lb, rb) != OK)
                return 1;
        } else {
            if (button_update_state(button, false, lb, rb) != OK)
                return 1;
        }
    }
    
    for (size_t i = 0; i < num_listening_text_boxes; i++) {
        text_box_t *text_box = listening_text_boxes[i];
        if (!hovering && text_box_is_hovering(text_box, x, y)) {
            hovering = true;
            if (text_box_update_state(text_box, true, lb, rb, x, y) != OK)
                return 1;
        } else {
            if (text_box_update_state(text_box, false, lb, rb, x, y) != OK)
                return 1;
        }
        if (text_box_is_reacting_to_cursor_hovering(text_box)) {
            cursor_set_state(CURSOR_WRITE);
        }
    }

    if (bound_canvas) {
        if (!hovering && canvas_is_hovering(x, y)) {
            hovering = true;
            if (canvas_update_state(true, lb, rb) != OK)
                return 1;
        } else {
            if (canvas_update_state(false, lb, rb) != OK)
                return 1;
        }
        
        if (canvas_get_state() != CANVAS_STATE_NORMAL) {
            if (canvas_is_enabled()) {
                cursor_set_state(CURSOR_PAINT);
            } else {
                cursor_set_state(CURSOR_DISABLED);
            }
        } 
    }

    return 0;
}

static void dispatch_keyboard_event() {
    kbd_event_t kbd_event;
    if (kbd_handle_scancode(&kbd_event) != OK) {
        printf("Failed to handle keyboard scancode\n");
        return;
    }

    if (menu_react_kbd(kbd_event) != OK) {
        printf("Failed to handle keyboard event in menu\n");
    }
    
    for (size_t i = 0; i < num_listening_text_boxes; i++) {
        text_box_t *text_box = listening_text_boxes[i];
        if (text_box_react_kbd(text_box, kbd_event) != OK) {
            printf("Failed to handle keyboard event in text box\n");
        }
    }

    if (bound_canvas) {
        if (canvas_react_kbd(kbd_event) != OK) {
            printf("Failed to handle keyboard event in canvas\n");
        }
    }
}

static void dispatch_rtc_periodic_int() {
    if (menu_get_state() == GAME || menu_get_state() == PAUSE_MENU) {
        if (game_rtc_pi_tick() != OK) {
            printf("Failed to handle rtc periodic interrupt in game\n");
        }
    }
}

static void dispatch_rtc_alarm_int() {
    if (menu_get_state() == GAME || menu_get_state() == PAUSE_MENU) {
        if (game_rtc_alarm() != OK) {
            printf("Failed to handle alarm interrupt in game\n");
        }
    } else if (menu_get_state() == DRAWER_NEW_ROUND_SCREEN) {
        if (rtc_disable_int(ALARM_INTERRUPT) != OK) {
            printf("Failed to dispatch new round alarm\n");
        }
        if (protocol_send_start_round() != OK) {
            printf("Failed to dispatch new round alarm\n");
        }
        if (handle_start_round() != OK) {
            printf("Failed to dispatch new round alarm\n");
        }
    }
}

static void dispatch_uart_received_data() {
    if (protocol_handle_received_bytes() != OK) {
        printf("Error handling received uart bytes\n");
    }
}

static void dispatch_uart_error() {
    if (protocol_handle_error() != OK) {
        printf("Failed to handle uart error\n");
    }
}

static void dispatch_timer_tick() {
    if (protocol_tick() != OK) {
        printf("Failed to handle protocol tick\n");
    }

    if (menu_get_state() == GAME || menu_get_state() == PAUSE_MENU) {
        if (game_timer_tick() != OK) {
            printf("Error while updating game ticks\n");
        }
    }

    if (draw_frame() != OK) {
        printf("Error while drawing frame\n");
    }
}

typedef void (*event_dispatcher_t)();
#define NUMBER_OF_DISPATCHERS 7
static const event_dispatcher_t dispatchers[7] = {
    dispatch_mouse_packet,
    dispatch_keyboard_event,
    dispatch_rtc_periodic_int,
    dispatch_rtc_alarm_int,
    dispatch_uart_received_data,
    dispatch_uart_error,
    dispatch_timer_tick,
};

void dispatcher_dispatch_events() {
    while (!queue_is_empty(events_to_handle)) {
        event_t event;
        if (queue_top(events_to_handle, &event) != OK) {
            printf("Failed to retrieve queued event\n");
            continue;
        }
        if (queue_pop(events_to_handle) != OK) {
            printf("Failed to retrieve queued event\n");
            continue;
        }
        
        dispatchers[event]();
    }
}

int draw_frame() {
    menu_state_t state = menu_get_state();

    if (state == GAME || state == PAUSE_MENU) {
        if (game_draw() != OK)
            return 1;
    }

    if (state != GAME) {
        if (menu_draw() != OK)
            return 1;
    }

    if (date_draw_current() != OK)
        return 1;
    if (cursor_draw() != OK)
        return 1;
    if (vg_flip_page() != OK)
        return 1;

    return 0;
}

int handle_other_player_opened_program() {
    if (this_player_state == READY) {
        if (protocol_send_ready_to_play() != OK)
            return 1;
    }

    return 0;
}

int handle_notify_not_in_game() {
    if (protocol_send_leave_game() != OK)
        return 1;
    if (this_player_state == RANDOM_NUMBER_SENT) {
        this_player_state = READY;
    }
    if (this_player_state == READY) {
        if (protocol_send_ready_to_play() != OK)
            return 1;
    }
    return 0;
}

int handle_leave_game() {
    if (rtc_disable_int(ALARM_INTERRUPT) != OK)
        return 1;
    delete_game();
    canvas_exit();
    if (protocol_send_leave_game() != OK)
        return 1;
    if (menu_set_main_menu() != OK)
        return 1;
    this_player_state = NOT_READY;
    if (other_player_state == RANDOM_NUMBER_SENT) {
        other_player_state = READY;
    }

    return 0;
}

int handle_other_player_leave_game() {
    other_player_state = NOT_READY;
    if (menu_is_game_ongoing() && !game_is_over()) {
        if (rtc_disable_int(ALARM_INTERRUPT) != OK)
            return 1;
        if (handle_end_round() != OK)
            return 1;
        game_set_over();
        if (menu_set_other_player_left_screen() != OK)
            return 1;
    }

    if (this_player_state == RANDOM_NUMBER_SENT) {
        this_player_state = READY;
    }

    return 0;
}

int handle_ready_to_play() {
    if (menu_set_awaiting_player_menu() != OK)
        return 1;
    if (protocol_send_ready_to_play() != OK)
        return 1;
    this_player_state = READY;
    if (other_player_state == READY) {
        if (handle_this_player_random_number() != OK)
            return 1;
    }

    return 0;
}

int handle_other_player_ready_to_play() {
    other_player_state = READY;
    if (this_player_state == READY || this_player_state == RANDOM_NUMBER_SENT) {
        if (handle_this_player_random_number() != OK)
            return 1;
    }

    return 0;
}

static int compare_random_numbers() {
    if (this_player_random_number > other_player_random_number) {
        // Player is about to begin game. Should not be ready to start a new one while this one is ongoing
        this_player_state = NOT_READY;
        other_player_state = NOT_READY;
        if (new_game() != OK)
            return 1;
        if (handle_new_round_as_drawer() != OK)
            return 1;

    } else if (this_player_random_number < other_player_random_number) {
        // Player is about to begin game. Should not be ready to start a new one while this one is ongoing
        this_player_state = NOT_READY;
        other_player_state = NOT_READY;
        if (new_game() != OK)
            return 1;

    } else {
        this_player_state = READY;
        other_player_state = READY;
        if (handle_this_player_random_number() != OK)
            return 1;
    }

    return 0;
}

int handle_this_player_random_number() {
    if (other_player_state == NOT_READY) {
        return 0;
    }

    this_player_state = RANDOM_NUMBER_SENT;
    this_player_random_number = rand();
    if (protocol_send_random_number(this_player_random_number) != OK)
        return 1;

    if (other_player_state == RANDOM_NUMBER_SENT) {
        if (compare_random_numbers() != OK)
            return 1;
    }

    return 0;
}



int handle_other_player_random_number(int random_number) {
    if (this_player_state == NOT_READY) {
        return 0;
    }

    other_player_state = RANDOM_NUMBER_SENT;
    other_player_random_number = random_number;

    if (this_player_state == RANDOM_NUMBER_SENT) {
        if (compare_random_numbers() != OK)
            return 1;
    }

    return 0;
}

int handle_new_round_as_guesser(const char *word) {
    if (game_new_round(GUESSER, word) != OK)
        return 1;

    if (menu_set_new_round_screen(GUESSER) != OK)
        return 1;

    return 0;
}

int handle_new_round_as_drawer() {
    const char *word = get_random_word();

    if (game_new_round(DRAWER, word) != OK)
        return 1;

    if (protocol_send_new_round(word) != OK)
        return 1;

    if (menu_set_new_round_screen(DRAWER) != OK)
        return 1;

    static const rtc_alarm_time_t time_to_start_round = {.hours = 0, .minutes = 0, .seconds = 3};
    if (rtc_set_alarm_in(time_to_start_round) != OK)
        return 1;

    return 0;
}

int handle_start_round() {
    bool canvas_enabled = game_get_role() == DRAWER;
    if (canvas_init(vg_get_hres(), vg_get_vres() - GAME_BAR_HEIGHT, canvas_enabled) != OK)
        return 1;

    if (game_start_round() != OK)
        return 1;
    
    return 0;
}

int handle_end_round() {
    canvas_exit();
    game_delete_round();
    return 0;
}

int handle_new_stroke() {
    if (canvas_new_stroke(drawer_get_selected_color(), drawer_get_selected_thickness()) != OK)
        return 1;
    if (protocol_send_new_stroke(drawer_get_selected_color(), drawer_get_selected_thickness()) != OK)
        return 1;
    
    return 0;
}

int handle_new_atom(uint16_t x, uint16_t y) {
    if (canvas_new_stroke_atom(x, y) != OK)
        return 1;
    if (protocol_send_new_atom(x, y) != OK)
        return 1;

    return 0;
}

int handle_undo() {
    if (canvas_undo_stroke() != OK)
        return 1;
    if (protocol_send_undo_canvas() != OK)
        return 1;

    return 0;
}

int handle_redo() {
    if (canvas_redo_stroke() != OK)
        return 1;
    if (protocol_send_redo_canvas() != OK)
        return 1;

    return 0;
}

int handle_guess_word(char *guess) {
    if (guess != NULL && strncmp(guess, "", 1) && game_is_round_ongoing()) {
        if (game_guess_word(guess) != OK) {
            free(guess);
            return 1;
        }
            
        if (protocol_send_guess(guess) != OK) {
            free(guess);
            return 1;
        }       
    } else {
        free(guess);
    }

    return 0;
}

int handle_round_win(uint32_t score) {
    if (game_round_over(score, true) != OK)
        return 1;
    if (protocol_send_round_win(score) != OK)
        return 1;
    return 0;
}

int trigger_end_program() {
    end = true;
    return 0;
}

bool should_end() {
    return end;
}
