#include <lcom/lcf.h>

#include "rtc.h"
#include "dispatcher.h"
#include "date.h"

/** @defgroup rtc rtc 
 * @{
 *
 */

static int hook_id_rtc = 3; /**< @brief RTC interrupts hook id */
static date_t current_date; /**< @brief Current date */
static date_t last_alarm_set_to = {.year = 0, .month = 0, .day = 0, .hour = 0, .minute = 0, .second = 0}; /**< @brief Date the last alarm interrupt was set to */

/**
 * @brief Prints the given value in binary format.
 * 
 * @param val value to print
 * 
 */
static void rtc_print_byte_binary_format(uint8_t val) {
    for (int i = 7; i >= 0; i--) {
        printf("%d", (val>>i) & 0x1);
    }
    printf("\n");
}

int rtc_read_conf() {
    uint8_t reg_a, reg_b, reg_c, reg_d;
    if (rtc_read_register(RTC_REGISTER_A, &reg_a) != OK)
        return 1;
    if (rtc_read_register(RTC_REGISTER_B, &reg_b) != OK)
        return 1;
    if (rtc_read_register(RTC_REGISTER_C, &reg_c) != OK)
        return 1;
    if (rtc_read_register(RTC_REGISTER_D, &reg_d) != OK)
        return 1;

    printf("REGISTER A: ");
    rtc_print_byte_binary_format(reg_a);
    printf("REGISTER B: ");
    rtc_print_byte_binary_format(reg_b);
    printf("REGISTER C: ");
    rtc_print_byte_binary_format(reg_c);
    printf("REGISTER D: ");
    rtc_print_byte_binary_format(reg_d);

    return 0;
}

/**
 * @brief Converts the given alarm time from binary into bcd.
 * 
 * @param time the time to convert
 * @return Return 0 upon success and non-zero otherwise
 */
static int alarm_time_binary_to_bcd(rtc_alarm_time_t * time) {
    if (time == NULL)
        return 1;
    
    time->hours = BINARY_TO_BCD(time->hours);
    time->minutes = BINARY_TO_BCD(time->minutes);
    time->seconds = BINARY_TO_BCD(time->seconds);
    return 0;
}

int rtc_get_current_date(date_t *date) {
    if (date == NULL)
        return 1;
    if (memcpy(date, &current_date, sizeof(date_t)) == NULL)
        return 1;
    return 0;
}

int rtc_read_date() {
    uint8_t reg_a;
    if (rtc_read_register(RTC_REGISTER_A, &reg_a) != OK)
        return 1;
    
    if (reg_a & RTC_UIP) { // slide 15
        tickdelay(micros_to_ticks(RTC_DELAY_U));
    }
    uint8_t year;

    if (rtc_read_register(RTC_REGISTER_SECONDS, &current_date.second) != OK)
        return 1;
    if (rtc_read_register(RTC_REGISTER_MINUTES, &current_date.minute) != OK)
        return 1;
    if (rtc_read_register(RTC_REGISTER_HOURS, &current_date.hour) != OK)
        return 1;
    if (rtc_read_register(RTC_REGISTER_DAY_OF_THE_MONTH, &current_date.day) != OK)
        return 1;
    if (rtc_read_register(RTC_REGISTER_MONTH, &current_date.month) != OK)
        return 1;
    if (rtc_read_register(RTC_REGISTER_YEAR, &year) != OK)
        return 1;

    current_date.year = year;
    if (date_bcd_to_binary(&current_date) != OK)
        return 1;
    
    current_date.year += 2000;
    return 0;
}

int rtc_subscribe_int(uint8_t *bit_no) {
    *bit_no = hook_id_rtc;
    return sys_irqsetpolicy(RTC_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE, &hook_id_rtc);
}

int rtc_unsubscribe_int() {
    return sys_irqrmpolicy(&hook_id_rtc);
}

int rtc_enable_update_int() {
    rtc_interrupt_config_t config; // no config is used when enabling update interrupts
    if (rtc_enable_int(UPDATE_INTERRUPT, config) != OK)
        return 1;

    return 0;
}

int rtc_set_alarm_in(rtc_alarm_time_t remaining_time_to_alarm) {
    last_alarm_set_to = current_date;
    if (date_plus_alarm_time(remaining_time_to_alarm, &last_alarm_set_to) != OK) 
        return 1;
    
    rtc_interrupt_config_t config = {.alarm_time = {.hours = last_alarm_set_to.hour, .minutes = last_alarm_set_to.minute, .seconds = last_alarm_set_to.second}};
    if (rtc_enable_int(ALARM_INTERRUPT, config) != OK)
        return 1;

    return 0;
}

