// stm32fxx-prefix.c becomes the initial portion of the generated pins file.

#include <stdio.h>
#include <stdint.h>
#include <mk20dx128.h>

#include "teensy_hal.h"

#include "misc.h"
#include "mpconfig.h"
#include "qstr.h"
#include "obj.h"
#include "pin.h"

#define AF(af_idx, af_fn, af_unit, af_type, af_ptr) \
{ \
    { &pin_af_type }, \
    .idx = (af_idx), \
    .fn = AF_FN_ ## af_fn, \
    .unit = (af_unit), \
    .type = AF_PIN_TYPE_ ## af_fn ## _ ## af_type, \
    .af_fn = (af_ptr) \
}

#define PIN(p_port, p_pin, p_num_af, p_af, p_adc_num, p_adc_channel) \
{ \
    { &pin_type }, \
    .name = #p_port #p_pin, \
    .port = PORT_ ## p_port, \
    .pin = (p_pin), \
    .num_af = (p_num_af), \
    .pin_mask = (1 << (p_pin)), \
    .gpio = GPIO ## p_port, \
    .af = p_af, \
    .adc_num = p_adc_num, \
    .adc_channel = p_adc_channel, \
}
