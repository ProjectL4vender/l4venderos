/* SystemC 3.0 BL0SS0M MOD.0 — L4vender freestanding i386 kernel and desktop. */
#include "../net/net.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002u
#define MULTIBOOT_INFO_FRAMEBUFFER (1u << 12)
static u32 ui_bg = 0x14213Du;
static u32 ui_panel = 0x202C4Du;
static u32 ui_panel2 = 0x2A3A60u;
static u32 ui_accent = 0xB39DFFu;
static u32 ui_cyan = 0x7CE7F2u;
static u32 ui_white = 0xF7F7FFu;
static u32 ui_muted = 0xB4BED9u;
static u32 ui_red = 0xFF718Bu;
static u32 ui_green = 0x6BE5AEu;
static u32 ui_yellow = 0xFFD477u;
#define COLOR_BG       ui_bg
#define COLOR_PANEL    ui_panel
#define COLOR_PANEL2   ui_panel2
#define COLOR_ACCENT   ui_accent
#define COLOR_CYAN     ui_cyan
#define COLOR_WHITE    ui_white
#define COLOR_MUTED    ui_muted
#define COLOR_RED      ui_red
#define COLOR_GREEN    ui_green
#define COLOR_YELLOW   ui_yellow
#define TASKBAR_H      42u
#define APP_ICON_COUNT 16u
#define SYSTEMC_VERSION "3.0"
#define SYSTEMC_CODENAME "BL0SS0M MOD.0"
#define PAINT_W 240u
#define PAINT_H 120u
#define PAINT_SCALE 2u
#define NOTEPAD_CAP 2048u
#define BROWSER_CAP 2048u

struct framebuffer {
    volatile u8 *address;
    u32 pitch;
    u32 width;
    u32 height;
    u8 bpp;
};

static struct framebuffer fb;
/* Off-screen frame: drawing completes before one copy updates the VBE framebuffer. */
#define BACKBUFFER_MAX_PITCH 8192u
#define BACKBUFFER_MAX_HEIGHT 1080u
static u8 framebuffer_backbuffer[BACKBUFFER_MAX_PITCH * BACKBUFFER_MAX_HEIGHT];
static int backbuffer_valid;
static int graphical;
static int active_app;
static int screen_stage; /* 0 first-boot setup, 1 login, 2 desktop */
static int setup_field;
static int settings_page;
static int theme_id;
static int start_menu_open;
static int settings_resolution_focus;
static char setup_username[32];
static char setup_password[32];
static char machine_name[32];
static char login_password[32];
static u32 setup_username_len, setup_password_len, machine_name_len, login_password_len;
static char setup_status[64] = "CREATE A LOCAL SESSION PROFILE";
static char login_status[64] = "ENTER YOUR SESSION PASSWORD";
static char cpu_vendor[13] = "UNKNOWN";
static char cpu_brand[49] = "UNKNOWN CPU";
static u32 system_ram_mb;
static u32 boot_memory_kb;
/* Per-app drawing translation: title bar and client area move together. */
static int draw_offset_x, draw_offset_y;
static int window_dx[20], window_dy[20];
static int dragging_window, drag_app;
static int drag_mouse_x, drag_mouse_y, drag_origin_dx, drag_origin_dy;
static u8 clock_shown_hour = 255u, clock_shown_minute = 255u;
static u32 clock_poll_counter;
static int paint_x = 80;
static int paint_y = 45;
static u32 paint_colour = 0x7CE7F2u;
static u32 paint_pixels[PAINT_W * PAINT_H];
static u16 paint_queue[PAINT_W * PAINT_H];
static int paint_tool;
static int paint_start_x, paint_start_y, paint_last_x, paint_last_y;
static int paint_drawing;
static u32 paint_background = 0xF4F6FFu;
static char notepad_buffer[NOTEPAD_CAP];
static u32 notepad_len;
static u32 notepad_scroll;
static char browser_url[128] = "http://neverssl.com/";
static u32 browser_url_len = 20;
static char browser_text[BROWSER_CAP];
static char browser_raw[4096];
static u32 browser_text_len;
static char browser_status[48] = "TYPE HTTP URL AND PRESS ENTER";
static int mouse_x = 320, mouse_y = 240;
static int mouse_left;
static int mouse_right;
static int context_menu_open;
static int context_menu_x, context_menu_y, context_target_app;
static u32 requested_resolution_width = 640u, requested_resolution_height = 480u;
static char resolution_input[24] = "640 480";
static u32 resolution_input_len = 7u;
static char resolution_status[64] = "DEFAULT BOOT MODE: 640X480";
static u8 mouse_packet[3];
static u8 mouse_packet_pos;
static int mouse_ready;
static char terminal_line[64];
static char terminal_output[64] = "TYPE HELP FOR COMMANDS";
static u32 terminal_len;
static char calc_line[32];
static u32 calc_len;
static char calc_result[32];
static int installer_mode;
static int text_mode;
static int installer_stage;
static int installer_image_valid;
static int installer_drive = -1;
static char installer_confirm[8];
static u32 installer_confirm_len;
static char installer_status[64] = "SELECT AN IDE DRIVE";
static const u8 *install_image;
static u32 install_image_size;
static u32 install_image_sectors;

struct ata_device {
    u16 io;
    u8 slave;
    u8 present;
    u32 sectors;
    char model[41];
};
static struct ata_device ata_devices[4];

static inline u8 inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static u32 min_u32(u32 a, u32 b) { return a < b ? a : b; }

static void draw_clock(void);
static void present_frame(void);
static void draw_context_menu(void);
static void draw_mouse_cursor(void);
static int rtc_read_hour_minute(u8 *hour, u8 *minute);
static int rtc_read_day_month_year(u8 *day, u8 *month, u16 *year);
static void outb_local(u16 p, u8 v);
static u8 inb_local(u16 p);
static void u32toa(u32 value, char *out);
static int vbe_apply_resolution(u32 width, u32 height);
static void draw_setup_screen(void);
static void draw_login_screen(void);
static void apply_theme(u32 id);
static void identify_cpu(void);
static int is_window_app(int app);
static void finish_setup(void);
static void try_login(void);
static void window_default_geometry(int app, int *x, int *y, int *w, int *h);
static void update_resolution_input(u32 w, u32 h);
static void serial_print(const char *s);

static void pixel(u32 x, u32 y, u32 color) {
    volatile u8 *p;
    if (!graphical || x >= fb.width || y >= fb.height) return;
    p = backbuffer_valid ? framebuffer_backbuffer + y * fb.pitch + x * (fb.bpp / 8u) : fb.address + y * fb.pitch + x * (fb.bpp / 8u);
    /* Multiboot RGB framebuffer memory is little-endian B,G,R,(X). */
    p[0] = (u8)(color & 0xFFu);
    p[1] = (u8)((color >> 8) & 0xFFu);
    p[2] = (u8)((color >> 16) & 0xFFu);
    if (fb.bpp == 32) p[3] = 0;
}

static void rectangle(u32 x, u32 y, u32 w, u32 h, u32 color) {
    u32 yy, xx;
    int sx = (int)x + draw_offset_x;
    int sy = (int)y + draw_offset_y;
    if (!graphical || w == 0u || h == 0u) return;
    if (sx < 0) { u32 cut = (u32)(-sx); if (cut >= w) return; w -= cut; sx = 0; }
    if (sy < 0) { u32 cut = (u32)(-sy); if (cut >= h) return; h -= cut; sy = 0; }
    x = (u32)sx; y = (u32)sy;
    if (x >= fb.width || y >= fb.height) return;
    w = min_u32(w, fb.width - x);
    h = min_u32(h, fb.height - y);
    for (yy = y; yy < y + h; ++yy)
        for (xx = x; xx < x + w; ++xx)
            pixel(xx, yy, color);
}

static void line(u32 x, u32 y, u32 w, u32 color) { rectangle(x, y, w, 1, color); }

/* Five-by-seven bitmap font for the desktop's uppercase labels and digits. */
static const u8 font[37][7] = {
 {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14}, {0,0,0,0,0,0,0}
};

static u8 glyph_row(char ch, u32 row) {
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    if (ch >= 'A' && ch <= 'Z') return font[(u32)(ch - 'A')][row];
    if (ch >= '0' && ch <= '9') return font[26u + (u32)(ch - '0')][row];
    switch (ch) {
        case '.': return row == 6 ? 4 : 0;
        case ',': return row == 6 ? 4 : (row == 5 ? 4 : 0);
        case ':': return (row == 2 || row == 5) ? 4 : 0;
        case ';': return (row == 2 || row == 5) ? 4 : (row == 6 ? 4 : 0);
        case '-': return row == 3 ? 14 : 0;
        case '_': return row == 6 ? 31 : 0;
        case '/': return (u8)(1u << (row < 5 ? 4u - row : 0u));
        case '\\': return (u8)(1u << (row < 5 ? row : 4u));
        case '+': return row == 3 ? 14 : (row >= 1 && row <= 5 ? 4 : 0);
        case '=': return (row == 2 || row == 4) ? 14 : 0;
        case '!': return row < 5 ? 4 : (row == 6 ? 4 : 0);
        case '?': { static const u8 q[7] = {14,17,1,2,4,0,4}; return q[row]; }
        case '(': return row < 2 ? 2 : (row < 5 ? 4 : 2);
        case ')': return row < 2 ? 8 : (row < 5 ? 4 : 8);
        case '[': return (row == 0 || row == 6) ? 14 : 8;
        case ']': return (row == 0 || row == 6) ? 14 : 2;
        case '<': return row == 3 ? 16 : (row == 2 || row == 4 ? 8 : 4);
        case '>': return row == 3 ? 1 : (row == 2 || row == 4 ? 2 : 4);
        case '*': return row == 3 ? 10 : (row == 2 || row == 4 ? 4 : 0);
        case '#': return (row == 2 || row == 5) ? 31 : ((row >= 1 && row <= 6) ? 10 : 0);
        case ' ': return 0;
        default: return 0;
    }
}

static void draw_char(u32 x, u32 y, char ch, u32 color, u32 scale) {
    u32 row, col;
    for (row = 0; row < 7; ++row) {
        u8 bits = glyph_row(ch, row);
        for (col = 0; col < 5; ++col) {
            if ((bits & (1u << (4u - col))) != 0)
                rectangle(x + col * scale, y + row * scale, scale, scale, color);
        }
    }
}

static void text(u32 x, u32 y, const char *s, u32 color, u32 scale) {
    while (*s) {
        draw_char(x, y, *s++, color, scale);
        x += 6u * scale;
        if (x + 5u * scale >= fb.width) break;
    }
}

static int str_eq(const char *a, const char *b) {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == 0 && *b == 0;
}

static int str_eq_ci(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 32);
        if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 32);
        if (ca != cb) return 0;
        ++a; ++b;
    }
    return *a == 0 && *b == 0;
}

static void draw_window(const char *title, u32 x, u32 y, u32 w, u32 h) {
    rectangle(x + 4, y + 5, w, h, 0x070B18u);
    rectangle(x, y, w, h, COLOR_PANEL);
    rectangle(x, y, w, 28, COLOR_ACCENT);
    text(x + 10, y + 9, title, 0x10172Fu, 1);
    rectangle(x + w - 23, y + 5, 17, 17, COLOR_RED);
    line(x + w - 19, y + 13, 9, COLOR_WHITE);
    rectangle(x, y + 28, w, 1, COLOR_CYAN);
}

static void desktop(void) {
    u32 y, x, i;
    static const char *names[APP_ICON_COUNT] = {
        "CALC", "CMD", "FILES", "DISK", "BROWSER", "PAINT", "NOTEPAD",
        "NETWORK", "SETTINGS", "WINVER", "ABOUT", "MONITOR", "DIAGNOSTICS",
        "CALENDAR", "HELP", "TASKMGR"
    };
    static const u8 apps[APP_ICON_COUNT] = {1,2,3,4,5,6,7,8,9,11,12,13,14,15,16,17};
    static const u32 colors[APP_ICON_COUNT] = {
        0x61D9F8u,0x9A8CFFu,0x62E6A7u,0xFFD479u,0x7EB6FFu,0xFF8FD8u,0xFFB86Bu,
        0x61D9F8u,0x9A8CFFu,0xFFC857u,0x62E6A7u,0x7EB6FFu,0xFF718Bu,
        0xA5E6D0u,0xFFD477u,0xB39DFFu
    };
    if (!graphical) return;
    draw_offset_x = draw_offset_y = 0;
    rectangle(0, 0, fb.width, fb.height, COLOR_BG);
    for (y = 0; y < fb.height; y += 4u) {
        u32 shade = (y * 0x000018u) / (fb.height ? fb.height : 1u);
        rectangle(0, y, fb.width, 4u, COLOR_BG + shade);
    }
    /* Subtle blossom arcs keep the left icon grid readable. */
    for (y = 20; y + 40u < fb.height - TASKBAR_H; y += 3u) {
        u32 offset = (y / 3u) % 130u;
        x = fb.width > 300u ? 330u + offset : 180u + offset / 2u;
        if (x + 130u < fb.width) rectangle(x, y, 130u, 1u, (y % 9u) ? 0x29375Du : 0x344773u);
    }
    /* Windows-like desktop: 32-pixel icon tiles and compact labels. */
    for (i = 0; i < (int)APP_ICON_COUNT; ++i) {
        u32 col = i % 3u, row = i / 3u;
        u32 left = 8u + col * 82u;
        u32 ix = left + 12u, iy = 34u + row * 65u;
        if (active_app == (int)apps[i]) rectangle(left, iy - 4u, 66u, 58u, 0x37496Fu);
        rectangle(ix + 2u, iy + 2u, 28u, 28u, 0x080D1Bu);
        rectangle(ix, iy, 28u, 28u, colors[i]);
        rectangle(ix, iy, 28u, 2u, COLOR_WHITE);
        draw_char(ix + 8u, iy + 7u, names[i][0], 0x10172Fu, 2u);
        text(left + 1u, iy + 34u, names[i], COLOR_WHITE, 1u);
    }
    /* Windows 7-sized taskbar with a start orb, launchers and notification clock. */
    rectangle(0, fb.height - TASKBAR_H, fb.width, TASKBAR_H, 0x111B31u);
    rectangle(0, fb.height - TASKBAR_H, fb.width, 2u, COLOR_ACCENT);
    rectangle(6u, fb.height - TASKBAR_H + 6u, 66u, 30u, COLOR_PANEL2);
    rectangle(8u, fb.height - TASKBAR_H + 8u, 26u, 26u, COLOR_ACCENT);
    draw_char(17u, fb.height - TASKBAR_H + 16u, 'S', 0x10172Fu, 1u);
    text(38u, fb.height - TASKBAR_H + 17u, "START", COLOR_WHITE, 1u);
    for (i = 0; i < APP_ICON_COUNT; ++i) {
        u32 bx = 78u + i * 28u, by = fb.height - TASKBAR_H + 7u;
        if (bx + 24u >= fb.width - 57u) break;
        rectangle(bx, by, 24u, 28u, active_app == (int)apps[i] ? COLOR_PANEL2 : 0x17233Eu);
        rectangle(bx, by, 24u, 2u, colors[i]);
        draw_char(bx + 9u, by + 10u, names[i][0], colors[i], 1u);
        if (active_app == (int)apps[i]) rectangle(bx + 2u, by + 26u, 20u, 2u, COLOR_ACCENT);
    }
    draw_clock();
}
static void app_header(const char *title, const char *hint) {
    u32 w = fb.width > 430 ? 390 : fb.width - 20;
    u32 x = (fb.width - w) / 2u;
    u32 h = fb.height > 390 ? 320 : fb.height - 70;
    u32 y = (fb.height - h) / 2u;
    draw_window(title, x, y, w, h);
    if (hint) text(x + 12, y + h - 21, hint, COLOR_MUTED, 1);
}

