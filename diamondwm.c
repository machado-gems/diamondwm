#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <dirent.h>
#include <pwd.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>

#define PANEL_HEIGHT 50
#define BORDER_WIDTH 1
#define TITLEBAR_HEIGHT 30
#define BUTTON_SIZE 12
#define BUTTON_SPACING 5
#define FRAME_BORDER 5
#define RESIZE_HANDLE_SIZE 8
#define MENU_WIDTH 120
#define MENU_ITEM_HEIGHT 30
#define CORNER_RADIUS 8
#define SHADOW_OFFSET 4
#define SHADOW_BLUR 8
#define ANIMATION_STEPS 10
#define ANIMATION_DELAY 5000

// Debug logging function
void debug_log(const char* format, ...) {
    FILE* log_file = fopen("/tmp/diamondwm_debug.log", "a");
    if (!log_file) return;

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[20];
    strftime(timestamp, 20, "%H:%M:%S", tm_info);

    fprintf(log_file, "[%s] ", timestamp);

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fprintf(log_file, "\n");
    fflush(log_file);
    fclose(log_file);
}

typedef struct {
    Window win;
    Window frame;
    int x, y;
    int width, height;
    int is_mapped;
    int is_fullscreen;
    int original_x, original_y;
    int original_width, original_height;
    char *title;
    int is_active;
    int button_hover; // 0=none, 1=close, 2=minimize, 3=maximize
} Client;

typedef struct {
    Window win;
    int x, y;
    int width, height;
} Panel;

typedef struct {
    Window win;
    int visible;
    int x, y;
    int width, height;
    int hover_item;
    float alpha;
} Menu;

typedef struct {
    char *name;
    char *exec;
    char *icon_path;
    Window icon_win;
    int x_position;
} PinnedApp;

typedef struct {
    PinnedApp *apps;
    int app_count;
    int max_apps;
} PinnedAppsList;

Display *dpy;
Window root;
int screen;
GC gc, panel_gc, title_gc, button_gc, close_gc, minimize_gc, maximize_gc, text_gc, menu_gc;
Client *clients[100];
int client_count = 0;
Panel panel;
Menu menu;
PinnedAppsList pinned_apps = {0};
int panel_dragging = 0;
int drag_start_x, drag_start_y;

int window_dragging = 0;
Client *dragged_client = NULL;
int drag_win_start_x, drag_win_start_y;
int drag_offset_x, drag_offset_y;

int window_resizing = 0;
Client *resized_client = NULL;
int resize_start_x, resize_start_y;
int resize_start_width, resize_start_height;
int resize_edge;

// Modern color palette
unsigned long black, white, dark_gray, light_gray, red, dark_blue;
unsigned long titlebar_gray, titlebar_active, button_red, button_yellow, button_green, purple_color;
unsigned long menu_bg, menu_hover_bg, accent_color, shadow_color;
unsigned long bg_gradient_start, bg_gradient_end;

// Enhanced color scheme
unsigned long accent_light, background_dark, background_light;
unsigned long text_primary, text_secondary;

typedef struct {
    Window win;
    int visible;
    int x, y;
    int width, height;
    int hover_item;
    float alpha;
    Client *target_client;
} WindowControlMenu;

WindowControlMenu window_control_menu;

typedef struct {
    Window win;
    int visible;
    int x, y;
    int width, height;
    int hover_item;
    float alpha;
    PinnedApp *target_pinned_app;
} PinnedAppMenu;

PinnedAppMenu pinned_app_menu;

typedef struct {
    char *name;
    char *exec;
    char *icon;
    char *categories;
    char *comment;
} AppInfo;

typedef struct {
    char *category_name;
    AppInfo *apps;
    int app_count;
    int expanded;
} AppCategory;

typedef struct {
    Window win;
    int visible;
    int x, y;
    int width, height;
    AppCategory *categories;
    int category_count;
    int hover_item;
    float alpha;
    char search_text[256];
    int search_mode;
} AppLauncherMenu;

AppLauncherMenu app_launcher;

typedef struct {
    XftFont *regular;
    XftFont *bold;
    XftFont *title;
} XftFontSystem;

// Add these global variables after your existing ones
XftFontSystem xft_fonts;
XftDraw *xft_draw = NULL;
XftColor xft_color, xft_panel_color, xft_menu_color, xft_title_color;

// Function declarations
void create_panel();
void draw_panel();
void draw_window_decorations(Client *c);
void handle_button_press(XButtonEvent *e);
void handle_button_release(XButtonEvent *e);
void handle_motion_notify(XMotionEvent *e);
void handle_key_press(XKeyEvent *e);
void manage_window(Window w);
void unmanage_window(Window w);
void toggle_fullscreen(Client *c);
void resize_window(Client *c, int width, int height);
void move_window(Client *c, int x, int y);
void lower_window(Client *c);
void close_window(Client *c);
Client* find_client(Window w);
char* get_window_title(Window w);
void setup_mouse_cursor();
int is_in_close_button(Client *c, int x, int y);
int is_in_minimize_button(Client *c, int x, int y);
int is_in_maximize_button(Client *c, int x, int y);
int is_in_titlebar(Client *c, int x, int y);
int get_resize_edge(Client *c, int x, int y);
void send_wm_delete(Window w);
void draw_clock();
void draw_diamond_icon(int x, int y, int size);
int is_window_visible(Client *c);
void check_clock_update();
void create_menu();
void show_menu();
void hide_menu();
void draw_menu();
int is_in_diamondwm_area(int x, int y);
int is_in_menu_area(int x, int y);
int get_menu_item_at(int x, int y);
void load_applications();
void create_app_launcher();
void show_app_launcher(int x, int y);
void hide_app_launcher();
void draw_app_launcher();
int is_in_app_launcher_area(int x, int y);
int get_app_launcher_item_at(int x, int y);
void handle_app_launcher_click(int x, int y);
void free_applications();
void draw_rounded_rectangle(Drawable d, GC gc, int x, int y, int w, int h, int r);
void draw_shadow(Drawable d, int x, int y, int w, int h);
void animate_window_move(Client *c, int target_x, int target_y);
void animate_window_resize(Client *c, int target_w, int target_h);
void draw_gradient_rect(Drawable d, GC gc, int x, int y, int w, int h, unsigned long c1, unsigned long c2, int vertical);
void draw_glow_button(Drawable d, int x, int y, int size, unsigned long color, int hover);
void update_button_hover(Client *c, int x, int y);
void show_operation_feedback(const char* message);
void create_window_control_menu();
void show_window_control_menu(int x, int y, Client *c);
void hide_window_control_menu();
void draw_window_control_menu();
void draw_app_launcher_text(int x, int y, const char *text, unsigned long color, XftFont *font);
void filter_applications_by_search();
void lock_screen();

// Pinned apps functions
char* get_panel_config_path();
void ensure_config_dir();
void save_pinned_apps();
void load_pinned_apps();
int is_app_pinned(Client *c);
void pin_app_to_panel(Client *c);
void unpin_app(PinnedApp *app);
void create_pinned_app_menu();
void show_pinned_app_menu(int x, int y, PinnedApp *app);
void hide_pinned_app_menu();
void draw_pinned_app_menu();
void free_pinned_apps();

// Xft font loading (for anti-aliased fonts)
void load_xft_fonts() {
    debug_log("Loading Xft fonts...");

    // Try modern fonts first, then fallbacks
    const char* font_families[] = {
        "Inter", "SF Pro Display", "Segoe UI", "Roboto", "Ubuntu", "DejaVu Sans", "Liberation Sans", "sans-serif", NULL
    };

    xft_fonts.regular = NULL;
    for (int i = 0; font_families[i] && !xft_fonts.regular; i++) {
        xft_fonts.regular = XftFontOpen(dpy, screen,
            XFT_FAMILY, XftTypeString, font_families[i],
            XFT_SIZE, XftTypeDouble, 8.0,
            XFT_WEIGHT, XftTypeInteger, 400, // Medium
            NULL);
        if (xft_fonts.regular) debug_log("Loaded Xft regular: %s", font_families[i]);
    }

    xft_fonts.bold = NULL;
    for (int i = 0; font_families[i] && !xft_fonts.bold; i++) {
        xft_fonts.bold = XftFontOpen(dpy, screen,
            XFT_FAMILY, XftTypeString, font_families[i],
            XFT_SIZE, XftTypeDouble, 9.0,
            XFT_WEIGHT, XftTypeInteger, 700, // Bold
            NULL);
        if (xft_fonts.bold) debug_log("Loaded Xft bold: %s", font_families[i]);
    }

    xft_fonts.title = NULL;
    for (int i = 0; font_families[i] && !xft_fonts.title; i++) {
        xft_fonts.title = XftFontOpen(dpy, screen,
            XFT_FAMILY, XftTypeString, font_families[i],
            XFT_SIZE, XftTypeDouble, 10.0,
            XFT_WEIGHT, XftTypeInteger, 600, // Semi-bold
            NULL);
        if (xft_fonts.title) debug_log("Loaded Xft title: %s", font_families[i]);
    }

    if (!xft_fonts.regular) {
        debug_log("WARNING: No Xft fonts available, using basic fonts");
    }
}

// Draw text with Xft (anti-aliased)
void draw_xft_text(Drawable d, int x, int y, const char *text, XftFont *font, unsigned long color) {
    if (!font) return;

    if (!xft_draw) {
        xft_draw = XftDrawCreate(dpy, d, DefaultVisual(dpy, screen),
                                 DefaultColormap(dpy, screen));
    }

    if (!xft_draw) {
        debug_log("ERROR: Could not create Xft draw");
        return;
    }

    // Convert color to XftColor
    XRenderColor render_color;
    render_color.red = ((color >> 16) & 0xFF) * 257;
    render_color.green = ((color >> 8) & 0xFF) * 257;
    render_color.blue = (color & 0xFF) * 257;
    render_color.alpha = 0xFFFF;

    if (XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                          DefaultColormap(dpy, screen),
                          &render_color, &xft_color)) {

        XftDrawStringUtf8(xft_draw, &xft_color, font, x, y + font->ascent,
                         (XftChar8 *)text, strlen(text));

        XftColorFree(dpy, DefaultVisual(dpy, screen),
                    DefaultColormap(dpy, screen), &xft_color);
    }
}

void set_background() {
    system("feh --bg-scale /usr/share/backgrounds/* 2>/dev/null &");

    Pixmap bg_pixmap = XCreatePixmap(dpy, root,
                                    DisplayWidth(dpy, screen),
                                    DisplayHeight(dpy, screen),
                                    DefaultDepth(dpy, screen));

    GC bg_gc = XCreateGC(dpy, bg_pixmap, 0, NULL);

    // Modern gradient background (dark purple to dark blue)
    for (int y = 0; y < DisplayHeight(dpy, screen); y++) {
        float ratio = (float)y / DisplayHeight(dpy, screen);
        int r = (int)(10 + ratio * 5);
        int g = (int)(10 + ratio * 15);
        int b = (int)(20 + ratio * 30);
        unsigned long color = (r << 16) | (g << 8) | b;
        XSetForeground(dpy, bg_gc, color);
        XDrawLine(dpy, bg_pixmap, bg_gc, 0, y, DisplayWidth(dpy, screen), y);
    }

    XSetWindowBackgroundPixmap(dpy, root, bg_pixmap);
    XClearWindow(dpy, root);

    XFreeGC(dpy, bg_gc);
    XFreePixmap(dpy, bg_pixmap);
}

void show_operation_feedback(const char* message) {
    // Create a temporary toast notification
    Window toast = XCreateSimpleWindow(dpy, root,
                                      DisplayWidth(dpy, screen)/2 - 100,
                                      DisplayHeight(dpy, screen)/2,
                                      200, 40, 0, accent_color, background_dark);
    XSelectInput(dpy, toast, ExposureMask);
    XMapWindow(dpy, toast);

    // Draw message
    XGCValues toast_gc_vals;
    toast_gc_vals.foreground = text_primary;
    toast_gc_vals.background = background_dark;
    toast_gc_vals.font = XLoadFont(dpy, "fixed");
    GC toast_gc = XCreateGC(dpy, toast, GCForeground | GCBackground | GCFont, &toast_gc_vals);

    XSetForeground(dpy, toast_gc, text_primary);
    XDrawString(dpy, toast, toast_gc, 10, 25, message, strlen(message));
    XFlush(dpy);

    // Auto-hide after delay
    usleep(1500000);
    XDestroyWindow(dpy, toast);
    XFreeGC(dpy, toast_gc);
}

void draw_gradient_rect(Drawable d, GC gc, int x, int y, int w, int h, unsigned long c1, unsigned long c2, int vertical) {
    for (int i = 0; i < (vertical ? h : w); i++) {
        float ratio = (float)i / (vertical ? h : w);

        int r1 = (c1 >> 16) & 0xFF;
        int g1 = (c1 >> 8) & 0xFF;
        int b1 = c1 & 0xFF;

        int r2 = (c2 >> 16) & 0xFF;
        int g2 = (c2 >> 8) & 0xFF;
        int b2 = c2 & 0xFF;

        int r = r1 + (int)((r2 - r1) * ratio);
        int g = g1 + (int)((g2 - g1) * ratio);
        int b = b1 + (int)((b2 - b1) * ratio);

        unsigned long color = (r << 16) | (g << 8) | b;
        XSetForeground(dpy, gc, color);

        if (vertical) {
            XDrawLine(dpy, d, gc, x, y + i, x + w, y + i);
        } else {
            XDrawLine(dpy, d, gc, x + i, y, x + i, y + h);
        }
    }
}

void draw_rounded_rectangle(Drawable d, GC gc, int x, int y, int w, int h, int r) {
    // Draw main rectangle
    XFillRectangle(dpy, d, gc, x + r, y, w - 2*r, h);
    XFillRectangle(dpy, d, gc, x, y + r, w, h - 2*r);

    // Draw corners using arcs
    XFillArc(dpy, d, gc, x, y, 2*r, 2*r, 90*64, 90*64);
    XFillArc(dpy, d, gc, x + w - 2*r, y, 2*r, 2*r, 0, 90*64);
    XFillArc(dpy, d, gc, x, y + h - 2*r, 2*r, 2*r, 180*64, 90*64);
    XFillArc(dpy, d, gc, x + w - 2*r, y + h - 2*r, 2*r, 2*r, 270*64, 90*64);
}

void draw_shadow(Drawable d, int x, int y, int w, int h) {
    // Simple shadow effect using multiple rectangles with decreasing opacity
    for (int i = 0; i < SHADOW_BLUR; i++) {
        int alpha = 255 - (i * 32);
        unsigned long shadow = 0x000000 | ((alpha / 16) << 16) | ((alpha / 16) << 8) | (alpha / 16);

        XSetForeground(dpy, gc, shadow);
        XDrawRectangle(dpy, d, gc,
                      x + SHADOW_OFFSET - i,
                      y + SHADOW_OFFSET - i,
                      w + 2*i,
                      h + 2*i);
    }
}

void animate_window_move(Client *c, int target_x, int target_y) {
    int start_x = c->x;
    int start_y = c->y;

    // Use cubic easing for smoother animation
    for (int step = 1; step <= ANIMATION_STEPS; step++) {
        float t = (float)step / ANIMATION_STEPS;
        // Cubic ease-out
        t--;
        float progress = t*t*t + 1;

        int new_x = start_x + (int)((target_x - start_x) * progress);
        int new_y = start_y + (int)((target_y - start_y) * progress);

        XMoveWindow(dpy, c->frame, new_x, new_y);
        XFlush(dpy);
        usleep(ANIMATION_DELAY / ANIMATION_STEPS);
    }
    c->x = target_x;
    c->y = target_y;
}

void animate_window_resize(Client *c, int target_w, int target_h) {
    int start_w = c->width;
    int start_h = c->height;

    for (int step = 1; step <= ANIMATION_STEPS; step++) {
        float progress = (float)step / ANIMATION_STEPS;
        progress = 1 - (1 - progress) * (1 - progress);

        int new_w = start_w + (int)((target_w - start_w) * progress);
        int new_h = start_h + (int)((target_h - start_h) * progress);

        c->width = new_w;
        c->height = new_h;

        XResizeWindow(dpy, c->frame, new_w, new_h);
        XResizeWindow(dpy, c->win, new_w - 2 * FRAME_BORDER,
                     new_h - TITLEBAR_HEIGHT - 2 * FRAME_BORDER);

        draw_window_decorations(c);
        XFlush(dpy);
        usleep(ANIMATION_DELAY);
    }
}

