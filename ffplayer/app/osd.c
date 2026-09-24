#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#include "mi_common.h"
#include "mi_sys.h"
#include "mi_disp.h"
#include "osd.h"

#ifndef ALIGN_UP
#define ALIGN_UP(x, align)  (((x) + ((align) - 1)) & ~((align) - 1))
#endif

#define OSD_BAR_H       60
#define OSD_REFRESH_MS  50

static int      g_osd_w = 1024;
static int      g_osd_h = 600;
static int      g_osd_stride = 0;
static uint8_t *g_osd_vir = NULL;
static MI_PHY   g_osd_phy = 0;
static int      g_osd_size = 0;

static pthread_mutex_t g_osd_lock = PTHREAD_MUTEX_INITIALIZER;

static volatile int  g_show_until_ms = 0;
static volatile bool g_visible = false;

static char        g_left[16]   = {0};
static char        g_center[16] = {0};
static char        g_right[24]  = {0};
static osd_icon_e  g_icon       = OSD_ICON_NONE;
static int         g_progress   = -1;

static volatile bool g_osd_run = false;
static pthread_t     g_osd_tid = 0;

static osd_gettime_cb_t g_gettime_cb = NULL;
static volatile int64_t g_last_user_time_ms = 0;

#define Y_BG     45
#define Y_WHITE  235
#define Y_GRAY   90
#define Y_ICON   235