static void draw_calc(void) {
    app_header("CALCULATOR", "TYPE DIGITS + - * / THEN ENTER   ESC CLOSES");
    text(150, 118, "EXPRESSION:", COLOR_MUTED, 1);
    rectangle(145, 135, 350, 34, 0x0A1024u);
    text(155, 148, calc_line, COLOR_WHITE, 2);
    text(150, 194, "RESULT:", COLOR_MUTED, 1);
    text(150, 214, calc_result, COLOR_GREEN, 2);
    text(150, 270, "EXAMPLE: 12+30", COLOR_CYAN, 1);
}

static void draw_terminal(void) {
    app_header("SYSTEMC TERMINAL - C:\\SYSTEMC", "HELP / ABOUT / APPS / CLEAR / ENTER TO RUN");
    rectangle(145, 118, 350, 150, 0x050608u);
    text(155, 128, "SYSTEMC COMMAND PROCESSOR [I386]", COLOR_WHITE, 1);
    text(155, 146, "Copyright (c) 2023-2026 L4vender Project.", COLOR_WHITE, 1);
    text(155, 174, "C:\\SYSTEMC>", COLOR_GREEN, 1);
    text(221, 174, terminal_output, COLOR_WHITE, 1);
    text(155, 207, "C:\\SYSTEMC>", COLOR_GREEN, 1);
    text(221, 207, terminal_line, COLOR_WHITE, 1);
    text(155, 236, "TYPE HELP FOR AVAILABLE COMMANDS", COLOR_MUTED, 1);
}

static void draw_files(void) {
    app_header("FILE MANAGER", "ESC CLOSES");
    text(150, 116, "SYSTEMC VIRTUAL DIRECTORY", COLOR_CYAN, 1);
    text(150, 145, "[DIR] /", COLOR_WHITE, 1);
    text(170, 166, "[DIR] apps", COLOR_GREEN, 1);
    text(170, 187, "[DIR] system", COLOR_GREEN, 1);
    text(170, 208, "[FILE] README.TXT", COLOR_WHITE, 1);
    text(170, 229, "[FILE] WALLPAPER.PPM", COLOR_WHITE, 1);
    text(170, 250, "[FILE] SYSTEMC-I386.KRN", COLOR_WHITE, 1);
    text(150, 279, "VIRTUAL VIEW ONLY - NO DISK MOUNT", COLOR_YELLOW, 1);
}

static void draw_disk(void) {
    char ip[20];
    app_header("DISK UTILITY", "ESC CLOSES");
    text(150, 116, "STORAGE MODE: LEGACY IDE PIO", COLOR_CYAN, 1);
    text(150, 143, "INSTALLER: FROM ISO MENU", COLOR_WHITE, 1);
    text(150, 166, "TARGET SUPPORT: IDE PIO / LBA28", COLOR_GREEN, 1);
    text(150, 189, "NOT SUPPORTED: NVME / AHCI / USB", COLOR_YELLOW, 1);
    text(150, 225, "INSTALLING ERASES THE SELECTED DISK.", COLOR_RED, 1);
    if (install_image_size) text(150, 252, "INSTALL IMAGE IS INCLUDED", COLOR_GREEN, 1);
    else text(150, 252, "REBUILD ISO TO INCLUDE DISK IMAGE", COLOR_YELLOW, 1);
    (void)ip;
}

static void draw_network(void) {
    char mac_text[24], ip[20], gw[20], dns[20];
    app_header("NETWORK STATUS", "E1000 / DHCP / ESC CLOSES");
    net_get_mac(mac_text); net_get_ipv4(ip); net_get_gateway(gw); net_get_dns(dns);
    text(150, 116, net_has_driver() ? "INTEL E1000 DRIVER FOUND" : "NO SUPPORTED E1000 NIC FOUND", net_has_driver()?COLOR_GREEN:COLOR_RED, 1);
    text(150, 143, "MAC:", COLOR_MUTED, 1); text(205, 143, mac_text, COLOR_WHITE, 1);
    text(150, 168, "IP:", COLOR_MUTED, 1); text(205, 168, ip, COLOR_WHITE, 1);
    text(150, 193, "GATEWAY:", COLOR_MUTED, 1); text(225, 193, gw, COLOR_WHITE, 1);
    text(150, 218, "DNS:", COLOR_MUTED, 1); text(205, 218, dns, COLOR_WHITE, 1);
    text(150, 245, net_is_online() ? "DHCP ONLINE" : "WAITING FOR DHCP LEASE", net_is_online()?COLOR_GREEN:COLOR_YELLOW, 1);
    text(150, 275, "EXPERIMENTAL: E1000 AND BASIC IPV4/TCP", COLOR_MUTED, 1);
}

static void draw_settings(void) {
    int wx, wy, ww, wh;
    u32 i;
    static const char *themes[4] = {"LAVENDER", "CLASSIC BLUE", "SAGE GREEN", "HIGH CONTRAST"};
    static const u32 theme_colors[4] = {0xB39DFFu,0x4C9BFFu,0x78D3A4u,0xFFFFFFu};
    char active_w[12], active_h[12];
    window_default_geometry(9, &wx, &wy, &ww, &wh);
    draw_window("SETTINGS", (u32)wx, (u32)wy, (u32)ww, (u32)wh);
    rectangle((u32)wx, (u32)wy + 29u, 104u, (u32)wh - 29u, COLOR_PANEL2);
    text((u32)wx + 9u, (u32)wy + 46u, "PERSONALIZE", COLOR_MUTED, 1u);
    rectangle((u32)wx + 5u, (u32)wy + 62u, 94u, 28u, settings_page == 0 ? COLOR_ACCENT : COLOR_PANEL2);
    text((u32)wx + 14u, (u32)wy + 72u, "COLORS", settings_page == 0 ? 0x10172Fu : COLOR_WHITE, 1u);
    rectangle((u32)wx + 5u, (u32)wy + 95u, 94u, 38u, settings_page == 1 ? COLOR_ACCENT : COLOR_PANEL2);
    text((u32)wx + 14u, (u32)wy + 103u, "SCREEN", settings_page == 1 ? 0x10172Fu : COLOR_WHITE, 1u);
    text((u32)wx + 14u, (u32)wy + 115u, "RESOLUTION", settings_page == 1 ? 0x10172Fu : COLOR_WHITE, 1u);
    if (settings_page == 0) {
        text((u32)wx + 119u, (u32)wy + 48u, "COLORS AND APPEARANCE", COLOR_CYAN, 1u);
        text((u32)wx + 119u, (u32)wy + 70u, "CHOOSE A DESKTOP THEME", COLOR_WHITE, 1u);
        for (i = 0; i < 4u; ++i) {
            u32 by = (u32)wy + 92u + i * 39u;
            rectangle((u32)wx + 120u, by, (u32)ww - 137u, 31u, theme_id == (int)i ? COLOR_PANEL2 : 0x0C1326u);
            rectangle((u32)wx + 129u, by + 6u, 20u, 19u, theme_colors[i]);
            text((u32)wx + 158u, by + 12u, themes[i], COLOR_WHITE, 1u);
            if (theme_id == (int)i) text((u32)wx + (u32)ww - 34u, by + 12u, "*", COLOR_GREEN, 1u);
        }
        text((u32)wx + 119u, (u32)wy + (u32)wh - 37u, "CHANGES APPLY IMMEDIATELY", COLOR_MUTED, 1u);
    } else {
        u32toa(fb.width, active_w); u32toa(fb.height, active_h);
        text((u32)wx + 119u, (u32)wy + 48u, "SCREEN RESOLUTION", COLOR_CYAN, 1u);
        text((u32)wx + 119u, (u32)wy + 74u, "WIDTH HEIGHT", COLOR_WHITE, 1u);
        rectangle((u32)wx + 119u, (u32)wy + 89u, (u32)ww - 137u, 30u, 0x070B18u);
        text((u32)wx + 128u, (u32)wy + 100u, resolution_input, COLOR_WHITE, 1u);
        rectangle((u32)wx + 119u, (u32)wy + 127u, 82u, 27u, COLOR_ACCENT);
        text((u32)wx + 134u, (u32)wy + 137u, "APPLY NOW", 0x10172Fu, 1u);
        text((u32)wx + 119u, (u32)wy + 177u, "CURRENT MODE:", COLOR_MUTED, 1u);
        text((u32)wx + 218u, (u32)wy + 177u, active_w, COLOR_WHITE, 1u);
        text((u32)wx + 248u, (u32)wy + 177u, "X", COLOR_WHITE, 1u);
        text((u32)wx + 258u, (u32)wy + 177u, active_h, COLOR_WHITE, 1u);
        text((u32)wx + 119u, (u32)wy + 205u, resolution_status, COLOR_GREEN, 1u);
        text((u32)wx + 119u, (u32)wy + (u32)wh - 45u, "NEEDS LEGACY BIOS VBE SUPPORT", COLOR_MUTED, 1u);
    }
}

static void draw_winver(void) {
    char ram[16];
    app_header("WINVER - ABOUT SYSTEMC", "SYSTEM INFORMATION");
    u32toa(system_ram_mb, ram);
    text(145u, 116u, "SYSTEMC VERSION 3.0", COLOR_CYAN, 1u);
    text(145u, 140u, "CODENAME: BL0SS0M MOD.0", COLOR_WHITE, 1u);
    text(145u, 162u, "PROJECT: L4VENDER", COLOR_WHITE, 1u);
    text(145u, 190u, "COPYRIGHT (C) 2023-2026 L4VENDER PROJECT", COLOR_MUTED, 1u);
    text(145u, 207u, "FREESTANDING I386 EDITION", COLOR_MUTED, 1u);
    text(145u, 242u, "RAM: APPROX", COLOR_WHITE, 1u);
    text(225u, 242u, ram, COLOR_GREEN, 1u);
    text(145u, 268u, "USER:", COLOR_MUTED, 1u);
    text(190u, 268u, setup_username[0] ? setup_username : "LOCAL USER", COLOR_WHITE, 1u);
    text(145u, 289u, "MACHINE:", COLOR_MUTED, 1u);
    text(205u, 289u, machine_name[0] ? machine_name : "SYSTEMC-PC", COLOR_WHITE, 1u);
}

static void draw_about(void) {
    char ram[16], width[12], height[12];
    app_header("ABOUT SYSTEMC", "BL0SS0M MOD.0 / PROJECT L4VENDER");
    u32toa(system_ram_mb, ram); u32toa(fb.width, width); u32toa(fb.height, height);
    text(145u, 116u, "OPERATING SYSTEM: SYSTEMC", COLOR_CYAN, 1u);
    text(145u, 139u, "KERNEL: FREESTANDING C + X86 ASM", COLOR_WHITE, 1u);
    text(145u, 163u, "ARCHITECTURE: I386 PROTECTED MODE", COLOR_WHITE, 1u);
    text(145u, 191u, "CPU VENDOR:", COLOR_MUTED, 1u); text(235u, 191u, cpu_vendor, COLOR_WHITE, 1u);
    text(145u, 214u, "CPU:", COLOR_MUTED, 1u); text(180u, 214u, cpu_brand, COLOR_WHITE, 1u);
    text(145u, 239u, "RAM MB:", COLOR_MUTED, 1u); text(205u, 239u, ram, COLOR_WHITE, 1u);
    text(145u, 263u, "DISPLAY:", COLOR_MUTED, 1u); text(205u, 263u, width, COLOR_WHITE, 1u);
    text(238u, 263u, "X", COLOR_WHITE, 1u); text(250u, 263u, height, COLOR_WHITE, 1u);
    text(145u, 288u, "BOOT: LEGACY BIOS / GRUB", COLOR_GREEN, 1u);
}

static void draw_system_monitor(void) {
    char ram[16], width[12], height[12], mac[24], ip[20];
    app_header("SYSTEM MONITOR", "LIVE KERNEL AND DEVICE STATUS");
    u32toa(system_ram_mb, ram); u32toa(fb.width, width); u32toa(fb.height, height);
    net_get_mac(mac); net_get_ipv4(ip);
    text(145u, 116u, "CPU", COLOR_CYAN, 1u); text(188u, 116u, cpu_vendor, COLOR_WHITE, 1u);
    text(145u, 139u, cpu_brand, COLOR_WHITE, 1u);
    text(145u, 169u, "MEMORY MB:", COLOR_MUTED, 1u); text(225u, 169u, ram, COLOR_GREEN, 1u);
    text(145u, 193u, "FRAMEBUFFER:", COLOR_MUTED, 1u); text(235u, 193u, width, COLOR_WHITE, 1u);
    text(268u, 193u, "X", COLOR_WHITE, 1u); text(280u, 193u, height, COLOR_WHITE, 1u);
    text(145u, 220u, "NETWORK:", COLOR_MUTED, 1u); text(210u, 220u, net_is_online() ? "ONLINE" : "OFFLINE", net_is_online()?COLOR_GREEN:COLOR_YELLOW, 1u);
    text(145u, 245u, "IP:", COLOR_MUTED, 1u); text(175u, 245u, ip, COLOR_WHITE, 1u);
    text(145u, 270u, "MAC:", COLOR_MUTED, 1u); text(185u, 270u, mac, COLOR_WHITE, 1u);
}

