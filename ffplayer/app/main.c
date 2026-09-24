#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include <linux/input.h>
#include <sys/select.h>
#include <fcntl.h>

#ifdef CHIP_IS_SS268
#include "ss268_panel.h"
#elif defined CHIP_IS_SS22X
#include "ss22x_panel.h"
#else
#include "sd20xpanel.h"
#endif
#include "player.h"
#include "interface.h"

#include "osd.h"
#include <linux/input.h>
#include <fcntl.h>
#include <sys/select.h>
#include <math.h>

typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);

#define MAX_PLAYLIST 128
#define TOUCH_DEV       "/dev/input/event0"
#define SWIPE_THRESHOLD 80      // 最小滑动距离(像素)，按屏调
#define SEEK_STEP       10.0    // 快进快退秒数
#define DOUBLE_CLICK_MS  300
#define LONG_PRESS_MS    500

static int  g_touch_x = 0, g_touch_y = 0;
static int  g_start_x = 0, g_start_y = 0;
static bool g_touching = false;
static int  g_touch_fd = -1;

static char *playlist[MAX_PLAYLIST];
static int playlist_count = 0;
static int current_index = 0;
static pthread_mutex_t player_mutex = PTHREAD_MUTEX_INITIALIZER;

static int width, height;
static volatile bool b_exit = false;
static bool player_working = false;
static double duration, position;

// 全局音量 / 静音状态，切换后能恢复
static int volumn = 20;
static bool mute = false;

void signal_handler_fun(int signum) {
    printf("catch signal [%d]\n", signum);
    b_exit = true;
}

/**
 * 统一播放函数：和第一次播放完全一样
 * 设置音频选项 -> 设置视频选项 -> open -> 取时长 -> 设音量
 */
static int player_start(int index)
{
    if (index < 0 || index >= playlist_count) return -1;

#ifdef SUPPORT_HDMI
    mm_player_set_opts("audio_device", "", 3);
    mm_player_set_opts("audio_layout", "", AV_CH_LAYOUT_MONO);
#else
    mm_player_set_opts("audio_device", "", 0);
#endif

    mm_player_set_opts("video_rotate", "", AV_ROTATE_NONE);
    mm_player_set_opts("video_only", "", 0);
    mm_player_set_opts("video_ratio", "", AV_SCREEN_MODE);
    mm_player_set_opts("enable_scaler", "", 0);
    mm_player_set_opts("resolution", "8294400", 0);
    mm_player_set_opts("play_mode", "", AV_ONCE);

    int ret = mm_player_open(playlist[index], 0, 0, width, height);
    if (ret < 0) {
        printf("open %s failed\n", playlist[index]);
        return -1;
    }

    mm_player_getduration(&duration);

    mm_player_set_volumn(volumn);
    if (mute) mm_player_set_mute(true);

    printf("try playing %s ...\n", playlist[index]);
    return 0;
}

static void * mm_player_thread(void *args)
{
    int ret;
    (void)args;

    while (!b_exit)
    {
        if (!player_working)
        {
            sleep(1);
            continue;
        }

        ret = mm_player_get_status();
        if (ret < 0)
        {
            printf("mmplayer has been closed!\n");
            player_working = false;
            continue;
        }

        if (ret & AV_PLAY_ERROR)
        {
            pthread_mutex_lock(&player_mutex);
            mm_player_close();
            pthread_mutex_unlock(&player_mutex);
            b_exit = true;
        }
        else if (ret & AV_PLAY_LOOP)
        {
            // 单曲循环，什么都不做
        }
        else if ((ret & AV_PLAY_COMPLETE) == AV_PLAY_COMPLETE)
        {
            player_working = false;
            pthread_mutex_lock(&player_mutex);
            mm_player_close();
            pthread_mutex_unlock(&player_mutex);
            usleep(800 * 1000);
            current_index = (current_index + 1) % playlist_count;
            ret = player_start(current_index);
            if (ret < 0) {
                b_exit = true;
            } else {
                player_working = true;
            }
            pthread_mutex_unlock(&player_mutex);
        }
        av_usleep(50 * 1000);
    }

    return NULL;
}