/* ASCII 32~126 的 5x7 点阵，列优先，每字符 5 字节，每字节低 7 位有效 */
static const uint8_t font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x00,0x00,0x5F,0x00,0x00}, /* '!' */
    {0x00,0x07,0x00,0x07,0x00}, /* '"' */
    {0x14,0x7F,0x14,0x7F,0x14}, /* '#' */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* '$' */
    {0x23,0x13,0x08,0x64,0x62}, /* '%' */
    {0x36,0x49,0x55,0x22,0x50}, /* '&' */
    {0x00,0x05,0x03,0x00,0x00}, /* ''' */
    {0x00,0x1C,0x22,0x41,0x00}, /* '(' */
    {0x00,0x41,0x22,0x1C,0x00}, /* ')' */
    {0x14,0x08,0x3E,0x08,0x14}, /* '*' */
    {0x08,0x08,0x3E,0x08,0x08}, /* '+' */
    {0x00,0x50,0x30,0x00,0x00}, /* ',' */
    {0x08,0x08,0x08,0x08,0x08}, /* '-' */
    {0x00,0x60,0x60,0x00,0x00}, /* '.' */
    {0x20,0x10,0x08,0x04,0x02}, /* '/' */
    {0x3E,0x51,0x49,0x45,0x3E}, /* '0' */
    {0x00,0x42,0x7F,0x40,0x00}, /* '1' */
    {0x42,0x61,0x51,0x49,0x46}, /* '2' */
    {0x21,0x41,0x45,0x4B,0x31}, /* '3' */
    {0x18,0x14,0x12,0x7F,0x10}, /* '4' */
    {0x27,0x45,0x45,0x45,0x39}, /* '5' */
    {0x3C,0x4A,0x49,0x49,0x30}, /* '6' */
    {0x01,0x71,0x09,0x05,0x03}, /* '7' */
    {0x36,0x49,0x49,0x49,0x36}, /* '8' */
    {0x06,0x49,0x49,0x29,0x1E}, /* '9' */
    {0x00,0x36,0x36,0x00,0x00}, /* ':' */
    {0x00,0x56,0x36,0x00,0x00}, /* ';' */
    {0x00,0x08,0x14,0x22,0x41}, /* '<' */
    {0x14,0x14,0x14,0x14,0x14}, /* '=' */
    {0x00,0x41,0x22,0x14,0x08}, /* '>' */
    {0x02,0x01,0x51,0x09,0x06}, /* '?' */
    {0x32,0x49,0x79,0x41,0x3E}, /* '@' */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 'A' */
    {0x7F,0x49,0x49,0x49,0x36}, /* 'B' */
    {0x3E,0x41,0x41,0x41,0x22}, /* 'C' */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 'D' */
    {0x7F,0x49,0x49,0x49,0x41}, /* 'E' */
    {0x7F,0x09,0x09,0x09,0x01}, /* 'F' */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 'G' */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 'H' */
    {0x00,0x41,0x7F,0x41,0x00}, /* 'I' */
    {0x20,0x40,0x41,0x3F,0x01}, /* 'J' */
    {0x7F,0x08,0x14,0x22,0x41}, /* 'K' */
    {0x7F,0x40,0x40,0x40,0x40}, /* 'L' */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* 'M' */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 'N' */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 'O' */
    {0x7F,0x09,0x09,0x09,0x06}, /* 'P' */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 'Q' */
    {0x7F,0x09,0x19,0x29,0x46}, /* 'R' */
    {0x46,0x49,0x49,0x49,0x31}, /* 'S' */
    {0x01,0x01,0x7F,0x01,0x01}, /* 'T' */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 'U' */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 'V' */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 'W' */
    {0x63,0x14,0x08,0x14,0x63}, /* 'X' */
    {0x07,0x08,0x70,0x08,0x07}, /* 'Y' */
    {0x61,0x51,0x49,0x45,0x43}, /* 'Z' */
    {0x00,0x7F,0x41,0x41,0x00}, /* '[' */
    {0x02,0x04,0x08,0x10,0x20}, /* '\' */
    {0x00,0x41,0x41,0x7F,0x00}, /* ']' */
    {0x04,0x02,0x01,0x02,0x04}, /* '^' */
    {0x40,0x40,0x40,0x40,0x40}, /* '_' */
    {0x00,0x01,0x02,0x04,0x00}, /* '`' */
    {0x20,0x54,0x54,0x54,0x78}, /* 'a' */
    {0x7F,0x48,0x44,0x44,0x38}, /* 'b' */
    {0x38,0x44,0x44,0x44,0x20}, /* 'c' */
    {0x38,0x44,0x44,0x48,0x7F}, /* 'd' */
    {0x38,0x54,0x54,0x54,0x18}, /* 'e' */
    {0x08,0x7E,0x09,0x01,0x02}, /* 'f' */
    {0x0C,0x52,0x52,0x52,0x3E}, /* 'g' */
    {0x7F,0x08,0x04,0x04,0x78}, /* 'h' */
    {0x00,0x44,0x7D,0x40,0x00}, /* 'i' */
    {0x20,0x40,0x44,0x3D,0x00}, /* 'j' */
    {0x7F,0x10,0x28,0x44,0x00}, /* 'k' */
    {0x00,0x41,0x7F,0x40,0x00}, /* 'l' */
    {0x7C,0x04,0x18,0x04,0x78}, /* 'm' */
    {0x7C,0x08,0x04,0x04,0x78}, /* 'n' */
    {0x38,0x44,0x44,0x44,0x38}, /* 'o' */
    {0x7C,0x14,0x14,0x14,0x08}, /* 'p' */
    {0x08,0x14,0x14,0x18,0x7C}, /* 'q' */
    {0x7C,0x08,0x04,0x04,0x08}, /* 'r' */
    {0x48,0x54,0x54,0x54,0x20}, /* 's' */
    {0x04,0x3F,0x44,0x40,0x20}, /* 't' */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 'u' */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 'v' */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 'w' */
    {0x44,0x28,0x10,0x28,0x44}, /* 'x' */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 'y' */
    {0x44,0x64,0x54,0x4C,0x44}, /* 'z' */
    {0x00,0x08,0x36,0x41,0x00}, /* '{' */
    {0x00,0x00,0x7F,0x00,0x00}, /* '|' */
    {0x00,0x41,0x36,0x08,0x00}, /* '}' */
    {0x08,0x08,0x2A,0x1C,0x08}, /* '~' */
};

static int64_t now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static inline void put_pixel(int x, int y, uint8_t y_val)
{
    if (x < 0 || x >= g_osd_w || y < 0 || y >= OSD_BAR_H) return;
    g_osd_vir[y * g_osd_stride + x] = y_val;
}

static void draw_rect(int x, int y, int w, int h, uint8_t y_val)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            put_pixel(x + i, y + j, y_val);
}

static void draw_char(int x, int y, char c, int scale, uint8_t y_val)
{
    if (c < 0x20 || c > 0x7E) return;
    const uint8_t *bm = font5x7[(int)c - 0x20];
    /* 列优先：bm[i] 表示第 i 列，bit j 表示第 j 行 */
    for (int i = 0; i < 5; i++)
        for (int j = 0; j < 7; j++)
            if (bm[i] & (1 << j))
                draw_rect(x + i * scale, y + j * scale, scale, scale, y_val);
}