static void draw_diagnostics(void) {
    app_header("HARDWARE DIAGNOSTICS", "READ-ONLY CHECKS");
    text(145u, 116u, "CPU IDENTIFICATION AVAILABLE", COLOR_GREEN, 1u);
    text(145u, 139u, "I386 KERNEL / MULTIBOOT V1", COLOR_WHITE, 1u);
    text(145u, 162u, graphical ? "GRAPHICS FRAMEBUFFER OK" : "TEXT MODE FALLBACK", graphical?COLOR_GREEN:COLOR_YELLOW, 1u);
    text(145u, 185u, mouse_ready ? "PS/2 MOUSE INITIALIZED" : "PS/2 MOUSE NOT DETECTED", mouse_ready?COLOR_GREEN:COLOR_YELLOW, 1u);
    text(145u, 208u, net_has_driver() ? "INTEL E1000 DRIVER READY" : "INTEL E1000 NOT FOUND", net_has_driver()?COLOR_GREEN:COLOR_YELLOW, 1u);
    text(145u, 231u, net_is_online() ? "DHCP LEASE ACQUIRED" : "NO DHCP LEASE", net_is_online()?COLOR_GREEN:COLOR_YELLOW, 1u);
    text(145u, 265u, "NO HARDWARE HAS BEEN MODIFIED", COLOR_MUTED, 1u);
}
static void draw_calendar(void) {
    u8 day = 1u, month = 1u;
    u16 year = 2026u;
    char d[12], m[12], y[12];
    app_header("L4VENDER CALENDAR", "LOCAL RTC DATE / SIMPLE AGENDA");
    (void)rtc_read_day_month_year(&day, &month, &year);
    u32toa((u32)day, d); u32toa((u32)month, m); u32toa((u32)year, y);
    text(145u, 116u, "TODAY (DD / MM / YYYY)", COLOR_MUTED, 1u);
    text(145u, 140u, d, COLOR_CYAN, 2u); text(175u, 140u, "/", COLOR_WHITE, 2u);
    text(190u, 140u, m, COLOR_CYAN, 2u); text(220u, 140u, "/", COLOR_WHITE, 2u);
    text(235u, 140u, y, COLOR_CYAN, 2u);
    text(145u, 193u, "AGENDA", COLOR_WHITE, 1u);
    rectangle(145u, 211u, 300u, 38u, COLOR_PANEL2);
    text(155u, 226u, "NO EVENTS SAVED", COLOR_MUTED, 1u);
    text(145u, 266u, "RTC DATE MAY DEPEND ON VM SETTINGS", COLOR_YELLOW, 1u);
}
static void draw_help_center(void) {
    app_header("BL0SS0M HELP CENTER", "QUICK START / KEYBOARD AND MOUSE");
    text(145u, 116u, "DESKTOP: DOUBLE-CLICK ICON TO OPEN", COLOR_WHITE, 1u);
    text(145u, 140u, "WINDOWS: DRAG TITLE BAR / X TO CLOSE", COLOR_WHITE, 1u);
    text(145u, 164u, "RIGHT CLICK: CONTEXT MENU", COLOR_WHITE, 1u);
    text(145u, 188u, "PAINT: SELECT TOOL, COLOR, DRAW", COLOR_WHITE, 1u);
    text(145u, 212u, "CMD: HELP / APPS / VER / ABOUT", COLOR_CYAN, 1u);
    text(145u, 236u, "ITWLMCLI INSTALL NETWORK AUTO", COLOR_CYAN, 1u);
    text(145u, 260u, "SYSTEMCSETUP-CLI --RESOLUTION W H", COLOR_CYAN, 1u);
    text(145u, 286u, "HTTP ONLY; HTTPS/TLS IS NOT BUILT IN", COLOR_YELLOW, 1u);
}
static void draw_task_manager(void) {
    char ram[12], width[12], height[12];
    app_header("TASK MANAGER", "BUILT-IN COMPONENT STATUS");
    u32toa(system_ram_mb, ram); u32toa(fb.width, width); u32toa(fb.height, height);
    text(145u, 116u, "SYSTEMC KERNEL", COLOR_CYAN, 1u); text(330u, 116u, "RUNNING", COLOR_GREEN, 1u);
    text(145u, 140u, "GUI / FRAMEBUFFER", COLOR_WHITE, 1u); text(330u, 140u, "RUNNING", COLOR_GREEN, 1u);
    text(145u, 164u, "KEYBOARD / MOUSE", COLOR_WHITE, 1u); text(330u, 164u, mouse_ready ? "READY" : "CHECK DEVICE", mouse_ready ? COLOR_GREEN : COLOR_YELLOW, 1u);
    text(145u, 188u, "E1000 NETWORK", COLOR_WHITE, 1u); text(330u, 188u, net_has_driver() ? "DRIVER FOUND" : "NOT FOUND", net_has_driver()?COLOR_GREEN:COLOR_YELLOW, 1u);
    text(145u, 212u, "NETWORK LEASE", COLOR_WHITE, 1u); text(330u, 212u, net_is_online() ? "ONLINE" : "OFFLINE", net_is_online()?COLOR_GREEN:COLOR_YELLOW, 1u);
    text(145u, 246u, "RAM MB:", COLOR_MUTED, 1u); text(210u, 246u, ram, COLOR_WHITE, 1u);
    text(145u, 269u, "FRAMEBUFFER:", COLOR_MUTED, 1u); text(235u, 269u, width, COLOR_WHITE, 1u); text(265u, 269u, "X", COLOR_WHITE, 1u); text(277u, 269u, height, COLOR_WHITE, 1u);
}
static void draw_browser(void) {
    u32 i, x, y; u32 boxw = fb.width > 430 ? 390u : fb.width - 20u;
    app_header("SYSTEMC BROWSER", "TYPE URL AND ENTER - HTTP ONLY");
    x=(fb.width-boxw)/2u; y=112u;
    rectangle(x+8,y,boxw-16,28,0x070B18u);
    text(x+14,y+9,browser_url,COLOR_WHITE,1);
    text(x+12,y+45,browser_status,net_is_online()?COLOR_GREEN:COLOR_YELLOW,1);
    rectangle(x+8,y+66,boxw-16,136,0xF4F6FFu);
    for(i=0;i<browser_text_len && i<1000u;i++) {
        u32 line_no=i/58u, col=i%58u;
        if(line_no>=7u) break;
        x=(fb.width-boxw)/2u+14u+col*6u;
        y=183u+line_no*17u;
        if(browser_text[i]=='\n') continue;
        draw_char(x,y,browser_text[i],0x10172Fu,1);
    }
    text((fb.width-boxw)/2u+12u, 326u, "HTTPS/TLS IS NOT IMPLEMENTED; HTTP PAGES ONLY", COLOR_RED, 1);
    text((fb.width-boxw)/2u+12u, 345u, "TRY http://neverssl.com/", COLOR_MUTED, 1);
}

static void paint_draw_line(int x0, int y0, int x1, int y1, u32 color) {
    int dx=x1>x0?x1-x0:x0-x1, sx=x0<x1?1:-1;
    int dy=y1>y0?y0-y1:y1-y0, sy=y0<y1?1:-1;
    int err=dx+dy;
    for(;;){ if(x0>=0&&x0<(int)PAINT_W&&y0>=0&&y0<(int)PAINT_H)paint_pixels[(u32)y0*PAINT_W+(u32)x0]=color; if(x0==x1&&y0==y1)break; {int e2=2*err;if(e2>=dy){err+=dy;x0+=sx;}if(e2<=dx){err+=dx;y0+=sy;}} }
}

static void paint_rectangle(int x0,int y0,int x1,int y1,u32 color) {
    int x,y;if(x0>x1){int t=x0;x0=x1;x1=t;}if(y0>y1){int t=y0;y0=y1;y1=t;}
    for(x=x0;x<=x1;x++){if(x>=0&&x<(int)PAINT_W){if(y0>=0&&y0<(int)PAINT_H)paint_pixels[(u32)y0*PAINT_W+(u32)x]=color;if(y1>=0&&y1<(int)PAINT_H)paint_pixels[(u32)y1*PAINT_W+(u32)x]=color;}}
    for(y=y0;y<=y1;y++){if(y>=0&&y<(int)PAINT_H){if(x0>=0&&x0<(int)PAINT_W)paint_pixels[(u32)y*PAINT_W+(u32)x0]=color;if(x1>=0&&x1<(int)PAINT_W)paint_pixels[(u32)y*PAINT_W+(u32)x1]=color;}}
}

static void paint_fill(int sx, int sy, u32 color) {
    u32 front=0, back=0;u32 old;if(sx<0||sy<0||sx>=(int)PAINT_W||sy>=(int)PAINT_H)return;old=paint_pixels[(u32)sy*PAINT_W+(u32)sx];if(old==color)return;paint_pixels[(u32)sy*PAINT_W+(u32)sx]=color;paint_queue[back++]=(u16)((u32)sy*PAINT_W+(u32)sx);
    while(front<back){u32 idx=paint_queue[front++];int x=(int)(idx%PAINT_W),y=(int)(idx/PAINT_W);u32 n;
        if(x>0){n=idx-1u;if(paint_pixels[n]==old&&back<PAINT_W*PAINT_H){paint_pixels[n]=color;paint_queue[back++]=(u16)n;}}
        if(x+1<(int)PAINT_W){n=idx+1u;if(paint_pixels[n]==old&&back<PAINT_W*PAINT_H){paint_pixels[n]=color;paint_queue[back++]=(u16)n;}}
        if(y>0){n=idx-PAINT_W;if(paint_pixels[n]==old&&back<PAINT_W*PAINT_H){paint_pixels[n]=color;paint_queue[back++]=(u16)n;}}
        if(y+1<(int)PAINT_H){n=idx+PAINT_W;if(paint_pixels[n]==old&&back<PAINT_W*PAINT_H){paint_pixels[n]=color;paint_queue[back++]=(u16)n;}}
    }
}

static void draw_paint(void) {
    u32 i,x,y; u32 wx=20,wy=40,ww=570,wh=390;u32 cx=80,cy=151;
    draw_window("PAINT STUDIO",wx,wy,ww,wh);
    /* Toolbar buttons: Pencil, line, rectangle, eraser, fill, clear. */
    { static const char *labels[6]={"PEN","LINE","RECT","ERASE","FILL","CLEAR"};
      for(i=0;i<6;i++){u32 bx=32u+i*77u;u32 col=(paint_tool==(int)i)?COLOR_ACCENT:COLOR_PANEL2;rectangle(bx,91,68,22,col);text(bx+8,99,labels[i],COLOR_WHITE,1);} }
    /* Color palette. */
    { static const u32 palette[8]={0x10172Fu,0xF4F6FFu,0xFF6B83u,0xFFD479u,0x62E6A7u,0x61D9F8u,0x9A8CFFu,0xFF8FD8u};
      for(i=0;i<8;i++){rectangle(34u+i*30u,119,23,19,palette[i]);if(paint_colour==palette[i]){rectangle(33u+i*30u,118,25,1,COLOR_WHITE);rectangle(33u+i*30u,139,25,1,COLOR_WHITE);}} }
    rectangle(cx-3,cy-3,PAINT_W*PAINT_SCALE+6,PAINT_H*PAINT_SCALE+6,COLOR_CYAN);
    rectangle(cx,cy,PAINT_W*PAINT_SCALE,PAINT_H*PAINT_SCALE,paint_background);
    for(y=0;y<PAINT_H;y++)for(x=0;x<PAINT_W;x++)if(paint_pixels[y*PAINT_W+x]!=paint_background)rectangle(cx+x*PAINT_SCALE,cy+y*PAINT_SCALE,PAINT_SCALE,PAINT_SCALE,paint_pixels[y*PAINT_W+x]);
    text(35,390,"LEFT MOUSE DRAW / SELECT TOOL",COLOR_WHITE,1);
    text(35,408,"PEN LINE RECT ERASE FILL CLEAR",COLOR_MUTED,1);
}

static void draw_notepad(void) {
    u32 i,line_no=0,col=0,start=0;u32 bx=98,by=120;
    app_header("NOTEPAD", "TYPE TEXT / ENTER NEW LINE / BACKSPACE DELETE / ESC CLOSE");
    rectangle(bx,by,fb.width>430?350:fb.width-120,190,0xF4F6FFu);
    for(i=notepad_scroll;i<notepad_len&&line_no<10;i++){
        char c=notepad_buffer[i];
        if(c=='\n'||col>=48){line_no++;col=0;start=i+1;if(c=='\n')continue;}
        if(line_no>=10)break;
        if(c!='\r'){draw_char(bx+8u+col*6u,by+8u+line_no*17u,c,0x10172Fu,1);col++;}
    }
    if(line_no<10)rectangle(bx+8u+col*6u,by+7u+line_no*17u,5,9,0x9A8CFFu);
    text(105,322,"DOCUMENT STORED IN RAM FOR THIS SESSION",COLOR_YELLOW,1);
    text(105,341,"DISK SAVE NEEDS A FILESYSTEM; NOT IN THIS BUILD",COLOR_MUTED,1);
    (void)start;
}

static void render_installer_notice(void);

static void present_frame(void) {
    u32 i, bytes;
    if (!graphical || !backbuffer_valid) return;
    bytes = fb.pitch * fb.height;
    for (i = 0; i < bytes; ++i) fb.address[i] = framebuffer_backbuffer[i];
}

