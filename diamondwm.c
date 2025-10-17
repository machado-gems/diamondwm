#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <dirent.h>
#include <pwd.h>
#include <ctype.h>

#define PANEL_HEIGHT 50
#define BORDER_WIDTH 1
#define TITLEBAR_HEIGHT 30
#define BUTTON_SIZE 12
#define BUTTON_SPACING 5
#define FRAME_BORDER 5
#define RESIZE_HANDLE_SIZE 8
#define MENU_WIDTH 120
#define MENU_ITEM_HEIGHT 30

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
    Window win;        // Client window
    Window frame;      // Frame window
    int x, y;
    int width, height;
    int is_mapped;
    int is_fullscreen;
    int original_x, original_y;
    int original_width, original_height;
    char *title;
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
} Menu;

Display *dpy;
Window root;
int screen;
GC gc, panel_gc, title_gc, button_gc, close_gc, minimize_gc, maximize_gc, text_gc, menu_gc;
Client *clients[100];
int client_count = 0;
Panel panel;
Menu menu;
int panel_dragging = 0;
int drag_start_x, drag_start_y;

// Window dragging variables
int window_dragging = 0;
Client *dragged_client = NULL;
int drag_win_start_x, drag_win_start_y;
int drag_offset_x, drag_offset_y;

// Window resizing variables
int window_resizing = 0;
Client *resized_client = NULL;
int resize_start_x, resize_start_y;
int resize_start_width, resize_start_height;
int resize_edge; // 0=left, 1=right, 2=top, 3=bottom, 4=top-left, 5=top-right, 6=bottom-left, 7=bottom-right

// Color definitions
unsigned long black, white, dark_gray, light_gray, red, dark_blue;
unsigned long titlebar_gray, button_red, button_yellow, button_green, purple_color;
unsigned long menu_bg, menu_hover_bg;

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
} AppLauncherMenu;

AppLauncherMenu app_launcher;

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
void draw_diamond_icon();
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

void set_background() {
    // Try to set a background image using feh
    system("feh --bg-scale /usr/share/backgrounds/* 2>/dev/null &");

    // Fallback: create a simple gradient background
    Pixmap bg_pixmap = XCreatePixmap(dpy, root,
                                    DisplayWidth(dpy, screen),
                                    DisplayHeight(dpy, screen),
                                    DefaultDepth(dpy, screen));

    GC bg_gc = XCreateGC(dpy, bg_pixmap, 0, NULL);

    // Create a dark gradient background
    for (int y = 0; y < DisplayHeight(dpy, screen); y++) {
        int color = 0x0a0a0a + (y * 0x010101) / 4;
        XSetForeground(dpy, bg_gc, color);
        XDrawLine(dpy, bg_pixmap, bg_gc, 0, y, DisplayWidth(dpy, screen), y);
    }

    XSetWindowBackgroundPixmap(dpy, root, bg_pixmap);
    XClearWindow(dpy, root);

    XFreeGC(dpy, bg_gc);
    XFreePixmap(dpy, bg_pixmap);
}

int is_window_visible(Client *c) {
    if (!c) return 0;

    int screen_width = DisplayWidth(dpy, screen);
    int screen_height = DisplayHeight(dpy, screen);

    // Check if window is completely outside screen bounds
    if (c->x >= screen_width || c->y >= screen_height) {
        return 0;
    }

    // Check if window is partially visible (at least some part should be visible)
    if (c->x + c->width <= 0 || c->y + c->height <= 0) {
        return 0;
    }

    return 1;
}

void draw_diamond_icon(int x, int y, int size) {
    // Define diamond points
    XPoint diamond[] = {
        {x + size/2, y},
        {x + size, y + size/2},
        {x + size/2, y + size},
        {x, y + size/2}
    };

    // Draw the purple diamond
    XSetForeground(dpy, panel_gc, purple_color);
    XFillPolygon(dpy, panel.win, panel_gc, diamond, 4, Convex, CoordModeOrigin);
}

void draw_clock() {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[6]; // HH:MM format

    strftime(time_str, sizeof(time_str), "%H:%M", tm_info);

    // Draw clock on the right side
    int text_width = XTextWidth(XLoadQueryFont(dpy, "fixed"), time_str, strlen(time_str));
    int x = panel.width - text_width - 20; // 20px from right edge

    XSetForeground(dpy, panel_gc, white);
    XDrawString(dpy, panel.win, panel_gc, x, 30, time_str, strlen(time_str));
}

void check_clock_update() {
    static time_t last_update = 0;
    time_t now = time(NULL);

    // Update clock every second
    if (now != last_update) {
        last_update = now;
        draw_panel(); // Redraw panel to update clock
    }
}