static void draw_string(int x, int y, const char *s, int scale, uint8_t y_val)
{
    while (*s) { draw_char(x, y, *s, scale, y_val); x += 6 * scale; s++; }
}

/* 播放三角形 */
static void draw_play_icon(int x, int y, int size, uint8_t y_val)
{
    for (int i = 0; i < size; i++)
        for (int j = -i; j <= i; j += 2)
            put_pixel(x + i, y + size/2 + j/2, y_val);
}

/* 暂停两条竖线 */
static void draw_pause_icon(int x, int y, int size, uint8_t y_val)
{
    int bar_w = size / 3;
    if (bar_w < 2) bar_w = 2;
    for (int j = 0; j < size; j++) {
        for (int i = 0; i < bar_w; i++) {
            put_pixel(x + i, y + j, y_val);
            put_pixel(x + size - bar_w + i, y + j, y_val);
        }
    }
}

/* 左侧图标 + 文字，居中文字，右侧时间 */
static void render(void)
{
    int scale = 3;
    int row_y = 12;

    int left_x = 20;
    if (g_icon != OSD_ICON_NONE) {
        if (g_icon == OSD_ICON_PLAY)
            draw_play_icon(20, row_y, 18, Y_ICON);
        else
            draw_pause_icon(20, row_y, 18, Y_ICON);
        left_x = 50;
    }

    if (g_left[0])
        draw_string(left_x, row_y, g_left, scale, Y_WHITE);

    if (g_center[0]) {
        int len = strlen(g_center);
        int cx  = (g_osd_w - len * 6 * scale) / 2;
        draw_string(cx, row_y, g_center, scale, Y_WHITE);
    }

    if (g_right[0]) {
        int len = strlen(g_right);
        int rx  = g_osd_w - 20 - len * 6 * scale;
        if (rx < 0) rx = 0;
        draw_string(rx, row_y, g_right, scale, Y_WHITE);
    }

    if (g_progress >= 0) {
        int by = OSD_BAR_H - 14;
        draw_rect(20, by, g_osd_w - 40, 6, Y_GRAY);
        int w = (g_osd_w - 40) * g_progress / 100;
        draw_rect(20, by, w, 6, Y_WHITE);
    }
}

static void *osd_thread(void *arg)
{
    (void)arg;
    int64_t last_time_update = 0;

    while (g_osd_run) {
        int64_t t = now_ms();
        int show  = (g_show_until_ms > 0 && t <= g_show_until_ms);

        /* OSD 显示中，且用户最近 1 秒没手动操作过，每 500ms 刷新一次时间 */
        if (show && g_gettime_cb && g_progress >= 0 &&
            t - last_time_update > 500 &&
            t - g_last_user_time_ms > 1000) {
            int cur = 0, total = 0;
            g_gettime_cb(&cur, &total);
            pthread_mutex_lock(&g_osd_lock);
            snprintf(g_right, sizeof(g_right), "%d:%02d / %d:%02d",
                     cur / 60, cur % 60, total / 60, total % 60);
            pthread_mutex_unlock(&g_osd_lock);
            last_time_update = t;
        }

        pthread_mutex_lock(&g_osd_lock);
        if (show) {
            memset(g_osd_vir, Y_BG, g_osd_size);
            render();
            MI_SYS_FlushInvCache(g_osd_vir, g_osd_size);
        }
        pthread_mutex_unlock(&g_osd_lock);

        g_visible = show;
        usleep(OSD_REFRESH_MS * 1000);
    }
    return NULL;
}

int osd_init(void)
{
    MI_DISP_PubAttr_t stPubAttr;
    if (MI_SUCCESS == MI_DISP_GetPubAttr(0, &stPubAttr)) {
        if (stPubAttr.stSyncInfo.u16Hact > 0) g_osd_w = stPubAttr.stSyncInfo.u16Hact;
        if (stPubAttr.stSyncInfo.u16Vact > 0) g_osd_h = stPubAttr.stSyncInfo.u16Vact;
    }
    printf("osd_init: %dx%d, bar=%d\n", g_osd_w, g_osd_h, OSD_BAR_H);

    g_osd_stride = ALIGN_UP(g_osd_w, 16);
    g_osd_size   = g_osd_stride * OSD_BAR_H;

    if (MI_SUCCESS != MI_SYS_MMA_Alloc((MI_U8 *)"#osd",
            (MI_U32)g_osd_size, &g_osd_phy)) {
        printf("osd MMA_Alloc failed\n");
        return -1;
    }
    if (MI_SUCCESS != MI_SYS_Mmap(g_osd_phy, (MI_U32)g_osd_size,
            (void **)&g_osd_vir, TRUE)) {
        printf("osd Mmap failed\n");
        MI_SYS_MMA_Free(g_osd_phy);
        return -1;
    }
    memset(g_osd_vir, 0, g_osd_size);
    MI_SYS_FlushInvCache(g_osd_vir, g_osd_size);
    return 0;
}