void draw_glow_button(Drawable d, int x, int y, int size, unsigned long color, int hover) {
    if (hover) {
        // Enhanced glow with multiple layers
        for (int i = 3; i > 0; i--) {
            int alpha = 120 - (i * 30);
            // Simulate alpha by blending with background
            int r = (color >> 16) & 0xFF;
            int g = (color >> 8) & 0xFF;
            int b = color & 0xFF;

            // Blend with dark background
            r = (r * alpha + 0x2D * (255 - alpha)) / 255;
            g = (g * alpha + 0x2D * (255 - alpha)) / 255;
            b = (b * alpha + 0x2D * (255 - alpha)) / 255;

            unsigned long glow_color = (r << 16) | (g << 8) | b;
            XSetForeground(dpy, button_gc, glow_color);

            XFillArc(dpy, d, button_gc,
                    x - i, y - i,
                    size + 2*i, size + 2*i,
                    0, 360*64);
        }
    }

    // Main button
    XSetForeground(dpy, button_gc, color);
    XPoint diamond[] = {
        {x + size/2, y},
        {x + size, y + size/2},
        {x + size/2, y + size},
        {x, y + size/2}
    };
    XFillPolygon(dpy, d, button_gc, diamond, 4, Convex, CoordModeOrigin);

    // Inner highlight for 3D effect
    if (hover) {
        XSetForeground(dpy, button_gc, white);
        XDrawLine(dpy, d, button_gc, x + size/2, y + 1, x + size - 1, y + size/2);
    }
}

void update_button_hover(Client *c, int x, int y) {
    int old_hover = c->button_hover;
    c->button_hover = 0;

    if (is_in_close_button(c, x, y)) {
        c->button_hover = 1;
    } else if (is_in_minimize_button(c, x, y)) {
        c->button_hover = 2;
    } else if (is_in_maximize_button(c, x, y)) {
        c->button_hover = 3;
    }

    if (old_hover != c->button_hover) {
        draw_window_decorations(c);
    }
}

int is_window_visible(Client *c) {
    if (!c) return 0;

    int screen_width = DisplayWidth(dpy, screen);
    int screen_height = DisplayHeight(dpy, screen);

    if (c->x >= screen_width || c->y >= screen_height) {
        return 0;
    }

    if (c->x + c->width <= 0 || c->y + c->height <= 0) {
        return 0;
    }

    return 1;
}

void draw_diamond_icon(int x, int y, int size) {
    int center_x = x + size / 2;
    int center_y = y + size / 2;
    float scale_factor = size / 32.0f; // Scale based on original 32px design

    // 1. Outer glow effect (multi-layered aurora)
    for (int i = 6; i > 0; i--) {
        float alpha = (float)i / 6.0f;
        int glow_offset = (int)(i * 2 * scale_factor);

        // Aurora effect - purple to blue gradient
        int r = (int)(138 * alpha + (66 * (1 - alpha)));
        int g = (int)(43 * alpha + (92 * (1 - alpha)));
        int b = (int)(226 * alpha + (231 * (1 - alpha)));
        unsigned long glow_color = (r << 16) | (g << 8) | b;

        XSetForeground(dpy, panel_gc, glow_color);
        XPoint glow[] = {
            {center_x, y - glow_offset},
            {x + size + glow_offset, center_y},
            {center_x, y + size + glow_offset},
            {x - glow_offset, center_y}
        };
        XDrawLines(dpy, panel.win, panel_gc, glow, 4, CoordModeOrigin);
    }

    // 2. Main diamond body with gradient facets
    // Pavilion (bottom part - darker base)
    int pav_offset = (int)(6 * scale_factor);
    XPoint pavilion[] = {
        {center_x, center_y + (int)(3 * scale_factor)},
        {x + size - pav_offset, center_y},
        {center_x, y + size},
        {x + pav_offset, center_y}
    };
    XSetForeground(dpy, panel_gc, 0x5D3BA8); // Deep purple
    XFillPolygon(dpy, panel.win, panel_gc, pavilion, 4, Convex, CoordModeOrigin);

    // 3. Crown facets (upper part with highlights)
    // Left facet
    int crown_height = (int)(10 * scale_factor);
    XPoint left_facet[] = {
        {center_x, y + crown_height},
        {x + pav_offset, center_y},
        {center_x, center_y + (int)(3 * scale_factor)}
    };
    XSetForeground(dpy, panel_gc, 0x7C3AED); // Medium purple
    XFillPolygon(dpy, panel.win, panel_gc, left_facet, 3, Convex, CoordModeOrigin);

    // Right facet (highlight area)
    XPoint right_facet[] = {
        {center_x, y + crown_height},
        {center_x, center_y + (int)(3 * scale_factor)},
        {x + size - pav_offset, center_y}
    };
    XSetForeground(dpy, panel_gc, 0x9D7BF5); // Light purple highlight
    XFillPolygon(dpy, panel.win, panel_gc, right_facet, 3, Convex, CoordModeOrigin);

    // 4. Table (top flat surface)
    int table_width = (int)(8 * scale_factor);
    int table_height = (int)(3 * scale_factor);
    XPoint table[] = {
        {center_x - table_width, y + crown_height},
        {center_x + table_width, y + crown_height},
        {center_x + table_width - (int)(2 * scale_factor), center_y - table_height},
        {center_x - table_width + (int)(2 * scale_factor), center_y - table_height}
    };
    XSetForeground(dpy, panel_gc, 0xA78BFA); // Lightest purple
    XFillPolygon(dpy, panel.win, panel_gc, table, 4, Convex, CoordModeOrigin);

    // 5. Sparkle highlights (realistic light reflection)
    XSetForeground(dpy, panel_gc, 0xFFFFFF); // Pure white
    // Main sparkle
    int sparkle_size = (int)(3 * scale_factor);
    if (sparkle_size > 0) {
        XFillArc(dpy, panel.win, panel_gc,
                center_x - sparkle_size/2, y + (int)(8 * scale_factor),
                sparkle_size, sparkle_size, 0, 360*64);
    }
    // Secondary sparkles
    XDrawPoint(dpy, panel.win, panel_gc, center_x + (int)(5 * scale_factor), y + (int)(12 * scale_factor));
    XDrawPoint(dpy, panel.win, panel_gc, center_x - (int)(4 * scale_factor), center_y - (int)(2 * scale_factor));

    // 6. Edge highlights for 3D depth
    XSetForeground(dpy, panel_gc, 0xE9D5FF); // Very light purple
    XDrawLine(dpy, panel.win, panel_gc, center_x, y + crown_height,
              center_x + table_width, y + crown_height); // Top edge
    XDrawLine(dpy, panel.win, panel_gc, center_x + table_width, y + crown_height,
              center_x + table_width - (int)(2 * scale_factor), center_y - table_height); // Right-top edge

    // 7. Pavilion shadow for depth
    XSetForeground(dpy, panel_gc, 0x4C2889); // Dark purple shadow
    XDrawLine(dpy, panel.win, panel_gc,
              center_x - (int)(4 * scale_factor), y + size - (int)(2 * scale_factor),
              center_x + (int)(4 * scale_factor), y + size - (int)(2 * scale_factor));
}

void draw_clock() {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[6];

    strftime(time_str, sizeof(time_str), "%H:%M", tm_info);

    // Use anti-aliased font for clock
    if (xft_fonts.regular) {
        XGlyphInfo extents;
        XftTextExtents8(dpy, xft_fonts.regular, (XftChar8 *)time_str, strlen(time_str), &extents);
        int text_width = extents.width;
        int x = 40 + panel.width - text_width - 180;
        int y = 17;

        draw_app_launcher_text(x, y, time_str, text_primary, xft_fonts.regular);
    } else {
        // Fallback
        int text_width = XTextWidth(XLoadQueryFont(dpy, "fixed"), time_str, strlen(time_str));
        int x = panel.width - text_width - 180;
        XSetForeground(dpy, panel_gc, text_primary);
        XDrawString(dpy, panel.win, panel_gc, x, 30, time_str, strlen(time_str));
    }
}

void check_clock_update() {
    static time_t last_update = 0;
    time_t now = time(NULL);

    if (now != last_update) {
        last_update = now;
        draw_panel();
    }
}

void create_menu() {
    menu.width = MENU_WIDTH;
    menu.height = MENU_ITEM_HEIGHT * 4;
    menu.x = panel.width - menu.width - 20;
    menu.y = panel.y - menu.height;
    menu.hover_item = -1;
    menu.alpha = 0.0f;

    menu.win = XCreateSimpleWindow(dpy, root, menu.x, menu.y,
                                  menu.width, menu.height, 0,
                                  white, dark_blue);

    XSelectInput(dpy, menu.win, ButtonPressMask | ExposureMask | PointerMotionMask);
    menu.visible = 0;
}

void show_menu() {
    if (!menu.visible) {
        int diamondwm_width = 120;
        menu.x = panel.width - diamondwm_width - 20;
        menu.y = panel.y - menu.height - 5;

        XMoveWindow(dpy, menu.win, menu.x, menu.y);
        XMapWindow(dpy, menu.win);
        XRaiseWindow(dpy, menu.win);
        menu.visible = 1;

        // Fade-in animation
        for (int i = 0; i <= 10; i++) {
            menu.alpha = (float)i / 10.0f;
            draw_menu();
            XFlush(dpy);
            usleep(10000);
        }

        XGrabPointer(dpy, root, False, ButtonPressMask, GrabModeAsync,
                    GrabModeAsync, None, None, CurrentTime);
    }
}

void hide_menu() {
    if (menu.visible) {
        // Fade-out animation
        for (int i = 10; i >= 0; i--) {
            menu.alpha = (float)i / 10.0f;
            draw_menu();
            XFlush(dpy);
            usleep(8000);
        }

        XUnmapWindow(dpy, menu.win);
        menu.visible = 0;
        menu.hover_item = -1;
        XUngrabPointer(dpy, CurrentTime);
    }
}

void draw_menu() {
    if (!menu.visible) return;

    XClearWindow(dpy, menu.win);

    // Draw rounded background with gradient
    XSetForeground(dpy, menu_gc, menu_bg);
    draw_rounded_rectangle(menu.win, menu_gc, 0, 0, menu.width, menu.height, CORNER_RADIUS);

    // Draw subtle border
    XSetForeground(dpy, menu_gc, accent_color);
    XDrawRectangle(dpy, menu.win, menu_gc, 1, 1, menu.width - 3, menu.height - 3);

    char *items[] = {"Terminal", "Lock", "Logout", "Shutdown"};

    for (int i = 0; i < 4; i++) {
        int y = i * MENU_ITEM_HEIGHT;

        // Hover effect
        if (menu.hover_item == i) {
            XSetForeground(dpy, menu_gc, menu_hover_bg);
            draw_rounded_rectangle(menu.win, menu_gc, 2, y + 2, menu.width - 4, MENU_ITEM_HEIGHT - 4, 4);
        }

        if (i > 0) {
            XSetForeground(dpy, menu_gc, 0x404040);
            XDrawLine(dpy, menu.win, menu_gc, 10, y, menu.width - 10, y);
        }

        XSetForeground(dpy, menu_gc, text_primary);
        int text_width = XTextWidth(XLoadQueryFont(dpy, "fixed"), items[i], strlen(items[i]));
        int text_x = (menu.width - text_width) / 2;
        int text_y = y + (MENU_ITEM_HEIGHT / 2) + 5;

        XDrawString(dpy, menu.win, menu_gc, text_x, text_y, items[i], strlen(items[i]));
    }
}

int is_in_diamondwm_area(int x, int y) {
    int diamond_size = 20;
    int text_width = XTextWidth(XLoadQueryFont(dpy, "fixed"), "DiamondWM", 9);
    int total_width = diamond_size + 5 + text_width;

    int area_x = panel.width - total_width - 20;
    int area_y = 10;
    int area_height = 30;

    return (x >= area_x && x <= area_x + total_width &&
            y >= area_y && y <= area_y + area_height);
}

int is_in_menu_area(int x, int y) {
    if (!menu.visible) return 0;

    return (x >= menu.x && x <= menu.x + menu.width &&
            y >= menu.y && y <= menu.y + menu.height);
}

int get_menu_item_at(int x, int y) {
    if (!menu.visible) return -1;

    int relative_x = x - menu.x;
    int relative_y = y - menu.y;

    if (relative_x < 0 || relative_x > menu.width ||
        relative_y < 0 || relative_y > menu.height) {
        return -1;
    }

    return relative_y / MENU_ITEM_HEIGHT;
}