void create_menu() {
    menu.width = MENU_WIDTH;
    menu.height = MENU_ITEM_HEIGHT * 4; // 4 items
    menu.x = panel.width - menu.width - 20; // Position near DiamondWM text
    menu.y = panel.y - menu.height; // Position above panel

    menu.win = XCreateSimpleWindow(dpy, root, menu.x, menu.y,
                                  menu.width, menu.height, 1,
                                  white, dark_blue);

    XSelectInput(dpy, menu.win, ButtonPressMask | ExposureMask);
    menu.visible = 0;
}

void show_menu() {
    if (!menu.visible) {
        // Position menu above DiamondWM area
        int diamondwm_width = 120; // Approximate width of diamond + text
        menu.x = panel.width - diamondwm_width - 20;
        menu.y = panel.y - menu.height;

        XMoveWindow(dpy, menu.win, menu.x, menu.y);
        XMapWindow(dpy, menu.win);
        menu.visible = 1;
        draw_menu();

        // Grab the pointer to detect clicks outside menu
        XGrabPointer(dpy, root, False, ButtonPressMask, GrabModeAsync,
                    GrabModeAsync, None, None, CurrentTime);
    }
}

void hide_menu() {
    if (menu.visible) {
        XUnmapWindow(dpy, menu.win);
        menu.visible = 0;

        // Ungrab the pointer
        XUngrabPointer(dpy, CurrentTime);
    }
}

void draw_menu() {
    if (!menu.visible) return;

    XClearWindow(dpy, menu.win);

    // Draw menu background
    XSetForeground(dpy, menu_gc, menu_bg);
    XFillRectangle(dpy, menu.win, menu_gc, 0, 0, menu.width, menu.height);

    // Draw menu border
    XSetForeground(dpy, menu_gc, white);
    XDrawRectangle(dpy, menu.win, menu_gc, 0, 0, menu.width - 1, menu.height - 1);

    // Draw menu items
    char *items[] = {"Terminal", "Lock", "Logout", "Shutdown"};
    XSetForeground(dpy, menu_gc, white);

    for (int i = 0; i < 4; i++) {
        int y = i * MENU_ITEM_HEIGHT;

        // Draw separator line between items (except after last item)
        if (i > 0) {
            XDrawLine(dpy, menu.win, menu_gc, 0, y, menu.width, y);
        }

        // Draw item text centered
        int text_width = XTextWidth(XLoadQueryFont(dpy, "fixed"), items[i], strlen(items[i]));
        int text_x = (menu.width - text_width) / 2;
        int text_y = y + (MENU_ITEM_HEIGHT / 2) + 5;

        XDrawString(dpy, menu.win, menu_gc, text_x, text_y, items[i], strlen(items[i]));
    }
}

int is_in_diamondwm_area(int x, int y) {
    // Calculate DiamondWM text area (diamond + text)
    int diamond_size = 20;
    int text_width = XTextWidth(XLoadQueryFont(dpy, "fixed"), "DiamondWM", 9);
    int total_width = diamond_size + 5 + text_width; // diamond + spacing + text

    int area_x = panel.width - total_width - 20; // 20px from right edge
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

    // Check if click is within menu bounds
    if (relative_x < 0 || relative_x > menu.width ||
        relative_y < 0 || relative_y > menu.height) {
        return -1;
    }

    return relative_y / MENU_ITEM_HEIGHT;
}