static void draw_context_menu(void) {
    static const char *items[4] = {"OPEN", "SETTINGS", "REFRESH DESKTOP", "TERMINAL"};
    u32 i, x, y;
    if (!context_menu_open) return;
    x = (u32)context_menu_x; y = (u32)context_menu_y;
    rectangle(x + 3u, y + 3u, 160u, 94u, 0x050608u);
    rectangle(x, y, 160u, 94u, 0xE8ECF5u);
    rectangle(x, y, 160u, 2u, COLOR_ACCENT);
    for (i = 0; i < 4u; ++i) {
        u32 yy = y + 5u + i * 22u;
        if (i == 1u) rectangle(x + 3u, yy - 2u, 154u, 21u, 0xD0D8ECu);
        text(x + 9u, yy + 3u, items[i], 0x10172Fu, 1);
        if (i != 3u) rectangle(x + 6u, yy + 20u, 148u, 1u, 0xC5CCDDu);
    }
}

static void draw_start_menu(void) {
    static const char *items[9] = {"TERMINAL", "SETTINGS", "WINVER", "ABOUT", "SYSTEM MONITOR", "CALENDAR", "HELP CENTER", "TASK MANAGER", "LOCK SESSION"};
    u32 i, y = fb.height > 315u ? fb.height - TASKBAR_H - 260u : 26u;
    rectangle(8u, y + 4u, 224u, 260u, 0x05070Du);
    rectangle(5u, y, 224u, 260u, 0xE8ECF5u);
    rectangle(5u, y, 224u, 28u, COLOR_ACCENT);
    text(16u, y + 10u, "BL0SS0M MOD.0", 0x10172Fu, 1u);
    for (i = 0; i < 9u; ++i) {
        u32 yy = y + 34u + i * 24u;
        rectangle(10u, yy, 214u, 22u, (i & 1u) ? 0xE0E6F2u : 0xF4F6FBu);
        text(20u, yy + 7u, items[i], 0x10172Fu, 1u);
    }
}

static void draw_setup_screen(void) {
    u32 w = fb.width > 460u ? 440u : fb.width - 20u;
    u32 h = fb.height > 390u ? 348u : fb.height - 48u;
    u32 x = (fb.width - w) / 2u, y = (fb.height - h) / 2u;
    char masked[32]; u32 i;
    rectangle(0, 0, fb.width, fb.height, 0x10172Fu);
    rectangle(x + 5u, y + 6u, w, h, 0x05070Du);
    rectangle(x, y, w, h, 0x1C2948u);
    rectangle(x, y, w, 38u, COLOR_ACCENT);
    text(x + 14u, y + 13u, "SYSTEMC 3.0 - FIRST BOOT SETUP", 0x10172Fu, 1u);
    text(x + 18u, y + 53u, "WELCOME TO BL0SS0M MOD.0", COLOR_WHITE, 1u);
    text(x + 18u, y + 73u, "CREATE YOUR LOCAL SESSION PROFILE", COLOR_MUTED, 1u);
    text(x + 18u, y + 100u, "USERNAME", setup_field == 0 ? COLOR_CYAN : COLOR_WHITE, 1u);
    rectangle(x + 18u, y + 113u, w - 36u, 23u, setup_field == 0 ? 0x2C3B62u : 0x0B1225u);
    text(x + 26u, y + 121u, setup_username, COLOR_WHITE, 1u);
    text(x + 18u, y + 143u, "PASSWORD", setup_field == 1 ? COLOR_CYAN : COLOR_WHITE, 1u);
    rectangle(x + 18u, y + 156u, w - 36u, 23u, setup_field == 1 ? 0x2C3B62u : 0x0B1225u);
    for (i = 0; i < setup_password_len && i < sizeof(masked)-1u; ++i) masked[i] = '*';
    masked[i] = 0;
    text(x + 26u, y + 164u, masked, COLOR_WHITE, 1u);
    text(x + 18u, y + 186u, "MACHINE NAME", setup_field == 2 ? COLOR_CYAN : COLOR_WHITE, 1u);
    rectangle(x + 18u, y + 199u, w - 36u, 23u, setup_field == 2 ? 0x2C3B62u : 0x0B1225u);
    text(x + 26u, y + 207u, machine_name, COLOR_WHITE, 1u);
    rectangle(x + w - 124u, y + h - 52u, 106u, 28u, COLOR_ACCENT);
    text(x + w - 105u, y + h - 42u, "CONTINUE", 0x10172Fu, 1u);
    text(x + 18u, y + h - 43u, setup_status, COLOR_YELLOW, 1u);
    text(x + 18u, y + h - 18u, "TAB / MOUSE SELECTS FIELD - ENTER MOVES ON", COLOR_MUTED, 1u);
}

static void draw_login_screen(void) {
    u32 w = fb.width > 400u ? 360u : fb.width - 20u;
    u32 h = fb.height > 330u ? 268u : fb.height - 48u;
    u32 x = (fb.width - w) / 2u, y = (fb.height - h) / 2u;
    char masked[32]; u32 i;
    rectangle(0, 0, fb.width, fb.height, 0x111B31u);
    rectangle(x + 5u, y + 6u, w, h, 0x05070Du);
    rectangle(x, y, w, h, 0x1C2948u);
    rectangle(x, y, w, 38u, COLOR_ACCENT);
    text(x + 14u, y + 13u, "SYSTEMC 3.0 - SIGN IN", 0x10172Fu, 1u);
    text(x + 22u, y + 59u, "WELCOME BACK", COLOR_WHITE, 1u);
    text(x + 22u, y + 82u, setup_username, COLOR_CYAN, 2u);
    text(x + 22u, y + 120u, "PASSWORD", COLOR_WHITE, 1u);
    rectangle(x + 22u, y + 134u, w - 44u, 27u, 0x0B1225u);
    for (i = 0; i < login_password_len && i < sizeof(masked)-1u; ++i) masked[i] = '*';
    masked[i] = 0;
    text(x + 30u, y + 143u, masked, COLOR_WHITE, 1u);
    rectangle(x + w - 122u, y + h - 56u, 100u, 28u, COLOR_ACCENT);
    text(x + w - 103u, y + h - 46u, "SIGN IN", 0x10172Fu, 1u);
    text(x + 22u, y + h - 46u, login_status, COLOR_YELLOW, 1u);
    text(x + 22u, y + h - 20u, machine_name, COLOR_MUTED, 1u);
}

static void render_app(void) {
    if (text_mode) return;
    draw_offset_x = draw_offset_y = 0;
    if (screen_stage == 0) {
        draw_setup_screen();
        if (mouse_ready) draw_mouse_cursor();
        present_frame();
        return;
    }
    if (screen_stage == 1) {
        draw_login_screen();
        if (mouse_ready) draw_mouse_cursor();
        present_frame();
        return;
    }
    desktop();
    if (is_window_app(active_app)) {
        draw_offset_x = window_dx[active_app];
        draw_offset_y = window_dy[active_app];
    }
    switch (active_app) {
        case 1: draw_calc(); break;
        case 2: draw_terminal(); break;
        case 3: draw_files(); break;
        case 4: draw_disk(); break;
        case 5: draw_browser(); break;
        case 6: draw_paint(); break;
        case 7: draw_notepad(); break;
        case 8: draw_network(); break;
        case 9: draw_settings(); break;
        case 10: draw_offset_x = draw_offset_y = 0; render_installer_notice(); break;
        case 11: draw_winver(); break;
        case 12: draw_about(); break;
        case 13: draw_system_monitor(); break;
        case 14: draw_diagnostics(); break;
        case 15: draw_calendar(); break;
        case 16: draw_help_center(); break;
        case 17: draw_task_manager(); break;
        default: break;
    }
    draw_offset_x = draw_offset_y = 0;
    draw_context_menu();
    if (start_menu_open) draw_start_menu();
    if (mouse_ready) draw_mouse_cursor();
    present_frame();
}
static void text_mode_message(const char *a, const char *b, const char *c) {
    volatile u16 *vga = (volatile u16 *)0xB8000;
    u32 i = 0;
    const char *lines[3] = {a, b, c};
    u32 row;
    for (i = 0; i < 80u * 25u; ++i) vga[i] = (u16)0x1F20u;
    for (row = 0; row < 3; ++row) {
        const char *s = lines[row];
        i = 0;
        while (s[i] && i < 78) {
            vga[(2u + row * 2u) * 80u + 1u + i] = (u16)(0x1Fu << 8) | (u8)s[i];
            ++i;
        }
    }
}

static char scancode_char(u8 s, int shifted) {
    static const char map[128] = {
        [0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',[0x07]='6',[0x08]='7',[0x09]='8',[0x0A]='9',[0x0B]='0',
        [0x0C]='-',[0x0D]='=',[0x10]='q',[0x11]='w',[0x12]='e',[0x13]='r',[0x14]='t',[0x15]='y',[0x16]='u',[0x17]='i',[0x18]='o',[0x19]='p',
        [0x1E]='a',[0x1F]='s',[0x20]='d',[0x21]='f',[0x22]='g',[0x23]='h',[0x24]='j',[0x25]='k',[0x26]='l',[0x27]=';',[0x28]='\'',
        [0x2C]='z',[0x2D]='x',[0x2E]='c',[0x2F]='v',[0x30]='b',[0x31]='n',[0x32]='m',[0x33]=',',[0x34]='.',[0x35]='/',[0x39]=' '
    };
    char c = s < 128 ? map[s] : 0;
    if (!shifted || c == 0) return c;
    if (c >= 'a' && c <= 'z') return (char)(c - 'a' + 'A');
    if (c >= '1' && c <= '9') {
        static const char shifted_digits[10] = {')','!','@','#','$','%','^','&','*','('};
        return shifted_digits[c - '0'];
    }
    if (c == '0') return ')';
    if (c == '-') return '_';
    if (c == '=') return '+';
    if (c == ';') return ':';
    if (c == '\'') return '\"';
    if (c == ',') return '<';
    if (c == '.') return '>';
    if (c == '/') return '?';
    if (c == '[') return '{';
    if (c == ']') return '}';
    if (c == '`') return '~';
    if (c == '\\') return '|';
    return c;
}

static int next_key(void) {
    static int extended = 0;
    static int shift_down = 0;
    u8 s;
    if ((inb(0x64) & 1u) == 0) return -1;
    if ((inb(0x64) & 0x20u) != 0) return -1;
    s = inb(0x60);
    if (s == 0x2A || s == 0x36) { shift_down = 1; return -1; }
    if (s == 0xAA || s == 0xB6) { shift_down = 0; return -1; }
    if (s == 0xE0) { extended = 1; return -1; }
    if ((s & 0x80u) != 0) { extended = 0; return -1; }
    if (extended) {
        extended = 0;
        if (s == 0x48) return 0x100; /* up */
        if (s == 0x50) return 0x101; /* down */
        if (s == 0x4B) return 0x102; /* left */
        if (s == 0x4D) return 0x103; /* right */
        return -1;
    }
    if (s == 0x01) return 27;
    if (s == 0x1C) return 13;
    if (s == 0x0E) return 8;
    if (s == 0x3B) return 0x110; /* F1 */
    return (int)(u8)scancode_char(s, shift_down);
}

static void calculate(void) {
    long left = 0, right = 0, value;
    u32 i = 0;
    char op = 0;
    int sign = 1;
    if (calc_line[0] == '-') { sign = -1; ++i; }
    while (calc_line[i] >= '0' && calc_line[i] <= '9') { left = left * 10 + calc_line[i] - '0'; ++i; }
    left *= sign;
    op = calc_line[i++];
    while (calc_line[i] >= '0' && calc_line[i] <= '9') { right = right * 10 + calc_line[i] - '0'; ++i; }
    if (i != calc_len || op == 0 || right > 2147483647L || left > 2147483647L || left < -2147483647L) {
        calc_result[0] = 'E'; calc_result[1] = 'R'; calc_result[2] = 'R'; calc_result[3] = 0; return;
    }
    if (op == '+') value = left + right;
    else if (op == '-') value = left - right;
    else if (op == '*') value = left * right;
    else if (op == '/' && right != 0) value = left / right;
    else { calc_result[0] = 'E'; calc_result[1] = 'R'; calc_result[2] = 'R'; calc_result[3] = 0; return; }
    {
        char temp[24]; u32 n = 0, out = 0;
        if (value < 0) { calc_result[out++] = '-'; value = -value; }
        do { temp[n++] = (char)('0' + value % 10); value /= 10; } while (value && n < sizeof(temp));
        while (n) calc_result[out++] = temp[--n];
        calc_result[out] = 0;
    }
}

static int begins_ci(const char *value, const char *prefix) {
    while (*prefix) {
        char a = *value++, b = *prefix++;
        if (a >= 'a' && a <= 'z') a = (char)(a - 32);
        if (b >= 'a' && b <= 'z') b = (char)(b - 32);
        if (a != b) return 0;
    }
    return 1;
}

static void set_terminal_output(const char *message) {
    u32 i = 0;
    while (message[i] && i < sizeof(terminal_output) - 1u) { terminal_output[i] = message[i]; ++i; }
    terminal_output[i] = 0;
}

static int parse_resolution_command(const char *cmd, u32 *w, u32 *h) {
    const char *p = cmd + 29; /* "systemcsetup-cli --resolution" */
    u32 a = 0, b = 0;
    if (*p != ' ' && *p != '\t') return 0;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p < '0' || *p > '9') return 0;
    while (*p >= '0' && *p <= '9') { a = a * 10u + (u32)(*p++ - '0'); if (a > 10000u) return 0; }
    while (*p == ' ' || *p == '\t') ++p;
    if (*p < '0' || *p > '9') return 0;
    while (*p >= '0' && *p <= '9') { b = b * 10u + (u32)(*p++ - '0'); if (b > 10000u) return 0; }
    while (*p == ' ' || *p == '\t') ++p;
    if (*p || a < 320u || a > 1920u || b < 240u || b > 1080u) return 0;
    *w = a; *h = b; return 1;
}

static void set_resolution_status(const char *message) {
    u32 i = 0;
    while (message[i] && i < sizeof(resolution_status) - 1u) { resolution_status[i] = message[i]; ++i; }
    resolution_status[i] = 0;
}