void load_applications() {
    debug_log("=== STARTING APPLICATION LOADING ===");

    const char *desktop_dirs[] = {
        "/usr/share/applications",
        "/usr/local/share/applications",
        "/home/%s/.local/share/applications",
        NULL
    };

    typedef struct {
        const char *category;
        const char *name;
    } CategoryMap;

    CategoryMap category_map[] = {
        {"AudioVideo", "Multimedia"},
        {"Audio", "Audio"},
        {"Video", "Video"},
        {"Development", "Development"},
        {"Education", "Education"},
        {"Game", "Games"},
        {"Graphics", "Graphics"},
        {"Network", "Internet"},
        {"Office", "Office"},
        {"Science", "Science"},
        {"Settings", "Settings"},
        {"System", "System"},
        {"Utility", "Utilities"},
        {"GTK", "GTK Apps"},
        {"Qt", "Qt Apps"},
        {"XFCE", "XFCE Apps"},
        {"GNOME", "GNOME Apps"},
        {"KDE", "KDE Apps"},
        {NULL, NULL}
    };

    char username[100];
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        strncpy(username, pw->pw_name, sizeof(username)-1);
        username[sizeof(username)-1] = '\0';
    } else {
        strcpy(username, "user");
    }
    debug_log("Loading apps for user: %s", username);

    int total_files = 0;
    for (int i = 0; desktop_dirs[i] != NULL; i++) {
        char path[512];
        if (strstr(desktop_dirs[i], "%s")) {
            snprintf(path, sizeof(path), desktop_dirs[i], username);
        } else {
            strcpy(path, desktop_dirs[i]);
        }

        debug_log("Scanning directory: %s", path);

        DIR *dir = opendir(path);
        if (!dir) {
            debug_log("  Cannot open directory: %s", path);
            continue;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".desktop")) {
                total_files++;
            }
        }
        closedir(dir);
    }

    debug_log("Found %d total .desktop files", total_files);

    if (total_files == 0) {
        debug_log("ERROR: No .desktop files found!");
        app_launcher.category_count = 0;
        app_launcher.categories = NULL;
        return;
    }

    AppInfo *all_apps = malloc(sizeof(AppInfo) * total_files);
    if (!all_apps) {
        debug_log("ERROR: Failed to allocate memory for %d apps", total_files);
        return;
    }
    memset(all_apps, 0, sizeof(AppInfo) * total_files);

    int app_index = 0;

    for (int i = 0; desktop_dirs[i] != NULL; i++) {
        char path[512];
        if (strstr(desktop_dirs[i], "%s")) {
            snprintf(path, sizeof(path), desktop_dirs[i], username);
        } else {
            strcpy(path, desktop_dirs[i]);
        }

        DIR *dir = opendir(path);
        if (!dir) continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".desktop")) {
                char filepath[1024];
                snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);

                FILE *file = fopen(filepath, "r");
                if (!file) {
                    debug_log("  Cannot open file: %s", filepath);
                    continue;
                }

                AppInfo app = {0};
                char line[512];
                int in_desktop_entry = 0;
                int is_application = 0;
                int hidden = 0;
                int no_display = 0;

                debug_log("  Parsing: %s", entry->d_name);

                while (fgets(line, sizeof(line), file)) {
                    line[strcspn(line, "\n")] = 0;
                    char *end = line + strlen(line) - 1;
                    while (end > line && isspace(*end)) end--;
                    *(end + 1) = 0;

                    if (strcmp(line, "[Desktop Entry]") == 0) {
                        in_desktop_entry = 1;
                        continue;
                    } else if (line[0] == '[' && line[strlen(line)-1] == ']') {
                        in_desktop_entry = 0;
                        continue;
                    }

                    if (!in_desktop_entry) continue;

                    if (strncmp(line, "Type=", 5) == 0) {
                        if (strstr(line + 5, "Application")) {
                            is_application = 1;
                        } else {
                            break;
                        }
                    }

                    if (strncmp(line, "Hidden=", 7) == 0) {
                        if (strstr(line + 7, "true")) {
                            hidden = 1;
                            break;
                        }
                    }

                    if (strncmp(line, "NoDisplay=", 10) == 0) {
                        if (strstr(line + 10, "true")) {
                            no_display = 1;
                            break;
                        }
                    }

                    if (strncmp(line, "Name=", 5) == 0 && !app.name) {
                        app.name = strdup(line + 5);
                    }

                    if (strncmp(line, "Exec=", 5) == 0 && !app.exec) {
                        char *exec_cmd = line + 5;
                        char clean_exec[512];
                        char *dest = clean_exec;

                        for (char *src = exec_cmd; *src && (dest - clean_exec) < 510; src++) {
                            if (*src == '%' && *(src+1)) {
                                src++;
                                continue;
                            }
                            if (*src == ' ' && (src == exec_cmd || *(src-1) == ' ')) {
                                continue;
                            }
                            *dest++ = *src;
                        }
                        *dest = '\0';

                        if (dest > clean_exec && *(dest-1) == ' ') {
                            *(dest-1) = '\0';
                        }

                        app.exec = strdup(clean_exec);
                    }

                    if (strncmp(line, "Categories=", 11) == 0 && !app.categories) {
                        app.categories = strdup(line + 11);
                    }

                    if (strncmp(line, "Comment=", 8) == 0 && !app.comment) {
                        app.comment = strdup(line + 8);
                    }

                    if (strncmp(line, "Icon=", 5) == 0 && !app.icon) {
                        app.icon = strdup(line + 5);
                    }
                }

                fclose(file);

                if (is_application && !hidden && !no_display && app.name && app.exec) {
                    if (!app.categories) {
                        app.categories = strdup("Utility");
                    }

                    if (!app.comment) {
                        app.comment = strdup("");
                    }

                    if (!app.icon) {
                        app.icon = strdup("");
                    }

                    all_apps[app_index++] = app;
                    debug_log("    ✓ Loaded: %s -> %s (Categories: %s)",
                             app.name, app.exec, app.categories);
                } else {
                    debug_log("    ✗ Skipped: %s (is_app=%d, hidden=%d, no_display=%d, has_name=%d, has_exec=%d)",
                             entry->d_name, is_application, hidden, no_display,
                             app.name != NULL, app.exec != NULL);
                    if (app.name) free(app.name);
                    if (app.exec) free(app.exec);
                    if (app.categories) free(app.categories);
                    if (app.comment) free(app.comment);
                    if (app.icon) free(app.icon);
                }
            }
        }
        closedir(dir);
    }

    debug_log("Successfully parsed %d applications", app_index);

    if (app_index == 0) {
        debug_log("ERROR: No valid applications found after parsing!");
        free(all_apps);
        app_launcher.category_count = 0;
        app_launcher.categories = NULL;
        return;
    }

    int category_count = 0;
    for (int i = 0; category_map[i].category != NULL; i++) {
        category_count++;
    }

    app_launcher.categories = malloc(sizeof(AppCategory) * category_count);
    if (!app_launcher.categories) {
        debug_log("ERROR: Failed to allocate categories");
        for (int i = 0; i < app_index; i++) {
            free(all_apps[i].name);
            free(all_apps[i].exec);
            free(all_apps[i].categories);
            free(all_apps[i].comment);
            free(all_apps[i].icon);
        }
        free(all_apps);
        return;
    }

    app_launcher.category_count = category_count;

    for (int i = 0; i < category_count; i++) {
        app_launcher.categories[i].category_name = strdup(category_map[i].name);
        app_launcher.categories[i].apps = NULL;
        app_launcher.categories[i].app_count = 0;
        app_launcher.categories[i].expanded = 0;
    }

    for (int i = 0; i < app_index; i++) {
        char *categories = strdup(all_apps[i].categories);
        char *token = strtok(categories, ";");

        while (token) {
            char clean_token[100];
            char *dest = clean_token;
            for (char *src = token; *src && (dest - clean_token) < 99; src++) {
                if (!isspace(*src)) {
                    *dest++ = *src;
                }
            }
            *dest = '\0';

            for (int j = 0; j < category_count; j++) {
                if (strcasecmp(clean_token, category_map[j].category) == 0) {
                    app_launcher.categories[j].app_count++;
                    break;
                }
            }
            token = strtok(NULL, ";");
        }
        free(categories);
    }

    for (int i = 0; i < category_count; i++) {
        if (app_launcher.categories[i].app_count > 0) {
            app_launcher.categories[i].apps = malloc(sizeof(AppInfo) * app_launcher.categories[i].app_count);
            if (!app_launcher.categories[i].apps) {
                debug_log("ERROR: Failed to allocate apps for category %s",
                         app_launcher.categories[i].category_name);
                app_launcher.categories[i].app_count = 0;
            } else {
                app_launcher.categories[i].app_count = 0;
            }
        }
    }

    for (int i = 0; i < app_index; i++) {
        char *categories = strdup(all_apps[i].categories);
        char *token = strtok(categories, ";");
        int assigned = 0;

        while (token && !assigned) {
            char clean_token[100];
            char *dest = clean_token;
            for (char *src = token; *src && (dest - clean_token) < 99; src++) {
                if (!isspace(*src)) {
                    *dest++ = *src;
                }
            }
            *dest = '\0';

            for (int j = 0; j < category_count; j++) {
                if (strcasecmp(clean_token, category_map[j].category) == 0) {
                    if (app_launcher.categories[j].apps) {
                        int idx = app_launcher.categories[j].app_count++;
                        app_launcher.categories[j].apps[idx] = all_apps[i];
                        assigned = 1;
                        debug_log("  Assigned '%s' to category '%s'",
                                 all_apps[i].name, app_launcher.categories[j].category_name);
                    }
                    break;
                }
            }
            token = strtok(NULL, ";");
        }

        if (!assigned) {
            for (int j = 0; j < category_count; j++) {
                if (strcasecmp(category_map[j].category, "Utility") == 0) {
                    if (app_launcher.categories[j].apps) {
                        int idx = app_launcher.categories[j].app_count++;
                        app_launcher.categories[j].apps[idx] = all_apps[i];
                        debug_log("  Assigned '%s' to default category 'Utilities'", all_apps[i].name);
                    }
                    break;
                }
            }
        }

        free(categories);
    }

    free(all_apps);

    debug_log("=== APPLICATION LOADING SUMMARY ===");
    int total_loaded_apps = 0;
    int non_empty_categories = 0;

    for (int i = 0; i < category_count; i++) {
        if (app_launcher.categories[i].app_count > 0) {
            non_empty_categories++;
            total_loaded_apps += app_launcher.categories[i].app_count;
            debug_log("Category: %s - %d apps",
                     app_launcher.categories[i].category_name,
                     app_launcher.categories[i].app_count);
        }
    }

    debug_log("TOTAL: %d applications in %d categories", total_loaded_apps, non_empty_categories);
    debug_log("=== APPLICATION LOADING COMPLETE ===");
}

void draw_diamond_to_window(Window win, GC gc, int x, int y, int size) {
    int center_x = x + size / 2;
    int center_y = y + size / 2;
    float scale_factor = size / 32.0f;

    // Save original values
    GC original_panel_gc = panel_gc;
    Window original_panel_win = panel.win;

    // Use provided window and GC
    panel_gc = gc;
    panel.win = win;

    // Draw the diamond
    draw_diamond_icon(x, y, size);

    // Restore original values
    panel_gc = original_panel_gc;
    panel.win = original_panel_win;
}

void lock_screen() {
    debug_log("Creating enhanced lock screen with modern diamond logo");

    // Create a fullscreen lock window
    Window lock_win = XCreateSimpleWindow(dpy, root, 0, 0,
                                         DisplayWidth(dpy, screen),
                                         DisplayHeight(dpy, screen),
                                         0, black, background_dark);

    // Create a GC for the lock screen
    XGCValues lock_gc_vals;
    lock_gc_vals.foreground = text_primary;
    lock_gc_vals.background = background_dark;
    GC lock_gc = XCreateGC(dpy, lock_win, GCForeground | GCBackground, &lock_gc_vals);

    // Grab keyboard and pointer
    XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime);
    XGrabPointer(dpy, root, True, ButtonPressMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

    // Map the lock window
    XMapWindow(dpy, lock_win);
    XRaiseWindow(dpy, lock_win);

    int screen_width = DisplayWidth(dpy, screen);
    int screen_height = DisplayHeight(dpy, screen);
    int center_x = screen_width / 2;
    int center_y = screen_height / 2;

    // Create Xft draw for anti-aliased text
    XftDraw *lock_xft_draw = XftDrawCreate(dpy, lock_win,
                                          DefaultVisual(dpy, screen),
                                          DefaultColormap(dpy, screen));

    // Draw modern diamond logo (same as panel) - manually draw since we can't use panel_gc
    int diamond_size = 120;
    int diamond_x = center_x - diamond_size / 2;
    int diamond_y = center_y - diamond_size / 2 - 80;

    // Draw the full diamond icon similar to panel but with lock screen dimensions
    draw_diamond_to_window(lock_win, lock_gc, diamond_x, diamond_y, diamond_size);

    // Draw "DiamondWM" text with anti-aliased font
    char *title = "DiamondWM";
    if (xft_fonts.title) {
        XGlyphInfo extents;
        XftTextExtents8(dpy, xft_fonts.title, (XftChar8 *)title, strlen(title), &extents);
        int title_width = extents.width;
        int title_x = center_x - title_width / 2;
        int title_y = center_y + 40;

        XRenderColor title_color;
        title_color.red = ((text_primary >> 16) & 0xFF) * 257;
        title_color.green = ((text_primary >> 8) & 0xFF) * 257;
        title_color.blue = (text_primary & 0xFF) * 257;
        title_color.alpha = 0xFFFF;

        XftColor xft_title;
        if (XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                              DefaultColormap(dpy, screen),
                              &title_color, &xft_title)) {
            XftDrawStringUtf8(lock_xft_draw, &xft_title, xft_fonts.title,
                             title_x, title_y + xft_fonts.title->ascent, (XftChar8 *)title, strlen(title));
            XftColorFree(dpy, DefaultVisual(dpy, screen),
                        DefaultColormap(dpy, screen), &xft_title);
        }
    }

    // Draw lock message with anti-aliased font
    char *message = "Press any key to unlock";
    if (xft_fonts.regular) {
        XGlyphInfo extents;
        XftTextExtents8(dpy, xft_fonts.regular, (XftChar8 *)message, strlen(message), &extents);
        int msg_width = extents.width;
        int msg_x = center_x - msg_width / 2;
        int msg_y = center_y + 80;

        XRenderColor msg_color;
        msg_color.red = ((text_secondary >> 16) & 0xFF) * 257;
        msg_color.green = ((text_secondary >> 8) & 0xFF) * 257;
        msg_color.blue = (text_secondary & 0xFF) * 257;
        msg_color.alpha = 0xFFFF;

        XftColor xft_msg;
        if (XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                              DefaultColormap(dpy, screen),
                              &msg_color, &xft_msg)) {
            XftDrawStringUtf8(lock_xft_draw, &xft_msg, xft_fonts.regular,
                             msg_x, msg_y + xft_fonts.regular->ascent, (XftChar8 *)message, strlen(message));
            XftColorFree(dpy, DefaultVisual(dpy, screen),
                        DefaultColormap(dpy, screen), &xft_msg);
        }
    }

    XFlush(dpy);

    // Wait for any key press to unlock
    XEvent ev;
    int unlocked = 0;

    while (!unlocked) {
        XNextEvent(dpy, &ev);

        if (ev.type == KeyPress || ev.type == ButtonPress) {
            unlocked = 1;
        }
    }

    // Cleanup
    XftDrawDestroy(lock_xft_draw);
    XUngrabKeyboard(dpy, CurrentTime);
    XUngrabPointer(dpy, CurrentTime);
    XUnmapWindow(dpy, lock_win);
    XDestroyWindow(dpy, lock_win);
    XFreeGC(dpy, lock_gc);

    show_operation_feedback("Screen unlocked");
    debug_log("Enhanced lock screen deactivated");
}


void create_app_launcher() {
    load_applications();

    app_launcher.width = 350;
    app_launcher.height = 600;
    app_launcher.x = 100;
    app_launcher.y = 100;
    app_launcher.hover_item = -1;
    app_launcher.alpha = 0.0f;
    app_launcher.search_mode = 0;
    app_launcher.search_text[0] = '\0';

    app_launcher.win = XCreateSimpleWindow(dpy, root, app_launcher.x, app_launcher.y,
                                          app_launcher.width, app_launcher.height, 0,
                                          white, dark_blue);

    // ADD KeyPressMask to receive keyboard events
    XSelectInput(dpy, app_launcher.win, ButtonPressMask | ExposureMask | PointerMotionMask | KeyPressMask);
    app_launcher.visible = 0;

    debug_log("App launcher created: %dx%d", app_launcher.width, app_launcher.height);
}

void show_app_launcher(int x, int y) {
    if (!app_launcher.visible) {
        int screen_width = DisplayWidth(dpy, screen);
        int screen_height = DisplayHeight(dpy, screen);

        app_launcher.x = x;
        app_launcher.y = y;

        if (app_launcher.x + app_launcher.width > screen_width) {
            app_launcher.x = screen_width - app_launcher.width - 10;
        }
        if (app_launcher.y + app_launcher.height > screen_height) {
            app_launcher.y = screen_height - app_launcher.height - 10;
        }

        debug_log("show_app_launcher: positioning at %d,%d (clicked at %d,%d)",
                 app_launcher.x, app_launcher.y, x, y);

        XMoveWindow(dpy, app_launcher.win, app_launcher.x, app_launcher.y);
        XMapWindow(dpy, app_launcher.win);
        XRaiseWindow(dpy, app_launcher.win);
        app_launcher.visible = 1;

        // GRAB THE KEYBOARD with proper error handling
        int grab_result = XGrabKeyboard(dpy, root, False, GrabModeAsync, GrabModeAsync, CurrentTime);
        if (grab_result != GrabSuccess) {
            debug_log("WARNING: Could not grab keyboard, search may not work properly");
        }

        // Fade-in animation
        for (int i = 0; i <= 10; i++) {
            app_launcher.alpha = (float)i / 10.0f;
            draw_app_launcher();
            XFlush(dpy);
            usleep(10000);
        }

        XGrabPointer(dpy, root, False, ButtonPressMask, GrabModeAsync,
                    GrabModeAsync, None, None, CurrentTime);
    }
}

void hide_app_launcher() {
    if (app_launcher.visible) {
        // Fade-out animation
        for (int i = 10; i >= 0; i--) {
            app_launcher.alpha = (float)i / 10.0f;
            draw_app_launcher();
            XFlush(dpy);
            usleep(8000);
        }

        XUnmapWindow(dpy, app_launcher.win);
        app_launcher.visible = 0;
        app_launcher.hover_item = -1;
        app_launcher.search_mode = 0;
        app_launcher.search_text[0] = '\0';

        // UNGRAB THE KEYBOARD when hiding
        XUngrabKeyboard(dpy, CurrentTime);
        XUngrabPointer(dpy, CurrentTime);
    }
}

// Add this function to filter applications based on search text
void filter_applications_by_search() {
    if (app_launcher.search_text[0] == '\0') {
        // If search is empty, show all categories expanded
        for (int i = 0; i < app_launcher.category_count; i++) {
            if (app_launcher.categories[i].app_count > 0) {
                app_launcher.categories[i].expanded = 1;
            }
        }
        return;
    }

    // Convert search text to lowercase for case-insensitive search
    char search_lower[256];
    strncpy(search_lower, app_launcher.search_text, sizeof(search_lower)-1);
    search_lower[sizeof(search_lower)-1] = '\0';
    for (char *p = search_lower; *p; p++) *p = tolower(*p);

    // Filter applications based on search text
    for (int i = 0; i < app_launcher.category_count; i++) {
        AppCategory *cat = &app_launcher.categories[i];
        int match_count = 0;

        // Count matching apps in this category
        for (int j = 0; j < cat->app_count; j++) {
            char app_name_lower[256];
            strncpy(app_name_lower, cat->apps[j].name, sizeof(app_name_lower)-1);
            app_name_lower[sizeof(app_name_lower)-1] = '\0';
            for (char *p = app_name_lower; *p; p++) *p = tolower(*p);

            if (strstr(app_name_lower, search_lower) != NULL) {
                match_count++;
            }
        }

        // Expand category only if it has matching apps
        cat->expanded = (match_count > 0);
    }
}


