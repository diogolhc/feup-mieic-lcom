// IMPORTANT: you must include the following line in all your C files
#include <lcom/lcf.h>

#include <lcom/proj.h>

#include <stdint.h>
#include <stdio.h>

// Any header files included below this line should have been created by you
#include "keyboard.h"
#include "mouse.h"
#include "video_gr.h"
extern int interrupt_counter;

int main(int argc, char *argv[]) {
  // sets the language of LCF messages (can be either EN-US or PT-PT)
  lcf_set_language("EN-US");

  // enables to log function invocations that are being "wrapped" by LCF
  // [comment this out if you don't want/need it]
  lcf_trace_calls("/home/lcom/labs/proj/trace.txt");

  // enables to save the output of printf function calls on a file
  // [comment this out if you don't want/need it]
  lcf_log_output("/home/lcom/labs/proj/output.txt");

  // handles control over to LCF
  // [LCF handles command line arguments and invokes the right function]
  if (lcf_start(argc, argv))
    return 1;

  // LCF clean up tasks
  // [must be the last statement before return]
  lcf_cleanup();

  return 0;
}

int draw_frame() {
    // TODO
    if (vg_flip_page() != OK)
        return 1;
    return 0;
}

int (proj_main_loop)(int argc, char *argv[]) {
    uint8_t timer_irq_set, kbd_irq_set, mouse_irq_set;
    uint16_t mode = 0x118;



    if (vg_init(mode) == NULL) 
        return 1;

    if (timer_subscribe_int(&timer_irq_set) != OK) 
        return 1;

    if (kbd_subscribe_int(&kbd_irq_set) != OK) 
        return 1;

    if (mouse_enable_dr() != OK)
        return 1;

    if (mouse_subscribe_int(&mouse_irq_set) != OK) 
        return 1;

    int ipc_status, r;
    message msg;
    while ( interrupt_counter < 600 ) { /* You may want to use a different condition */
        /* Get a request message. */
        if ( (r = driver_receive(ANY, &msg, &ipc_status)) != 0) { 
            printf("driver_receive failed with: %d\n", r);
            continue;
        }
        if (is_ipc_notify(ipc_status)) { /* received notification */
            switch (_ENDPOINT_P(msg.m_source)) {
            case HARDWARE: /* hardware interrupt notification */				
                if (msg.m_notify.interrupts & BIT(mouse_irq_set)) {
                    mouse_ih();
                    if (mouse_ih_return != OK) {
                        printf("mouse interrupt handler failed\n");
                        continue;
                    }

                    if (mouse_packet_ready()) {
                        struct packet p;
                        if (mouse_retrieve_packet(&p) != OK) {
                            printf("mouse_retrieve_packet failed\n");
                            continue;
                        }
                        
                        // TODO
                        printf("mouse packets are not yet handled\n");
                        mouse_print_packet(&p);
                    }
                }
                if (msg.m_notify.interrupts & BIT(kbd_irq_set)) {
                    kbc_ih();
                    if (kbd_ih_return != OK) {
                        printf("keyboard interrupt handler failed\n");
                        continue;
                    }
                    
                    if (kbd_scancode_ready()) {
                        if (kbd_handle_scancode() != OK) {
                            printf("kbd_handle_scancode failed\n");
                            continue;
                        }

                        // TODO
                    } 
                }
                if (msg.m_notify.interrupts & BIT(timer_irq_set)) {
                    timer_int_handler();

                    if (draw_frame() != OK) {
                        printf("error while drawing frame\n");
                    }
                }
                break;
            default:
                break; /* no other notifications expected: do nothing */	
            }
        } else { /* received a standard message, not a notification */
            /* no standard messages expected: do nothing */
        }
    }

    if (kbd_unsubscribe_int() != OK)
        return 1;

    if (mouse_unsubscribe_int() != OK) 
        return 1;
    
    if (mouse_disable_dr() != OK) 
        return 1;

    if (timer_unsubscribe_int() != OK)
        return 1;

    if (vg_exit() != OK)
        return 1;

    return 0;
}