struct sc_vbe_result { u32 width, height, pitch, bpp, framebuffer; };
extern int vbe_set_mode(u32 width, u32 height, struct sc_vbe_result *out);
static int vbe_apply_resolution(u32 width, u32 height) {
    struct sc_vbe_result result;
    if(width<320u||width>1920u||height<240u||height>1080u)return 0;
    if(!vbe_set_mode(width,height,&result))return 0;
    if(result.width!=width||result.height!=height||(result.bpp!=24u&&result.bpp!=32u)||result.framebuffer==0u||result.pitch<width*(result.bpp/8u)||result.pitch>BACKBUFFER_MAX_PITCH||result.height>BACKBUFFER_MAX_HEIGHT)return 0;
    fb.address=(volatile u8*)result.framebuffer;
    fb.width=result.width;fb.height=result.height;fb.pitch=result.pitch;fb.bpp=(u8)result.bpp;
    graphical=1;backbuffer_valid=1;
    requested_resolution_width=width;requested_resolution_height=height;
    update_resolution_input(width,height);
    mouse_x=(int)(width/2u);mouse_y=(int)(height/2u);
    set_resolution_status("DISPLAY MODE CHANGED IMMEDIATELY");
    return 1;
}

static int append_char(char *buffer, u32 *length, u32 capacity, int key, int allow_space) {
    char c=(char)key;
    if(key==8){if(*length){buffer[--(*length)]=0;}return 1;}
    if(key<32||key>126||*length>=capacity-1u)return 0;
    if(!allow_space && c==' ')return 0;
    if(c=='\\'||c=='/'||c==':'||c=='*'||c=='?'||c=='"'||c=='<'||c=='>'||c=='|')return 0;
    buffer[(*length)++]=c;buffer[*length]=0;return 1;
}

static void finish_setup(void) {
    if(!setup_username_len||!setup_password_len||!machine_name_len){
        const char *m="FILL IN ALL THREE FIELDS";u32 i=0;while(m[i]&&i<sizeof(setup_status)-1u){setup_status[i]=m[i];i++;}setup_status[i]=0;return;
    }
    screen_stage=1;login_password_len=0;login_password[0]=0;
    {const char *m="SETUP COMPLETE - SIGN IN TO CONTINUE";u32 i=0;while(m[i]&&i<sizeof(login_status)-1u){login_status[i]=m[i];i++;}login_status[i]=0;}
    active_app=0;start_menu_open=0;
}

static void try_login(void) {
    if(str_eq(login_password,setup_password)){
        screen_stage=2;active_app=0;start_menu_open=0;login_password_len=0;login_password[0]=0;
        set_terminal_output("SIGNED IN");
        serial_print("SYSTEMC: SESSION_LOGIN_OK\\r\\n");
    } else {
        const char *m="INCORRECT PASSWORD - TRY AGAIN";u32 i=0;while(m[i]&&i<sizeof(login_status)-1u){login_status[i]=m[i];i++;}login_status[i]=0;
        login_password_len=0;login_password[0]=0;
        serial_print("SYSTEMC: SESSION_LOGIN_FAILED\\r\\n");
    }
}

static void update_resolution_input(u32 w, u32 h) {
    char number[12]; u32 i = 0, j;
    u32toa(w, number); while (number[i] && i < sizeof(resolution_input) - 1u) { resolution_input[i] = number[i]; ++i; }
    if (i < sizeof(resolution_input) - 1u) resolution_input[i++] = ' ';
    u32toa(h, number); j = 0;
    while (number[j] && i < sizeof(resolution_input) - 1u) resolution_input[i++] = number[j++];
    resolution_input[i] = 0; resolution_input_len = i;
}

static void run_terminal(void) {
    u32 w, h;
    if (str_eq_ci(terminal_line, "HELP")) set_terminal_output("HELP VER ABOUT APPS LOCK CLEAR ITWLMCLI SYSTEMCSETUP-CLI");
    else if (str_eq_ci(terminal_line, "VER") || str_eq_ci(terminal_line, "WINVER")) set_terminal_output("SYSTEMC VERSION 3.0 BL0SS0M MOD.0");
    else if (str_eq_ci(terminal_line, "ABOUT")) set_terminal_output("SYSTEMC 3.0 BL0SS0M MOD.0 I386");
    else if (str_eq_ci(terminal_line, "APPS")) set_terminal_output("CALC CMD FILES DISK BROWSER PAINT NOTEPAD NETWORK SETTINGS WINVER ABOUT MONITOR DIAGNOSTICS CALENDAR HELP TASKMGR");
    else if (str_eq_ci(terminal_line, "LOCK")) { screen_stage=1;login_password_len=0;login_password[0]=0;start_menu_open=0;active_app=0;set_terminal_output("SESSION LOCKED"); }
    else if (str_eq_ci(terminal_line, "CLEAR")) set_terminal_output("SCREEN CLEARED");
    else if (str_eq_ci(terminal_line, "ITWLMCLI INSTALL NETWORK AUTO")) {
        int state=net_install_auto();
        if(state>0)set_terminal_output("ITWLMCLI: DRIVER READY; DHCP LEASE ACQUIRED");
        else if(state==0)set_terminal_output("ITWLMCLI: NO SUPPORTED INTEL E1000 ADAPTER");
        else set_terminal_output("ITWLMCLI: DRIVER READY; DHCP LEASE TIMED OUT");
    }
    else if (begins_ci(terminal_line, "SYSTEMCSETUP-CLI --RESOLUTION")) {
        if (parse_resolution_command(terminal_line, &w, &h)) {
            if(vbe_apply_resolution(w,h)) set_terminal_output("DISPLAY RESOLUTION CHANGED NOW");
            else {requested_resolution_width=w;requested_resolution_height=h;update_resolution_input(w,h);set_resolution_status("VBE MODE UNAVAILABLE - DISPLAY UNCHANGED");set_terminal_output("VBE MODE UNAVAILABLE; CURRENT MODE KEPT");}
        } else set_terminal_output("USAGE: SYSTEMCSETUP-CLI --RESOLUTION 1024 768");
    }
    else set_terminal_output("UNKNOWN COMMAND - TYPE HELP");
    terminal_line[0] = 0;
    terminal_len = 0;
}