void draw_app_launcher_text(int x, int y, const char *text, unsigned long color, XftFont *font) {
    if (font && xft_draw) {
        XRenderColor render_color;
        render_color.red = ((color >> 16) & 0xFF) * 257;
        render_color.green = ((color >> 8) & 0xFF) * 257;
        render_color.blue = (color & 0xFF) * 257;
        render_color.alpha = 0xFFFF;

        XftColor xft_color;
        if (XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                              DefaultColormap(dpy, screen),
                              &render_color, &xft_color)) {
            // Add font ascent to Y coordinate for proper vertical alignment
            XftDrawStringUtf8(xft_draw, &xft_color, font, x, y + font->ascent,
                             (XftChar8 *)text, strlen(text));
            XftColorFree(dpy, DefaultVisual(dpy, screen),
                        DefaultColormap(dpy, screen), &xft_color);
        }
    }
}

void draw_app_launcher() {
    if (!app_launcher.visible) return;

    XClearWindow(dpy, app_launcher.win);

    // Modern background with rounded corners
    XSetForeground(dpy, menu_gc, background_dark);
    draw_rounded_rectangle(app_launcher.win, menu_gc, 0, 0, app_launcher.width, app_launcher.height, CORNER_RADIUS);

    // Draw border with accent color
    XSetForeground(dpy, menu_gc, accent_color);
    XDrawRectangle(dpy, app_launcher.win, menu_gc, 1, 1, app_launcher.width - 3, app_launcher.height - 3);

    // Draw search bar
    if (app_launcher.search_mode) {
        XSetForeground(dpy, menu_gc, accent_color);
    } else {
        XSetForeground(dpy, menu_gc, 0x3D3D4D);
    }
    XFillRectangle(dpy, app_launcher.win, menu_gc, 10, 10, app_launcher.width-20, 30);

    XSetForeground(dpy, menu_gc, app_launcher.search_mode ? accent_color : 0x555555);
    XDrawRectangle(dpy, app_launcher.win, menu_gc, 10, 10, app_launcher.width-20, 30);

    XSetForeground(dpy, menu_gc, text_primary);
    if (app_launcher.search_text[0] == '\0') {
        XDrawString(dpy, app_launcher.win, menu_gc, 20, 30, "Search applications...", 21);
    } else {
        XDrawString(dpy, app_launcher.win, menu_gc, 20, 30, app_launcher.search_text, strlen(app_launcher.search_text));

        // Show cursor when in search mode
        if (app_launcher.search_mode) {
            int text_width = XTextWidth(XLoadQueryFont(dpy, "fixed"), app_launcher.search_text, strlen(app_launcher.search_text));
            XDrawLine(dpy, app_launcher.win, menu_gc, 20 + text_width, 15, 20 + text_width, 25);
        }
    }

    // Draw title with search info - FIXED TRUNCATION
    XSetForeground(dpy, menu_gc, text_primary);
    if (app_launcher.search_text[0] != '\0') {
        char title[256];  // INCREASED BUFFER SIZE
        // Truncate search text if too long for display
        char display_search[50];
        strncpy(display_search, app_launcher.search_text, sizeof(display_search)-1);
        display_search[sizeof(display_search)-1] = '\0';
        if (strlen(app_launcher.search_text) > sizeof(display_search)-1) {
            strcpy(display_search + sizeof(display_search)-4, "...");
        }
        snprintf(title, sizeof(title), "Search: '%s'", display_search);
        XDrawString(dpy, app_launcher.win, menu_gc, 10, 55, title, strlen(title));
    } else {
        XDrawString(dpy, app_launcher.win, menu_gc, 10, 55, "Applications", 12);
    }

    XSetForeground(dpy, menu_gc, 0x404040);
    XDrawLine(dpy, app_launcher.win, menu_gc, 0, 60, app_launcher.width, 60);

    int y_pos = 75;

    // Draw categories and apps with modern styling
    for (int i = 0; i < app_launcher.category_count; i++) {
        AppCategory *cat = &app_launcher.categories[i];

        if (cat->app_count == 0) continue;
        if (!cat->expanded) continue; // Skip non-expanded categories

        // Draw category name
        char category_text[100];
        snprintf(category_text, sizeof(category_text), "%s (%d)", cat->category_name, cat->app_count);

        // Hover effect for category
        if (app_launcher.hover_item == (i << 16)) {
            XSetForeground(dpy, menu_gc, menu_hover_bg);
            draw_rounded_rectangle(app_launcher.win, menu_gc, 15, y_pos - 15,
                                  app_launcher.width - 30, 20, 4);
        }

        XSetForeground(dpy, menu_gc, text_primary);
        XDrawString(dpy, app_launcher.win, menu_gc, 20, y_pos, category_text, strlen(category_text));
        y_pos += 25;

        // Draw apps
        for (int j = 0; j < cat->app_count; j++) {
            // Hover effect for apps
            if (app_launcher.hover_item == ((i << 16) | (j + 1))) {
                XSetForeground(dpy, menu_gc, menu_hover_bg);
                draw_rounded_rectangle(app_launcher.win, menu_gc, 35, y_pos - 15,
                                      app_launcher.width - 45, 20, 4);
            }

            XSetForeground(dpy, menu_gc, text_secondary);
            XDrawString(dpy, app_launcher.win, menu_gc, 40, y_pos,
                       cat->apps[j].name, strlen(cat->apps[j].name));
            y_pos += 20;
        }

        y_pos += 5;

        if (y_pos > app_launcher.height - 20) break;
    }

    // Show search instructions
    if (app_launcher.search_mode) {
        XSetForeground(dpy, menu_gc, text_secondary);
        XDrawString(dpy, app_launcher.win, menu_gc, 10, app_launcher.height - 10,
                   "Press Enter to launch first result, Esc to cancel", 50);
    }
}

int is_in_app_launcher_area(int x, int y) {
    if (!app_launcher.visible) return 0;

    int result = (x >= app_launcher.x && x <= app_launcher.x + app_launcher.width &&
                 y >= app_launcher.y && y <= app_launcher.y + app_launcher.height);

    debug_log("is_in_app_launcher_area: checking %d,%d against [%d,%d - %d,%d] -> %d",
             x, y, app_launcher.x, app_launcher.y,
             app_launcher.x + app_launcher.width, app_launcher.y + app_launcher.height,
             result);

    return result;
}

int get_app_launcher_item_at(int x, int y) {
    if (!app_launcher.visible) return -1;

    int relative_x = x - app_launcher.x;
    int relative_y = y - app_launcher.y;

    debug_log("get_app_launcher_item_at: abs(%d,%d) -> rel(%d,%d) in window %dx%d",
             x, y, relative_x, relative_y, app_launcher.width, app_launcher.height);

    if (relative_x < 0 || relative_x > app_launcher.width ||
        relative_y < 0 || relative_y > app_launcher.height) {
        debug_log("  -> Outside app launcher bounds");
        return -1;
    }

    // Check if click is in search area
    if (relative_y >= 10 && relative_y <= 40) {
        debug_log("  -> In search area");
        return -2; // Special value for search area
    }

    if (relative_y < 65) {
        debug_log("  -> In title area (y=%d)", relative_y);
        return -1;
    }

    int y_pos = 75;
    int category_index = 0;

    for (int i = 0; i < app_launcher.category_count; i++) {
        AppCategory *cat = &app_launcher.categories[i];
        if (cat->app_count == 0) continue;

        debug_log("  Checking category %s at y_pos=%d", cat->category_name, y_pos);

        if (relative_y >= y_pos && relative_y <= y_pos + 20) {
            debug_log("  -> Clicked category %d: %s", category_index, cat->category_name);
            return category_index << 16;
        }
        y_pos += 25;

        if (cat->expanded) {
            for (int j = 0; j < cat->app_count; j++) {
                debug_log("    Checking app %s at y_pos=%d", cat->apps[j].name, y_pos);
                if (relative_y >= y_pos && relative_y <= y_pos + 20) {
                    debug_log("    -> Clicked app %d in category %d", j, category_index);
                    return (category_index << 16) | (j + 1);
                }
                y_pos += 20;
            }
        }

        y_pos += 5;
        category_index++;

        if (y_pos > app_launcher.height - 20) break;
    }

    debug_log("  -> No item found at relative_y=%d", relative_y);
    return -1;
}

void handle_app_launcher_click(int x, int y) {
    int item = get_app_launcher_item_at(x, y);

    debug_log("App launcher click: item=0x%x at %d,%d", item, x, y);

    if (item == -1) {
        hide_app_launcher();
        return;
    }

    if (item == -2) {
        // Clicked search area - toggle search mode
        app_launcher.search_mode = !app_launcher.search_mode;
        if (app_launcher.search_mode) {
            show_operation_feedback("Search mode activated - type to search");
        }
        draw_app_launcher();
        return;
    }

    if (item < 0x10000) {
        int cat_index = item;
        int actual_cat_index = -1;
        int count = 0;

        for (int i = 0; i < app_launcher.category_count; i++) {
            if (app_launcher.categories[i].app_count > 0) {
                if (count == cat_index) {
                    actual_cat_index = i;
                    break;
                }
                count++;
            }
        }

        if (actual_cat_index != -1) {
            app_launcher.categories[actual_cat_index].expanded =
                !app_launcher.categories[actual_cat_index].expanded;
            debug_log("Toggled category %s (index %d, expanded=%d)",
                     app_launcher.categories[actual_cat_index].category_name,
                     actual_cat_index, app_launcher.categories[actual_cat_index].expanded);
            draw_app_launcher();
        } else {
            debug_log("ERROR: Could not find category for index %d", cat_index);
        }
    } else {
        int cat_index = (item >> 16) & 0xFFFF;
        int app_index = (item & 0xFFFF) - 1;

        debug_log("App clicked: category=%d, app=%d", cat_index, app_index);

        int actual_cat_index = -1;
        int count = 0;
        for (int i = 0; i < app_launcher.category_count; i++) {
            if (app_launcher.categories[i].app_count > 0) {
                if (count == cat_index) {
                    actual_cat_index = i;
                    break;
                }
                count++;
            }
        }

        if (actual_cat_index != -1 &&
            app_index >= 0 &&
            app_index < app_launcher.categories[actual_cat_index].app_count) {

            AppInfo *app = &app_launcher.categories[actual_cat_index].apps[app_index];
            debug_log("Launching: %s -> %s", app->name, app->exec);

            pid_t pid = fork();
            if (pid == 0) {
                setsid();
                execl("/bin/sh", "sh", "-c", app->exec, NULL);
                exit(0);
            } else if (pid > 0) {
                show_operation_feedback("Application launched");
                hide_app_launcher();
            } else {
                debug_log("ERROR: Failed to fork for application launch");
                show_operation_feedback("Failed to launch application");
            }
        } else {
            debug_log("ERROR: Invalid app selection: cat_index=%d, actual_cat_index=%d, app_index=%d",
                     cat_index, actual_cat_index, app_index);
        }
    }
}

void free_applications() {
    for (int i = 0; i < app_launcher.category_count; i++) {
        AppCategory *cat = &app_launcher.categories[i];

        free(cat->category_name);

        for (int j = 0; j < cat->app_count; j++) {
            free(cat->apps[j].name);
            free(cat->apps[j].exec);
            free(cat->apps[j].categories);
            if (cat->apps[j].comment) free(cat->apps[j].comment);
            if (cat->apps[j].icon) free(cat->apps[j].icon);
        }

        if (cat->apps) free(cat->apps);
    }

    if (app_launcher.categories) free(app_launcher.categories);
}

// Pinned Apps Functions Implementation

char* get_panel_config_path() {
    static char path[512];
    struct passwd *pw = getpwuid(getuid());
    const char *home = pw ? pw->pw_dir : getenv("HOME");

    snprintf(path, sizeof(path), "%s/.diamondwm/pinned_apps.conf", home);
    return path;
}

void ensure_config_dir() {
    struct passwd *pw = getpwuid(getuid());
    const char *home = pw ? pw->pw_dir : getenv("HOME");
    char dir_path[512];

    snprintf(dir_path, sizeof(dir_path), "%s/.diamondwm", home);
    mkdir(dir_path, 0755);
}

void save_pinned_apps() {
    ensure_config_dir();
    FILE *file = fopen(get_panel_config_path(), "w");
    if (!file) {
        debug_log("ERROR: Could not save pinned apps to %s", get_panel_config_path());
        return;
    }

    for (int i = 0; i < pinned_apps.app_count; i++) {
        fprintf(file, "%s|%s|%s\n",
                pinned_apps.apps[i].name ? pinned_apps.apps[i].name : "",
                pinned_apps.apps[i].exec ? pinned_apps.apps[i].exec : "",
                pinned_apps.apps[i].icon_path ? pinned_apps.apps[i].icon_path : "");
    }

    fclose(file);
    debug_log("Saved %d pinned apps to %s", pinned_apps.app_count, get_panel_config_path());
}

void load_pinned_apps() {
    FILE *file = fopen(get_panel_config_path(), "r");
    if (!file) {
        debug_log("No pinned apps config found at %s", get_panel_config_path());
        return;
    }

    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;

        char *name = strtok(line, "|");
        char *exec = strtok(NULL, "|");
        char *icon_path = strtok(NULL, "|");

        if (name && exec) {
            // Add to pinned apps
            if (pinned_apps.app_count >= pinned_apps.max_apps) {
                pinned_apps.max_apps += 10;
                pinned_apps.apps = realloc(pinned_apps.apps, sizeof(PinnedApp) * pinned_apps.max_apps);
            }

            PinnedApp *app = &pinned_apps.apps[pinned_apps.app_count++];
            app->name = strdup(name);
            app->exec = strdup(exec);
            app->icon_path = icon_path ? strdup(icon_path) : NULL;
            app->icon_win = 0;
            app->x_position = 0;

            debug_log("Loaded pinned app: %s -> %s", name, exec);
        }
    }

    fclose(file);
    debug_log("Loaded %d pinned apps", pinned_apps.app_count);
}

int is_app_pinned(Client *c) {
    if (!c || !c->title) return 0;

    for (int i = 0; i < pinned_apps.app_count; i++) {
        if (pinned_apps.apps[i].name && strcmp(pinned_apps.apps[i].name, c->title) == 0) {
            return 1;
        }
    }
    return 0;
}

void pin_app_to_panel(Client *c) {
    if (!c || !c->title || is_app_pinned(c)) {
        debug_log("App already pinned or invalid client");
        return;
    }

    // Add to pinned apps
    if (pinned_apps.app_count >= pinned_apps.max_apps) {
        pinned_apps.max_apps += 10;
        pinned_apps.apps = realloc(pinned_apps.apps, sizeof(PinnedApp) * pinned_apps.max_apps);
    }

    PinnedApp *app = &pinned_apps.apps[pinned_apps.app_count++];
    app->name = strdup(c->title);
    app->exec = NULL; // We don't have the original exec command for existing windows
    app->icon_path = NULL;
    app->icon_win = 0;
    app->x_position = 0;

    // Save to config
    save_pinned_apps();

    // Redraw panel to show new pinned app
    draw_panel();

    debug_log("Pinned app to panel: %s", c->title);
}

void unpin_app(PinnedApp *app) {
    // Find and remove the app
    for (int i = 0; i < pinned_apps.app_count; i++) {
        if (&pinned_apps.apps[i] == app) {
            free(pinned_apps.apps[i].name);
            free(pinned_apps.apps[i].exec);
            free(pinned_apps.apps[i].icon_path);

            // Shift remaining apps
            for (int j = i; j < pinned_apps.app_count - 1; j++) {
                pinned_apps.apps[j] = pinned_apps.apps[j + 1];
            }
            pinned_apps.app_count--;

            save_pinned_apps();
            draw_panel();
            debug_log("Unpinned app: %s", app->name);
            return;
        }
    }
}

