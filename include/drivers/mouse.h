#ifndef DRIVERS_MOUSE_H
#define DRIVERS_MOUSE_H

#include <libc/stdint.h>

typedef struct {
    int x;
    int y;
    int left_button;
    int right_button;
    int middle_button;
    int scroll_delta;
} mouse_state_t;

void mouse_init(void);
int  mouse_has_wheel(void);
void mouse_get_state(mouse_state_t* state);
mouse_state_t mouse_get_state_val(void);
void mouse_set_position(int x, int y);
void mouse_set_bounds(int min_x, int min_y, int max_x, int max_y);
void mouse_handler_c(void);

#endif // DRIVERS_MOUSE_H