static int64_t touch_now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void *touch_thread(void *arg)
{
    (void)arg;
    struct input_event ev;
    int tx = 0, ty = 0;
    int sx = 0, sy = 0;
    int touching = 0, dragging = 0;
    int64_t last_click_ms = 0;
    int fd = open(TOUCH_DEV, O_RDONLY);
    if (fd < 0) { printf("open %s failed\n", TOUCH_DEV); return NULL; }
    printf("touch_thread started\n");

    while (!b_exit) {
        fd_set fds;
        struct timeval tv = {0, 100000};
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        if (select(fd + 1, &fds, NULL, NULL, &tv) <= 0) continue;
        if (read(fd, &ev, sizeof(ev)) != sizeof(ev)) continue;

        if (ev.type == EV_ABS) {
            if (ev.code == ABS_MT_POSITION_X || ev.code == ABS_X) tx = ev.value;
            else if (ev.code == ABS_MT_POSITION_Y || ev.code == ABS_Y) ty = ev.value;

            if (dragging) {
                int pct  = osd_get_progress_from_x(tx);
                int secs = (int)(duration * pct / 100.0);
                osd_show_progress(pct, 1500);
                osd_show_time(secs, (int)duration, 1500);
            }
        }
        else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            if (ev.value == 1) {
                touching = 1;
                dragging = 0;
                sx = tx; sy = ty;
                osd_clear_all();

                /* 按下时立即判断是否在进度条区域，不等 ABS */
                if (osd_is_touch_in_progress_area(tx, ty)) {
                    dragging = 1;
                    int pct = osd_get_progress_from_x(tx);
                    int secs = (int)(duration * pct / 100.0);
                    osd_show_progress(pct, 1500);
                    osd_show_time(secs, (int)duration, 1500);
                }
            }
            else if (ev.value == 0 && touching) {
                touching = 0;
                int dx  = tx - sx;
                int dy  = ty - sy;
                int adx = dx < 0 ? -dx : dx;
                int ady = dy < 0 ? -dy : dy;

                /* 拖拽结束：seek 到目标 */
                if (dragging) {
                    int pct = osd_get_progress_from_x(tx);
                    double target = duration * pct / 100.0;
                    mm_player_seek2time(target);
                    osd_show_progress(pct, 1500);
                    osd_show_time((int)target, (int)duration, 1500);
                    dragging = 0;
                    continue;
                }

                /* 点击 */
                if (adx < SWIPE_THRESHOLD && ady < SWIPE_THRESHOLD) {
                    if (last_click_ms > 0 &&
                        touch_now_ms() - last_click_ms < DOUBLE_CLICK_MS) {
                        /* 双击：暂停 / 播放，图标 + 文字 */
                        if (g_mmplayer && g_mmplayer->paused) {
                            mm_player_resume();
                            osd_show_icon(OSD_ICON_PLAY, 1500);
                            osd_show_left("PLAY", 1500);
                        } else {
                            mm_player_pause();
                            osd_show_icon(OSD_ICON_PAUSE, 1500);
                            osd_show_left("PAUSE", 1500);
                        }
                        last_click_ms = 0;
                    } else {
                        /* 单击：进度 + 时间 + 当前播放状态图标 */
                        double pos;
                        mm_player_getposition(&pos);
                        if (duration > 0.1) {
                            int pct = (int)(pos * 100 / duration);
                            osd_show_progress(pct, 3000);
                            osd_show_time((int)pos, (int)duration, 3000);
                            if (g_mmplayer && g_mmplayer->paused) {
                                osd_show_icon(OSD_ICON_PAUSE, 3000);
                                osd_show_left("PAUSE", 3000);
                            } else {
                                osd_show_icon(OSD_ICON_PLAY, 3000);
                                osd_show_left("PLAY", 3000);
                            }
                        }
                        last_click_ms = touch_now_ms();
                    }
                }
                /* 横向：快进快退，同步更新进度条 */
                else if (adx > ady) {
                    double pos;
                    mm_player_getposition(&pos);
                    char buf[16];
                    if (dx > 0) {
                        pos += SEEK_STEP;
                        if (pos > duration) pos = duration;
                        mm_player_seek2time(pos);
                        snprintf(buf, sizeof(buf), ">> %ds", (int)SEEK_STEP);
                    } else {
                        pos -= SEEK_STEP;
                        if (pos < 0) pos = 0;
                        mm_player_seek2time(pos);
                        snprintf(buf, sizeof(buf), "<< %ds", (int)SEEK_STEP);
                    }
                    int pct = (duration > 0.1) ? (int)(pos * 100 / duration) : 0;
                    osd_show_left(buf, 1500);
                    osd_show_progress(pct, 1500);
                    osd_show_time((int)pos, (int)duration, 1500);
                }
                /* 纵向：音量，只更新中间，保留进度条和时间 */
                else {
                    if (dy < 0) { volumn += 5; if (volumn > 100) volumn = 100; }
                    else        { volumn -= 5; if (volumn < 0) volumn = 0; }
                    mm_player_set_volumn(volumn);
                    osd_show_volume(volumn, 1500);
                }
            }
        }
    }
    close(fd);
    return NULL;
}