void osd_deinit(void)
{
    if (g_osd_run) osd_stop();
    if (g_osd_vir) { MI_SYS_Munmap(g_osd_vir, g_osd_size); g_osd_vir = NULL; }
    if (g_osd_phy) { MI_SYS_MMA_Free(g_osd_phy); g_osd_phy = 0; }
}

static void bump_show(int ms)
{
    g_show_until_ms = now_ms() + ms;
}

void osd_show_left(const char *left, int ms)
{
    pthread_mutex_lock(&g_osd_lock);
    if (left) {
        strncpy(g_left, left, sizeof(g_left) - 1);
        g_left[sizeof(g_left) - 1] = 0;
    } else {
        g_left[0] = 0;
    }
    bump_show(ms);
    pthread_mutex_unlock(&g_osd_lock);
}

void osd_show_volume(int vol, int ms)
{
    pthread_mutex_lock(&g_osd_lock);
    snprintf(g_center, sizeof(g_center), "VOL %d", vol);
    bump_show(ms);
    pthread_mutex_unlock(&g_osd_lock);
}

void osd_show_time(int cur_sec, int total_sec, int ms)
{
    pthread_mutex_lock(&g_osd_lock);
    if (cur_sec  < 0) cur_sec = 0;
    if (total_sec < 0) total_sec = 0;
    snprintf(g_right, sizeof(g_right), "%d:%02d / %d:%02d",
             cur_sec / 60, cur_sec % 60, total_sec / 60, total_sec % 60);
    g_last_user_time_ms = now_ms();
    bump_show(ms);
    pthread_mutex_unlock(&g_osd_lock);
}

void osd_show_icon(osd_icon_e icon, int ms)
{
    pthread_mutex_lock(&g_osd_lock);
    g_icon = icon;
    bump_show(ms);
    pthread_mutex_unlock(&g_osd_lock);
}

void osd_show_progress(int percent, int ms)
{
    pthread_mutex_lock(&g_osd_lock);
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;
    g_progress = percent;
    bump_show(ms);
    pthread_mutex_unlock(&g_osd_lock);
}

void osd_clear_all(void)
{
    pthread_mutex_lock(&g_osd_lock);
    g_left[0]  = 0;
    g_center[0] = 0;
    g_icon = OSD_ICON_NONE;
    /* g_right 和 g_progress 不动，保留上次的值 */
    pthread_mutex_unlock(&g_osd_lock);
}

int osd_get_progress_from_x(int x)
{
    int pct = (x - 20) * 100 / (g_osd_w - 40);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

int osd_is_touch_in_progress_area(int x, int y)
{
    (void)x;
    return (y > g_osd_h - 100) ? 1 : 0;
}

void osd_start(void)
{
    if (g_osd_run) return;
    g_osd_run = true;
    pthread_create(&g_osd_tid, NULL, osd_thread, NULL);
}

void osd_stop(void)
{
    if (!g_osd_run) return;
    g_osd_run = false;
    if (g_osd_tid) { pthread_join(g_osd_tid, NULL); g_osd_tid = 0; }
}

MI_PHY osd_get_phy_addr(void)   { return g_osd_phy; }
int    osd_get_bar_height(void){ return OSD_BAR_H; }
int    osd_get_width(void)     { return g_osd_w; }
int    osd_get_height(void)    { return g_osd_h; }
int    osd_get_stride(void)    { return g_osd_stride; }
bool   osd_is_visible(void)    { return g_visible; }

void osd_lock(void)   { pthread_mutex_lock(&g_osd_lock); }
void osd_unlock(void) { pthread_mutex_unlock(&g_osd_lock); }

void osd_set_gettime_cb(osd_gettime_cb_t cb)
{
    g_gettime_cb = cb;
}