void load_applications() {
    debug_log("=== STARTING APPLICATION LOADING ===");

    // Common desktop file directories
    const char *desktop_dirs[] = {
        "/usr/share/applications",
        "/usr/local/share/applications",
        "/home/%s/.local/share/applications",
        NULL
    };

    // Category mapping - more comprehensive
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

    // Get current username
    char username[100];
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        strncpy(username, pw->pw_name, sizeof(username)-1);
        username[sizeof(username)-1] = '\0';
    } else {
        strcpy(username, "user");
    }
    debug_log("Loading apps for user: %s", username);

    // First pass: count total .desktop files
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

    // Allocate temporary array for all apps
    AppInfo *all_apps = malloc(sizeof(AppInfo) * total_files);
    if (!all_apps) {
        debug_log("ERROR: Failed to allocate memory for %d apps", total_files);
        return;
    }
    memset(all_apps, 0, sizeof(AppInfo) * total_files);

    int app_index = 0;

    // Second pass: read and parse desktop files
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
                    // Remove newline and trailing whitespace
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

                    // Check Type=Application
                    if (strncmp(line, "Type=", 5) == 0) {
                        if (strstr(line + 5, "Application")) {
                            is_application = 1;
                        } else {
                            break; // Not an application, skip this file
                        }
                    }

                    // Check Hidden
                    if (strncmp(line, "Hidden=", 7) == 0) {
                        if (strstr(line + 7, "true")) {
                            hidden = 1;
                            break;
                        }
                    }

                    // Check NoDisplay
                    if (strncmp(line, "NoDisplay=", 10) == 0) {
                        if (strstr(line + 10, "true")) {
                            no_display = 1;
                            break;
                        }
                    }

                    // Read Name
                    if (strncmp(line, "Name=", 5) == 0 && !app.name) {
                        app.name = strdup(line + 5);
                    }

                    // Read Exec
                    if (strncmp(line, "Exec=", 5) == 0 && !app.exec) {
                        char *exec_cmd = line + 5;
                        // Remove common field codes and arguments
                        char clean_exec[512];
                        char *dest = clean_exec;

                        for (char *src = exec_cmd; *src && (dest - clean_exec) < 510; src++) {
                            if (*src == '%' && *(src+1)) {
                                // Skip field codes like %U, %F, %u, %f
                                src++; // Skip the percent and the following character
                                continue;
                            }
                            if (*src == ' ' && (src == exec_cmd || *(src-1) == ' ')) {
                                // Skip leading and multiple spaces
                                continue;
                            }
                            *dest++ = *src;
                        }
                        *dest = '\0';

                        // Remove trailing space if present
                        if (dest > clean_exec && *(dest-1) == ' ') {
                            *(dest-1) = '\0';
                        }

                        app.exec = strdup(clean_exec);
                    }

                    // Read Categories
                    if (strncmp(line, "Categories=", 11) == 0 && !app.categories) {
                        app.categories = strdup(line + 11);
                    }

                    // Read Comment
                    if (strncmp(line, "Comment=", 8) == 0 && !app.comment) {
                        app.comment = strdup(line + 8);
                    }

                    // Read Icon
                    if (strncmp(line, "Icon=", 5) == 0 && !app.icon) {
                        app.icon = strdup(line + 5);
                    }
                }

                fclose(file);

                // Validate and add the application
                if (is_application && !hidden && !no_display && app.name && app.exec) {
                    // If no categories, assign to Utilities
                    if (!app.categories) {
                        app.categories = strdup("Utility");
                    }

                    // If no comment, use empty string
                    if (!app.comment) {
                        app.comment = strdup("");
                    }

                    // If no icon, use empty string
                    if (!app.icon) {
                        app.icon = strdup("");
                    }

                    all_apps[app_index++] = app;
                    debug_log("    ✓ Loaded: %s -> %s (Categories: %s)",
                             app.name, app.exec, app.categories);
                } else {
                    // Free any allocated memory for invalid app
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

    // Count category occurrences
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

    // Initialize categories
    for (int i = 0; i < category_count; i++) {
        app_launcher.categories[i].category_name = strdup(category_map[i].name);
        app_launcher.categories[i].apps = NULL;
        app_launcher.categories[i].app_count = 0;
        app_launcher.categories[i].expanded = 0;
    }

    // First, count apps per category
    for (int i = 0; i < app_index; i++) {
        char *categories = strdup(all_apps[i].categories);
        char *token = strtok(categories, ";");

        while (token) {
            // Clean up category name (remove spaces)
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

    // Allocate memory for apps in each category
    for (int i = 0; i < category_count; i++) {
        if (app_launcher.categories[i].app_count > 0) {
            app_launcher.categories[i].apps = malloc(sizeof(AppInfo) * app_launcher.categories[i].app_count);
            if (!app_launcher.categories[i].apps) {
                debug_log("ERROR: Failed to allocate apps for category %s",
                         app_launcher.categories[i].category_name);
                // Set count to 0 on allocation failure
                app_launcher.categories[i].app_count = 0;
            } else {
                // Reset counter for filling
                app_launcher.categories[i].app_count = 0;
            }
        }
    }

    // Now assign apps to categories
    for (int i = 0; i < app_index; i++) {
        char *categories = strdup(all_apps[i].categories);
        char *token = strtok(categories, ";");
        int assigned = 0;

        while (token && !assigned) {
            // Clean up category name
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

        // If app wasn't assigned to any known category, assign to Utilities
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

    // Free the temporary array (but not the contents, as they're now in categories)
    free(all_apps);

    // Final summary
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

            // Log first few apps in each category for verification
            for (int j = 0; j < app_launcher.categories[i].app_count && j < 3; j++) {
                debug_log("    %s -> %s",
                         app_launcher.categories[i].apps[j].name,
                         app_launcher.categories[i].apps[j].exec);
            }
            if (app_launcher.categories[i].app_count > 3) {
                debug_log("    ... and %d more", app_launcher.categories[i].app_count - 3);
            }
        }
    }

    debug_log("TOTAL: %d applications in %d categories", total_loaded_apps, non_empty_categories);
    debug_log("=== APPLICATION LOADING COMPLETE ===");
}

void create_app_launcher() {
    load_applications();

    app_launcher.width = 350;
    app_launcher.height = 600;
    app_launcher.x = 100;
    app_launcher.y = 100;

    app_launcher.win = XCreateSimpleWindow(dpy, root, app_launcher.x, app_launcher.y,
                                          app_launcher.width, app_launcher.height, 2,
                                          white, dark_blue);

    XSelectInput(dpy, app_launcher.win, ButtonPressMask | ExposureMask);
    app_launcher.visible = 0;

    debug_log("App launcher created: %dx%d", app_launcher.width, app_launcher.height);
}

void show_app_launcher(int x, int y) {
    if (!app_launcher.visible) {
        // Position at click location, but ensure it stays on screen
        int screen_width = DisplayWidth(dpy, screen);
        int screen_height = DisplayHeight(dpy, screen);

        app_launcher.x = x;
        app_launcher.y = y;

        // Adjust if going off screen
        if (app_launcher.x + app_launcher.width > screen_width) {
            app_launcher.x = screen_width - app_launcher.width - 10;
        }
        if (app_launcher.y + app_launcher.height > screen_height) {
            app_launcher.y = screen_height - app_launcher.height - 10;
        }

        XMoveWindow(dpy, app_launcher.win, app_launcher.x, app_launcher.y);
        XMapWindow(dpy, app_launcher.win);
        app_launcher.visible = 1;
        draw_app_launcher();

        // Grab pointer to detect clicks outside
        XGrabPointer(dpy, root, False, ButtonPressMask, GrabModeAsync,
                    GrabModeAsync, None, None, CurrentTime);
    }
}

void hide_app_launcher() {
    if (app_launcher.visible) {
        XUnmapWindow(dpy, app_launcher.win);
        app_launcher.visible = 0;
        XUngrabPointer(dpy, CurrentTime);
    }
}

void draw_app_launcher() {
    if (!app_launcher.visible) return;

    XClearWindow(dpy, app_launcher.win);

    // Draw background
    XSetForeground(dpy, menu_gc, dark_blue);
    XFillRectangle(dpy, app_launcher.win, menu_gc, 0, 0, app_launcher.width, app_launcher.height);

    // Draw border
    XSetForeground(dpy, menu_gc, white);
    XDrawRectangle(dpy, app_launcher.win, menu_gc, 0, 0, app_launcher.width - 1, app_launcher.height - 1);

    // Draw title
    XDrawString(dpy, app_launcher.win, menu_gc, 10, 20, "Applications", 12);
    XDrawLine(dpy, app_launcher.win, menu_gc, 0, 25, app_launcher.width, 25);

    int y_pos = 40;

    // Draw categories and apps
    for (int i = 0; i < app_launcher.category_count; i++) {
        AppCategory *cat = &app_launcher.categories[i];

        if (cat->app_count == 0) continue;

        // Draw category name with expand/collapse indicator
        char category_text[100];
        snprintf(category_text, sizeof(category_text), "%s [%s] (%d)",
                cat->category_name, cat->expanded ? "-" : "+", cat->app_count);

        XSetForeground(dpy, menu_gc, white);
        XDrawString(dpy, app_launcher.win, menu_gc, 20, y_pos, category_text, strlen(category_text));
        y_pos += 25;

        // Draw apps if expanded
        if (cat->expanded) {
            for (int j = 0; j < cat->app_count; j++) {
                XSetForeground(dpy, menu_gc, 0xCCCCCC); // Lighter color for apps
                XDrawString(dpy, app_launcher.win, menu_gc, 40, y_pos,
                           cat->apps[j].name, strlen(cat->apps[j].name));
                y_pos += 20;
            }
        }

        // Add some spacing between categories
        y_pos += 5;

        // Stop if we're running out of space
        if (y_pos > app_launcher.height - 20) break;
    }
}

int is_in_app_launcher_area(int x, int y) {
    if (!app_launcher.visible) return 0;

    return (x >= app_launcher.x && x <= app_launcher.x + app_launcher.width &&
            y >= app_launcher.y && y <= app_launcher.y + app_launcher.height);
}

int get_app_launcher_item_at(int x, int y) {
    if (!app_launcher.visible) return -1;

    int relative_x = x - app_launcher.x;
    int relative_y = y - app_launcher.y;

    if (relative_x < 0 || relative_x > app_launcher.width ||
        relative_y < 0 || relative_y > app_launcher.height) {
        return -1;
    }

    // Skip title area
    if (relative_y < 35) return -1;

    int y_pos = 40; // Start after title
    int category_index = 0;

    for (int i = 0; i < app_launcher.category_count; i++) {
        AppCategory *cat = &app_launcher.categories[i];
        if (cat->app_count == 0) continue;

        // Check if clicking category header (25px height)
        if (relative_y >= y_pos && relative_y <= y_pos + 20) {
            return category_index; // Return category index
        }
        y_pos += 25;

        // Check apps if category is expanded
        if (cat->expanded) {
            for (int j = 0; j < cat->app_count; j++) {
                if (relative_y >= y_pos && relative_y <= y_pos + 20) {
                    // Encode: category index in upper 16 bits, app index + 1 in lower 16 bits
                    return (i << 16) | (j + 1);
                }
                y_pos += 20;
            }
        }

        // Add spacing between categories
        y_pos += 5;
        category_index++;
    }

    return -1;
}



void handle_app_launcher_click(int x, int y) {
    int item = get_app_launcher_item_at(x, y);

    debug_log("App launcher click: item=0x%x at %d,%d", item, x, y);

    if (item == -1) {
        hide_app_launcher();
        return;
    }

    // Category click (simple index)
    if (item < 0x10000) {
        int cat_index = item;
        int actual_cat_index = -1;
        int count = 0;

        // Find the actual category index (skip empty categories)
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
    }
    // App click (encoded)
    else {
        int cat_index = (item >> 16) & 0xFFFF;
        int app_index = (item & 0xFFFF) - 1;

        debug_log("App clicked: category=%d, app=%d", cat_index, app_index);

        // Find the actual category
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

            // Fork and execute the application
            pid_t pid = fork();
            if (pid == 0) {
                // Child process
                setsid();
                execl("/bin/sh", "sh", "-c", app->exec, NULL);
                exit(0);
            } else if (pid > 0) {
                // Parent process
                hide_app_launcher();
            } else {
                debug_log("ERROR: Failed to fork for application launch");
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

int main() {
    // Clear previous log and start fresh
    remove("/tmp/diamondwm_debug.log");
    debug_log("=== DiamondWM Starting ===");

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        debug_log("ERROR: Cannot open display");
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }
    debug_log("Display opened successfully");

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    debug_log("Screen: %d, Root window: %lu", screen, root);

    // Define colors
    black = BlackPixel(dpy, screen);
    white = WhitePixel(dpy, screen);
    dark_gray = 0x202020;
    light_gray = 0x404040;
    red = 0xFF4444;
    dark_blue = 0x1b1b1c;
    titlebar_gray = 0x2D2D2D;
    button_red = 0x00FF;
    button_yellow = 0xFFDA;
    button_green = 0xDC14;
    purple_color = 0x8A2BE2;
    menu_bg = 0x2D2D2D;
    menu_hover_bg = 0x404040;
    debug_log("Colors defined");

    // Create GC for window frames
    XGCValues gv;
    gv.foreground = light_gray;
    gv.background = black;
    gv.line_width = BORDER_WIDTH;
    gc = XCreateGC(dpy, root, GCForeground | GCBackground | GCLineWidth, &gv);
    debug_log("Frame GC created");

    // Create GC for titlebar background
    XGCValues title_gv;
    title_gv.foreground = titlebar_gray;
    title_gv.background = titlebar_gray;
    title_gc = XCreateGC(dpy, root, GCForeground | GCBackground, &title_gv);
    debug_log("Titlebar GC created");

    set_background();
    debug_log("Background set");

    // Create GC for title text
    XGCValues text_gv;
    text_gv.foreground = white;
    text_gv.background = titlebar_gray;
    text_gv.font = XLoadFont(dpy, "fixed");
    text_gc = XCreateGC(dpy, root, GCForeground | GCBackground | GCFont, &text_gv);
    debug_log("Text GC created");

    // Create GC for panel
    XGCValues panel_gv;
    panel_gv.foreground = white;
    panel_gv.background = dark_blue;
    panel_gv.font = XLoadFont(dpy, "fixed");
    panel_gc = XCreateGC(dpy, root, GCForeground | GCBackground | GCFont, &panel_gv);
    debug_log("Panel GC created");

    // Create GC for menu
    XGCValues menu_gv;
    menu_gv.foreground = white;
    menu_gv.background = menu_bg;
    menu_gv.font = XLoadFont(dpy, "fixed");
    menu_gc = XCreateGC(dpy, root, GCForeground | GCBackground | GCFont, &menu_gv);
    debug_log("Menu GC created");

    // Create GC for close button
    XGCValues close_gv;
    close_gv.foreground = button_red;
    close_gv.background = titlebar_gray;
    close_gc = XCreateGC(dpy, root, GCForeground | GCBackground, &close_gv);
    debug_log("Close button GC created");

    // Create GC for minimize button
    XGCValues minimize_gv;
    minimize_gv.foreground = button_yellow;
    minimize_gv.background = titlebar_gray;
    minimize_gc = XCreateGC(dpy, root, GCForeground | GCBackground, &minimize_gv);
    debug_log("Minimize button GC created");

    // Create GC for maximize button
    XGCValues maximize_gv;
    maximize_gv.foreground = button_green;
    maximize_gv.background = titlebar_gray;
    maximize_gc = XCreateGC(dpy, root, GCForeground | GCBackground, &maximize_gv);
    debug_log("Maximize button GC created");

    // Create GC for buttons (fallback)
    XGCValues button_gv;
    button_gv.foreground = light_gray;
    button_gv.background = titlebar_gray;
    button_gc = XCreateGC(dpy, root, GCForeground | GCBackground, &button_gv);
    debug_log("Button GC created");

    // Set up event masks
    XSelectInput(dpy, root,
        SubstructureRedirectMask | SubstructureNotifyMask |
        ButtonPressMask | ButtonReleaseMask | PointerMotionMask | KeyPressMask);
    debug_log("Event masks set");

    // Create panel
    create_panel();
    debug_log("Panel created");

    // Create menu
    create_menu();
    debug_log("Menu created");

    create_app_launcher();
    debug_log("Application launcher created");

    // Setup mouse cursor
    setup_mouse_cursor();
    debug_log("Mouse cursor setup");

    // Grab keys for window management
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_F11), 0, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_F12), 0, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Escape), ControlMask, root, True, GrabModeAsync, GrabModeAsync);
    debug_log("Keys grabbed: F11, F12, Ctrl+Esc");

    debug_log("DiamondWM initialization complete - entering main loop");
    printf("DiamondWM started with macOS-style decorations!\n");
    printf("Controls: F11=Fullscreen, F12=Lower window, Ctrl+Esc=Close window\n");
    printf("Click window buttons: Red=Close, Yellow=Minimize, Green=Maximize\n");
    printf("Drag titlebar to move windows! Drag edges to resize!\n");
    printf("Click DiamondWM text on panel to show system menu!\n");
    printf("LEFT-click desktop background to show application launcher!\n");
    printf("RIGHT-click desktop background to always show application launcher!\n");
    printf("Check /tmp/diamondwm_debug.log for detailed logs\n");

    // Main event loop
    XEvent ev;
    int event_count = 0;

    while (1) {
        // Check for clock updates
        check_clock_update();

        // Check for events with a short timeout
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
                    // Handle client window destruction
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
                    // Check if click is on root window (desktop background)
                    if (ev.xbutton.window == root) {
                        if (ev.xbutton.button == Button1) {
                            // Left click - only show app launcher if no other menus are visible
                            if (!app_launcher.visible && !menu.visible) {
                                debug_log("Desktop background LEFT clicked at %d,%d - showing app launcher",
                                         ev.xbutton.x_root, ev.xbutton.y_root);
                                show_app_launcher(ev.xbutton.x_root, ev.xbutton.y_root);
                            } else {
                                // If menus are visible, the click will be handled by handle_button_press
                                // which will close the visible menus
                                handle_button_press(&ev.xbutton);
                            }
                        } else if (ev.xbutton.button == Button3) {
                            // Right click - always show app launcher
                            debug_log("Desktop background RIGHT clicked at %d,%d - showing app launcher",
                                     ev.xbutton.x_root, ev.xbutton.y_root);
                            show_app_launcher(ev.xbutton.x_root, ev.xbutton.y_root);
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
                    handle_key_press(&ev.xkey);
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
                    } else {
                        // Check if this is a frame window that needs redrawing
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
            // No events, sleep briefly
            usleep(100000); // 100ms
        }
    }

    free_applications();

    debug_log("=== DiamondWM Exiting ===");
    XCloseDisplay(dpy);
    return 0;
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

    // Make panel always on bottom
    XLowerWindow(dpy, panel.win);
    debug_log("Panel lowered to bottom");
}

void setup_mouse_cursor() {
    debug_log("Setting up mouse cursor");

    // Create standard arrow cursor using font cursor
    Cursor cursor = XCreateFontCursor(dpy, XC_left_ptr);

    // Set cursor for root window and all UI elements
    XDefineCursor(dpy, root, cursor);
    XDefineCursor(dpy, panel.win, cursor);
    XDefineCursor(dpy, menu.win, cursor);
    XDefineCursor(dpy, app_launcher.win, cursor);

    debug_log("Mouse cursor initialization complete");
}

void draw_panel() {
    XClearWindow(dpy, panel.win);

    // Draw panel background
    XSetForeground(dpy, panel_gc, dark_blue);
    XFillRectangle(dpy, panel.win, panel_gc, 0, 0, panel.width, panel.height);

    // Draw client icons/buttons - ONLY FOR MAPPED WINDOWS
    int x = 20;
    XSetForeground(dpy, panel_gc, white);

    for (int i = 0; i < client_count; i++) {
        if (clients[i] && clients[i]->is_mapped) {  // Check is_mapped here
            // Draw app icon (simple rectangle with window number)
            XSetForeground(dpy, panel_gc, 0x666666);
            XFillRectangle(dpy, panel.win, panel_gc, x, 10, 40, 30);

            // Draw border
            XSetForeground(dpy, panel_gc, white);
            XDrawRectangle(dpy, panel.win, panel_gc, x, 10, 40, 30);

            // Draw window identifier
            char label[10];
            snprintf(label, sizeof(label), "%d", i+1);
            XDrawString(dpy, panel.win, panel_gc, x + 15, 30, label, strlen(label));
            x += 50;
        }
    }

    // Draw DiamondWM area on the right (diamond + text)
    int diamond_size = 20;
    int diamond_x = panel.width - 150; // Position for diamond
    int diamond_y = (PANEL_HEIGHT - diamond_size) / 2;

    // Draw diamond icon
    draw_diamond_icon(diamond_x, diamond_y, diamond_size);

    // Draw DiamondWM text
    int text_x = diamond_x + diamond_size + 5;
    int text_y = 30;
    XDrawString(dpy, panel.win, panel_gc, text_x, text_y, "DiamondWM", 9);

    // Draw clock on the right (before DiamondWM area)
    draw_clock();
}

void draw_window_decorations(Client *c) {
    if (!c || !c->frame) return;

    debug_log("Drawing decorations for window %lu (frame: %lu) - Size: %dx%d",
             c->win, c->frame, c->width, c->height);

    // Clear the entire frame
    XClearWindow(dpy, c->frame);

    // Draw titlebar background - FULL WIDTH of the frame
    XSetForeground(dpy, title_gc, titlebar_gray);
    XFillRectangle(dpy, c->frame, title_gc, 0, 0, c->width, TITLEBAR_HEIGHT);

    // Draw window title - centered in available space after buttons
    if (c->title) {
        // Calculate available space for title (after buttons)
        int title_available_width = c->width - 100; // Reserve space for buttons on left
        int title_x = 80; // Start after buttons

        // Truncate long titles if necessary
        char display_title[256];
        if (strlen(c->title) > 30) {
            strncpy(display_title, c->title, 27);
            strcpy(display_title + 27, "...");
        } else {
            strcpy(display_title, c->title);
        }

        // Center title in available space
        int text_width = XTextWidth(XLoadQueryFont(dpy, "fixed"), display_title, strlen(display_title));
        if (text_width < title_available_width) {
            title_x = 80 + (title_available_width - text_width) / 2;
        }

        int title_y = TITLEBAR_HEIGHT / 2 + 5; // Vertically center

        debug_log("Drawing title: '%s' at position %d,%d (width: %d)",
                 display_title, title_x, title_y, text_width);
        XDrawString(dpy, c->frame, text_gc, title_x, title_y, display_title, strlen(display_title));
    } else {
        debug_log("Drawing default title 'Untitled'");
        XDrawString(dpy, c->frame, text_gc, 80, TITLEBAR_HEIGHT / 2 + 5, "Untitled", 8);
    }

    // Draw diamond-shaped buttons - positioned relative to left side
    int button_y = (TITLEBAR_HEIGHT - BUTTON_SIZE) / 2;

    // Close button (red) - DIAMOND
    XPoint close_diamond[] = {
        {15 + BUTTON_SIZE/2, button_y},
        {15 + BUTTON_SIZE, button_y + BUTTON_SIZE/2},
        {15 + BUTTON_SIZE/2, button_y + BUTTON_SIZE},
        {15, button_y + BUTTON_SIZE/2}
    };
    XSetForeground(dpy, close_gc, button_red);
    XFillPolygon(dpy, c->frame, close_gc, close_diamond, 4, Convex, CoordModeOrigin);

    // Minimize button (yellow) - DIAMOND
    XPoint minimize_diamond[] = {
        {15 + BUTTON_SIZE + BUTTON_SPACING + BUTTON_SIZE/2, button_y},
        {15 + BUTTON_SIZE + BUTTON_SPACING + BUTTON_SIZE, button_y + BUTTON_SIZE/2},
        {15 + BUTTON_SIZE + BUTTON_SPACING + BUTTON_SIZE/2, button_y + BUTTON_SIZE},
        {15 + BUTTON_SIZE + BUTTON_SPACING, button_y + BUTTON_SIZE/2}
    };
    XSetForeground(dpy, minimize_gc, button_yellow);
    XFillPolygon(dpy, c->frame, minimize_gc, minimize_diamond, 4, Convex, CoordModeOrigin);

    // Maximize button (green) - DIAMOND
    XPoint maximize_diamond[] = {
        {15 + 2*(BUTTON_SIZE + BUTTON_SPACING) + BUTTON_SIZE/2, button_y},
        {15 + 2*(BUTTON_SIZE + BUTTON_SPACING) + BUTTON_SIZE, button_y + BUTTON_SIZE/2},
        {15 + 2*(BUTTON_SIZE + BUTTON_SPACING) + BUTTON_SIZE/2, button_y + BUTTON_SIZE},
        {15 + 2*(BUTTON_SIZE + BUTTON_SPACING), button_y + BUTTON_SIZE/2}
    };
    XSetForeground(dpy, maximize_gc, button_green);
    XFillPolygon(dpy, c->frame, maximize_gc, maximize_diamond, 4, Convex, CoordModeOrigin);

    // Draw window border - around entire frame
    XSetForeground(dpy, gc, light_gray);
    XDrawRectangle(dpy, c->frame, gc, 0, 0, c->width - 1, c->height - 1);

    // Draw separator between titlebar and content - full width
    XDrawLine(dpy, c->frame, gc, 0, TITLEBAR_HEIGHT, c->width, TITLEBAR_HEIGHT);

    debug_log("Decorations drawn for window %lu with full width %d", c->win, c->width);
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
        if (e->window == app_launcher.win) {
            // Click on app launcher itself - handle normally
            handle_app_launcher_click(e->x_root, e->y_root);
            return;
        } else {
            // Click outside app launcher - close it
            debug_log("Click outside app launcher - hiding it");
            hide_app_launcher();
            // Don't return here - let the click be processed by other handlers
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
                        return;
                    case 1: // Lock
                        debug_log("Lock clicked - not implemented");
                        hide_menu();
                        return;
                    case 2: // Logout
                        debug_log("Logout clicked - exiting window manager");
                        hide_menu();
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
        int x = 20;
        for (int i = 0; i < client_count; i++) {
            if (clients[i] && clients[i]->is_mapped) {
                if (e->x >= x && e->x <= x + 40 && e->y >= 10 && e->y <= 40) {
                    debug_log("Panel button clicked for client %d", i);

                    // Check if window is not visible and reposition it if needed
                    if (!is_window_visible(clients[i])) {
                        debug_log("Window %d is not visible, repositioning to 10,10", i);

                        int screen_width = DisplayWidth(dpy, screen);
                        int screen_height = DisplayHeight(dpy, screen);

                        // Calculate maximum size that fits on screen
                        int max_width = screen_width - 20;
                        int max_height = screen_height - PANEL_HEIGHT - 20;

                        // Resize window to fit screen if it's too large
                        if (clients[i]->width > max_width || clients[i]->height > max_height) {
                            int new_width = (clients[i]->width > max_width) ? max_width : clients[i]->width;
                            int new_height = (clients[i]->height > max_height) ? max_height : clients[i]->height;

                            debug_log("Resizing window from %dx%d to %dx%d",
                                     clients[i]->width, clients[i]->height, new_width, new_height);

                            resize_window(clients[i], new_width, new_height);
                        }

                        // Move window to top-left corner with 10px offset
                        move_window(clients[i], 10, 10);
                        debug_log("Window repositioned to 10,10 with size %dx%d",
                                 clients[i]->width, clients[i]->height);
                    }

                    XRaiseWindow(dpy, clients[i]->frame);
                    XSetInputFocus(dpy, clients[i]->win, RevertToPointerRoot, CurrentTime);
                    break;
                }
                x += 50;
            }
        }

        // Start panel drag if clicked on empty area
        if (e->button == Button1 && !menu.visible && !app_launcher.visible) {
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
                    close_window(c);
                    return;
                } else if (is_in_minimize_button(c, e->x, e->y)) {
                    debug_log("Minimize button clicked");
                    lower_window(c);
                    return;
                } else if (is_in_maximize_button(c, e->x, e->y)) {
                    debug_log("Maximize button clicked");
                    toggle_fullscreen(c);
                    return;
                } else {
                    debug_log("Titlebar clicked - starting window drag");
                    XRaiseWindow(dpy, c->frame);
                    XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);

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
    // Handle panel dragging
    if (panel_dragging && e->window == panel.win) {
        int delta_x = e->x_root - drag_start_x;
        int delta_y = e->y_root - drag_start_y;

        // Move panel
        panel.x += delta_x;
        panel.y += delta_y;

        XMoveWindow(dpy, panel.win, panel.x, panel.y);

        drag_start_x = e->x_root;
        drag_start_y = e->y_root;
    }

    // Handle window dragging
    if (window_dragging && dragged_client) {
        int new_x = e->x_root - drag_offset_x;
        int new_y = e->y_root - drag_offset_y;

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

        // Move the window
        XMoveWindow(dpy, dragged_client->frame, new_x, new_y);

        // Update client position
        dragged_client->x = new_x;
        dragged_client->y = new_y;

        debug_log("Window moved to %d,%d", new_x, new_y);
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
}

void handle_key_press(XKeyEvent *e) {
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
            toggle_fullscreen(c);
            break;
        case XK_F12:
            debug_log("F12 pressed - lowering window");
            lower_window(c);
            break;
        case XK_Escape:
            if (e->state & ControlMask) {
                debug_log("Ctrl+Esc pressed - closing window");
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

    // Try to get window title - IMPROVED: Try multiple properties
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
