#ifndef __OSD_H__
#define __OSD_H__

#include <stdbool.h>
#include <stdint.h>
#include "mi_common.h"

typedef enum {
    OSD_ICON_NONE = 0,
    OSD_ICON_PLAY,
    OSD_ICON_PAUSE,
} osd_icon_e;

int  osd_init(void);
void osd_deinit(void);
void osd_start(void);
void osd_stop(void);

void osd_show_left(const char *left, int duration_ms);
void osd_show_volume(int vol, int duration_ms);
void osd_show_time(int cur_sec, int total_sec, int duration_ms);
void osd_show_icon(osd_icon_e icon, int duration_ms);
void osd_show_progress(int percent, int duration_ms);
void osd_clear_all(void);

int  osd_get_progress_from_x(int x);
int  osd_is_touch_in_progress_area(int x, int y);

MI_PHY osd_get_phy_addr(void);
int    osd_get_bar_height(void);
int    osd_get_width(void);
int    osd_get_height(void);
int    osd_get_stride(void);
bool   osd_is_visible(void);

void   osd_lock(void);
void   osd_unlock(void);

#endif