void create_pinned_app_menu() {
    pinned_app_menu.width = MENU_WIDTH;
    pinned_app_menu.height = MENU_ITEM_HEIGHT * 2;
    pinned_app_menu.x = 0;
    pinned_app_menu.y = 0;
    pinned_app_menu.hover_item = -1;
    pinned_app_menu.alpha = 0.0f;
    pinned_app_menu.target_pinned_app = NULL;

    pinned_app_menu.win = XCreateSimpleWindow(dpy, root,
                                             pinned_app_menu.x,
                                             pinned_app_menu.y,
                                             pinned_app_menu.width,
                                             pinned_app_menu.height,
                                             0, white, dark_blue);

    XSelectInput(dpy, pinned_app_menu.win, ButtonPressMask | ExposureMask | PointerMotionMask);
    pinned_app_menu.visible = 0;
}

void show_pinned_app_menu(int x, int y, PinnedApp *app) {
    if (!pinned_app_menu.visible && app) {
        pinned_app_menu.target_pinned_app = app;

        // Position ABOVE the pinned app icon
        pinned_app_menu.x = x;
        pinned_app_menu.y = panel.y - pinned_app_menu.height - 5;

        // Keep menu on screen horizontally
        if (pinned_app_menu.x + pinned_app_menu.width > DisplayWidth(dpy, screen)) {
            pinned_app_menu.x = DisplayWidth(dpy, screen) - pinned_app_menu.width;
        }
        if (pinned_app_menu.x < 0) {
            pinned_app_menu.x = 0;
        }

        XMoveWindow(dpy, pinned_app_menu.win, pinned_app_menu.x, pinned_app_menu.y);
        XMapWindow(dpy, pinned_app_menu.win);
        XRaiseWindow(dpy, pinned_app_menu.win);
        pinned_app_menu.visible = 1;

        // Fade-in animation
        for (int i = 0; i <= 10; i++) {
            pinned_app_menu.alpha = (float)i / 10.0f;
            draw_pinned_app_menu();
            XFlush(dpy);
            usleep(10000);
        }

        XGrabPointer(dpy, root, False, ButtonPressMask, GrabModeAsync,
                    GrabModeAsync, None, None, CurrentTime);
    }
}

void hide_pinned_app_menu() {
    if (pinned_app_menu.visible) {
        // Fade-out animation
        for (int i = 10; i >= 0; i--) {
            pinned_app_menu.alpha = (float)i / 10.0f;
            draw_pinned_app_menu();
            XFlush(dpy);
            usleep(8000);
        }

        XUnmapWindow(dpy, pinned_app_menu.win);
        pinned_app_menu.visible = 0;
        pinned_app_menu.hover_item = -1;
        pinned_app_menu.target_pinned_app = NULL;
        XUngrabPointer(dpy, CurrentTime);
    }
}

void draw_pinned_app_menu() {
    if (!pinned_app_menu.visible) return;

    XClearWindow(dpy, pinned_app_menu.win);

    // Draw rounded background with gradient
    XSetForeground(dpy, menu_gc, menu_bg);
    draw_rounded_rectangle(pinned_app_menu.win, menu_gc, 0, 0,
                          pinned_app_menu.width, pinned_app_menu.height,
                          CORNER_RADIUS);

    // Draw subtle border
    XSetForeground(dpy, menu_gc, accent_color);
    XDrawRectangle(dpy, pinned_app_menu.win, menu_gc, 1, 1,
                   pinned_app_menu.width - 3, pinned_app_menu.height - 3);

    char *items[] = {"Unpin from Panel", "Launch"};

    for (int i = 0; i < 2; i++) {
        int y = i * MENU_ITEM_HEIGHT;

        // Hover effect
        if (pinned_app_menu.hover_item == i) {
            XSetForeground(dpy, menu_gc, menu_hover_bg);
            draw_rounded_rectangle(pinned_app_menu.win, menu_gc, 2, y + 2,
                                  pinned_app_menu.width - 4, MENU_ITEM_HEIGHT - 4, 4);
        }

        if (i > 0) {
            XSetForeground(dpy, menu_gc, 0x404040);
            XDrawLine(dpy, pinned_app_menu.win, menu_gc, 10, y,
                     pinned_app_menu.width - 10, y);
        }

        XSetForeground(dpy, menu_gc, text_primary);
        int text_width = XTextWidth(XLoadQueryFont(dpy, "fixed"), items[i], strlen(items[i]));
        int text_x = (pinned_app_menu.width - text_width) / 2;
        int text_y = y + (MENU_ITEM_HEIGHT / 2) + 5;

        XDrawString(dpy, pinned_app_menu.win, menu_gc, text_x, text_y,
                   items[i], strlen(items[i]));
    }
}

void free_pinned_apps() {
    for (int i = 0; i < pinned_apps.app_count; i++) {
        free(pinned_apps.apps[i].name);
        free(pinned_apps.apps[i].exec);
        free(pinned_apps.apps[i].icon_path);
    }
    free(pinned_apps.apps);
}

void create_panel() {
    int screen_width = DisplayWidth(dpy, screen);
    int screen_height = DisplayHeight(dpy, screen);

    debug_log("Screen dimensions: %dx%d", screen_width, screen_height);

    panel.width = screen_width;
    panel.height = PANEL_HEIGHT;
    panel.x = 0;
    panel.y = screen_height - PANEL_HEIGHT;

    panel.win = XCreateSimpleWindow(dpy, root, panel.x, panel.y,
                                   panel.width, panel.height, 0,
                                   dark_blue, dark_blue);
    debug_log("Panel window created: %lu", panel.win);

    XSelectInput(dpy, panel.win, ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask | ExposureMask);
    XMapWindow(dpy, panel.win);
    debug_log("Panel window mapped");

    XLowerWindow(dpy, panel.win);
    debug_log("Panel lowered to bottom");
}

void setup_mouse_cursor() {
    debug_log("Setting up mouse cursor");

    Cursor cursor = XCreateFontCursor(dpy, XC_left_ptr);

    XDefineCursor(dpy, root, cursor);
    XDefineCursor(dpy, panel.win, cursor);
    XDefineCursor(dpy, menu.win, cursor);
    XDefineCursor(dpy, app_launcher.win, cursor);
    XDefineCursor(dpy, window_control_menu.win, cursor);
    XDefineCursor(dpy, pinned_app_menu.win, cursor);

    debug_log("Mouse cursor initialization complete");
}

void draw_panel() {
    XClearWindow(dpy, panel.win);

    // Modern panel with gradient
    draw_gradient_rect(panel.win, panel_gc, 0, 0, panel.width, panel.height,
                      background_dark, background_light, 1);

    int x = 10;

    // Draw pinned apps first
    for (int i = 0; i < pinned_apps.app_count; i++) {
        pinned_apps.apps[i].x_position = x;

        // Draw pinned app icon (simplified version - you can enhance with actual icons)
        XSetForeground(dpy, panel_gc, accent_color);
        draw_rounded_rectangle(panel.win, panel_gc, x, 10, 30, 30, 6);

        // Draw app initial or simple representation
        XSetForeground(dpy, panel_gc, text_primary);
        if (pinned_apps.apps[i].name && strlen(pinned_apps.apps[i].name) > 0) {
            char initial[2] = {toupper(pinned_apps.apps[i].name[0]), '\0'};
            XDrawString(dpy, panel.win, panel_gc, x + 11, 28, initial, 1);
        }

        x += 40;
    }

    // Draw separator if there are both pinned apps and window buttons
    if (pinned_apps.app_count > 0) {
        XSetForeground(dpy, panel_gc, 0x555555);
        XDrawLine(dpy, panel.win, panel_gc, x, 15, x, 35);
        x += 10;
    }

    // Draw client icons/buttons with modern styling
    int window_index = 1;
    for (int i = 0; i < client_count; i++) {
        if (clients[i] && clients[i]->is_mapped && !is_app_pinned(clients[i])) {
            // Modern app icon with rounded corners
            XSetForeground(dpy, panel_gc, 0x3D3D4D);
            draw_rounded_rectangle(panel.win, panel_gc, x, 10, 40, 30, 6);

            // Border with accent color for active window
            if (clients[i]->is_active) {
                XSetForeground(dpy, panel_gc, accent_color);
                XSetLineAttributes(dpy, panel_gc, 2, LineSolid, CapRound, JoinRound);
            } else {
                XSetForeground(dpy, panel_gc, 0x555555);
                XSetLineAttributes(dpy, panel_gc, 1, LineSolid, CapRound, JoinRound);
            }
            XDrawRectangle(dpy, panel.win, panel_gc, x, 10, 40, 30);
            XSetLineAttributes(dpy, panel_gc, 1, LineSolid, CapRound, JoinRound);

            // Draw window identifier with anti-aliased font
            char label[10];
            snprintf(label, sizeof(label), "%d", window_index++);

            // Center the number in the button - use smaller font
            int text_x = x + 18;
            int text_y = 17; // Better vertical centering for smaller font

            // Use a smaller font for the window numbers
            if (xft_fonts.regular) {
                draw_xft_text(panel.win, text_x, text_y, label, xft_fonts.regular,
                             clients[i]->is_active ? text_primary : text_secondary);
            } else {
                // Fallback to original font if Xft not available
                XSetForeground(dpy, panel_gc, clients[i]->is_active ? text_primary : text_secondary);
                XDrawString(dpy, panel.win, panel_gc, x + 15, 30, label, strlen(label));
            }
            x += 50;
        }
    }

    // Draw DiamondWM area with modern styling
    int diamond_size = 15;
    int diamond_x = panel.width - 120;
    int diamond_y = (PANEL_HEIGHT - diamond_size) / 2;

    draw_diamond_icon(diamond_x, diamond_y, diamond_size);

    // Draw "DiamondWM" text with proper positioning
    int text_x = diamond_x + diamond_size + 10;
    int text_y = 17; // Better vertical alignment

    // Use regular font instead of title font for proper size
    if (xft_fonts.regular) {
        draw_xft_text(panel.win, text_x, text_y, "DiamondWM", xft_fonts.regular, text_primary);
    } else {
      // Fallback to original font if Xft not available
      XSetForeground(dpy, panel_gc, text_primary);
      XDrawString(dpy, panel.win, panel_gc, text_x, 30, "DiamondWM", 9);
  }

  draw_clock();
}

void draw_window_decorations(Client *c) {
  if (!c || !c->frame) return;

  debug_log("Drawing modern decorations for window %lu", c->win);

  XClearWindow(dpy, c->frame);

  // Draw shadow effect
  draw_shadow(c->frame, 0, 0, c->width, c->height);

  // Enhanced window activation feedback
  if (c->is_active) {
      // Brighter gradient and border for active window
      draw_gradient_rect(c->frame, title_gc, 0, 0, c->width, TITLEBAR_HEIGHT,
                        accent_color, 0x5D5D7D, 1);
      XSetForeground(dpy, gc, accent_color);
      XSetLineAttributes(dpy, gc, 2, LineSolid, CapRound, JoinRound);
  } else {
      // More subtle for inactive windows
      draw_gradient_rect(c->frame, title_gc, 0, 0, c->width, TITLEBAR_HEIGHT,
                        titlebar_gray, 0x2D2D2D, 1);
      XSetForeground(dpy, gc, 0x606060);
      XSetLineAttributes(dpy, gc, 1, LineSolid, CapRound, JoinRound);
  }
  XDrawRectangle(dpy, c->frame, gc, 0, 0, c->width - 1, c->height - 1);
  XSetLineAttributes(dpy, gc, 1, LineSolid, CapRound, JoinRound);

  // Draw window title
  if (c->title) {
      char display_title[256];
      if (strlen(c->title) > 30) {
          strncpy(display_title, c->title, 27);
          strcpy(display_title + 27, "...");
      } else {
          strcpy(display_title, c->title);
      }

      int text_width = XTextWidth(XLoadQueryFont(dpy, "fixed"), display_title, strlen(display_title));
      int title_x = 80;
      int title_available_width = c->width - 100;

      if (text_width < title_available_width) {
          title_x = 80 + (title_available_width - text_width) / 2;
      }

      int title_y = TITLEBAR_HEIGHT / 2 + 5;

      XSetForeground(dpy, text_gc, c->is_active ? text_primary : text_secondary);
      XDrawString(dpy, c->frame, text_gc, title_x, title_y, display_title, strlen(display_title));
  } else {
      XSetForeground(dpy, text_gc, c->is_active ? text_primary : text_secondary);
      XDrawString(dpy, c->frame, text_gc, 80, TITLEBAR_HEIGHT / 2 + 5, "Untitled", 8);
  }

  // Draw modern glow buttons with hover effects
  int button_y = (TITLEBAR_HEIGHT - BUTTON_SIZE) / 2;

  // Close button with hover effect
  draw_glow_button(c->frame, 15, button_y, BUTTON_SIZE, button_red, c->button_hover == 1);

  // Minimize button with hover effect
  draw_glow_button(c->frame, 15 + BUTTON_SIZE + BUTTON_SPACING, button_y, BUTTON_SIZE, button_yellow, c->button_hover == 2);

  // Maximize button with hover effect
  draw_glow_button(c->frame, 15 + 2*(BUTTON_SIZE + BUTTON_SPACING), button_y, BUTTON_SIZE, button_green, c->button_hover == 3);

  // Draw separator between titlebar and content
  XSetForeground(dpy, gc, 0x404040);
  XDrawLine(dpy, c->frame, gc, 0, TITLEBAR_HEIGHT, c->width, TITLEBAR_HEIGHT);
}

int is_in_close_button(Client *c, int x, int y) {
  int button_y = (TITLEBAR_HEIGHT - BUTTON_SIZE) / 2;
  return (x >= 15 && x <= 15 + BUTTON_SIZE &&
          y >= button_y && y <= button_y + BUTTON_SIZE);
}

int is_in_minimize_button(Client *c, int x, int y) {
  int button_y = (TITLEBAR_HEIGHT - BUTTON_SIZE) / 2;
  int button_x = 15 + BUTTON_SIZE + BUTTON_SPACING;
  return (x >= button_x && x <= button_x + BUTTON_SIZE &&
          y >= button_y && y <= button_y + BUTTON_SIZE);
}

int is_in_maximize_button(Client *c, int x, int y) {
  int button_y = (TITLEBAR_HEIGHT - BUTTON_SIZE) / 2;
  int button_x = 15 + 2*(BUTTON_SIZE + BUTTON_SPACING);
  return (x >= button_x && x <= button_x + BUTTON_SIZE &&
          y >= button_y && y <= button_y + BUTTON_SIZE);
}

int is_in_titlebar(Client *c, int x, int y) {
  return (y >= 0 && y <= TITLEBAR_HEIGHT);
}

int get_resize_edge(Client *c, int x, int y) {
  if (!c) return -1;

  // Check corners first
  if (x < RESIZE_HANDLE_SIZE && y < RESIZE_HANDLE_SIZE) return 4; // top-left
  if (x >= c->width - RESIZE_HANDLE_SIZE && y < RESIZE_HANDLE_SIZE) return 5; // top-right
  if (x < RESIZE_HANDLE_SIZE && y >= c->height - RESIZE_HANDLE_SIZE) return 6; // bottom-left
  if (x >= c->width - RESIZE_HANDLE_SIZE && y >= c->height - RESIZE_HANDLE_SIZE) return 7; // bottom-right

  // Check edges
  if (x < RESIZE_HANDLE_SIZE) return 0; // left
  if (x >= c->width - RESIZE_HANDLE_SIZE) return 1; // right
  if (y < RESIZE_HANDLE_SIZE) return 2; // top
  if (y >= c->height - RESIZE_HANDLE_SIZE) return 3; // bottom

  return -1; // not on resize edge
}

void send_wm_delete(Window w) {
  Atom wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
  Atom wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  Atom *protocols;
  int num_protocols;  // Changed from unsigned long to int

  if (XGetWMProtocols(dpy, w, &protocols, &num_protocols)) {
      for (int i = 0; i < num_protocols; i++) {  // Changed from unsigned long to int
          if (protocols[i] == wm_delete_window) {
              XEvent ev;
              ev.type = ClientMessage;
              ev.xclient.window = w;
              ev.xclient.message_type = wm_protocols;
              ev.xclient.format = 32;
              ev.xclient.data.l[0] = wm_delete_window;
              ev.xclient.data.l[1] = CurrentTime;
              XSendEvent(dpy, w, False, NoEventMask, &ev);
              XFree(protocols);
              return;
          }
      }
      XFree(protocols);
  }

  // Fallback: kill the client
  XKillClient(dpy, w);
}