int rtc_enable_int(rtc_interrupt_t rtc_interrupt, rtc_interrupt_config_t config) {
    uint8_t reg_b = 0;
    if (rtc_read_register(RTC_REGISTER_B, &reg_b) != OK) {
        return 1;
    }

    switch (rtc_interrupt) {
    case ALARM_INTERRUPT:
        reg_b |= RTC_AIE;

        if (alarm_time_binary_to_bcd(&config.alarm_time) != OK)
            return 1;
        if (rtc_write_register(RTC_REGISTER_SECONDS_ALARM, config.alarm_time.seconds) != OK)
            return 1;
        if (rtc_write_register(RTC_REGISTER_MINUTES_ALARM, config.alarm_time.minutes) != OK)
            return 1;
        if (rtc_write_register(RTC_REGISTER_HOURS_ALARM, config.alarm_time.hours) != OK)
            return 1;

        break;

    case UPDATE_INTERRUPT:
        reg_b |= RTC_UIE;
        break;
      
    case PERIODIC_INTERRUPT:
        reg_b |= RTC_PIE;
        
        uint8_t reg_a;
        if (rtc_read_register(RTC_REGISTER_A, &reg_a) != OK)
            return 1;

        reg_a = (reg_a & 0xf0) | (config.periodic_RS3210 & 0x0f);
        
        if (rtc_write_register(RTC_REGISTER_A, reg_a) != OK)
            return 1;

        break;
    }

    if (rtc_write_register(RTC_REGISTER_B, reg_b) != OK) {
        return 1;
    }

    return 0;
}

int rtc_disable_int(rtc_interrupt_t rtc_interrupt) {
    uint8_t reg_b = 0;
    if (rtc_read_register(RTC_REGISTER_B, &reg_b) != OK) {
        return 1;
    }

    switch (rtc_interrupt) {
    case ALARM_INTERRUPT:
        reg_b &= RTC_MASK_DISABLE(RTC_AIE);
        break;

    case UPDATE_INTERRUPT:
        reg_b &= RTC_MASK_DISABLE(RTC_UIE);
        break;
      
    case PERIODIC_INTERRUPT:
        reg_b &= RTC_MASK_DISABLE(RTC_PIE);
        break;
    }

    if (rtc_write_register(RTC_REGISTER_B, reg_b) != OK) {
        return 1;
    }

    return 0;
}

int rtc_read_register(uint8_t reg, uint8_t *value) {
    if (sys_outb(RTC_ADDR_REG, reg) != OK) {
        return 1;
    }

    if (util_sys_inb(RTC_DATA_REG, value) != OK) {
        return 1;
    }
    
    return 0;
}

int rtc_write_register(uint8_t reg, uint8_t value) {
    if (sys_outb(RTC_ADDR_REG, reg) != OK) {
        return 1;
    }
    
    if (sys_outb(RTC_DATA_REG, value) != OK) {
        return 1;
    }
    
    return 0;
}

void rtc_ih() {
    uint8_t register_c = 0;
    if (rtc_read_register(RTC_REGISTER_C, &register_c) != OK) {
        printf("Error while reading register C\n");
        return; 
    }

    if (register_c & RTC_UF) {
        if (rtc_read_date() != OK) {
            printf("Error while reading date\n");
        }
    }
    if (register_c & RTC_PF) {
        if (dispatcher_queue_event(RTC_PERIODIC_INTERRUPT_EVENT) != OK) {
            printf("Failed to queue rtc periodic interrupt event\n");
        }
    }
    if (register_c & RTC_AF) {
        // If the alarm interrupt is triggered right when a new alarm date is set,
        // in order to avoid the old alarm interrupt to be mistaken with the new
        // alarm interrupt, the old one should not trigger an event.
        // This check with the current date and the date the alarm is set to ensures that.
        if (!date_operator_less_than(current_date, last_alarm_set_to)) {
            if (dispatcher_queue_event(RTC_ALARM_EVENT) != OK) {
                printf("Failed to queue rtc alarm event\n");
            }
        }
    }
}

int rtc_flush() {
    uint8_t register_c = 0;
    if (rtc_read_register(RTC_REGISTER_C, &register_c) != OK) {
        printf("Error while flushing register C\n");
        return 1;
    }
    return 0;
}

unsigned int rtc_get_seed() {
    // Even though years and months don't always have 366 days and 31 days respectively,
    // this algorithm ensures that two different dates will very likely return two different
    // seeds.
    unsigned int seed = 366 * current_date.year;
    seed += 31 * current_date.month;
    seed += 24 * current_date.day;
    seed += 60 * current_date.hour;
    seed += 60 * current_date.minute;
    seed += current_date.second;

    return seed;
}

/**@}*/