static void outb_local(u16 p,u8 v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static u8 inb_local(u16 p){u8 v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static u16 inw_local(u16 p){u16 v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p));return v;}
static void outw_local(u16 p,u16 v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static u8 cmos_read(u8 index) { outb_local(0x70u, index); return inb_local(0x71u); }
static u8 bcd_to_bin(u8 value) { return (u8)((value & 15u) + ((value >> 4) * 10u)); }
static int rtc_read_day_month_year(u8 *day, u8 *month, u16 *year) {
    u8 a, d, m, y, century, b;
    u32 tries;
    for (tries=0; tries<1000u; ++tries) {
        a=cmos_read(0x0Au); if(a&0x80u)continue;
        d=cmos_read(0x07u);m=cmos_read(0x08u);y=cmos_read(0x09u);century=cmos_read(0x32u);b=cmos_read(0x0Bu);
        if(cmos_read(0x0Au)&0x80u)continue;
        if((b&0x04u)==0){d=bcd_to_bin(d);m=bcd_to_bin(m);y=bcd_to_bin(y);century=bcd_to_bin(century);}
        if(d<1u||d>31u||m<1u||m>12u||y>99u) return 0;
        if(century>=19u&&century<=99u)*year=(u16)((u16)century*100u+y);
        else *year=(u16)(y<70u?2000u+y:1900u+y);
        *day=d;*month=m;return 1;
    }
    return 0;
}
static int rtc_read_hour_minute(u8 *hour, u8 *minute) {
    u32 tries;
    for (tries = 0; tries < 1000u; ++tries) {
        u8 a = cmos_read(0x0Au);
        u8 sec1, sec2, h, m, b;
        if (a & 0x80u) continue;
        sec1 = cmos_read(0x00u); m = cmos_read(0x02u); h = cmos_read(0x04u);
        b = cmos_read(0x0Bu); sec2 = cmos_read(0x00u);
        if (sec1 != sec2 || (cmos_read(0x0Au) & 0x80u)) continue;
        if ((b & 0x04u) == 0) { m = bcd_to_bin(m); h = (u8)(h & 0x80u ? bcd_to_bin((u8)(h & 0x7Fu)) | 0x80u : bcd_to_bin(h)); }
        if ((b & 0x02u) == 0) { u8 pm = (u8)(h & 0x80u); h = (u8)(h & 0x7Fu); h = (u8)((h % 12u) + (pm ? 12u : 0u)); }
        if (h > 23u || m > 59u) return 0;
        *hour = h; *minute = m; return 1;
    }
    return 0;
}
static void draw_clock(void) {
    u8 h, m; char clock_text[6];
    clock_text[0] = '-'; clock_text[1] = '-'; clock_text[2] = ':'; clock_text[3] = '-'; clock_text[4] = '-'; clock_text[5] = 0;
    if (rtc_read_hour_minute(&h, &m)) {
        clock_text[0] = (char)('0' + h / 10u); clock_text[1] = (char)('0' + h % 10u);
        clock_text[3] = (char)('0' + m / 10u); clock_text[4] = (char)('0' + m % 10u);
        clock_shown_hour = h; clock_shown_minute = m;
    }
    rectangle(fb.width > 46u ? fb.width - 42u : 1u, fb.height - 28u, 40u, 22u, 0x182441u);
    text(fb.width > 42u ? fb.width - 39u : 3u, fb.height - 21u, clock_text, COLOR_WHITE, 1);
}
static void draw_mouse_cursor(void) {
    static const u16 arrow[20] = {
        0x0001u,0x0003u,0x0007u,0x000Fu,0x001Fu,0x003Fu,0x007Fu,0x00FFu,
        0x01FFu,0x03FFu,0x07FEu,0x0FFCu,0x0FF8u,0x1BF0u,0x31E0u,0x60C0u,
        0xC180u,0x8180u,0x0300u,0x0300u
    };
    u32 r,c;
    /* Sharp, outlined Blossom pointer; rendered into the backbuffer before present. */
    for(r=0;r<20u;r++)for(c=0;c<16u;c++)if((arrow[r]&(1u<<c))!=0){
        int edge=(c==0u||r==0u||(c>0u&&(arrow[r]&(1u<<(c-1u)))==0)||(r>0u&&(arrow[r-1u]&(1u<<c))==0));
        pixel((u32)mouse_x+c,(u32)mouse_y+r,edge?0x05070Du:((r<9u)?0xFFFFFFu:COLOR_ACCENT));
    }
}
static void io_pause(void){(void)inb_local(0x80);}
static void serial_init(void){outb_local(0x3F9,0);outb_local(0x3FB,0x80);outb_local(0x3F8,3);outb_local(0x3F9,0);outb_local(0x3FB,3);outb_local(0x3FA,0xC7);outb_local(0x3FC,0x0B);}
static void serial_print(const char *s){u32 n;while(*s){for(n=0;n<100000u;n++){if(inb_local(0x3FD)&0x20u)break;}outb_local(0x3F8,(u8)*s++);}}
static int wait_kbc_input(void){u32 n;for(n=0;n<100000u;n++){if((inb_local(0x64)&2u)==0)return 1;}return 0;}
static int wait_kbc_output(void){u32 n;for(n=0;n<100000u;n++){if((inb_local(0x64)&1u)!=0)return 1;}return 0;}
static void mouse_send(u8 v){if(!wait_kbc_input())return;outb_local(0x64,0xD4);if(!wait_kbc_input())return;outb_local(0x60,v);if(wait_kbc_output())(void)inb_local(0x60);}
static void mouse_init(void){u8 c;if(!wait_kbc_input())return;outb_local(0x64,0xA8);if(!wait_kbc_input())return;outb_local(0x64,0x20);if(!wait_kbc_output())return;c=inb_local(0x60);c|=2u;c&=(u8)~0x20u;if(!wait_kbc_input())return;outb_local(0x64,0x60);if(!wait_kbc_input())return;outb_local(0x60,c);mouse_send(0xF6);mouse_send(0xF4);mouse_packet_pos=0;mouse_ready=1;}
static void paint_put(int x,int y,u32 color){if(x>=0&&y>=0&&x<(int)PAINT_W&&y<(int)PAINT_H)paint_pixels[(u32)y*PAINT_W+(u32)x]=color;}
static void paint_mouse_draw(int x,int y){if(x<0||y<0||x>=(int)PAINT_W||y>=(int)PAINT_H)return;if(paint_tool==0){paint_draw_line(paint_last_x,paint_last_y,x,y,paint_colour);}else if(paint_tool==3){paint_draw_line(paint_last_x,paint_last_y,x,y,paint_background);}paint_last_x=x;paint_last_y=y;}
static void set_paint_tool(int t){if(t>=0&&t<=5)paint_tool=t;}
static void window_default_geometry(int app, int *x, int *y, int *w, int *h) {
    if (app == 6) { *x = 20; *y = 40; *w = 570; *h = 390; }
    else { *w = fb.width > 430u ? 390 : (int)fb.width - 20; *h = fb.height > 390u ? 320 : (int)fb.height - 70; *x = ((int)fb.width - *w) / 2; *y = ((int)fb.height - *h) / 2; }
    if (*w > (int)fb.width) *w = (int)fb.width;
    if (*h > (int)fb.height - 30) *h = (int)fb.height - 30;
}
static int taskbar_app_at(int x, int y) {
    int top = (int)fb.height - (int)TASKBAR_H;
    int i;
    if (y < top || y >= (int)fb.height) return 0;
    if (x >= 8 && x < 70) return -2; /* Start button returns to desktop. */
    { static const int ids[APP_ICON_COUNT] = {1,2,3,4,5,6,7,8,9,11,12,13,14,15,16,17};
      for (i = 0; i < (int)APP_ICON_COUNT; ++i) {
        int bx = 78 + i * 28;
        if (x >= bx && x < bx + 24) return ids[i];
      }
    }
    return -1;
}
static void paint_mouse_event(int x, int y, int left, int was) {
    int lx = x - window_dx[6], ly = y - window_dy[6];
    int px = (lx - 80) / (int)PAINT_SCALE, py = (ly - 151) / (int)PAINT_SCALE;
    static const u32 palette[8] = {0x10172Fu,0xF4F6FFu,0xFF6B83u,0xFFD479u,0x62E6A7u,0x61D9F8u,0x9A8CFFu,0xFF8FD8u};
    if (left && !was) {
        if (ly >= 91 && ly < 114 && lx >= 32 && lx < 494) {
            int idx = (lx - 32) / 77;
            if (idx >= 0 && idx < 5) set_paint_tool(idx);
            else if (idx == 5) { u32 pi; for (pi = 0; pi < PAINT_W * PAINT_H; ++pi) paint_pixels[pi] = paint_background; paint_drawing = 0; }
        } else if (ly >= 118 && ly < 140 && lx >= 34 && lx < 274) {
            int idx = (lx - 34) / 30; if (idx >= 0 && idx < 8) paint_colour = palette[idx];
        } else if (lx >= 80 && lx < 80 + (int)(PAINT_W * PAINT_SCALE) && ly >= 151 && ly < 151 + (int)(PAINT_H * PAINT_SCALE)) {
            if (px >= 0 && py >= 0 && px < (int)PAINT_W && py < (int)PAINT_H) {
                paint_start_x = paint_last_x = px; paint_start_y = paint_last_y = py; paint_drawing = 1;
                if (paint_tool == 0 || paint_tool == 3) paint_put(px, py, paint_tool == 3 ? paint_background : paint_colour);
                else if (paint_tool == 4) paint_fill(px, py, paint_colour);
            }
        }
    }
    if (left && was && paint_drawing && (paint_tool == 0 || paint_tool == 3)) {
        if (px >= 0 && py >= 0 && px < (int)PAINT_W && py < (int)PAINT_H) paint_mouse_draw(px, py);
    }
    if (!left && was && paint_drawing) {
        if (px < 0) px = 0;
        if (py < 0) py = 0;
        if (px >= (int)PAINT_W) px = (int)PAINT_W - 1;
        if (py >= (int)PAINT_H) py = (int)PAINT_H - 1;
        if (paint_tool == 1) paint_draw_line(paint_start_x, paint_start_y, px, py, paint_colour);
        else if (paint_tool == 2) paint_rectangle(paint_start_x, paint_start_y, px, py, paint_colour);
        paint_drawing = 0;
    }
}
static int desktop_icon_at(int x, int y) {
    static const int ids[APP_ICON_COUNT] = {1,2,3,4,5,6,7,8,9,11,12,13,14,15,16,17};
    int col,row,index;
    if(x<8||x>=8+3*82||y<34||y>=34+6*65)return 0;
    col=(x-8)/82;row=(y-34)/65;index=row*3+col;
    return (index>=0&&index<(int)APP_ICON_COUNT)?ids[index]:0;
}

static void open_context_menu(int x, int y) {
    context_target_app = active_app == 0 ? desktop_icon_at(x, y) : active_app;
    context_menu_x = x; context_menu_y = y;
    if (context_menu_x + 160 > (int)fb.width) context_menu_x = (int)fb.width - 160;
    if (context_menu_y + 94 > (int)fb.height - (int)TASKBAR_H) context_menu_y = (int)fb.height - 128;
    if (context_menu_x < 0) context_menu_x = 0;
    if (context_menu_y < 27) context_menu_y = 27;
    context_menu_open = 1;
}

static void handle_mouse(int x, int y, int left, int right) {
    int was = mouse_left;
    int right_was = mouse_right;
    right = right != 0;
    mouse_x = x; mouse_y = y;
    if(screen_stage<2){
        if(left&&!was){
            if(screen_stage==0){
                u32 w=fb.width>460u?440u:fb.width-20u;u32 h=fb.height>390u?348u:fb.height-48u;u32 sx=(fb.width-w)/2u,sy=(fb.height-h)/2u;
                if((u32)x>=sx+18u&&(u32)x<sx+w-18u&&(u32)y>=sy+113u&&(u32)y<sy+136u)setup_field=0;
                else if((u32)x>=sx+18u&&(u32)x<sx+w-18u&&(u32)y>=sy+156u&&(u32)y<sy+179u)setup_field=1;
                else if((u32)x>=sx+18u&&(u32)x<sx+w-18u&&(u32)y>=sy+199u&&(u32)y<sy+222u)setup_field=2;
                else if((u32)x>=sx+w-124u&&(u32)x<sx+w-18u&&(u32)y>=sy+h-52u&&(u32)y<sy+h-24u)finish_setup();
            } else {
                u32 w=fb.width>400u?360u:fb.width-20u;u32 h=fb.height>330u?268u:fb.height-48u;u32 sx=(fb.width-w)/2u,sy=(fb.height-h)/2u;
                if((u32)x>=sx+22u&&(u32)x<sx+w-22u&&(u32)y>=sy+134u&&(u32)y<sy+161u){}
                else if((u32)x>=sx+w-122u&&(u32)x<sx+w-22u&&(u32)y>=sy+h-56u&&(u32)y<sy+h-28u)try_login();
            }
        }
        mouse_left=left;mouse_right=right;return;
    }
    if (right && !right_was && !dragging_window) {
        open_context_menu(x, y);
        mouse_right = right; mouse_left = left;
        return;
    }
    if (dragging_window) {
        if (left) {
            int wx, wy, ww, wh;
            window_default_geometry(drag_app, &wx, &wy, &ww, &wh);
            window_dx[drag_app] = drag_origin_dx + x - drag_mouse_x;
            window_dy[drag_app] = drag_origin_dy + y - drag_mouse_y;
            if (wx + window_dx[drag_app] < 0) window_dx[drag_app] = -wx;
            if (wy + window_dy[drag_app] < 27) window_dy[drag_app] = 27 - wy;
            if (wx + window_dx[drag_app] + ww > (int)fb.width) window_dx[drag_app] = (int)fb.width - wx - ww;
            if (wy + window_dy[drag_app] + wh > (int)fb.height - (int)TASKBAR_H) window_dy[drag_app] = (int)fb.height - (int)TASKBAR_H - wy - wh;
        } else dragging_window = 0;
        mouse_left = left; mouse_right = right;
        return;
    }
    if (left && !was) {
        if (context_menu_open) {
            int mx = x - context_menu_x, my = y - context_menu_y;
            if (mx >= 0 && mx < 160 && my >= 5 && my < 93) {
                int row = (my - 5) / 22;
                if (row == 0 && is_window_app(context_target_app)) active_app = context_target_app;
                else if (row == 1) active_app = 9;
                else if (row == 2) { active_app = 0; start_menu_open = 0; }
                else if (row == 3) active_app = 2;
            }
            context_menu_open = 0;
            mouse_left = left; mouse_right = right;
            return;
        }
        {
            int app_from_bar = taskbar_app_at(x, y);
            if (app_from_bar == -2) { start_menu_open = !start_menu_open; context_menu_open = 0; mouse_left = left; mouse_right = right; return; }
            if (app_from_bar > 0) { active_app = app_from_bar; start_menu_open = 0; context_menu_open = 0; mouse_left = left; mouse_right = right; return; }
        }
        if (start_menu_open) {
            int sy = fb.height > 315u ? (int)fb.height - (int)TASKBAR_H - 260 : 26;
            if (x >= 5 && x < 229 && y >= sy + 34 && y < sy + 250) {
                int row = (y - sy - 34) / 24;
                if(row==0)active_app=2;
                else if(row==1)active_app=9;
                else if(row==2)active_app=11;
                else if(row==3)active_app=12;
                else if(row==4)active_app=13;
                else if(row==5)active_app=15;
                else if(row==6)active_app=16;
                else if(row==7)active_app=17;
                else if(row==8){screen_stage=1;login_password_len=0;login_password[0]=0;active_app=0;}
                start_menu_open=0;context_menu_open=0;mouse_left=left;mouse_right=right;return;
            }
            start_menu_open = 0;
        }
        if (active_app == 0) {
            int app = desktop_icon_at(x, y);
            if (app > 0) { active_app = app; start_menu_open = 0; context_menu_open = 0; mouse_left = left; mouse_right = right; return; }
        } else if (is_window_app(active_app)) {
            int wx, wy, ww, wh, ax, ay;
            window_default_geometry(active_app, &wx, &wy, &ww, &wh);
            ax = wx + window_dx[active_app]; ay = wy + window_dy[active_app];
            if (x >= ax + ww - 24 && x < ax + ww - 5 && y >= ay + 4 && y < ay + 24) {
                active_app = 0; paint_drawing = 0; context_menu_open = 0; mouse_left = left; mouse_right = right; return;
            }
            if (x >= ax && x < ax + ww && y >= ay && y < ay + 28) {
                dragging_window = 1; drag_app = active_app; drag_mouse_x = x; drag_mouse_y = y;
                drag_origin_dx = window_dx[drag_app]; drag_origin_dy = window_dy[drag_app]; mouse_left = left; mouse_right = right; return;
            }
            if (active_app == 9) {
                int lx = x - ax, ly = y - ay;
                if (lx >= 5 && lx < 103 && ly >= 62 && ly < 90) { settings_page=0; settings_resolution_focus=0; }
                else if (lx >= 5 && lx < 103 && ly >= 95 && ly < 134) { settings_page=1; settings_resolution_focus=0; }
                else if (settings_page==0 && lx >= 120 && lx < ww-10 && ly >= 92 && ly < 248) {
                    int idx=(ly-92)/39;if(idx>=0&&idx<4){apply_theme((u32)idx);set_resolution_status("THEME APPLIED IMMEDIATELY");}
                }
                else if (settings_page==1 && lx >= 119 && lx < ww-10 && ly >= 89 && ly < 119) settings_resolution_focus=1;
                else if (settings_page==1 && lx >= 119 && lx < 201 && ly >= 127 && ly < 154) {
                    u32 w=0,h=0,i=0;while(resolution_input[i]>='0'&&resolution_input[i]<='9')w=w*10u+(u32)(resolution_input[i++]-'0');while(resolution_input[i]==' ')i++;while(resolution_input[i]>='0'&&resolution_input[i]<='9')h=h*10u+(u32)(resolution_input[i++]-'0');
                    if(w>=320u&&w<=1920u&&h>=240u&&h<=1080u){if(!vbe_apply_resolution(w,h))set_resolution_status("VBE MODE UNAVAILABLE - DISPLAY UNCHANGED");}
                    else set_resolution_status("INVALID MODE - TRY 800 600 OR 1024 768");
                    settings_resolution_focus=0;
                }
            }
        }
    }
    if (active_app == 6 && !dragging_window) paint_mouse_event(x, y, left, was);
    mouse_left = left; mouse_right = right;
}

static int poll_mouse(void) {
    u8 status, v; int changed = 0; u32 loops = 0;
    while ((inb_local(0x64) & 1u) != 0 && loops++ < 16u) {
        status = inb_local(0x64); if ((status & 0x20u) == 0) break; v = inb_local(0x60);
        mouse_packet[mouse_packet_pos++] = v;
        if (mouse_packet_pos == 3) {
            int dx = (int)(signed char)mouse_packet[1], dy = (int)(signed char)mouse_packet[2];
            int nx, ny; mouse_packet_pos = 0;
            if ((mouse_packet[0] & 0xC0u) == 0) {
                nx = mouse_x + dx; ny = mouse_y - dy;
                if (nx < 0) nx = 0;
                if (ny < 0) ny = 0;
                if (nx >= (int)fb.width) nx = (int)fb.width - 1;
                if (ny >= (int)fb.height) ny = (int)fb.height - 1;
                if (nx != mouse_x || ny != mouse_y || (((mouse_packet[0] & 1u) != 0) != mouse_left) || (((mouse_packet[0] & 2u) != 0) != mouse_right)) changed = 1;
                handle_mouse(nx, ny, mouse_packet[0] & 1u, mouse_packet[0] & 2u);
            }
        }
    }
    return changed;
}

static void u32toa(u32 value,char *out){char tmp[12];u32 n=0,i=0;do{tmp[n++]=(char)('0'+value%10u);value/=10u;}while(value&&n<sizeof(tmp));while(n)out[i++]=tmp[--n];out[i]=0;}

static void cpuid_leaf(u32 leaf, u32 subleaf, u32 *a, u32 *b, u32 *c, u32 *d) {
    u32 aa, bb, cc, dd;
    __asm__ volatile ("cpuid" : "=a"(aa), "=b"(bb), "=c"(cc), "=d"(dd) : "a"(leaf), "c"(subleaf));
    if (a) *a = aa;
    if (b) *b = bb;
    if (c) *c = cc;
    if (d) *d = dd;
}

static void identify_cpu(void) {
    u32 a=0,b=0,c=0,d=0,max_ext=0,i,j;
    cpuid_leaf(0,0,&a,&b,&c,&d);
    for (i=0;i<4;i++) cpu_vendor[i]=(char)(b>>(i*8u));
    for (i=0;i<4;i++) cpu_vendor[4u+i]=(char)(d>>(i*8u));
    for (i=0;i<4;i++) cpu_vendor[8u+i]=(char)(c>>(i*8u));
    cpu_vendor[12]=0;
    cpuid_leaf(0x80000000u,0,&max_ext,0,0,0);
    if(max_ext>=0x80000004u){
        for(i=0;i<3;i++){
            cpuid_leaf(0x80000002u+i,0,&a,&b,&c,&d);
            {u32 vals[4]={a,b,c,d};for(j=0;j<4;j++){u32 k;for(k=0;k<4;k++)cpu_brand[i*16u+j*4u+k]=(char)(vals[j]>>(k*8u));}}
        }
        cpu_brand[48]=0;
        for(i=0;cpu_brand[i]==' '&&i<47u;i++) {u32 k;for(k=0;k<48u-i;k++)cpu_brand[k]=cpu_brand[k+1u];}
    } else {
        const char *unknown="UNKNOWN CPU";for(i=0;i<sizeof(cpu_brand);i++)cpu_brand[i]=0;for(i=0;unknown[i];i++)cpu_brand[i]=unknown[i];
    }
}

static int is_window_app(int app) {
    return (app >= 1 && app <= 9) || (app >= 11 && app <= 17);
}

static void apply_theme(u32 id) {
    theme_id=(int)(id%4u);
    if(theme_id==0){ui_bg=0x14213Du;ui_panel=0x202C4Du;ui_panel2=0x2A3A60u;ui_accent=0xB39DFFu;ui_cyan=0x7CE7F2u;ui_white=0xF7F7FFu;ui_muted=0xB4BED9u;ui_red=0xFF718Bu;ui_green=0x6BE5AEu;ui_yellow=0xFFD477u;}
    else if(theme_id==1){ui_bg=0x102846u;ui_panel=0x16385Fu;ui_panel2=0x1D4A7Bu;ui_accent=0x4C9BFFu;ui_cyan=0x84D7FFu;ui_white=0xFFFFFFu;ui_muted=0xB8D2EAu;ui_red=0xFF667A;ui_green=0x6DE2AAu;ui_yellow=0xFFE08Au;}
    else if(theme_id==2){ui_bg=0x18362Fu;ui_panel=0x21473Du;ui_panel2=0x2E6152u;ui_accent=0x78D3A4u;ui_cyan=0xA5E6D0u;ui_white=0xF5FFF8u;ui_muted=0xB8D5C7u;ui_red=0xFF7B76u;ui_green=0x8AF0B0u;ui_yellow=0xF7DD8Au;}
    else {ui_bg=0x000000u;ui_panel=0x080808u;ui_panel2=0x242424u;ui_accent=0xFFFF00u;ui_cyan=0x00FFFFu;ui_white=0xFFFFFFu;ui_muted=0xE0E0E0u;ui_red=0xFF4040u;ui_green=0x00FF00u;ui_yellow=0xFFFF00u;}
}
static int text_contains(const char *a,const char *b){u32 i,j;if(!*b)return 1;for(i=0;a[i];i++){j=0;while(b[j]&&a[i+j]==b[j])++j;if(!b[j])return 1;}return 0;}
static int module_string_match(const char *s,const char *needle){return text_contains(s,needle);}
static void locate_install_image(const u8 *mbi){u32 flags=*(const u32*)mbi,count,addr,i;if((flags&(1u<<3))==0)return;count=*(const u32*)(mbi+20);addr=*(const u32*)(mbi+24);if(count>64u||addr==0)return;for(i=0;i<count;i++){const u8 *m=(const u8*)(addr+i*16u);u32 start=*(const u32*)(m),end=*(const u32*)(m+4),str=*(const u32*)(m+8);if(start<end&&str&&module_string_match((const char*)str,"systemc-hdd-image")){install_image=(const u8*)start;install_image_size=end-start;install_image_sectors=install_image_size/512u;installer_image_valid=(install_image_size>=512u&&install_image_size%512u==0);return;}}}
static int ata_wait(u16 io,u8 mask,u8 want){u32 n;u8 s;for(n=0;n<2000000u;n++){s=inb_local(io+7);if(s==0||s==0xFFu)return 0;if((s&0x80u)==0&&(s&mask)==want)return 1;}return 0;}
static int ata_identify(struct ata_device *d){u16 words[256];u32 i;u8 s;d->present=0;outb_local(d->io+6,(u8)(0xA0u|(d->slave<<4)));io_pause();io_pause();io_pause();io_pause();outb_local(d->io+2,0);outb_local(d->io+3,0);outb_local(d->io+4,0);outb_local(d->io+5,0);outb_local(d->io+7,0xEC);s=inb_local(d->io+7);if(s==0||s==0xFFu)return 0;if(!ata_wait(d->io,0x80u,0))return 0;if(inb_local(d->io+4)!=0||inb_local(d->io+5)!=0)return 0;if(!ata_wait(d->io,0x08u,0x08u))return 0;for(i=0;i<256;i++)words[i]=inw_local(d->io);d->sectors=(u32)words[60]|((u32)words[61]<<16);for(i=0;i<20;i++){u16 w=words[27+i];d->model[i*2]=(char)(w>>8);d->model[i*2+1]=(char)w;}d->model[40]=0;for(i=39;i>0&&d->model[i]==' ';i--)d->model[i]=0;if(d->sectors==0)return 0;d->present=1;return 1;}
static void ata_scan(void){u32 i;static const u16 ports[4]={0x1F0,0x1F0,0x170,0x170};for(i=0;i<4;i++){ata_devices[i].io=ports[i];ata_devices[i].slave=(u8)(i&1u);ata_devices[i].present=0;ata_devices[i].sectors=0;ata_devices[i].model[0]=0;(void)ata_identify(&ata_devices[i]);}}
static int ata_write_sectors(struct ata_device *d,u32 lba,u32 count,const u8 *data){u32 s,w;if(count==0||count>16u||lba+count>d->sectors||lba+count>0x10000000u)return 0;outb_local(d->io+6,(u8)(0xE0u|(d->slave<<4)|((lba>>24)&0x0Fu)));io_pause();if(!ata_wait(d->io,0x80u,0))return 0;outb_local(d->io+2,(u8)count);outb_local(d->io+3,(u8)lba);outb_local(d->io+4,(u8)(lba>>8));outb_local(d->io+5,(u8)(lba>>16));outb_local(d->io+7,0x30);for(s=0;s<count;s++){if(!ata_wait(d->io,0x08u,0x08u))return 0;for(w=0;w<256;w++){u16 v=(u16)data[s*512u+w*2u]|((u16)data[s*512u+w*2u+1u]<<8);outw_local(d->io,v);} }if(!ata_wait(d->io,0x80u,0))return 0;if(inb_local(d->io+7)&1u)return 0;return 1;}
static int ata_write_image(struct ata_device *d){u32 lba=0,batch;while(lba<install_image_sectors){batch=install_image_sectors-lba;if(batch>16u)batch=16u;if(!ata_write_sectors(d,lba,batch,install_image+lba*512u))return 0;lba+=batch;if((lba&8191u)==0||lba==install_image_sectors){char percent[8];u32 p=(lba*100u)/install_image_sectors;u32toa(p,percent);rectangle(55,260,fb.width-110u,18,COLOR_PANEL2);rectangle(55,260,((fb.width-110u)*p)/100u,18,COLOR_ACCENT);text(65,265,"WRITING DISK IMAGE - PERCENT:",COLOR_WHITE,1);text(245,265,percent,COLOR_GREEN,1);}}return 1;}
static void installer_reboot(void){u32 n;for(n=0;n<100000u;n++){if((inb_local(0x64)&2u)==0)break;}outb_local(0x64,0xFE);for(;;)__asm__ volatile("cli; hlt");}
static void render_installer_notice(void){u32 i;char nbuf[16];rectangle(0,0,fb.width,fb.height,COLOR_BG);rectangle(30,35,fb.width-60,fb.height-70,COLOR_PANEL);rectangle(30,35,fb.width-60,32,COLOR_ACCENT);text(44,47,"SYSTEMC DISK INSTALLER",0x10172Fu,1);if(!installer_image_valid){text(50,100,"INSTALL IMAGE IS MISSING FROM THIS ISO.",COLOR_RED,1);text(50,128,"BUILD ON UBUNTU/LINUX WITH LOOP + GRUB TOOLS.",COLOR_YELLOW,1);text(50,180,"PRESS ESC TO RETURN TO LIVE DESKTOP.",COLOR_WHITE,1);active_app=10;return;}
    if(installer_stage==0){text(50,88,"WARNING: INSTALLING OVERWRITES THE DISK.",COLOR_RED,1);text(50,107,"ONLY LEGACY IDE/PATA DISKS ARE SUPPORTED.",COLOR_YELLOW,1);for(i=0;i<4;i++){u32 yy=140u+i*39u;if(ata_devices[i].present){text(54,yy,i==0?"1:":i==1?"2:":i==2?"3:":"4:",COLOR_CYAN,1);text(76,yy,ata_devices[i].model,COLOR_WHITE,1);u32toa(ata_devices[i].sectors/2048u,nbuf);text(410,yy,nbuf,COLOR_MUTED,1);text(462,yy,"MB",COLOR_MUTED,1);}else{text(54,yy,i==0?"1:":i==1?"2:":i==2?"3:":"4:",COLOR_MUTED,1);text(76,yy,"NO ATA DISK DETECTED",COLOR_MUTED,1);}}text(50,315,"PRESS 1-4 TO SELECT A DISK.",COLOR_WHITE,1);}
    else if(installer_stage==1){text(50,100,"SELECTED DISK:",COLOR_CYAN,1);text(50,122,ata_devices[installer_drive].model,COLOR_WHITE,1);text(50,149,"THIS OPERATION REPLACES THE DISK PARTITION TABLE.",COLOR_RED,1);text(50,176,"TYPE ERASE AND PRESS ENTER TO CONFIRM.",COLOR_YELLOW,1);text(50,206,installer_confirm,COLOR_WHITE,2);text(50,246,"PRESS ESC TO CANCEL.",COLOR_MUTED,1);}
    else if(installer_stage==2){text(50,115,"INSTALLING SYSTEMC TO THE SELECTED IDE DISK...",COLOR_WHITE,1);text(50,145,"DO NOT POWER OFF THE MACHINE.",COLOR_YELLOW,1);}
    else if(installer_stage==3){text(50,115,"INSTALLATION COMPLETE.",COLOR_GREEN,1);text(50,145,"REBOOTING FROM THE INSTALLED DISK...",COLOR_WHITE,1);}
    else{text(50,115,"INSTALLATION FAILED.",COLOR_RED,1);text(50,145,installer_status,COLOR_YELLOW,1);text(50,190,"PRESS ESC TO RETURN TO LIVE DESKTOP.",COLOR_WHITE,1);}active_app=10;}
static void handle_installer_key(int key) {
    if (key == 27) {
        installer_stage = 0;
        active_app = 0;
        render_app();
        return;
    }
    if (installer_stage == 0) {
        if (key >= '1' && key <= '4') {
            int d = key - '1';
            if (ata_devices[d].present && ata_devices[d].sectors >= install_image_sectors && ata_devices[d].sectors < 0x10000000u) {
                installer_drive = d;
                installer_stage = 1;
                installer_confirm_len = 0;
                installer_confirm[0] = 0;
            }
        }
    } else if (installer_stage == 1) {
        if (key == 8 && installer_confirm_len) {
            installer_confirm[--installer_confirm_len] = 0;
        } else if (key == 13) {
            if (installer_confirm[0]=='E' && installer_confirm[1]=='R' && installer_confirm[2]=='A' && installer_confirm[3]=='S' && installer_confirm[4]=='E' && installer_confirm[5]==0) {
                installer_stage = 2;
                render_installer_notice();
                if (ata_write_image(&ata_devices[installer_drive])) {
                    installer_stage = 3;
                    render_installer_notice();
                    for (volatile u32 d=0; d<3000000u; d++) __asm__ volatile("pause");
                    installer_reboot();
                } else {
                    installer_stage = 4;
                    installer_status[0]='A'; installer_status[1]='T'; installer_status[2]='A';
                    installer_status[3]=' '; installer_status[4]='W'; installer_status[5]='R';
                    installer_status[6]='I'; installer_status[7]='T'; installer_status[8]='E'; installer_status[9]=0;
                }
            } else {
                installer_confirm_len = 0;
                installer_confirm[0] = 0;
            }
        } else if (((key>='A'&&key<='Z') || (key>='a'&&key<='z')) && installer_confirm_len<5u) {
            installer_confirm[installer_confirm_len++] = (key>='a'&&key<='z') ? (char)(key-32) : (char)key;
            installer_confirm[installer_confirm_len] = 0;
        }
    }
    render_installer_notice();
}

static void vga_line(u32 row,const char *s){volatile u16 *v=(volatile u16*)0xB8000;u32 col=0;while(*s&&col<78){v[row*80u+col]=(u16)0x1F00u|(u8)*s++;col++;}}
static void textmode_draw(void){volatile u16 *v=(volatile u16*)0xB8000;u32 i,row=0,col=0;const char *s;for(i=0;i<2000;i++)v[i]=0x1F20;vga_line(row++,"SYSTEMC 3.0 BL0SS0M MOD.0 - VGA SAFE MODE");
    if(active_app==0){vga_line(row++,"DESKTOP: PRESS 1 CALC 2 TERMINAL 3 FILES 4 DISK");vga_line(row++,"5 BROWSER 6 PAINT 7 NOTEPAD 8 NETWORK");vga_line(row++,"FRAMEBUFFER/MOUSE UNAVAILABLE; KEYBOARD MODE ACTIVE");}
    else if(active_app==7){vga_line(row++,"NOTEPAD - RAM-ONLY DOCUMENT");for(i=0;i<notepad_len&&row<22;i++){char c=notepad_buffer[i];if(c=='\n'){row++;col=0;}else if(col<78){v[row*80u+col]=(u16)0x1F00u|(u8)c;col++;}}}
    else if(active_app==6){vga_line(row++,"PAINT: ARROWS MOVE, SPACE DRAW, P COLOR, C CLEAR");vga_line(row++,"ASCII PREVIEW:");for(i=0;i<20&&row<23;i++){u32 x;for(x=0;x<40;x++){u32 px=(x*PAINT_W)/40u,py=(i*PAINT_H)/20u;char c=paint_pixels[py*PAINT_W+px]==paint_background?' ': '#';v[row*80u+col++]=(u16)0x1F00u|(u8)c;}row++;col=0;}}
    else if(active_app==5){vga_line(row++,"BROWSER: PLAIN HTTP ONLY; HTTPS/TLS NOT SUPPORTED");vga_line(row++,"CURRENT URL:");s=browser_url;while(*s&&col<78){v[row*80u+col]=(u16)0x1F00u|(u8)*s++;col++;}row++;col=0;vga_line(row++,browser_status);}
    else if(active_app==10){vga_line(row++,"SYSTEMC INSTALLER - ERASES THE SELECTED IDE DISK");if(!installer_image_valid)vga_line(row++,"INSTALL IMAGE MISSING; REBUILD ISO ON LINUX");else if(installer_stage==0){vga_line(row++,"SELECT DRIVE 1-4:");for(i=0;i<4;i++){if(ata_devices[i].present){char label[3]={(char)('1'+i),':',0};vga_line(row++,label);vga_line(row++,ata_devices[i].model);}}}else if(installer_stage==1){vga_line(row++,"TYPE ERASE AND PRESS ENTER TO CONFIRM:");vga_line(row++,installer_confirm);}else if(installer_stage==4)vga_line(row++,installer_status);}
    else {vga_line(row++,"APP OPEN - PRESS ESC TO RETURN TO DESKTOP");if(active_app==8){vga_line(row++,net_has_driver()?"E1000 NIC DETECTED":"NO E1000 NIC DETECTED");vga_line(row++,net_is_online()?"NETWORK ONLINE":"WAITING FOR DHCP");}}
}

static void browser_fetch(void){char host[96];const char *url=browser_url,*p,*path;u32 n=0,i;int got;if(browser_url_len>=7&&url[0]=='h'&&url[1]=='t'&&url[2]=='t'&&url[3]=='p'&&url[4]=='s'&&url[5]==':'&&url[6]=='/'){browser_status[0]='H';browser_status[1]='T';browser_status[2]='T';browser_status[3]='P';browser_status[4]='S';browser_status[5]=' ';browser_status[6]='N';browser_status[7]='O';browser_status[8]='T';browser_status[9]=' ';browser_status[10]='S';browser_status[11]='U';browser_status[12]='P';browser_status[13]='P';browser_status[14]='O';browser_status[15]='R';browser_status[16]='T';browser_status[17]='E';browser_status[18]='D';browser_status[19]=0;browser_text_len=0;return;}
    if(!(url[0]=='h'&&url[1]=='t'&&url[2]=='t'&&url[3]=='p'&&url[4]==':'&&url[5]=='/'&&url[6]=='/')){browser_status[0]='U';browser_status[1]='S';browser_status[2]='E';browser_status[3]=' ';browser_status[4]='h';browser_status[5]='t';browser_status[6]='t';browser_status[7]='p';browser_status[8]=':';browser_status[9]='/';browser_status[10]='/';browser_status[11]=0;return;}
    p=url+7;while(*p&&*p!='/'&&*p!=':'&&n<sizeof(host)-1)host[n++]=*p++;host[n]=0;if(*p==':'){browser_status[0]='O';browser_status[1]='N';browser_status[2]='L';browser_status[3]='Y';browser_status[4]=' ';browser_status[5]='P';browser_status[6]='O';browser_status[7]='R';browser_status[8]='T';browser_status[9]=' ';browser_status[10]='8';browser_status[11]='0';browser_status[12]=0;return;}path=*p=='/'?p:"/";browser_status[0]='F';browser_status[1]='E';browser_status[2]='T';browser_status[3]='C';browser_status[4]='H';browser_status[5]='I';browser_status[6]='N';browser_status[7]='G';browser_status[8]='.';browser_status[9]='.';browser_status[10]='.';browser_status[11]=0;browser_text_len=0;got=net_http_get(host,path,browser_raw,sizeof(browser_raw));if(got<0){const char *e=got==-2?"DNS FAILED":got==-3?"TCP CONNECT FAILED":got==-4?"NO HTTP RESPONSE":"NETWORK OFFLINE";u32 j=0;while(e[j]){browser_status[j]=e[j];j++;}browser_status[j]=0;return;}
    {char *body=0;for(i=0;i+3u<(u32)got;i++){if(browser_raw[i]=='\r'&&browser_raw[i+1]=='\n'&&browser_raw[i+2]=='\r'&&browser_raw[i+3]=='\n'){body=browser_raw+i+4;break;}}if(!body)body=browser_raw;{int in_tag=0,last_space=1;u32 b=0;for(i=0;body[i]&&b<BROWSER_CAP-2;i++){char c=body[i];if(c=='<'){in_tag=1;continue;}if(c=='>'){in_tag=0;if(!last_space&&b<BROWSER_CAP-2){browser_text[b++]=' ';last_space=1;}continue;}if(in_tag)continue;if(c=='\r'||c=='\n'||c=='\t')c=' ';if(c==' '){if(last_space)continue;last_space=1;}else last_space=0;browser_text[b++]=c;}browser_text[b]=0;browser_text_len=b;}}
    p=browser_raw;if(p[0]=='H'&&p[1]=='T'&&p[2]=='T'&&p[3]=='P'){if(p[9]=='3'){const char *e="REDIRECT RECEIVED - HTTPS IS NOT SUPPORTED";i=0;while(e[i]){browser_status[i]=e[i];i++;}browser_status[i]=0;}else{const char *e="HTTP RESPONSE RECEIVED";i=0;while(e[i]){browser_status[i]=e[i];i++;}browser_status[i]=0;}}else{const char *e="RESPONSE RECEIVED";i=0;while(e[i]){browser_status[i]=e[i];i++;}browser_status[i]=0;}}


static void notepad_update_scroll(void) {
    u32 i,lines=0,cut=0;for(i=0;i<notepad_len;i++)if(notepad_buffer[i]=='\n')lines++;
    if(lines<10u){notepad_scroll=0;return;}
    {u32 wanted=lines-9u;for(i=0;i<notepad_len&&wanted;i++)if(notepad_buffer[i]=='\n'){cut=i+1u;wanted--;}}
    notepad_scroll=cut;
}

static void handle_key(int key) {
    char c;
    if(screen_stage==0){
        if(key==9){setup_field=(setup_field+1)%3;return;}
        if(key==13){if(setup_field<2)setup_field++;else finish_setup();return;}
        if(setup_field==0)append_char(setup_username,&setup_username_len,sizeof(setup_username),key,0);
        else if(setup_field==1){char pc=(char)key;if(key==8){if(setup_password_len)setup_password[--setup_password_len]=0;}else if(key>=32&&key<=126&&setup_password_len<sizeof(setup_password)-1u){setup_password[setup_password_len++]=pc;setup_password[setup_password_len]=0;}}
        else append_char(machine_name,&machine_name_len,sizeof(machine_name),key,0);
        return;
    }
    if(screen_stage==1){
        if(key==13){try_login();return;}
        if(key==8){if(login_password_len)login_password[--login_password_len]=0;return;}
        c=(char)key;if(c>=32&&c<=126&&login_password_len<sizeof(login_password)-1u){login_password[login_password_len++]=c;login_password[login_password_len]=0;}return;
    }
    if(active_app==10){handle_installer_key(key);return;}
    if(key==27){active_app=0;paint_drawing=0;context_menu_open=0;return;}
    if(key==0x110){active_app=2;return;}
    if(active_app==0){if(key>='1'&&key<='9')active_app=key-'0';else if(key=='h'||key=='H')active_app=2;return;}
    if(active_app==6){
        if(key==0x100&&paint_y>0)--paint_y;else if(key==0x101&&paint_y<(int)PAINT_H-1)++paint_y;else if(key==0x102&&paint_x>0)--paint_x;else if(key==0x103&&paint_x<(int)PAINT_W-1)++paint_x;
        else if(key==' '){if(paint_tool==4)paint_fill(paint_x,paint_y,paint_colour);else paint_put(paint_x,paint_y,paint_tool==3?paint_background:paint_colour);}
        else if(key=='c'||key=='C'){u32 i;for(i=0;i<PAINT_W*PAINT_H;i++)paint_pixels[i]=paint_background;}
        else if(key>='1'&&key<='5')set_paint_tool(key-'1');
        else if(key=='p'||key=='P'){static const u32 palette[8]={0x10172Fu,0xF4F6FFu,0xFF6B83u,0xFFD479u,0x62E6A7u,0x61D9F8u,0x9A8CFFu,0xFF8FD8u};u32 i;for(i=0;i<8;i++)if(palette[i]==paint_colour){paint_colour=palette[(i+1u)%8u];break;}}
        return;
    }
    if(active_app==9){
        if(settings_page==1 && settings_resolution_focus){
            if(key==8&&resolution_input_len){resolution_input[--resolution_input_len]=0;}
            else if(key==13){u32 w=0,h=0,i=0;while(resolution_input[i]>='0'&&resolution_input[i]<='9')w=w*10u+(u32)(resolution_input[i++]-'0');while(resolution_input[i]==' ')i++;while(resolution_input[i]>='0'&&resolution_input[i]<='9')h=h*10u+(u32)(resolution_input[i++]-'0');if(w>=320u&&w<=1920u&&h>=240u&&h<=1080u){if(!vbe_apply_resolution(w,h))set_resolution_status("VBE MODE UNAVAILABLE - DISPLAY UNCHANGED");}else set_resolution_status("INVALID MODE - TRY 800 600 OR 1024 768");settings_resolution_focus=0;}
            else {c=(char)key;if(((c>='0'&&c<='9')||c==' ')&&resolution_input_len<sizeof(resolution_input)-1u){resolution_input[resolution_input_len++]=c;resolution_input[resolution_input_len]=0;}}
        } else if(settings_page==0 && key>='1'&&key<='4')apply_theme((u32)(key-'1'));
        return;
    }
    if(active_app==7){if(key==8&&notepad_len){notepad_buffer[--notepad_len]=0;}else if(key==13&&notepad_len<NOTEPAD_CAP-1u){notepad_buffer[notepad_len++]='\n';notepad_buffer[notepad_len]=0;}else{c=(char)key;if(c>=32&&c<=126&&notepad_len<NOTEPAD_CAP-1u){notepad_buffer[notepad_len++]=c;notepad_buffer[notepad_len]=0;}}notepad_update_scroll();return;}
    if(active_app==5){if(key==8&&browser_url_len){browser_url[--browser_url_len]=0;}else if(key==13){browser_fetch();}else{c=(char)key;if(c>=32&&c<=126&&browser_url_len<sizeof(browser_url)-1u){browser_url[browser_url_len++]=c;browser_url[browser_url_len]=0;}}return;}
    if(active_app==1){if(key==8&&calc_len)calc_line[--calc_len]=0;else if(key==13)calculate();else{c=(char)key;if(((c>='0'&&c<='9')||c=='+'||c=='-'||c=='*'||c=='/')&&calc_len<sizeof(calc_line)-1u){calc_line[calc_len++]=c;calc_line[calc_len]=0;calc_result[0]=0;}}return;}
    if(active_app==2){if(key==8&&terminal_len)terminal_line[--terminal_len]=0;else if(key==13)run_terminal();else{c=(char)key;if(((c>='a'&&c<='z')||(c>='0'&&c<='9')||c==' '||c=='-'||c=='.')&&terminal_len<sizeof(terminal_line)-1u){terminal_line[terminal_len++]=c;terminal_line[terminal_len]=0;}}}
}

static int contains_install(const char *cmd) {
    const char *p;
    if (!cmd) return 0;
    for (p = cmd; *p; ++p) {
        if (p[0] == 'i' && p[1] == 'n' && p[2] == 's' && p[3] == 't' && p[4] == 'a' && p[5] == 'l' && p[6] == 'l') return 1;
    }
    return 0;
}

void kernel_main(u32 magic, u32 mbi_address) {
    u32 flags=0,i;const u8 *mbi=(const u8*)mbi_address;u32 cmdptr=0;
    serial_init();
    serial_print("SYSTEMC: BOOT_ENTRY\r\n");
    if(magic!=MULTIBOOT_BOOTLOADER_MAGIC||mbi_address==0){serial_print("SYSTEMC: MULTIBOOT_HANDOFF_ERROR\r\n");text_mode_message("SYSTEMC 3.0 BL0SS0M MOD.0","BOOT ERROR: INVALID MULTIBOOT HANDOFF","CHECK GRUB CONFIGURATION");for(;;)__asm__ volatile("cli; hlt");}
    flags=*(const u32*)(mbi+0);
    if(flags&1u){boot_memory_kb=*(const u32*)(mbi+8);system_ram_mb=(boot_memory_kb+1024u)/1024u;}
    identify_cpu();
    if((flags&MULTIBOOT_INFO_FRAMEBUFFER)!=0){u64 address=*(const u64*)(mbi+88);fb.pitch=*(const u32*)(mbi+96);fb.width=*(const u32*)(mbi+100);fb.height=*(const u32*)(mbi+104);fb.bpp=*(const u8*)(mbi+108);if((address>>32)==0&&*(const u8*)(mbi+109)==1&&(fb.bpp==24||fb.bpp==32)&&fb.width>=320&&fb.width<=1920&&fb.height>=240&&fb.height<=BACKBUFFER_MAX_HEIGHT&&fb.pitch>=fb.width*(fb.bpp/8u)&&fb.pitch<=BACKBUFFER_MAX_PITCH){fb.address=(volatile u8*)(u32)address;graphical=1;backbuffer_valid=1;requested_resolution_width=fb.width;requested_resolution_height=fb.height;update_resolution_input(fb.width,fb.height);}}
    text_mode=!graphical;
    serial_print(graphical ? "SYSTEMC: FRAMEBUFFER_OK\r\n" : "SYSTEMC: TEXT_FALLBACK\r\n");
    for(i=0;i<PAINT_W*PAINT_H;i++)paint_pixels[i]=paint_background;
    calc_line[0]=0;calc_result[0]=0;terminal_line[0]=0;terminal_output[0]=0;set_terminal_output("TYPE HELP FOR COMMANDS");terminal_len=0;calc_len=0;notepad_buffer[0]=0;notepad_len=0;browser_text[0]=0;browser_text_len=0;paint_x=PAINT_W/2;paint_y=PAINT_H/2;
    locate_install_image(mbi);
    if(flags&(1u<<2)){cmdptr=*(const u32*)(mbi+16);if(cmdptr)installer_mode=contains_install((const char*)cmdptr);}
    net_init();
    if(graphical)mouse_init();
    screen_stage=installer_mode?2:0;
    if(installer_mode){active_app=10;installer_stage=0;installer_drive=-1;installer_confirm_len=0;ata_scan();serial_print("SYSTEMC: INSTALLER_READY\r\n");}
    else active_app=0;
    if(text_mode)textmode_draw();else render_app();
    serial_print(installer_mode?"SYSTEMC: INSTALLER_READY\r\n":"SYSTEMC: FIRST_BOOT_SETUP_READY\r\n");
    for(;;){int key;int changed=0;if(graphical){changed=poll_mouse();if(changed)render_app();}key=next_key();if(key>=0){handle_key(key);if(text_mode)textmode_draw();else render_app();}else{net_poll();if(++clock_poll_counter>=1000000u){u8 h,m;clock_poll_counter=0;if(rtc_read_hour_minute(&h,&m)&&(h!=clock_shown_hour||m!=clock_shown_minute)){if(graphical)render_app();else textmode_draw();}}__asm__ volatile("pause");}}
}