static int load_playlist_from_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char line[512];
    while (fgets(line, sizeof(line), fp) && playlist_count < MAX_PLAYLIST) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '\0' || line[0] == '#') continue;
        playlist[playlist_count++] = strdup(line);
    }
    fclose(fp);
    return 0;
}

static bool has_txt_ext(const char *s)
{
    size_t len = strlen(s);
    return len > 4 && strcasecmp(s + len - 4, ".txt") == 0;
}

int main(int argc, char *argv[])
{
    int ret;
    bool win_down = false;
    char cmd;
    pthread_t mm_thread = NULL;
	pthread_t touch_tid = 0;
    bool disp_flag = false;

    if (argc < 2) {
        printf("usage:\n");
        printf("  %s <media_file> [media_file2 ...]\n", argv[0]);
        printf("  %s <playlist.txt>\n", argv[0]);
        return -1;
    }

    printf("welcome to test ssplayer!\n");
    srand(time(NULL));
    signal(SIGINT, signal_handler_fun);

    // 解析播放列表
    if (has_txt_ext(argv[1])) {
        if (load_playlist_from_file(argv[1]) < 0) {
            printf("open playlist %s failed\n", argv[1]);
            return -1;
        }
    } else {
        for (int i = 1; i < argc && playlist_count < MAX_PLAYLIST; i++) {
            playlist[playlist_count++] = strdup(argv[i]);
        }
    }

    if (playlist_count == 0) {
        printf("empty playlist\n");
        return -1;
    }
    current_index = 0;

#ifdef CHIP_IS_SS268
    ss268_sys_init();
    ss268_screen_init();
    #ifdef SUPPORT_HDMI
    mm_player_set_opts("audio_device", "", 4);
    mm_player_set_opts("audio_layout", "", AV_CH_LAYOUT_STEREO);
    #else
    mm_player_set_opts("audio_device", "", 0);
    #endif
    ss268_getpanel_wh(&width, &height);
#elif defined CHIP_IS_SS22X
    ss22x_sys_init();
    ss22x_screen_init();
    #ifdef SUPPORT_HDMI
    mm_player_set_opts("audio_device", "", 4);
    mm_player_set_opts("audio_layout", "", AV_CH_LAYOUT_STEREO);
    #else
    mm_player_set_opts("audio_device", "", 0);
    #endif
    ss22x_getpanel_wh(&width, &height);
#else
    sd20x_sys_init();
    #ifdef SUPPORT_HDMI
    sd20x_panel_init(E_MI_DISP_INTF_HDMI, 0);
    mm_player_set_opts("audio_device", "", 3);
    mm_player_set_opts("audio_layout", "", AV_CH_LAYOUT_MONO);
    #else
    sd20x_panel_init(E_MI_DISP_INTF_LCD, 0);
    mm_player_set_opts("audio_device", "", 0);
    #endif
    ssd20x_getpanel_wh(&width, &height);
#endif

    // 第一次播放，走统一函数
    ret = player_start(current_index);
    if (ret < 0) {
        goto exit;
    }
    player_working = true;

    ret = pthread_create(&mm_thread, NULL, mm_player_thread, NULL);
    if (ret != 0) {
        goto exit;
    }

    /* OSD 初始化 */
    if (osd_init() != 0) {
        printf("osd_init failed\n");
    } else {
        osd_start();
    }

	ret = pthread_create(&touch_tid, NULL, touch_thread, NULL);
    if (ret != 0) {
        printf("touch_thread create failed\n");
    }

    b_exit = false;
    while (!b_exit)
    {
        fflush(stdin);
        cmd = getchar();
        switch (cmd)
        {
            case 's':
                pthread_mutex_lock(&player_mutex);
                player_start(current_index);
                player_working = true;
                pthread_mutex_unlock(&player_mutex);
                break;

            case 't':
                player_working = false;
                pthread_mutex_lock(&player_mutex);
                mm_player_close();
                pthread_mutex_unlock(&player_mutex);
                break;

            case 'f':
                mm_player_getposition(&position);
                position += 5.0;
                position = (position >= duration) ? duration : position;
                mm_player_seek2time(position);
                break;

            case 'v':   // 后退 5 秒
                mm_player_getposition(&position);
                position -= 5.0;
                position = (position <= 0) ? 0 : position;
                mm_player_seek2time(position);
                break;

            case 'u':
                mm_player_resume();
                break;

            case 'p':
                mm_player_pause();
                break;

            case 'g':
                mm_player_getduration(&duration);
                break;

            case 'd':
                mm_player_getposition(&position);
                printf("play %s in [%.3f]\n", playlist[current_index], position);
                break;

            case 'm':
                mute = !mute;
                mm_player_set_mute(mute);
                printf("audio mute status: %d\n", mm_player_get_status());
                break;

            case '+':
                volumn += 5;
                volumn = (volumn > 100) ? 100 : volumn;
                mm_player_set_volumn(volumn);
                break;

            case '-':
                volumn -= 5;
                volumn = (volumn < 0) ? 0 : volumn;
                mm_player_set_volumn(volumn);
                break;

            case 'w':
                if (!win_down) {
                    mm_player_set_window(0, 0, width / 2, height / 2);
                    win_down = true;
                } else {
                    mm_player_set_window(0, 0, width, height);
                    win_down = false;
                }
                break;

            case 'n':
                if (playlist_count > 1) {
                    player_working = false;
                    pthread_mutex_lock(&player_mutex);
                    mm_player_close();
                    pthread_mutex_unlock(&player_mutex);
                    usleep(800 * 1000);
                    current_index = (current_index + 1) % playlist_count;
                    ret = player_start(current_index);
                    if (ret < 0) {
                        b_exit = true;
                    } else {
                        player_working = true;
                    }
                }
            break;

            case 'b':
                if (playlist_count > 1) {
                    player_working = false;
                    pthread_mutex_lock(&player_mutex);
                    mm_player_close();
                    pthread_mutex_unlock(&player_mutex);
                    usleep(800 * 1000);
                    current_index = (current_index - 1 + playlist_count) % playlist_count;
                    ret = player_start(current_index);
                    if (ret < 0) {
                        b_exit = true;
                    } else {
                        player_working = true;
                    }
                }
            break;

            case 'r':
                if (playlist_count > 1) {
                    player_working = false;
                    pthread_mutex_lock(&player_mutex);
                    mm_player_close();
                    pthread_mutex_unlock(&player_mutex);
                    usleep(800 * 1000);
                    current_index = rand() % playlist_count;
                    ret = player_start(current_index);
                    if (ret < 0) {
                        b_exit = true;
                    } else {
                        player_working = true;
                    }
                }
            break;

            case 'c':
                disp_flag = !disp_flag;
                mm_player_flush_screen(disp_flag);
                break;

            case 'q':
                player_working = false;
                pthread_mutex_lock(&player_mutex);
                mm_player_close();
                pthread_mutex_unlock(&player_mutex);
                b_exit = true;
                break;
            case 'R':
                system("reboot");
                break;
            default:
                break;
        }
        fflush(stdout);
        cmd = '\0';
    }

    if (mm_thread)
        pthread_join(mm_thread, NULL);
	if (touch_tid)
        pthread_join(touch_tid, NULL);
    osd_stop();
    osd_deinit();    

exit:
    if (player_working) {
        player_working = false;
        pthread_mutex_lock(&player_mutex);
        mm_player_close();
        pthread_mutex_unlock(&player_mutex);
    }

#ifdef CHIP_IS_SS268
    ss268_screen_deinit();
    ss268_sys_deinit();
#elif defined CHIP_IS_SS22X
    ss22x_screen_deinit();
    ss22x_sys_deinit();
#else
    #ifdef SUPPORT_HDMI
    sd20x_panel_deinit(E_MI_DISP_INTF_HDMI);
    #else
    sd20x_panel_deinit(E_MI_DISP_INTF_LCD);
    #endif
    sd20x_sys_deinit();
#endif
    return 0;
}