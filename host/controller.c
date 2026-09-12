#include "controller.h"

void sm64_controller_update(struct Controller *controller, uint16_t buttons, int8_t x, int8_t y) {
    /* The original read_controllers button edge calculation, supplied with
     * host input instead of an N64 controller DMA buffer. */
    controller->buttonPressed = buttons & (buttons ^ controller->buttonDown);
    controller->buttonDown = buttons;
    controller->rawStickX = x;
    controller->rawStickY = y;
    adjust_analog_stick(controller);
}