void handle_button_press(XButtonEvent *e) {
  debug_log("Button press on window %lu (button=%d) at screen coords %d,%d",
           e->window, e->button, e->x_root, e->y_root);

  // Handle right-click on desktop (already handled in main loop, but keep for completeness)
  if (e->window == root && e->button == Button3) {
      debug_log("Right click on desktop - showing app launcher");
      show_app_launcher(e->x_root, e->y_root);
      return;
  }

  // ===== Check for app launcher visibility FIRST =====
  if (app_launcher.visible) {
      debug_log("App launcher visible, checking if click is inside...");
      if (is_in_app_launcher_area(e->x_root, e->y_root)) {
          debug_log("Click INSIDE app launcher at %d,%d", e->x_root, e->y_root);
          // Click on app launcher itself - handle normally
          handle_app_launcher_click(e->x_root, e->y_root);
          return;
      } else {
          debug_log("Click OUTSIDE app launcher at %d,%d", e->x_root, e->y_root);
          // Click outside app launcher - close it
          hide_app_launcher();
          // Don't return here - let the click be processed by other handlers
      }
  }

  // ===== Check for pinned app menu =====
  if (pinned_app_menu.visible) {
      if (e->x_root >= pinned_app_menu.x &&
          e->x_root <= pinned_app_menu.x + pinned_app_menu.width &&
          e->y_root >= pinned_app_menu.y &&
          e->y_root <= pinned_app_menu.y + pinned_app_menu.height) {

          int relative_y = e->y_root - pinned_app_menu.y;
          int item = relative_y / MENU_ITEM_HEIGHT;

          debug_log("Pinned app menu clicked: item=%d", item);

          if (item >= 0 && item < 2 && pinned_app_menu.target_pinned_app) {
              PinnedApp *app = pinned_app_menu.target_pinned_app;
              switch (item) {
                  case 0: // Unpin from Panel
                      debug_log("Unpin clicked");
                      hide_pinned_app_menu();
                      unpin_app(app);
                      show_operation_feedback("App unpinned from panel");
                      return;
                  case 1: // Launch
                      debug_log("Launch clicked");
                      hide_pinned_app_menu();
                      if (app->exec) {
                          pid_t pid = fork();
                          if (pid == 0) {
                              setsid();
                              execl("/bin/sh", "sh", "-c", app->exec, NULL);
                              exit(0);
                          }
                          show_operation_feedback("App launched");
                      }
                      return;
              }
          }
          hide_pinned_app_menu();
          return;
      } else {
          hide_pinned_app_menu();
      }
  }

  // ===== Check for window control menu =====
  if (window_control_menu.visible) {
      if (e->x_root >= window_control_menu.x &&
          e->x_root <= window_control_menu.x + window_control_menu.width &&
          e->y_root >= window_control_menu.y &&
          e->y_root <= window_control_menu.y + window_control_menu.height) {

          int relative_y = e->y_root - window_control_menu.y;
          int item = relative_y / MENU_ITEM_HEIGHT;

          debug_log("Window control menu clicked: item=%d", item);

          if (item >= 0 && item < 4 && window_control_menu.target_client) {
              Client *c = window_control_menu.target_client;
              switch (item) {
                  case 0: // Pin to Panel
                      debug_log("Pin to Panel clicked");
                      hide_window_control_menu();
                      pin_app_to_panel(c);
                      show_operation_feedback("App pinned to panel");
                      return;
                  case 1: // Maximize
                      debug_log("Maximize clicked");
                      hide_window_control_menu();
                      toggle_fullscreen(c);
                      show_operation_feedback(c->is_fullscreen ? "Window maximized" : "Window restored");
                      return;
                  case 2: // Minimize
                      debug_log("Minimize clicked");
                      hide_window_control_menu();
                      lower_window(c);
                      show_operation_feedback("Window minimized");
                      return;
                  case 3: // Close
                      debug_log("Close clicked");
                      hide_window_control_menu();
                      close_window(c);
                      show_operation_feedback("Window closed");
                      return;
              }
          }
          hide_window_control_menu();
          return;
      } else {
          hide_window_control_menu();
      }
  }

  // ===== Then check for menu visibility =====
  if (menu.visible) {
      // Check if click is inside menu area using root coordinates
      if (e->x_root >= menu.x && e->x_root <= menu.x + menu.width &&
          e->y_root >= menu.y && e->y_root <= menu.y + menu.height) {

          // Calculate which menu item was clicked
          int relative_y = e->y_root - menu.y;
          int item = relative_y / MENU_ITEM_HEIGHT;

          debug_log("Menu clicked: relative_y=%d, item=%d", relative_y, item);

          // Check if item is valid (0-3)
          if (item >= 0 && item < 4) {
              switch (item) {
                  case 0: // Terminal
                      debug_log("Terminal clicked - launching xterm");
                      hide_menu();
                      system("xterm &");
                      show_operation_feedback("Terminal launched");
                      return;
                  case 1: // Lock
                      debug_log("Lock clicked - locking screen");
                      hide_menu();
                      lock_screen();
                      return;
                  case 2: // Logout
                      debug_log("Logout clicked - exiting window manager");
                      hide_menu();
                      show_operation_feedback("Logging out...");
                      for (int i = 0; i < client_count; i++) {
                          if (clients[i]) {
                              XUnmapWindow(dpy, clients[i]->frame);
                              XReparentWindow(dpy, clients[i]->win, root, 0, 0);
                              XMapWindow(dpy, clients[i]->win);
                          }
                      }
                      XCloseDisplay(dpy);
                      exit(0);
                      return;
                  case 3: // Shutdown
                      debug_log("Shutdown clicked - calling system shutdown");
                      hide_menu();
                      show_operation_feedback("Shutting down...");
                      system("shutdown -h now");
                      return;
              }
          } else {
              debug_log("Invalid menu item: %d", item);
              hide_menu();
              return;
          }
      } else {
          // Click outside menu - hide it
          debug_log("Click outside menu area - hiding it");
          hide_menu();
          // Don't return - let the click be processed
      }
  }

  // Check if click is on panel
  if (e->window == panel.win) {
      debug_log("Click on panel at %d,%d", e->x, e->y);

      // Check pinned apps first
      for (int i = 0; i < pinned_apps.app_count; i++) {
          if (e->x >= pinned_apps.apps[i].x_position &&
              e->x <= pinned_apps.apps[i].x_position + 30 &&
              e->y >= 10 && e->y <= 40) {

              debug_log("Pinned app clicked: %s", pinned_apps.apps[i].name);

              if (e->button == Button3) {
                  // Right click - show unpin menu
                  show_pinned_app_menu(panel.x + pinned_apps.apps[i].x_position,
                                      panel.y + 10, &pinned_apps.apps[i]);
                  return;
              } else {
                  // Left click - launch app if we have exec command
                  if (pinned_apps.apps[i].exec) {
                      pid_t pid = fork();
                      if (pid == 0) {
                          setsid();
                          execl("/bin/sh", "sh", "-c", pinned_apps.apps[i].exec, NULL);
                          exit(0);
                      }
                      show_operation_feedback("App launched");
                  }
              }
              return;
          }
      }

      // Check if click is on DiamondWM area
      if (is_in_diamondwm_area(e->x, e->y)) {
          debug_log("DiamondWM area clicked - toggling menu (currently visible=%d)", menu.visible);
          if (menu.visible) {
              hide_menu();
          } else {
              show_menu();
          }
          return;
      }

      // Check window buttons
      int x = 10 + pinned_apps.app_count * 40;
      if (pinned_apps.app_count > 0) x += 10; // Add separator space

      int window_index = 1;
      for (int i = 0; i < client_count; i++) {
          if (clients[i] && clients[i]->is_mapped && !is_app_pinned(clients[i])) {
              if (e->x >= x && e->x <= x + 40 && e->y >= 10 && e->y <= 40) {
                  debug_log("Panel button clicked for client %d", i);

                  // RIGHT click - show control menu ABOVE this specific button
                  if (e->button == Button3) {
                      int menu_x = panel.x + x;
                      int menu_y = panel.y;
                      show_window_control_menu(menu_x, menu_y, clients[i]);
                      return;
                  }

                  // LEFT click - activate and show window
                  if (!is_window_visible(clients[i])) {
                      debug_log("Window %d is not visible, repositioning to 10,10", i);

                      int screen_width = DisplayWidth(dpy, screen);
                      int screen_height = DisplayHeight(dpy, screen);

                      int max_width = screen_width - 20;
                      int max_height = screen_height - PANEL_HEIGHT - 20;

                      if (clients[i]->width > max_width || clients[i]->height > max_height) {
                          int new_width = (clients[i]->width > max_width) ? max_width : clients[i]->width;
                          int new_height = (clients[i]->height > max_height) ? max_height : clients[i]->height;

                          debug_log("Resizing window from %dx%d to %dx%d",
                                   clients[i]->width, clients[i]->height, new_width, new_height);

                          resize_window(clients[i], new_width, new_height);
                      }

                      move_window(clients[i], 10, 10);
                      debug_log("Window repositioned to 10,10 with size %dx%d",
                               clients[i]->width, clients[i]->height);
                  }

                  XRaiseWindow(dpy, clients[i]->frame);
                  XSetInputFocus(dpy, clients[i]->win, RevertToPointerRoot, CurrentTime);
                  clients[i]->is_active = 1;
                  draw_window_decorations(clients[i]);
                  show_operation_feedback("Window activated");
                  break;
              }
              x += 50;
              window_index++;
          }
      }

      // Start panel drag if clicked on empty area
      if (e->button == Button1 && !menu.visible && !app_launcher.visible && !window_control_menu.visible && !pinned_app_menu.visible) {
          panel_dragging = 1;
          drag_start_x = e->x_root;
          drag_start_y = e->y_root;
          debug_log("Panel dragging started at %d,%d", drag_start_x, drag_start_y);
      }
      return;
  }

  // Rest of the function for frame windows...
  for (int i = 0; i < client_count; i++) {
      if (clients[i] && clients[i]->frame == e->window) {
          Client *c = clients[i];
          debug_log("Click on frame %lu at %d,%d", c->frame, e->x, e->y);

          int edge = get_resize_edge(c, e->x, e->y);
          if (edge != -1 && e->button == Button1) {
              debug_log("Resize edge %d clicked", edge);
              window_resizing = 1;
              resized_client = c;
              resize_start_x = e->x_root;
              resize_start_y = e->y_root;
              resize_start_width = c->width;
              resize_start_height = c->height;
              resize_edge = edge;

              Cursor resize_cursor;
              switch (edge) {
                  case 0: case 1: resize_cursor = XCreateFontCursor(dpy, XC_sb_h_double_arrow); break;
                  case 2: case 3: resize_cursor = XCreateFontCursor(dpy, XC_sb_v_double_arrow); break;
                  case 4: case 7: resize_cursor = XCreateFontCursor(dpy, XC_top_left_corner); break;
                  case 5: case 6: resize_cursor = XCreateFontCursor(dpy, XC_top_right_corner); break;
                  default: resize_cursor = XCreateFontCursor(dpy, XC_left_ptr);
              }
              XDefineCursor(dpy, c->frame, resize_cursor);
              return;
          }

          if (is_in_titlebar(c, e->x, e->y)) {
              if (is_in_close_button(c, e->x, e->y)) {
                  debug_log("Close button clicked");
                  show_operation_feedback("Closing window...");
                  close_window(c);
                  return;
              } else if (is_in_minimize_button(c, e->x, e->y)) {
                  debug_log("Minimize button clicked");
                  show_operation_feedback("Window minimized");
                  lower_window(c);
                  return;
              } else if (is_in_maximize_button(c, e->x, e->y)) {
                  debug_log("Maximize button clicked");
                  show_operation_feedback(c->is_fullscreen ? "Window restored" : "Window maximized");
                  toggle_fullscreen(c);
                  return;
              } else {
                  debug_log("Titlebar clicked - starting window drag");
                  XRaiseWindow(dpy, c->frame);
                  XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
                  c->is_active = 1;
                  draw_window_decorations(c);

                  window_dragging = 1;
                  dragged_client = c;
                  drag_win_start_x = c->x;
                  drag_win_start_y = c->y;
                  drag_offset_x = e->x_root - c->x;
                  drag_offset_y = e->y_root - c->y;

                  debug_log("Window drag started: client=%lu, start_pos=%d,%d, offset=%d,%d",
                           c->win, drag_win_start_x, drag_win_start_y, drag_offset_x, drag_offset_y);

                  Cursor move_cursor = XCreateFontCursor(dpy, XC_fleur);
                  XDefineCursor(dpy, c->frame, move_cursor);
              }
          }
          break;
      }
  }
}

void handle_button_release(XButtonEvent *e) {
  debug_log("Button release on window %lu", e->window);

  if (panel_dragging) {
      panel_dragging = 0;
      debug_log("Panel dragging ended");
  }

  if (window_dragging && dragged_client) {
      debug_log("Window dragging ended for client %lu", dragged_client->win);

      // Add window snapping
      int snap_threshold = 20;
      int screen_width = DisplayWidth(dpy, screen);
      int screen_height = DisplayHeight(dpy, screen);

      if (dragged_client->x < snap_threshold) dragged_client->x = 0;
      if (dragged_client->y < snap_threshold) dragged_client->y = 0;
      if (screen_width - (dragged_client->x + dragged_client->width) < snap_threshold)
          dragged_client->x = screen_width - dragged_client->width;
      if (screen_height - (dragged_client->y + dragged_client->height) < snap_threshold)
          dragged_client->y = screen_height - dragged_client->height - PANEL_HEIGHT;

      XMoveWindow(dpy, dragged_client->frame, dragged_client->x, dragged_client->y);

      window_dragging = 0;

      // Restore normal cursor
      Cursor normal_cursor = XCreateFontCursor(dpy, XC_left_ptr);
      XDefineCursor(dpy, dragged_client->frame, normal_cursor);

      dragged_client = NULL;
  }

  if (window_resizing && resized_client) {
      debug_log("Window resizing ended for client %lu", resized_client->win);
      window_resizing = 0;

      // Restore normal cursor
      Cursor normal_cursor = XCreateFontCursor(dpy, XC_left_ptr);
      XDefineCursor(dpy, resized_client->frame, normal_cursor);

      resized_client = NULL;
  }
}

