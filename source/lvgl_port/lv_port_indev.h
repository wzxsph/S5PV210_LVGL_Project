#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

void lv_port_indev_init(void);
lv_group_t * lv_port_indev_get_group(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_PORT_INDEV_H */