void handle_motion_notify(XMotionEvent *e) {
  static int last_x = 0, last_y = 0;
  static int panel_hover_index = -1;

  // Handle panel hover effects
  if (e->window == panel.win) {
      int new_hover_index = -1;
      int x = 10;

      // Check pinned apps hover
      for (int i = 0; i < pinned_apps.app_count; i++) {
          if (e->x >= pinned_apps.apps[i].x_position &&
              e->x <= pinned_apps.apps[i].x_position + 30 &&
              e->y >= 10 && e->y <= 40) {
              new_hover_index = i + 1000; // Use high number to distinguish from window indices
              break;
          }
          x += 40;
      }

      // Check window buttons hover if no pinned app hovered
      if (new_hover_index == -1) {
          x = 10 + pinned_apps.app_count * 40;
          if (pinned_apps.app_count > 0) x += 10;

          int window_index = 0;
          for (int i = 0; i < client_count; i++) {
              if (clients[i] && clients[i]->is_mapped && !is_app_pinned(clients[i])) {
                  if (e->x >= x && e->x <= x + 40 && e->y >= 10 && e->y <= 40) {
                      new_hover_index = i;
                      break;
                  }
                  x += 50;
                  window_index++;
              }
          }
      }

      if (new_hover_index != panel_hover_index) {
          panel_hover_index = new_hover_index;
          draw_panel();
      }
  }

  // Handle window dragging
  if (window_dragging && dragged_client) {
      if (last_x == 0 && last_y == 0) {
          // First motion event - initialize last positions
          last_x = e->x_root;
          last_y = e->y_root;
      }

      // Calculate movement delta
      int delta_x = e->x_root - last_x;
      int delta_y = e->y_root - last_y;

      // Calculate new position
      int new_x = dragged_client->x + delta_x;
      int new_y = dragged_client->y + delta_y;

      debug_log("Window dragging: delta=%d,%d, new_pos=%d,%d", delta_x, delta_y, new_x, new_y);

      // Constrain to screen boundaries
      int screen_width = DisplayWidth(dpy, screen);
      int screen_height = DisplayHeight(dpy, screen);

      // Keep window within screen bounds
      if (new_x < 0) new_x = 0;
      if (new_y < 0) new_y = 0;
      if (new_x + dragged_client->width > screen_width)
          new_x = screen_width - dragged_client->width;
      if (new_y + dragged_client->height > screen_height - PANEL_HEIGHT)
          new_y = screen_height - PANEL_HEIGHT - dragged_client->height;

      // Only move if position changed
      if (new_x != dragged_client->x || new_y != dragged_client->y) {
          // Move the window
          XMoveWindow(dpy, dragged_client->frame, new_x, new_y);

          // Update client position
          dragged_client->x = new_x;
          dragged_client->y = new_y;

          debug_log("Window moved to %d,%d", new_x, new_y);
      }

      // Update last positions
      last_x = e->x_root;
      last_y = e->y_root;
  } else {
      // Reset last positions when not dragging
      last_x = 0;
      last_y = 0;
  }

  // Handle window resizing
  if (window_resizing && resized_client) {
      int delta_x = e->x_root - resize_start_x;
      int delta_y = e->x_root - resize_start_y;
      int new_width = resize_start_width;
      int new_height = resize_start_height;
      int new_x = resized_client->x;
      int new_y = resized_client->y;

      // Minimum window size
      int min_width = 100;
      int min_height = 80;

      switch (resize_edge) {
          case 0: // left
              new_width = resize_start_width - delta_x;
              new_x = resized_client->x + delta_x;
              if (new_width < min_width) {
                  new_width = min_width;
                  new_x = resized_client->x + resize_start_width - min_width;
              }
              break;
          case 1: // right
              new_width = resize_start_width + delta_x;
              if (new_width < min_width) new_width = min_width;
              break;
          case 2: // top
              new_height = resize_start_height - delta_y;
              new_y = resized_client->y + delta_y;
              if (new_height < min_height) {
                  new_height = min_height;
                  new_y = resized_client->y + resize_start_height - min_height;
              }
              break;
          case 3: // bottom
              new_height = resize_start_height + delta_y;
              if (new_height < min_height) new_height = min_height;
              break;
          case 4: // top-left
              new_width = resize_start_width - delta_x;
              new_height = resize_start_height - delta_y;
              new_x = resized_client->x + delta_x;
              new_y = resized_client->y + delta_y;
              if (new_width < min_width) {
                  new_width = min_width;
                  new_x = resized_client->x + resize_start_width - min_width;
              }
              if (new_height < min_height) {
                  new_height = min_height;
                  new_y = resized_client->y + resize_start_height - min_height;
              }
              break;
          case 5: // top-right
              new_width = resize_start_width + delta_x;
              new_height = resize_start_height - delta_y;
              new_y = resized_client->y + delta_y;
              if (new_width < min_width) new_width = min_width;
              if (new_height < min_height) {
                  new_height = min_height;
                  new_y = resized_client->y + resize_start_height - min_height;
              }
              break;
          case 6: // bottom-left
              new_width = resize_start_width - delta_x;
              new_height = resize_start_height + delta_y;
              new_x = resized_client->x + delta_x;
              if (new_width < min_width) {
                  new_width = min_width;
                  new_x = resized_client->x + resize_start_width - min_width;
              }
              if (new_height < min_height) new_height = min_height;
              break;
          case 7: // bottom-right
              new_width = resize_start_width + delta_x;
              new_height = resize_start_height + delta_y;
              if (new_width < min_width) new_width = min_width;
              if (new_height < min_height) new_height = min_height;
              break;
      }

      // Apply the resize
      if (new_x != resized_client->x || new_y != resized_client->y) {
          XMoveWindow(dpy, resized_client->frame, new_x, new_y);
          resized_client->x = new_x;
          resized_client->y = new_y;
      }

      if (new_width != resized_client->width || new_height != resized_client->height) {
          resize_window(resized_client, new_width, new_height);
      }

      debug_log("Window resized to %dx%d at %d,%d", new_width, new_height, new_x, new_y);
  }

  // Handle hover effects for windows
  for (int i = 0; i < client_count; i++) {
      if (clients[i] && clients[i]->frame == e->window) {
          update_button_hover(clients[i], e->x, e->y);
          break;
      }
  }

  // Handle hover effects for menu
  if (menu.visible && e->window == menu.win) {
      int relative_y = e->y;
      int new_hover_item = relative_y / MENU_ITEM_HEIGHT;

      if (new_hover_item >= 0 && new_hover_item < 4) {
          if (menu.hover_item != new_hover_item) {
              menu.hover_item = new_hover_item;
              draw_menu();
          }
      } else if (menu.hover_item != -1) {
          menu.hover_item = -1;
          draw_menu();
      }
  }

  // Handle hover effects for window control menu
  if (window_control_menu.visible && e->window == window_control_menu.win) {
      int relative_y = e->y;
      int new_hover_item = relative_y / MENU_ITEM_HEIGHT;

      if (new_hover_item >= 0 && new_hover_item < 4) {
          if (window_control_menu.hover_item != new_hover_item) {
              window_control_menu.hover_item = new_hover_item;
              draw_window_control_menu();
          }
      } else if (window_control_menu.hover_item != -1) {
          window_control_menu.hover_item = -1;
          draw_window_control_menu();
      }
  }

  // Handle hover effects for pinned app menu
  if (pinned_app_menu.visible && e->window == pinned_app_menu.win) {
      int relative_y = e->y;
      int new_hover_item = relative_y / MENU_ITEM_HEIGHT;

      if (new_hover_item >= 0 && new_hover_item < 2) {
          if (pinned_app_menu.hover_item != new_hover_item) {
              pinned_app_menu.hover_item = new_hover_item;
              draw_pinned_app_menu();
          }
      } else if (pinned_app_menu.hover_item != -1) {
          pinned_app_menu.hover_item = -1;
          draw_pinned_app_menu();
      }
  }

  // Handle hover effects for app launcher
  if (app_launcher.visible) {
      int new_hover_item = get_app_launcher_item_at(e->x_root, e->y_root);
      if (app_launcher.hover_item != new_hover_item) {
          app_launcher.hover_item = new_hover_item;
          draw_app_launcher();
      }
  }
}

void handle_key_press(XKeyEvent *e) {
  // Handle app launcher search first
  if (app_launcher.visible && app_launcher.search_mode) {
      KeySym keysym;
      char buffer[10];
      int count = XLookupString(e, buffer, sizeof(buffer), &keysym, NULL);

      if (keysym == XK_Escape) {
          // Exit search mode
          app_launcher.search_mode = 0;
          app_launcher.search_text[0] = '\0';
          filter_applications_by_search();
          draw_app_launcher();
      } else if (keysym == XK_Return) {
          // Execute search - launch first matching app if any
          if (strlen(app_launcher.search_text) > 0) {
              // Find first matching application
              for (int i = 0; i < app_launcher.category_count; i++) {
                  AppCategory *cat = &app_launcher.categories[i];
                  for (int j = 0; j < cat->app_count; j++) {
                      char app_name_lower[256];
                      strcpy(app_name_lower, cat->apps[j].name);
                      for (char *p = app_name_lower; *p; p++) *p = tolower(*p);

                      char search_lower[256];
                      strcpy(search_lower, app_launcher.search_text);
                      for (char *p = search_lower; *p; p++) *p = tolower(*p);

                      if (strstr(app_name_lower, search_lower) != NULL) {
                          // Launch the first matching app
                          AppInfo *app = &cat->apps[j];
                          debug_log("Launching search result: %s -> %s", app->name, app->exec);

                          pid_t pid = fork();
                          if (pid == 0) {
                              setsid();
                              execl("/bin/sh", "sh", "-c", app->exec, NULL);
                              exit(0);
                          } else if (pid > 0) {
                              show_operation_feedback("Launched application");
                              hide_app_launcher();
                              return;
                          }
                          break;
                      }
                  }
              }
              show_operation_feedback("No matching applications found");
          }
      } else if (keysym == XK_BackSpace) {
          // Handle backspace
          int len = strlen(app_launcher.search_text);
          if (len > 0) {
              app_launcher.search_text[len - 1] = '\0';
              filter_applications_by_search();
              draw_app_launcher();
          }
      } else if (count > 0 && buffer[0] >= 32 && buffer[0] <= 126) {
          // Handle regular character input
          int len = strlen(app_launcher.search_text);
          if (len < sizeof(app_launcher.search_text) - 1) {
              app_launcher.search_text[len] = buffer[0];
              app_launcher.search_text[len + 1] = '\0';
              filter_applications_by_search();
              draw_app_launcher();
          }
      }
      return;
  }

  Client *c = NULL;
  if (client_count > 0) {
      // Get the most recently managed window as fallback
      c = clients[client_count - 1];
  }

  if (!c) return;

  // Use XLookupString instead of deprecated XKeycodeToKeysym
  KeySym keysym;
  char buffer[10];
  int count = XLookupString(e, buffer, sizeof(buffer), &keysym, NULL);

  switch (keysym) {
      case XK_F11:
          debug_log("F11 pressed - toggling fullscreen");
          show_operation_feedback(c->is_fullscreen ? "Exiting fullscreen" : "Entering fullscreen");
          toggle_fullscreen(c);
          break;
      case XK_F12:
          debug_log("F12 pressed - lowering window");
          show_operation_feedback("Window lowered");
          lower_window(c);
          break;
      case XK_Escape:
          if (e->state & ControlMask) {
              debug_log("Ctrl+Esc pressed - closing window");
              show_operation_feedback("Closing window");
              XKillClient(dpy, c->win);
          }
          break;
  }
}

void manage_window(Window w) {
  debug_log("Managing window %lu, current client count: %d", w, client_count);

  if (client_count >= 100) {
      debug_log("ERROR: Too many clients");
      return;
  }

  Client *c = malloc(sizeof(Client));
  if (!c) {
      debug_log("ERROR: Failed to allocate client memory");
      return;
  }

  c->win = w;
  c->is_mapped = 1;
  c->is_fullscreen = 0;
  c->is_active = 1; // New window is active by default
  c->button_hover = 0;

  // Get window attributes
  XWindowAttributes wa;
  if (XGetWindowAttributes(dpy, w, &wa)) {
      c->x = wa.x;
      c->y = wa.y;
      c->width = wa.width + 2 * FRAME_BORDER;
      c->height = wa.height + TITLEBAR_HEIGHT + 2 * FRAME_BORDER;

      c->original_x = wa.x;
      c->original_y = wa.y;
      c->original_width = c->width;
      c->original_height = c->height;

      debug_log("Window attributes: %dx%d at %d,%d", wa.width, wa.height, wa.x, wa.y);
  } else {
      debug_log("WARNING: Could not get window attributes, using defaults");
      c->x = 100;
      c->y = 100;
      c->width = 600;
      c->height = 400;
      c->original_x = 100;
      c->original_y = 100;
      c->original_width = 600;
      c->original_height = 400;
  }

  // Create frame window
  c->frame = XCreateSimpleWindow(dpy, root, c->x, c->y, c->width, c->height,
                                 BORDER_WIDTH, light_gray, black);
  debug_log("Frame window created: %lu", c->frame);

  // Set up frame window events
  XSelectInput(dpy, c->frame, ExposureMask | ButtonPressMask | ButtonReleaseMask |
               PointerMotionMask | SubstructureRedirectMask);

  // Set cursor for frame
  Cursor frame_cursor = XCreateFontCursor(dpy, XC_left_ptr);
  XDefineCursor(dpy, c->frame, frame_cursor);

  // Reparent the client window into the frame
  XReparentWindow(dpy, w, c->frame, FRAME_BORDER, TITLEBAR_HEIGHT + FRAME_BORDER);
  debug_log("Window reparented into frame");

  // Resize client to fit frame
  XResizeWindow(dpy, w, c->width - 2 * FRAME_BORDER,
                c->height - TITLEBAR_HEIGHT - 2 * FRAME_BORDER);

  // Try to get window title
  c->title = get_window_title(w);
  debug_log("Window title: '%s'", c->title);

  // Map the frame
  XMapWindow(dpy, c->frame);
  debug_log("Frame window mapped");

  clients[client_count++] = c;
  debug_log("Client added to list, new count: %d", client_count);

  // Draw decorations
  draw_window_decorations(c);

  // Redraw panel to show new window
  draw_panel();

  show_operation_feedback("New window managed");
}

void unmanage_window(Window w) {
  debug_log("Unmanaging window %lu", w);

  for (int i = 0; i < client_count; i++) {
      if (clients[i] && clients[i]->win == w) {
          debug_log("Found client at index %d", i);

          if (clients[i]->title) {
              free(clients[i]->title);
          }

          free(clients[i]);

          // Shift remaining clients
          for (int j = i; j < client_count - 1; j++) {
              clients[j] = clients[j + 1];
          }
          clients[client_count - 1] = NULL;
          client_count--;
          debug_log("Client removed, new count: %d", client_count);

          // Redraw panel
          draw_panel();
          return;
      }
  }
}

char* get_window_title(Window w) {
  Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
  Atom wm_name = XInternAtom(dpy, "WM_NAME", False);
  Atom utf8_string = XInternAtom(dpy, "UTF8_STRING", False);

  Atom type;
  int format;
  unsigned long nitems, bytes_after;
  unsigned char *data = NULL;

  // Try _NET_WM_NAME first (UTF-8)
  if (XGetWindowProperty(dpy, w, net_wm_name, 0, 1024, False,
                        utf8_string, &type, &format, &nitems, &bytes_after, &data) == Success) {
      if (data && nitems > 0) {
          char *title = strdup((char*)data);
          XFree(data);
          debug_log("Got _NET_WM_NAME title: '%s'", title);
          return title;
      }
      if (data) XFree(data);
  }

  // Try WM_NAME as fallback
  if (XGetWindowProperty(dpy, w, wm_name, 0, 1024, False,
                        XA_STRING, &type, &format, &nitems, &bytes_after, &data) == Success) {
      if (data && nitems > 0) {
          char *title = strdup((char*)data);
          XFree(data);
          debug_log("Got WM_NAME title: '%s'", title);
          return title;
      }
      if (data) XFree(data);
  }

  // Try XFetchName as last resort
  char *window_name;
  if (XFetchName(dpy, w, &window_name)) {
      if (window_name) {
          char *title = strdup(window_name);
          XFree(window_name);
          debug_log("Got XFetchName title: '%s'", title);
          return title;
      }
  }

  debug_log("No window title found, using 'Untitled'");
  return strdup("Untitled");
}

void toggle_fullscreen(Client *c) {
  if (!c->is_fullscreen) {
      // Save original position and size
      c->original_x = c->x;
      c->original_y = c->y;
      c->original_width = c->width;
      c->original_height = c->height;

      // Move to fullscreen (account for panel)
      int screen_width = DisplayWidth(dpy, screen);
      int screen_height = DisplayHeight(dpy, screen);

      // Resize the FRAME to full screen width
      c->width = screen_width;
      c->height = screen_height - PANEL_HEIGHT;

      XMoveResizeWindow(dpy, c->frame, 0, 0, c->width, c->height);
      XResizeWindow(dpy, c->win, c->width - 2 * FRAME_BORDER,
                    c->height - TITLEBAR_HEIGHT - 2 * FRAME_BORDER);
      c->is_fullscreen = 1;
      debug_log("Entered fullscreen mode: %dx%d", c->width, c->height);
  } else {
      // Restore original size and position
      c->width = c->original_width;
      c->height = c->original_height;

      XMoveResizeWindow(dpy, c->frame, c->original_x, c->original_y,
                       c->width, c->height);
      XResizeWindow(dpy, c->win, c->width - 2 * FRAME_BORDER,
                   c->height - TITLEBAR_HEIGHT - 2 * FRAME_BORDER);
      c->is_fullscreen = 0;
      debug_log("Exited fullscreen mode");
  }

  // Force redraw of decorations with new dimensions
  draw_window_decorations(c);
}

void resize_window(Client *c, int width, int height) {
  // Update client dimensions first
  c->width = width;
  c->height = height;

  // Resize the frame window
  XResizeWindow(dpy, c->frame, width, height);

  // Resize the client window inside the frame
  XResizeWindow(dpy, c->win, width - 2 * FRAME_BORDER,
                height - TITLEBAR_HEIGHT - 2 * FRAME_BORDER);

  // Redraw decorations with new dimensions
  draw_window_decorations(c);

  debug_log("Window resized to %dx%d", width, height);
}

void move_window(Client *c, int x, int y) {
  XMoveWindow(dpy, c->frame, x, y);
  c->x = x;
  c->y = y;
}

void lower_window(Client *c) {
  XLowerWindow(dpy, c->frame);
  c->is_active = 0;
  draw_window_decorations(c);
  draw_panel();
}

void close_window(Client *c) {
  debug_log("Closing window %lu", c->win);

  // Unmap and hide the frame
  XUnmapWindow(dpy, c->frame);
  XUnmapWindow(dpy, c->win);

  // Mark as unmapped
  c->is_mapped = 0;

  // Redraw panel to remove it
  draw_panel();

  XFlush(dpy);

  debug_log("Window %lu hidden from view", c->win);
}

Client* find_client(Window w) {
  for (int i = 0; i < client_count; i++) {
      if (clients[i] && (clients[i]->win == w || clients[i]->frame == w)) {
          return clients[i];
      }
  }
  return NULL;
}

void create_window_control_menu() {
  window_control_menu.width = MENU_WIDTH;
  window_control_menu.height = MENU_ITEM_HEIGHT * 4; // Increased from 3 to 4 for "Pin to Panel"
  window_control_menu.x = 0;
  window_control_menu.y = 0;
  window_control_menu.hover_item = -1;
  window_control_menu.alpha = 0.0f;
  window_control_menu.target_client = NULL;

  window_control_menu.win = XCreateSimpleWindow(dpy, root,
                                                 window_control_menu.x,
                                                 window_control_menu.y,
                                                 window_control_menu.width,
                                                 window_control_menu.height,
                                                 0, white, dark_blue);

  XSelectInput(dpy, window_control_menu.win, ButtonPressMask | ExposureMask | PointerMotionMask);
  window_control_menu.visible = 0;
}

void show_window_control_menu(int x, int y, Client *c) {
  if (!window_control_menu.visible && c) {
      window_control_menu.target_client = c;

      // Position ABOVE the panel button
      window_control_menu.x = x;
      window_control_menu.y = panel.y - window_control_menu.height - 5;

      // Keep menu on screen horizontally
      if (window_control_menu.x + window_control_menu.width > DisplayWidth(dpy, screen)) {
          window_control_menu.x = DisplayWidth(dpy, screen) - window_control_menu.width;
      }
      if (window_control_menu.x < 0) {
          window_control_menu.x = 0;
      }

      XMoveWindow(dpy, window_control_menu.win, window_control_menu.x, window_control_menu.y);
      XMapWindow(dpy, window_control_menu.win);
      XRaiseWindow(dpy, window_control_menu.win);
      window_control_menu.visible = 1;

      // Fade-in animation
      for (int i = 0; i <= 10; i++) {
          window_control_menu.alpha = (float)i / 10.0f;
          draw_window_control_menu();
          XFlush(dpy);
          usleep(10000);
      }

      XGrabPointer(dpy, root, False, ButtonPressMask, GrabModeAsync,
                  GrabModeAsync, None, None, CurrentTime);
  }
}

void hide_window_control_menu() {
  if (window_control_menu.visible) {
      // Fade-out animation
      for (int i = 10; i >= 0; i--) {
          window_control_menu.alpha = (float)i / 10.0f;
          draw_window_control_menu();
          XFlush(dpy);
          usleep(8000);
      }

      XUnmapWindow(dpy, window_control_menu.win);
      window_control_menu.visible = 0;
      window_control_menu.hover_item = -1;
      window_control_menu.target_client = NULL;
      XUngrabPointer(dpy, CurrentTime);
  }
}

void draw_window_control_menu() {
  if (!window_control_menu.visible) return;

  XClearWindow(dpy, window_control_menu.win);

  // Draw rounded background with gradient
  XSetForeground(dpy, menu_gc, menu_bg);
  draw_rounded_rectangle(window_control_menu.win, menu_gc, 0, 0,
                        window_control_menu.width, window_control_menu.height,
                        CORNER_RADIUS);

  // Draw subtle border
  XSetForeground(dpy, menu_gc, accent_color);
  XDrawRectangle(dpy, window_control_menu.win, menu_gc, 1, 1,
                 window_control_menu.width - 3, window_control_menu.height - 3);

  char *items[] = {"Pin to Panel", "Maximize", "Minimize", "Close"};

  for (int i = 0; i < 4; i++) {
      int y = i * MENU_ITEM_HEIGHT;

      // Hover effect
      if (window_control_menu.hover_item == i) {
          XSetForeground(dpy, menu_gc, menu_hover_bg);
          draw_rounded_rectangle(window_control_menu.win, menu_gc, 2, y + 2,
                                window_control_menu.width - 4, MENU_ITEM_HEIGHT - 4, 4);
      }

      if (i > 0) {
          XSetForeground(dpy, menu_gc, 0x404040);
          XDrawLine(dpy, window_control_menu.win, menu_gc, 10, y,
                   window_control_menu.width - 10, y);
      }

      XSetForeground(dpy, menu_gc, text_primary);
      int text_width = XTextWidth(XLoadQueryFont(dpy, "fixed"), items[i], strlen(items[i]));
      int text_x = (window_control_menu.width - text_width) / 2;
      int text_y = y + (MENU_ITEM_HEIGHT / 2) + 5;

      XDrawString(dpy, window_control_menu.win, menu_gc, text_x, text_y,
                 items[i], strlen(items[i]));
  }
}

int main() {
  remove("/tmp/diamondwm_debug.log");
  debug_log("=== Modern DiamondWM Starting ===");

  dpy = XOpenDisplay(NULL);
  if (!dpy) {
      debug_log("ERROR: Cannot open display");
      fprintf(stderr, "Cannot open display\n");
      exit(1);
  }

  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);

  // Enhanced modern color palette
  black = BlackPixel(dpy, screen);
  white = WhitePixel(dpy, screen);
  dark_gray = 0x202020;
  light_gray = 0x404040;
  red = 0xFF4444;
  dark_blue = 0x1b1b2c;
  titlebar_gray = 0x2D2D2D;
  titlebar_active = 0x3D3D3D;
  button_red = 0xFF4444;
  button_yellow = 0xFFAA00;
  button_green = 0x44FF44;
  purple_color = 0x8A2BE2;
  menu_bg = 0x2D2D3D;
  menu_hover_bg = 0x3D3D4D;
  accent_color = 0x6C5CE7;
  shadow_color = 0x101010;
  bg_gradient_start = 0x0a0a1a;
  bg_gradient_end = 0x1a1a2a;

  // New enhanced colors
  accent_light = 0x897DEA;
  background_dark = 0x0F0F1A;
  background_light = 0x1E1E2E;
  text_primary = 0xE0E0E0;
  text_secondary = 0x888888;

  // Create GCs with modern colors
  XGCValues gv;
  gv.foreground = light_gray;
  gv.background = black;
  gv.line_width = BORDER_WIDTH;
  gc = XCreateGC(dpy, root, GCForeground | GCBackground | GCLineWidth, &gv);

  XGCValues title_gv;
  title_gv.foreground = titlebar_gray;
  title_gv.background = titlebar_gray;
  title_gc = XCreateGC(dpy, root, GCForeground | GCBackground, &title_gv);

  set_background();

  XGCValues text_gv;
  text_gv.foreground = text_primary;
  text_gv.background = titlebar_gray;
  text_gv.font = XLoadFont(dpy, "fixed");
  text_gc = XCreateGC(dpy, root, GCForeground | GCBackground | GCFont, &text_gv);

  XGCValues panel_gv;
  panel_gv.foreground = text_primary;
  panel_gv.background = dark_blue;
  panel_gv.font = XLoadFont(dpy, "fixed");
  panel_gc = XCreateGC(dpy, root, GCForeground | GCBackground | GCFont, &panel_gv);

  XGCValues menu_gv;
  menu_gv.foreground = text_primary;
  menu_gv.background = menu_bg;
  menu_gv.font = XLoadFont(dpy, "fixed");
  menu_gc = XCreateGC(dpy, root, GCForeground | GCBackground | GCFont, &menu_gv);

  XGCValues close_gv;
  close_gv.foreground = button_red;
  close_gv.background = titlebar_gray;
  close_gc = XCreateGC(dpy, root, GCForeground | GCBackground, &close_gv);

  XGCValues minimize_gv;
  minimize_gv.foreground = button_yellow;
  minimize_gv.background = titlebar_gray;
  minimize_gc = XCreateGC(dpy, root, GCForeground | GCBackground, &minimize_gv);

  XGCValues maximize_gv;
  maximize_gv.foreground = button_green;
  maximize_gv.background = titlebar_gray;
  maximize_gc = XCreateGC(dpy, root, GCForeground | GCBackground, &maximize_gv);

  XGCValues button_gv;
  button_gv.foreground = light_gray;
  button_gv.background = titlebar_gray;
  button_gc = XCreateGC(dpy, root, GCForeground | GCBackground, &button_gv);

  // Initialize Xft colors
  XRenderColor render_color;
  render_color.red = ((text_primary >> 16) & 0xFF) * 257;
  render_color.green = ((text_primary >> 8) & 0xFF) * 257;
  render_color.blue = (text_primary & 0xFF) * 257;
  render_color.alpha = 0xFFFF;
  XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                    DefaultColormap(dpy, screen),
                    &render_color, &xft_panel_color);

  render_color.red = ((text_secondary >> 16) & 0xFF) * 257;
  render_color.green = ((text_secondary >> 8) & 0xFF) * 257;
  render_color.blue = (text_secondary & 0xFF) * 257;
  render_color.alpha = 0xFFFF;
  XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                    DefaultColormap(dpy, screen),
                    &render_color, &xft_menu_color);

  render_color.red = ((text_primary >> 16) & 0xFF) * 257;
  render_color.green = ((text_primary >> 8) & 0xFF) * 257;
  render_color.blue = (text_primary & 0xFF) * 257;
  render_color.alpha = 0xFFFF;
  XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                    DefaultColormap(dpy, screen),
                    &render_color, &xft_title_color);

  // Load Xft fonts
  load_xft_fonts();

  XSelectInput(dpy, root,
      SubstructureRedirectMask | SubstructureNotifyMask |
      ButtonPressMask | ButtonReleaseMask | PointerMotionMask | KeyPressMask);

  create_panel();

  // Initialize pinned apps
  pinned_apps.max_apps = 20;
  pinned_apps.apps = malloc(sizeof(PinnedApp) * pinned_apps.max_apps);
  pinned_apps.app_count = 0;
  load_pinned_apps();

  create_menu();
  create_app_launcher();
  create_window_control_menu();
  create_pinned_app_menu();

  debug_log("App launcher created with %d categories", app_launcher.category_count);
  for (int i = 0; i < app_launcher.category_count; i++) {
      if (app_launcher.categories[i].app_count > 0) {
          debug_log("Category %s has %d apps",
                   app_launcher.categories[i].category_name,
                   app_launcher.categories[i].app_count);
      }
  }

  debug_log("Pinned apps loaded: %d apps", pinned_apps.app_count);
  for (int i = 0; i < pinned_apps.app_count; i++) {
      debug_log("Pinned app: %s -> %s",
               pinned_apps.apps[i].name,
               pinned_apps.apps[i].exec ? pinned_apps.apps[i].exec : "no exec");
  }

  debug_log("Application launcher created");

  setup_mouse_cursor();

  XGrabKey(dpy, XKeysymToKeycode(dpy, XK_F11), 0, root, True, GrabModeAsync, GrabModeAsync);
  XGrabKey(dpy, XKeysymToKeycode(dpy, XK_F12), 0, root, True, GrabModeAsync, GrabModeAsync);
  XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Escape), ControlMask, root, True, GrabModeAsync, GrabModeAsync);

  debug_log("Modern DiamondWM initialization complete");
  printf("Modern DiamondWM v1 with Pin to Panel feature started!\n");
  printf("Features: Rounded corners, shadows, gradients, animations, hover effects!\n");
  printf("NEW: Pin to Panel - Right-click window buttons to pin apps!\n");
  printf("Controls: F11=Fullscreen, F12=Lower window, Ctrl+Esc=Close window\n");
  printf("Click window buttons: Red=Close, Yellow=Minimize, Green=Maximize\n");
  printf("Drag titlebar to move windows! Drag edges to resize!\n");
  printf("Click DiamondWM text on panel to show system menu!\n");
  printf("LEFT-click desktop background to show application launcher!\n");
  printf("RIGHT-click desktop background to always show application launcher!\n");
  printf("RIGHT-click window panel buttons to Pin/Unpin apps!\n");
  printf("Pinned apps stored in: ~/.diamondwm/pinned_apps.conf\n");
  printf("Check /tmp/diamondwm_debug.log for detailed logs\n");

  // Main event loop
  XEvent ev;
  int event_count = 0;

  while (1) {
      check_clock_update();

      if (XPending(dpy)) {
          XNextEvent(dpy, &ev);
          event_count++;

          if (event_count % 20 == 0) {
              debug_log("Event #%d: type=%d", event_count, ev.type);
          }

          switch (ev.type) {
              case MapRequest:
                  debug_log("MapRequest event for window %lu", ev.xmaprequest.window);
                  manage_window(ev.xmaprequest.window);
                  XMapWindow(dpy, ev.xmaprequest.window);
                  break;

              case UnmapNotify:
                  debug_log("UnmapNotify event for window %lu", ev.xunmap.window);
                  unmanage_window(ev.xunmap.window);
                  break;

              case DestroyNotify:
                  debug_log("DestroyNotify event for window %lu", ev.xdestroywindow.window);
                  for (int i = 0; i < client_count; i++) {
                      if (clients[i] && clients[i]->win == ev.xdestroywindow.window) {
                          debug_log("Client window %lu destroyed, destroying frame %lu",
                                   clients[i]->win, clients[i]->frame);
                          Window frame = clients[i]->frame;
                          unmanage_window(clients[i]->win);
                          XDestroyWindow(dpy, frame);
                          XFlush(dpy);
                          break;
                      }
                  }
                  break;

              case ButtonPress:
                  if (ev.xbutton.window == root) {
                      if (ev.xbutton.button == Button3) {
                          debug_log("Desktop background RIGHT clicked at %d,%d - showing app launcher",
                                   ev.xbutton.x_root, ev.xbutton.y_root);
                          show_app_launcher(ev.xbutton.x_root, ev.xbutton.y_root);
                      } else {
                          handle_button_press(&ev.xbutton);
                      }
                  } else {
                      handle_button_press(&ev.xbutton);
                  }
                  break;

              case ButtonRelease:
                  handle_button_release(&ev.xbutton);
                  break;

              case MotionNotify:
                  handle_motion_notify(&ev.xmotion);
                  break;

              case KeyPress:
                if (app_launcher.visible && ev.xkey.window == app_launcher.win) {
                    handle_key_press(&ev.xkey);
                } else {
                    handle_key_press(&ev.xkey);
                }
                break;

              case ConfigureRequest:
                  {
                      XConfigureRequestEvent *cre = &ev.xconfigurerequest;
                      XWindowChanges wc;
                      wc.x = cre->x;
                      wc.y = cre->y;
                      wc.width = cre->width;
                      wc.height = cre->height;
                      wc.border_width = cre->border_width;
                      wc.sibling = cre->above;
                      wc.stack_mode = cre->detail;
                      XConfigureWindow(dpy, cre->window, cre->value_mask, &wc);
                  }
                  break;

              case Expose:
                  if (ev.xexpose.window == panel.win) {
                      draw_panel();
                  } else if (ev.xexpose.window == menu.win) {
                      draw_menu();
                  } else if (ev.xexpose.window == app_launcher.win) {
                      draw_app_launcher();
                  } else if (ev.xexpose.window == window_control_menu.win) {
                      draw_window_control_menu();
                  } else if (ev.xexpose.window == pinned_app_menu.win) {
                      draw_pinned_app_menu();
                  } else {
                      for (int i = 0; i < client_count; i++) {
                          if (clients[i] && clients[i]->frame == ev.xexpose.window) {
                              draw_window_decorations(clients[i]);
                              break;
                          }
                      }
                  }
                  break;

              default:
                  break;
          }
      } else {
          usleep(100000);
      }
  }

  // Cleanup
  free_applications();
  free_pinned_apps();

  // Clean up Xft resources
  if (xft_fonts.regular) XftFontClose(dpy, xft_fonts.regular);
  if (xft_fonts.bold) XftFontClose(dpy, xft_fonts.bold);
  if (xft_fonts.title) XftFontClose(dpy, xft_fonts.title);
  if (xft_draw) XftDrawDestroy(xft_draw);

  debug_log("=== Modern DiamondWM Exiting ===");
  XCloseDisplay(dpy);
  return 0;
}
