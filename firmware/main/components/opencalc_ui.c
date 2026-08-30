// Cory Pearl
// 05/22/26

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <dirent.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board_init.h"
#include "opencalc_breakout.h"
#include "opencalc_config.h"
#include "opencalc_doom.h"
#include "opencalc_mario.h"
#include "opencalc_math.h"
#include "opencalc_persist.h"
#include "opencalc_power.h"
#include "opencalc_snake.h"
#include "opencalc_tetris.h"
#include "opencalc_ui.h"
#include "tiny-python.h"
#include "usb_msc.h"

static bool s_light_mode = false;

#define UI_W 320
#define UI_H 240
#define GRAPH_TOP 0
#define GRAPH_BOTTOM (UI_H - 1)
#define UI_APP_COUNT 12
#define UI_ICON_COLS 4
#define UI_ICON_ROWS 3
#define UI_TOP_SAFE_Y 0
#define UI_HEADER_H 28
#define UI_HOME_GRID_Y 48
#define UI_HOME_GRID_STEP_Y 50
#define SCRIPT_MAX 12
#define SCRIPT_EDITOR_MAX 2048
#define SETTINGS_COUNT 5
#define PROGRAM_MENU_COUNT 4
#define CALC_HISTORY_MAX 16
#define LIST_COUNT 6
#define LIST_MAX_VALUES 999
#define MATRIX_MAX_N 4
#define INEQ_MAX 6
#define GRAPH_POI_LIMIT 16
#define GRAPH_COLOR_COUNT 15

#define THEME_BG        (s_light_mode ? 0xf5f9ff : 0x0b0d10)
#define THEME_SURFACE   (s_light_mode ? 0xdbeafe : 0x171b21)
#define THEME_SURFACE_2 (s_light_mode ? 0xbfdbfe : 0x242a32)
#define THEME_HEADER    (s_light_mode ? 0x2563a8 : 0x30363f)
#define THEME_ACCENT    (s_light_mode ? 0x1677d2 : 0x4aa3ff)
#define THEME_ACCENT_2  (s_light_mode ? 0xb9dcff : 0x2d4258)
#define THEME_TEXT      (s_light_mode ? 0x10243e : 0xf4f7fb)
#define THEME_MUTED     (s_light_mode ? 0x506b88 : 0x9aa8b6)
#define THEME_BORDER    (s_light_mode ? 0x7aa8d8 : 0x586575)
#define THEME_HEADER_TEXT 0xf4f7fb
#define THEME_GRID      (s_light_mode ? 0xc8daf0 : 0x1b2027)
#define THEME_WHITE     0xf4f7fb
#define THEME_BATTERY_GREEN  0x33d17a
#define THEME_BATTERY_YELLOW 0xf5c542
#define THEME_BATTERY_RED    0xff4d4d
#define THEME_CHARGE_YELLOW  0xffd84d

typedef enum {
    APP_CALCULATOR = 0,
    APP_GRAPH,
    APP_TABLE,
    APP_PYTHON,
    APP_STATS,
    APP_LISTS,
    APP_MATRICES,
    APP_SOLVER,
    APP_SETTINGS,
    APP_FINANCE,
    APP_CONICS,
    APP_INEQUALITY,
} app_id_t;

typedef enum {
    PAGE_HOME = 0,
    PAGE_APP,
    PAGE_SETTINGS,
    PAGE_SCRIPTS,
    PAGE_CALCULATOR,
    PAGE_MATH_MENU,
    PAGE_GRAPH,
    PAGE_Y_EQUALS,
    PAGE_TABLE,
    PAGE_GRAPH_WINDOW,
    PAGE_GRAPH_CALC,
    PAGE_LIST_EDITOR,
    PAGE_MODE_MENU,
    PAGE_PROGRAM_MENU,
    PAGE_GAME_MENU,
    PAGE_SCRIPT_IO,
    PAGE_SCRIPT_EDITOR,
} page_id_t;

typedef enum {
    GAME_NONE = 0,
    GAME_TETRIS,
    GAME_DOOM,
    GAME_SNAKE,
    GAME_BREAKOUT,
    GAME_MARIO,
} game_id_t;

typedef struct {
    const char *title;
    const char *icon;
    uint32_t color;
    const char *lines[5];
} app_info_t;

static const app_info_t APPS[UI_APP_COUNT] = {
    {"Calculator", "C", 0x4aa3ff, {"Expression entry", "Numeric evaluate", "History", "Vars", NULL}},
    {"Graph", "G", 0x7ab8ff, {"Y= functions", "POI trace", "Fast intersections", "Tables", NULL}},
    {"Table", "T", 0x9aa8ff, {"Function values", "Table setup", "Split graph/table", NULL}},
    {"Python", "P", 0xb0bfd0, {"Program menu", "Run scripts", "USB scripts folder", NULL}},
    {"Statistics", "S", 0x6f9ee8, {"List edit", "1-var / 2-var", "Linear regression", NULL}},
    {"Lists", "L", 0x8f9db4, {"Named lists", "999 values", "List math", NULL}},
    {"Matrices", "M", 0xc1a6ff, {"Set A", "Det inverse rref", "Transpose", NULL}},
    {"Solver", "=", 0x5d8fd6, {"Numeric E1=E2", "Guess based solve", "Result fraction", NULL}},
    {"Settings", "*", 0x6d7d91, {"Brightness", "Auto sleep", "Dark / light theme", NULL}},
    {"Finance", "$", 0x8aa4c4, {"TVM solver", "NPV / IRR", "Begin / end", NULL}},
    {"Conics", "O", 0xd0d7e2, {"Lines", "Conic templates", "Multi-graph", "Intersections", NULL}},
    {"Inequality", "<", 0xa9b4c4, {"Graph relations", "Shade regions", "x/y inequalities", "Intersections", NULL}},
};

static const char *HOME_LABELS[UI_APP_COUNT] = {
    "Calc",
    "Graph",
    "Table",
    "Python",
    "Stats",
    "Lists",
    "Matrix",
    "Solver",
    "Settings",
    "Finance",
    "Conics",
    "Ineq",
};

typedef struct {
    const char *label;
    const char *insert;
} math_menu_item_t;

typedef struct {
    const char *label;
    const char *detail;
} app_tool_t;

typedef enum {
    GRAPH_POI_ZERO,
    GRAPH_POI_Y_INTERCEPT,
    GRAPH_POI_MIN,
    GRAPH_POI_LOCAL_MAX,
    GRAPH_POI_INTERSECTION,
} graph_poi_type_t;

typedef enum {
    SCRIPT_ACTION_RUN = 0,
    SCRIPT_ACTION_EDIT,
    SCRIPT_ACTION_DELETE,
} script_action_t;

typedef struct {
    double x;
    double y;
    int fn;
    int other_fn;
    graph_poi_type_t type;
} graph_poi_t;

typedef enum {
    UI_WORK_CALC_EVAL = 0,
    UI_WORK_SOLVER_SOLVE,
    UI_WORK_GRAPH_CALC,
} ui_work_type_t;

typedef struct {
    ui_work_type_t type;
    bool degrees;
    union {
        struct {
            char expr[96];
            char ans[32];
        } calc;
        struct {
            char e1[96];
            char e2[96];
            double guess;
        } solver;
        struct {
            int selection;
            char exprs[10][96];
            bool enabled[10];
            double xmin;
            double xmax;
            bool trace;
            double trace_x;
            int trace_fn;
        } graph;
    };
} ui_work_job_t;

typedef struct {
    ui_work_type_t type;
    bool ok;
    union {
        struct {
            char expr[96];
            char output[96];
            bool update_ans;
        } calc;
        struct {
            double root;
        } solver;
        struct {
            char status[72];
            bool trace;
            double trace_x;
            int trace_fn;
        } graph;
    };
} ui_work_result_t;

typedef struct {
    bool enabled;
    bool vertical;
    bool greater;
    bool inclusive;
    double x_value;
    char expr[96];
} inequality_t;

static const char *MATH_TAB_NAMES[] = {"MATH", "NUM", "CPX", "PRB"};

static const math_menu_item_t MATH_MENU[][10] = {
    {
        {"Frac", "frac("},
        {"Dec", "dec("},
        {"cube", "^3"},
        {"cuberoot", "cbrt("},
        {"xroot", "nroot("},
        {"deriv", "deriv("},
        {"nDeriv", "nDeriv("},
        {"fnInt", "fnInt("},
        {"int", "int("},
        {NULL, NULL},
    },
    {
        {"abs", "abs("},
        {"round", "round("},
        {"iPart", "iPart("},
        {"fPart", "fPart("},
        {"remainder", "remainder("},
        {"min", "min("},
        {"max", "max("},
        {"gcd", "gcd("},
        {"lcm", "lcm("},
    },
    {
        {"conj", "conj("},
        {"real", "real("},
        {"imag", "imag("},
        {"abs", "abs("},
        {"angle", "angle("},
        {NULL, NULL},
    },
    {
        {"rand", "rand()"},
        {"randInt", "randInt("},
        {"randNorm", "randNorm("},
        {"factorial", "!"},
        {"nPr", "nPr("},
        {"nCr", "nCr("},
        {NULL, NULL},
    },
};

static const app_tool_t STATS_TOOLS[] = {
    {"edit lists", "EDIT spreadsheet"},
    {"SortA L1", "ascending data"},
    {"SortD L1", "descending data"},
    {"ClearList", "wipe all lists"},
    {"1-var stats", "mean med sx quartiles"},
    {"2-var stats", "L1/L2 summary"},
    {"LinReg L1,L2", "y=a+bx"},
};

static const app_tool_t LIST_TOOLS[] = {
    {"edit list", "enter values"},
    {"new list", "next empty L"},
    {"next list", "switch L1-L6"},
    {"prev list", "switch L1-L6"},
    {"sum list", "selected total"},
    {"min max", "selected range"},
    {"clear list", "selected only"},
};

static const app_tool_t MATRIX_TOOLS[] = {
    {"set A", "calc: 1,2;3,4"},
    {"show A", "print matrix"},
    {"det A", "square matrix"},
    {"A inverse", "square matrix"},
    {"rref A", "reduce rows"},
    {"transpose A", "swap rows/cols"},
    {"identity", "set 3x3 I"},
};

static const app_tool_t SOLVER_TOOLS[] = {
    {"set E1", "from calc input"},
    {"set E2", "from calc input"},
    {"set guess", "from calc/Ans"},
    {"solve", "find x near guess"},
    {"result frac", "decimal to fraction"},
    {"example", "x^2 = 4"},
    {"clear", "reset solver"},
};

static const app_tool_t FINANCE_TOOLS[] = {
    {"set field", "from calc/Ans"},
    {"next field", "choose variable"},
    {"prev field", "choose variable"},
    {"solve field", "N PV PMT FV"},
    {"begin/end", "payment timing"},
    {"NPV list", "selected cash flows"},
    {"IRR list", "selected cash flows"},
};

static const app_tool_t CONICS_TOOLS[] = {
    {"line", "y=m(x-h)+k"},
    {"circle", "(x-h)^2+(y-k)^2=r^2"},
    {"parabola", "y=a(x-h)^2+k"},
    {"ellipse", "(x-h)^2/a^2+(y-k)^2/b^2=1"},
    {"hyperbola", "(x-h)^2/a^2-(y-k)^2/b^2=1"},
    {"add to graph", "keep existing conics"},
    {"solve/points", "center vertices radius"},
};

static const app_tool_t INEQUALITY_TOOLS[] = {
    {"y > x", "dotted shade up"},
    {"y < x^2", "dotted shade down"},
    {"x >= 0", "solid shade right"},
    {"y >= 0", "solid shade up"},
    {"system", "overlap shading"},
    {"open graph", "view shaded graph"},
    {"clear", "clear relations"},
};

static const char *GRAPH_CALC_MENU[] = {
    "value",
    "zero",
    "minimum",
    "maximum",
    "intersect",
    "y-intercept",
    "dy/dx",
    "integral 0..x",
};

#define GRAPH_CALC_COUNT ((int)(sizeof(GRAPH_CALC_MENU) / sizeof(GRAPH_CALC_MENU[0])))

static EXT_RAM_BSS_ATTR uint32_t s_frame[UI_W * UI_H];
static page_id_t s_page = PAGE_CALCULATOR;
static page_id_t s_math_return_page = PAGE_CALCULATOR;
static app_id_t s_current_app = APP_CALCULATOR;
static int s_home_selection = APP_CALCULATOR;
static int s_script_selection = 0;
static int s_program_selection = 0;
static int s_script_count = 0;
static script_action_t s_script_action = SCRIPT_ACTION_RUN;
static int s_app_selection = 0;
static int s_math_tab = 0;
static int s_math_selection = 0;
static char s_scripts[SCRIPT_MAX][32];
static char s_script_editor[SCRIPT_EDITOR_MAX];
static char s_script_edit_name[32] = "";
static size_t s_script_editor_len = 0;
static size_t s_script_editor_cursor = 0;
static int s_script_editor_scroll_line = 0;
static py_t s_script_py;
static char s_script_output[1024];
static char s_script_title[48] = "Python";
static char s_script_status[48] = "";
static bool s_script_running = false;
static bool s_script_input_active = false;
static char s_script_input[96] = "";
static size_t s_script_input_len = 0;
static QueueHandle_t s_serial_button_queue = NULL;
static QueueHandle_t s_work_queue = NULL;
static QueueHandle_t s_work_result_queue = NULL;
static TaskHandle_t s_work_task = NULL;
static bool s_calc_eval_pending = false;
static bool s_solver_solve_pending = false;
static bool s_graph_calc_pending = false;
static char s_calc_input[96] = "";
static size_t s_calc_cursor = 0;
static char s_calc_output[96] = "0";
static char s_calc_ans[32] = "0";
static char s_calc_history_expr[CALC_HISTORY_MAX][96];
static char s_calc_history_result[CALC_HISTORY_MAX][96];
static int s_calc_history_count = 0;
static int s_calc_history_selected = -1;
static bool s_calc_history_select_answer = false;
static double s_lists[LIST_COUNT][LIST_MAX_VALUES];
static int s_list_counts[LIST_COUNT];
static int s_list_index = 0;
static int s_list_cursor = 0;
static char s_list_entry[24] = "";
static char s_graph_exprs[10][96] = {"x", "", "", "", "", "", "", "", "", ""};
static bool s_graph_enabled[10] = {true, false, false, false, false, false, false, false, false, false};
static const uint32_t s_graph_colors[GRAPH_COLOR_COUNT] = {
    0x4aa3ff, 0xb38cff, 0xff8cc6, 0x76d7ff, 0xd0d7e2,
    0x8aa4c4, 0x5d8fd6, 0xc1a6ff, 0x9aa8ff, 0xf4f7fb,
    0x33d17a, 0xf5c542, 0xff4d4d, 0x00d2c6, 0xff9f43,
};
static int s_graph_selection = 0;
static double s_graph_xmin = -10.0;
static double s_graph_xmax = 10.0;
static double s_graph_ymin = -6.0;
static double s_graph_ymax = 6.0;
static double s_graph_xtick = 1.0;
static double s_graph_ytick = 1.0;
static int s_graph_window_selection = 0;
static int s_graph_calc_selection = 0;
static char s_graph_status[72] = "";
static bool s_graph_zoom_mode = false;
static double s_table_x_start = 0.0;
static int s_table_func_start = 0;
static bool s_graph_grid = true;
static inequality_t s_ineqs[INEQ_MAX];
static bool s_graph_trace = false;
static double s_graph_trace_x = 0.0;
static int s_graph_trace_fn = 0;
static bool s_cursor_blink_visible = true;
static bool s_cursor_blink_last_visible = true;
static bool s_second_active = false;
static bool s_alpha_active = false;
static bool s_alpha_locked = false;
static game_id_t s_active_game = GAME_NONE;
static int s_game_selection = 0;
static uint32_t s_doom_high_score = 0;
static uint32_t s_doom_last_saved_high_score = 0;
static bool s_usb_storage_enabled = OPENCALC_EXPORT_USB_STORAGE_TO_HOST != 0;
static bool s_sleep_enabled = true;
static bool s_power_save_enabled = false;
static int s_power_save_saved_brightness = 80;
static int s_mode_selection = 0;
static int s_display_format = 0;
static int s_print_mode = 0;
static int s_angle_mode = 0;
static int s_graphing_mode = 0;
static int s_complex_mode = 0;
static double s_matrix_a[MATRIX_MAX_N][MATRIX_MAX_N];
static int s_matrix_rows = 0;
static int s_matrix_cols = 0;
static char s_solver_e1[96] = "x^2";
static char s_solver_e2[96] = "4";
static double s_solver_guess = 1.0;
static double s_solver_result = 0.0;
static bool s_solver_has_result = false;
static double s_fin_n = 12.0;
static double s_fin_i = 5.0;
static double s_fin_pv = 1000.0;
static double s_fin_pmt = 0.0;
static double s_fin_fv = 0.0;
static double s_fin_py = 12.0;
static double s_fin_cy = 12.0;
static bool s_fin_begin = false;
static int s_fin_selection = 3;
static int s_conic_type = 0;
static double s_conic_h = 0.0;
static double s_conic_k = 0.0;
static double s_conic_a = 1.0;
static double s_conic_b = 1.0;
static double s_conic_r = 5.0;

static void ui_draw_current(void);
static void calc_history_push(const char *expr, const char *result);
static bool calc_take_wrapped_expression(const char *input, const char *name, char *out, size_t out_size);
static void calc_expand_ans_value(const char *input, const char *ans, char *out, size_t out_size);
static bool calc_format_fraction_value(double value, char *out, size_t out_size);
static void ui_work_task(void *arg);
static bool submit_calc_eval_job(void);
static bool submit_solver_solve_job(void);
static bool submit_graph_calc_job(void);
static void ui_work_apply_result(const ui_work_result_t *result);

static void ui_draw_current(void);
static void status_message(const char *text);
static void app_output(const char *text);
static void reset_graph_view_defaults(void);
static bool calc_format_fraction_value(double value, char *out, size_t out_size);
static void calc_expand_ans(const char *input, char *out, size_t out_size);
static int digit_for_key(int row, int col);
static void run_selected_script(void);

static const uint8_t FONT[38][7] = {
    {0x0e,0x11,0x13,0x15,0x19,0x11,0x0e}, {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e},
    {0x0e,0x11,0x01,0x02,0x04,0x08,0x1f}, {0x1e,0x01,0x01,0x0e,0x01,0x01,0x1e},
    {0x02,0x06,0x0a,0x12,0x1f,0x02,0x02}, {0x1f,0x10,0x10,0x1e,0x01,0x01,0x1e},
    {0x06,0x08,0x10,0x1e,0x11,0x11,0x0e}, {0x1f,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e}, {0x0e,0x11,0x11,0x0f,0x01,0x02,0x0c},
    {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11}, {0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e},
    {0x0e,0x11,0x10,0x10,0x10,0x11,0x0e}, {0x1e,0x11,0x11,0x11,0x11,0x11,0x1e},
    {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f}, {0x1f,0x10,0x10,0x1e,0x10,0x10,0x10},
    {0x0e,0x11,0x10,0x17,0x11,0x11,0x0e}, {0x11,0x11,0x11,0x1f,0x11,0x11,0x11},
    {0x0e,0x04,0x04,0x04,0x04,0x04,0x0e}, {0x07,0x02,0x02,0x02,0x12,0x12,0x0c},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, {0x10,0x10,0x10,0x10,0x10,0x10,0x1f},
    {0x11,0x1b,0x15,0x15,0x11,0x11,0x11}, {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e}, {0x1e,0x11,0x11,0x1e,0x10,0x10,0x10},
    {0x0e,0x11,0x11,0x11,0x15,0x12,0x0d}, {0x1e,0x11,0x11,0x1e,0x14,0x12,0x11},
    {0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e}, {0x1f,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0e}, {0x11,0x11,0x11,0x11,0x11,0x0a,0x04},
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0a}, {0x11,0x11,0x0a,0x04,0x0a,0x11,0x11},
    {0x11,0x11,0x0a,0x04,0x04,0x04,0x04}, {0x1f,0x01,0x02,0x04,0x08,0x10,0x1f},
    {0x0a,0x0a,0x1f,0x0a,0x1f,0x0a,0x0a},
    {0x0e,0x11,0x01,0x02,0x04,0x00,0x04},
};

static const uint8_t *font_for(char c)
{
    if (c >= '0' && c <= '9') {
        return FONT[c - '0'];
    }
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }
    if (c >= 'A' && c <= 'Z') {
        return FONT[10 + c - 'A'];
    }
    if (c == '#') {
        return FONT[36];
    }
    return FONT[37];
}

static void ui_pixel(int x, int y, uint32_t color)
{
    if (x >= 0 && x < UI_W && y >= 0 && y < UI_H) {
        s_frame[y * UI_W + x] = color;
    }
}

static void ui_clear(uint32_t color)
{
    for (int i = 0; i < UI_W * UI_H; i++) {
        s_frame[i] = color;
    }
}

static void ui_rect(int x, int y, int w, int h, uint32_t color)
{
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            ui_pixel(px, py, color);
        }
    }
}

static void ui_border(int x, int y, int w, int h, uint32_t color)
{
    ui_rect(x, y, w, 1, color);
    ui_rect(x, y + h - 1, w, 1, color);
    ui_rect(x, y, 1, h, color);
    ui_rect(x + w - 1, y, 1, h, color);
}

static void ui_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        ui_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void ui_circle_outline(int cx, int cy, int r, uint32_t color)
{
    int x = r;
    int y = 0;
    int err = 0;

    while (x >= y) {
        ui_pixel(cx + x, cy + y, color);
        ui_pixel(cx + y, cy + x, color);
        ui_pixel(cx - y, cy + x, color);
        ui_pixel(cx - x, cy + y, color);
        ui_pixel(cx - x, cy - y, color);
        ui_pixel(cx - y, cy - x, color);
        ui_pixel(cx + y, cy - x, color);
        ui_pixel(cx + x, cy - y, color);

        y++;
        if (err <= 0) {
            err += 2 * y + 1;
        }
        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
}

static void ui_app_icon(app_id_t app, int x, int y, int size, uint32_t color)
{
    int cx = x + size / 2;
    int cy = y + size / 2;
    int q = size / 4;

    switch (app) {
    case APP_CALCULATOR:
    {
        int arm = size / 2 - 3;
        int length = arm * 2 + 1;
        ui_rect(cx - 1, cy - arm, 3, length, color);
        ui_rect(cx - arm, cy - 1, length, 3, color);
        break;
    }
    case APP_GRAPH:
        ui_line(x + 2, y + size - 4, x + size - 2, y + size - 4, color);
        ui_line(x + 4, y + size - 2, x + 4, y + 2, color);
        ui_line(x + 5, y + size - 7, x + q + 3, y + q, color);
        ui_line(x + q + 3, y + q, x + size - 4, y + size / 2, color);
        break;
    case APP_TABLE:
        ui_border(x + 3, y + 3, size - 6, size - 6, color);
        ui_line(cx, y + 3, cx, y + size - 4, color);
        ui_line(x + 3, cy, x + size - 4, cy, color);
        break;
    case APP_PYTHON:
        ui_line(x + 7, y + 4, x + 3, cy, color);
        ui_line(x + 3, cy, x + 7, y + size - 4, color);
        ui_line(x + size - 7, y + 4, x + size - 3, cy, color);
        ui_line(x + size - 3, cy, x + size - 7, y + size - 4, color);
        break;
    case APP_STATS:
        ui_rect(x + 4, y + size - 9, 3, 7, color);
        ui_rect(cx - 1, y + size - 14, 3, 12, color);
        ui_rect(x + size - 7, y + 5, 3, size - 7, color);
        break;
    case APP_LISTS:
        for (int i = 0; i < 2; i++) {
            int row_y = y + 6 + i * 7;
            ui_rect(x + 5, row_y, 3, 3, color);
            ui_line(x + 11, row_y + 1, x + size - 5, row_y + 1, color);
        }
        break;
    case APP_MATRICES:
        ui_border(x + 4, y + 4, size - 8, size - 8, color);
        ui_line(cx, y + 4, cx, y + size - 5, color);
        ui_line(x + 4, cy, x + size - 5, cy, color);
        break;
    case APP_SOLVER:
        ui_rect(x + 3, cy - 4, size - 6, 3, color);
        ui_rect(x + 3, cy + 3, size - 6, 3, color);
        break;
    case APP_FINANCE:
        ui_line(cx, y + 3, cx, y + size - 3, color);
        ui_line(cx + 5, y + 6, cx - 3, y + 6, color);
        ui_line(cx - 3, y + 6, cx - 5, cy, color);
        ui_line(cx - 5, cy, cx + 4, cy, color);
        ui_line(cx + 4, cy, cx + 2, y + size - 6, color);
        ui_line(cx + 2, y + size - 6, cx - 6, y + size - 6, color);
        break;
    case APP_CONICS:
        ui_circle_outline(cx, cy, size / 2 - 4, color);
        break;
    case APP_SETTINGS:
        ui_rect(cx - 2, y + 2, 4, 4, color);
        ui_rect(cx - 2, y + size - 6, 4, 4, color);
        ui_rect(x + 2, cy - 2, 4, 4, color);
        ui_rect(x + size - 6, cy - 2, 4, 4, color);
        ui_rect(x + 5, y + 5, 3, 3, color);
        ui_rect(x + size - 8, y + 5, 3, 3, color);
        ui_rect(x + 5, y + size - 8, 3, 3, color);
        ui_rect(x + size - 8, y + size - 8, 3, 3, color);
        ui_border(cx - 5, cy - 5, 10, 10, color);
        ui_rect(cx - 2, cy - 2, 4, 4, THEME_BG);
        break;
    case APP_INEQUALITY:
        ui_line(x + size - 5, y + 5, x + 5, cy, color);
        ui_line(x + 5, cy, x + size - 5, y + size - 5, color);
        break;
    }
}

static app_id_t app_id_for_info(const app_info_t *app)
{
    if (app == NULL) {
        return (app_id_t)s_home_selection;
    }

    for (int i = 0; i < UI_APP_COUNT; i++) {
        if (&APPS[i] == app) {
            return (app_id_t)i;
        }
    }

    return APP_CALCULATOR;
}

static void ui_text(int x, int y, const char *text, uint32_t color, int scale)
{
    if (text == NULL) {
        return;
    }

    int cx = x;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (c == ' ') {
            cx += 4 * scale;
            continue;
        }
        if (c == '.') {
            ui_rect(cx + 2 * scale, y + 6 * scale, scale, scale, color);
            cx += 4 * scale;
            continue;
        }
        if (c == ',') {
            ui_rect(cx + 2 * scale, y + 6 * scale, scale, scale, color);
            ui_rect(cx + scale, y + 7 * scale, scale, scale, color);
            cx += 4 * scale;
            continue;
        }
        if (c == '\'') {
            ui_rect(cx + 2 * scale, y, scale, 2 * scale, color);
            ui_rect(cx + scale, y + 2 * scale, scale, scale, color);
            cx += 4 * scale;
            continue;
        }
        if (c == '(' || c == ')') {
            int stem = c == '(' ? 1 : 3;
            ui_rect(cx + stem * scale, y, scale, scale, color);
            ui_rect(cx + (c == '(' ? 0 : 4) * scale, y + scale, scale, 5 * scale, color);
            ui_rect(cx + stem * scale, y + 6 * scale, scale, scale, color);
            cx += 6 * scale;
            continue;
        }
        if (c == '^') {
            ui_rect(cx + 2 * scale, y, scale, scale, color);
            ui_rect(cx + scale, y + scale, scale, scale, color);
            ui_rect(cx + 3 * scale, y + scale, scale, scale, color);
            cx += 6 * scale;
            continue;
        }
        if (c == '-') {
            ui_rect(cx, y + 3 * scale, 5 * scale, scale, color);
            cx += 6 * scale;
            continue;
        }
        if (c == '+') {
            ui_rect(cx + 2 * scale, y + scale, scale, 5 * scale, color);
            ui_rect(cx, y + 3 * scale, 5 * scale, scale, color);
            cx += 6 * scale;
            continue;
        }
        if (c == '=') {
            ui_rect(cx, y + 2 * scale, 5 * scale, scale, color);
            ui_rect(cx, y + 4 * scale, 5 * scale, scale, color);
            cx += 6 * scale;
            continue;
        }
        if (c == '<') {
            ui_rect(cx + 3 * scale, y + scale, scale, scale, color);
            ui_rect(cx + 2 * scale, y + 2 * scale, scale, scale, color);
            ui_rect(cx + scale, y + 3 * scale, scale, scale, color);
            ui_rect(cx + 2 * scale, y + 4 * scale, scale, scale, color);
            ui_rect(cx + 3 * scale, y + 5 * scale, scale, scale, color);
            cx += 6 * scale;
            continue;
        }
        if (c == '>') {
            ui_rect(cx + scale, y + scale, scale, scale, color);
            ui_rect(cx + 2 * scale, y + 2 * scale, scale, scale, color);
            ui_rect(cx + 3 * scale, y + 3 * scale, scale, scale, color);
            ui_rect(cx + 2 * scale, y + 4 * scale, scale, scale, color);
            ui_rect(cx + scale, y + 5 * scale, scale, scale, color);
            cx += 6 * scale;
            continue;
        }
        if (c == '%') {
            ui_rect(cx, y + 6 * scale, 5 * scale, scale, color);
            for (int d = 0; d < 7; d++) {
                ui_rect(cx + (4 - d / 2) * scale, y + d * scale, scale, scale, color);
            }
            ui_rect(cx, y, 2 * scale, 2 * scale, color);
            ui_rect(cx + 3 * scale, y + 5 * scale, 2 * scale, 2 * scale, color);
            cx += 6 * scale;
            continue;
        }
        if (c == '/') {
            for (int d = 0; d < 7; d++) {
                ui_rect(cx + (4 - d / 2) * scale, y + d * scale, scale, scale, color);
            }
            cx += 6 * scale;
            continue;
        }
        if (c == '*') {
            ui_rect(cx + 2 * scale, y + scale, scale, 5 * scale, color);
            ui_rect(cx, y + 3 * scale, 5 * scale, scale, color);
            ui_rect(cx + scale, y + 2 * scale, scale, scale, color);
            ui_rect(cx + 3 * scale, y + 2 * scale, scale, scale, color);
            ui_rect(cx + scale, y + 4 * scale, scale, scale, color);
            ui_rect(cx + 3 * scale, y + 4 * scale, scale, scale, color);
            cx += 6 * scale;
            continue;
        }
        if (c == '$') {
            c = 'S';
        }

        const uint8_t *glyph = font_for(c);
        for (int gy = 0; gy < 7; gy++) {
            for (int gx = 0; gx < 5; gx++) {
                if ((glyph[gy] & (1 << (4 - gx))) != 0) {
                    ui_rect(cx + gx * scale, y + gy * scale, scale, scale, color);
                }
            }
        }
        cx += 6 * scale;
    }
}

static int ui_text_width(const char *text, size_t max_chars, int scale)
{
    int width = 0;
    if (text == NULL) {
        return 0;
    }
    for (size_t i = 0; text[i] != '\0' && i < max_chars; i++) {
        char c = text[i];
        if (c == ' ' || c == '.' || c == ',' || c == '\'') {
            width += 4 * scale;
        } else {
            width += 6 * scale;
        }
    }
    return width;
}

static const uint8_t *ui_calc_glyph(char c, uint8_t custom[7])
{
    memset(custom, 0, 7);
    switch (c) {
    case ' ': break;
    case '.': custom[6] = 0x04; break;
    case ',': custom[5] = 0x04; custom[6] = 0x08; break;
    case '\'': custom[0] = 0x04; custom[1] = 0x04; custom[2] = 0x08; break;
    case '(':
        custom[0] = 0x02; custom[1] = 0x04; custom[2] = 0x08;
        custom[3] = 0x08; custom[4] = 0x08; custom[5] = 0x04; custom[6] = 0x02;
        break;
    case ')':
        custom[0] = 0x08; custom[1] = 0x04; custom[2] = 0x02;
        custom[3] = 0x02; custom[4] = 0x02; custom[5] = 0x04; custom[6] = 0x08;
        break;
    case '^': custom[0] = 0x04; custom[1] = 0x0a; custom[2] = 0x11; break;
    case '-': custom[3] = 0x1f; break;
    case '+': custom[1] = 0x04; custom[2] = 0x04; custom[3] = 0x1f; custom[4] = 0x04; custom[5] = 0x04; break;
    case '=': custom[2] = 0x1f; custom[4] = 0x1f; break;
    case '<': custom[1] = 0x02; custom[2] = 0x04; custom[3] = 0x08; custom[4] = 0x04; custom[5] = 0x02; break;
    case '>': custom[1] = 0x08; custom[2] = 0x04; custom[3] = 0x02; custom[4] = 0x04; custom[5] = 0x08; break;
    case '/': custom[0] = 0x01; custom[1] = 0x02; custom[2] = 0x02; custom[3] = 0x04; custom[4] = 0x08; custom[5] = 0x08; custom[6] = 0x10; break;
    case '%': custom[0] = 0x19; custom[1] = 0x1a; custom[2] = 0x04; custom[3] = 0x04; custom[4] = 0x0b; custom[5] = 0x13; break;
    case '*':
        custom[1] = 0x04;
        custom[2] = 0x15;
        custom[3] = 0x0e;
        custom[4] = 0x15;
        custom[5] = 0x04;
        break;
    case '$': c = 'S'; break;
    default: return font_for(c);
    }
    return custom;
}

static int ui_calc_char_width(char c)
{
    return (c == ' ' || c == '.' || c == ',' || c == '\'') ? 8 : 12;
}

static void ui_calc_text(int x, int y, const char *text, uint32_t color)
{
    if (text == NULL) {
        return;
    }

    int cx = x;
    for (size_t i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        uint8_t custom[7];
        const uint8_t *glyph = ui_calc_glyph(c, custom);
        for (int gy = 0; gy < 7; gy++) {
            for (int gx = 0; gx < 5; gx++) {
                if ((glyph[gy] & (1 << (4 - gx))) == 0) {
                    continue;
                }
                ui_rect(cx + gx * 2, y + gy * 2, 2, 2, color);
            }
        }
        cx += ui_calc_char_width(c);
    }
}

static int ui_calc_text_width(const char *text, size_t max_chars)
{
    int width = 0;
    if (text == NULL) {
        return 0;
    }
    for (size_t i = 0; text[i] != '\0' && i < max_chars; i++) {
        width += ui_calc_char_width(text[i]);
    }
    return width;
}

static size_t expression_find_matching_paren(const char *text, size_t open_index)
{
    int depth = 0;
    for (size_t i = open_index; text[i] != '\0'; i++) {
        if (text[i] == '(') {
            depth++;
        } else if (text[i] == ')') {
            depth--;
            if (depth == 0) {
                return i;
            }
        }
    }
    return open_index;
}

static bool expression_find_frac_parts(const char *text, size_t start, size_t *num_start, size_t *comma, size_t *close)
{
    if (strncmp(text + start, "frac(", 5) != 0) {
        return false;
    }

    size_t open = start + 4;
    size_t end = expression_find_matching_paren(text, open);
    if (end <= open + 1) {
        return false;
    }

    int depth = 0;
    for (size_t i = open + 1; i < end; i++) {
        if (text[i] == '(') {
            depth++;
        } else if (text[i] == ')') {
            depth--;
        } else if (text[i] == ',' && depth == 0) {
            *num_start = open + 1;
            *comma = i;
            *close = end;
            return true;
        }
    }

    return false;
}

static bool expression_find_nroot_parts(const char *text, size_t start, size_t *index_start, size_t *comma, size_t *close)
{
    if (strncmp(text + start, "nroot(", 6) != 0) {
        return false;
    }

    size_t open = start + 5;
    size_t end = expression_find_matching_paren(text, open);
    if (end <= open + 1) {
        return false;
    }

    int depth = 0;
    for (size_t i = open + 1; i < end; i++) {
        if (text[i] == '(') {
            depth++;
        } else if (text[i] == ')') {
            depth--;
        } else if (text[i] == ',' && depth == 0) {
            *index_start = open + 1;
            *comma = i;
            *close = end;
            return true;
        }
    }

    return false;
}

static void ui_draw_calc_cursor(int x, int y, bool raised)
{
    if (!s_cursor_blink_visible) {
        return;
    }

    ui_rect(x, raised ? y - 11 : y - 2, 6, raised ? 10 : 11, THEME_ACCENT);
}

static void ui_draw_calc_cursor_large(int x, int y, char c, bool has_char)
{
    if (s_cursor_blink_visible) {
        ui_rect(x, y - 1, ui_calc_char_width(c), 16, THEME_ACCENT);
        if (has_char) {
            char one[2] = {c, '\0'};
            ui_calc_text(x, y, one, THEME_BG);
        }
    } else if (has_char) {
        char one[2] = {c, '\0'};
        ui_calc_text(x, y, one, THEME_TEXT);
    }
}

static int ui_tiny_text_width(const char *text, size_t max_len)
{
    int width = 0;
    if (text == NULL) {
        return 0;
    }
    for (size_t i = 0; text[i] != '\0' && i < max_len; i++) {
        char c = text[i];
        width += (c == '.' || c == ',' || c == ' ' || c == '\'') ? 4 : 6;
    }
    return width;
}

static void ui_tiny_symbol(int x, int y, char c, uint32_t color)
{
    switch (c) {
    case '.':
    case ',':
        ui_pixel(x + 2, y + 6, color);
        break;
    case '\'':
        ui_pixel(x + 2, y, color);
        ui_pixel(x + 2, y + 1, color);
        ui_pixel(x + 1, y + 2, color);
        break;
    case '-':
        ui_line(x, y + 3, x + 4, y + 3, color);
        break;
    case '+':
        ui_line(x + 2, y + 1, x + 2, y + 5, color);
        ui_line(x, y + 3, x + 4, y + 3, color);
        break;
    case '*':
        ui_pixel(x + 2, y + 1, color);
        ui_pixel(x + 1, y + 2, color);
        ui_pixel(x + 3, y + 2, color);
        ui_pixel(x + 2, y + 3, color);
        break;
    case '/':
        ui_pixel(x + 4, y, color);
        ui_pixel(x + 3, y + 1, color);
        ui_pixel(x + 2, y + 2, color);
        ui_pixel(x + 2, y + 3, color);
        ui_pixel(x + 1, y + 4, color);
        ui_pixel(x, y + 5, color);
        ui_pixel(x, y + 6, color);
        break;
    case '(':
        ui_pixel(x + 3, y, color);
        ui_pixel(x + 2, y + 1, color);
        ui_line(x + 1, y + 2, x + 1, y + 4, color);
        ui_pixel(x + 2, y + 5, color);
        ui_pixel(x + 3, y + 6, color);
        break;
    case ')':
        ui_pixel(x + 1, y, color);
        ui_pixel(x + 2, y + 1, color);
        ui_line(x + 3, y + 2, x + 3, y + 4, color);
        ui_pixel(x + 2, y + 5, color);
        ui_pixel(x + 1, y + 6, color);
        break;
    default: {
        const uint8_t *glyph = font_for(c);
        for (int gy = 0; gy < 7; gy++) {
            for (int gx = 0; gx < 5; gx++) {
                if ((glyph[gy] & (1 << (4 - gx))) != 0) {
                    ui_pixel(x + gx, y + gy, color);
                }
            }
        }
        break;
    }
    }
}

static void ui_tiny_text(int x, int y, const char *text, uint32_t color)
{
    if (text == NULL) {
        return;
    }
    int cx = x;
    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] != ' ') {
            ui_tiny_symbol(cx, y, text[i], color);
        }
        cx += (text[i] == '.' || text[i] == ',' || text[i] == ' ' || text[i] == '\'') ? 4 : 6;
    }
}

static void ui_draw_calc_cursor_tiny(int x, int y)
{
    if (s_cursor_blink_visible) {
        ui_rect(x, y - 1, 6, 9, THEME_ACCENT);
    }
}

static void ui_draw_calc_expression(int x, int y, const char *text, size_t cursor)
{
    if (text == NULL || text[0] == '\0') {
        ui_draw_calc_cursor_large(x, y, ' ', false);
        return;
    }

    size_t len = strlen(text);
    if (cursor > len) {
        cursor = len;
    }

    int cx = x;
    bool cursor_drawn = false;
    for (size_t i = 0; i < len;) {
        size_t root_index_start = 0;
        size_t root_comma = 0;
        size_t root_close = 0;
        if (expression_find_nroot_parts(text, i, &root_index_start, &root_comma, &root_close)) {
            char index[24];
            char value[48];
            size_t index_len = root_comma - root_index_start;
            size_t value_start = root_comma + 1;
            size_t value_len = root_close - value_start;
            if (index_len >= sizeof(index)) {
                index_len = sizeof(index) - 1;
            }
            if (value_len >= sizeof(value)) {
                value_len = sizeof(value) - 1;
            }
            memcpy(index, text + root_index_start, index_len);
            index[index_len] = '\0';
            memcpy(value, text + value_start, value_len);
            value[value_len] = '\0';

            int index_w = ui_tiny_text_width(index[0] ? index : " ", index[0] ? index_len : 1);
            int value_w = ui_tiny_text_width(value[0] ? value : " ", value[0] ? value_len : 1);
            if (index_w < 7) {
                index_w = 7;
            }
            if (value_w < 18) {
                value_w = 18;
            }
            int radical_x = cx + index_w + 1;
            int index_x = radical_x - index_w;
            int index_y = y - 12;
            int value_x = radical_x + 14;
            int overbar_y = y - 7;
            int value_end = value_x + value_w + 2;

            if (index[0] != '\0') {
                ui_tiny_text(index_x, index_y, index, THEME_TEXT);
            } else {
                ui_border(index_x, index_y, index_w, 7, THEME_BORDER);
            }
            ui_line(radical_x, y + 3, radical_x + 4, y + 8, THEME_TEXT);
            ui_line(radical_x + 4, y + 8, radical_x + 9, overbar_y, THEME_TEXT);
            ui_line(radical_x + 9, overbar_y, value_end, overbar_y, THEME_TEXT);
            ui_line(radical_x + 9, overbar_y + 1, radical_x + 10, overbar_y + 1, THEME_TEXT);
            if (value[0] == '\0') {
                ui_line(value_x, y + 9, value_end - 2, y + 9, THEME_BORDER);
            }
            if (value[0] != '\0') {
                ui_tiny_text(value_x, y + 1, value, THEME_TEXT);
            }

            if (cursor >= root_index_start && cursor <= root_comma) {
                size_t cursor_chars = cursor > root_index_start ? cursor - root_index_start : 0;
                if (cursor_chars > index_len) {
                    cursor_chars = index_len;
                }
                int cursor_x = index_x + ui_tiny_text_width(index, cursor_chars);
                ui_draw_calc_cursor_tiny(cursor_x, index_y);
                cursor_drawn = true;
            } else if (cursor > root_comma && cursor <= root_close) {
                size_t cursor_chars = cursor > value_start ? cursor - value_start : 0;
                if (cursor_chars > value_len) {
                    cursor_chars = value_len;
                }
                int cursor_x = value_x + ui_tiny_text_width(value, cursor_chars);
                ui_draw_calc_cursor_tiny(cursor_x, y + 1);
                cursor_drawn = true;
            }

            cx = value_x + value_w + 6;
            i = root_close + 1;
            continue;
        }

        size_t frac_num_start = 0;
        size_t frac_comma = 0;
        size_t frac_close = 0;
        if (expression_find_frac_parts(text, i, &frac_num_start, &frac_comma, &frac_close)) {
            char num[48];
            char den[48];
            size_t num_len = frac_comma - frac_num_start;
            size_t den_start = frac_comma + 1;
            size_t den_len = frac_close - den_start;
            if (num_len >= sizeof(num)) {
                num_len = sizeof(num) - 1;
            }
            if (den_len >= sizeof(den)) {
                den_len = sizeof(den) - 1;
            }
            memcpy(num, text + frac_num_start, num_len);
            num[num_len] = '\0';
            memcpy(den, text + den_start, den_len);
            den[den_len] = '\0';

            int num_w = ui_tiny_text_width(num[0] ? num : " ", num[0] ? num_len : 1);
            int den_w = ui_tiny_text_width(den[0] ? den : " ", den[0] ? den_len : 1);
            int frac_w = num_w > den_w ? num_w : den_w;
            if (frac_w < 14) {
                frac_w = 14;
            }
            int frac_x = cx + 1;
            int line_y = y + 2;
            int num_x = frac_x + (frac_w - num_w) / 2;
            int den_x = frac_x + (frac_w - den_w) / 2;
            ui_line(frac_x, line_y, frac_x + frac_w, line_y, THEME_TEXT);
            ui_border(frac_x - 2, y - 10, frac_w + 5, 24, THEME_BORDER);
            if (num[0] != '\0') {
                ui_tiny_text(num_x, y - 6, num, THEME_TEXT);
            }
            if (den[0] != '\0') {
                ui_tiny_text(den_x, y + 6, den, THEME_TEXT);
            }

            if (cursor >= frac_num_start && cursor <= frac_comma) {
                size_t cursor_chars = cursor > frac_num_start ? cursor - frac_num_start : 0;
                if (cursor_chars > num_len) {
                    cursor_chars = num_len;
                }
                int cursor_x = num_x + ui_tiny_text_width(num, cursor_chars);
                ui_draw_calc_cursor_tiny(cursor_x, y - 6);
                cursor_drawn = true;
            } else if (cursor > frac_comma && cursor <= frac_close) {
                size_t cursor_chars = cursor > den_start ? cursor - den_start : 0;
                if (cursor_chars > den_len) {
                    cursor_chars = den_len;
                }
                int cursor_x = den_x + ui_tiny_text_width(den, cursor_chars);
                ui_draw_calc_cursor_tiny(cursor_x, y + 6);
                cursor_drawn = true;
            }

            cx += frac_w + 8;
            i = frac_close + 1;
            continue;
        }

        if (text[i] == '^' && text[i + 1] == '(') {
            size_t exp_start = i + 2;
            size_t close = expression_find_matching_paren(text, i + 1);
            if (close > i + 1) {
                char exp[48];
                size_t exp_len = close - exp_start;
                if (exp_len >= sizeof(exp)) {
                    exp_len = sizeof(exp) - 1;
                }
                memcpy(exp, text + exp_start, exp_len);
                exp[exp_len] = '\0';

                int exp_w = ui_tiny_text_width(exp, exp_len);
                int box_w = exp_w + 6;
                if (box_w < 12) {
                    box_w = 12;
                }
                int box_x = cx + 1;
                int box_y = y - 10;
                ui_border(box_x, box_y, box_w, 10, THEME_BORDER);
                if (exp[0] != '\0') {
                    ui_tiny_text(box_x + 3, box_y + 2, exp, THEME_TEXT);
                }

                if (cursor == i + 1 || (cursor >= exp_start && cursor <= close)) {
                    size_t cursor_chars = cursor > exp_start ? cursor - exp_start : 0;
                    if (cursor_chars > exp_len) {
                        cursor_chars = exp_len;
                    }
                    int cursor_x = box_x + 3 + ui_tiny_text_width(exp, cursor_chars);
                    ui_draw_calc_cursor_tiny(cursor_x, box_y + 2);
                    cursor_drawn = true;
                }

                cx += box_w + 3;
                i = close + 1;
                continue;
            }
        }

        char one[2] = {text[i], '\0'};
        if (!cursor_drawn && cursor == i) {
            ui_draw_calc_cursor_large(cx, y, text[i], true);
            cursor_drawn = true;
        } else {
            ui_calc_text(cx, y, one, THEME_TEXT);
        }
        cx += ui_calc_text_width(one, 1);
        i++;
    }

    if (!cursor_drawn && cursor == len) {
        ui_draw_calc_cursor_large(cx, y, ' ', false);
    }
}

static bool display_simple_fraction(const char *text, size_t *slash)
{
    if (text == NULL || text[0] == '\0') {
        return false;
    }

    const char *found = strchr(text, '/');
    if (found == NULL || found == text || found[1] == '\0' || strchr(found + 1, '/') != NULL) {
        return false;
    }

    for (const char *p = text; *p != '\0'; p++) {
        if (p == found) {
            continue;
        }
        if (!((*p >= '0' && *p <= '9') || *p == '-' || *p == '+' || *p == '.')) {
            return false;
        }
    }

    *slash = (size_t)(found - text);
    return true;
}

static int ui_math_text(int x, int y, const char *text, uint32_t color, bool draw)
{
    if (text == NULL) {
        return 0;
    }

    size_t simple_slash = 0;
    if (display_simple_fraction(text, &simple_slash)) {
        char numerator[48];
        char denominator[48];
        size_t numerator_len = simple_slash;
        size_t denominator_len = strlen(text + simple_slash + 1);
        if (numerator_len >= sizeof(numerator)) {
            numerator_len = sizeof(numerator) - 1;
        }
        if (denominator_len >= sizeof(denominator)) {
            denominator_len = sizeof(denominator) - 1;
        }
        memcpy(numerator, text, numerator_len);
        numerator[numerator_len] = '\0';
        memcpy(denominator, text + simple_slash + 1, denominator_len);
        denominator[denominator_len] = '\0';

        int numerator_w = ui_tiny_text_width(numerator, numerator_len);
        int denominator_w = ui_tiny_text_width(denominator, denominator_len);
        int width = numerator_w > denominator_w ? numerator_w : denominator_w;
        if (width < 10) {
            width = 10;
        }
        if (draw) {
            ui_tiny_text(x + (width - numerator_w) / 2, y - 5, numerator, color);
            ui_line(x, y + 1, x + width, y + 1, color);
            ui_tiny_text(x + (width - denominator_w) / 2, y + 3, denominator, color);
        }
        return width + 1;
    }

    int cx = x;
    size_t len = strlen(text);
    for (size_t i = 0; i < len;) {
        size_t num_start = 0;
        size_t comma = 0;
        size_t close = 0;
        if (expression_find_frac_parts(text, i, &num_start, &comma, &close)) {
            char numerator[48];
            char denominator[48];
            size_t numerator_len = comma - num_start;
            size_t denominator_start = comma + 1;
            size_t denominator_len = close - denominator_start;
            if (numerator_len >= sizeof(numerator)) {
                numerator_len = sizeof(numerator) - 1;
            }
            if (denominator_len >= sizeof(denominator)) {
                denominator_len = sizeof(denominator) - 1;
            }
            memcpy(numerator, text + num_start, numerator_len);
            numerator[numerator_len] = '\0';
            memcpy(denominator, text + denominator_start, denominator_len);
            denominator[denominator_len] = '\0';

            int numerator_w = ui_tiny_text_width(numerator, numerator_len);
            int denominator_w = ui_tiny_text_width(denominator, denominator_len);
            int width = numerator_w > denominator_w ? numerator_w : denominator_w;
            if (width < 10) {
                width = 10;
            }
            if (draw) {
                ui_tiny_text(cx + (width - numerator_w) / 2, y - 5, numerator, color);
                ui_line(cx, y + 1, cx + width, y + 1, color);
                ui_tiny_text(cx + (width - denominator_w) / 2, y + 3, denominator, color);
            }
            cx += width + 3;
            i = close + 1;
            continue;
        }

        if (text[i] == '^') {
            size_t exp_start = i + 1;
            size_t exp_end = exp_start;
            if (text[exp_start] == '(') {
                size_t exp_close = expression_find_matching_paren(text, exp_start);
                exp_start++;
                exp_end = exp_close;
                i = exp_close + 1;
            } else if (text[exp_start] != '\0') {
                exp_end = exp_start + 1;
                i = exp_end;
            } else {
                i++;
            }

            size_t exp_len = exp_end > exp_start ? exp_end - exp_start : 0;
            if (exp_len > 0) {
                char exponent[48];
                if (exp_len >= sizeof(exponent)) {
                    exp_len = sizeof(exponent) - 1;
                }
                memcpy(exponent, text + exp_start, exp_len);
                exponent[exp_len] = '\0';
                int width = ui_tiny_text_width(exponent, exp_len);
                if (draw) {
                    ui_tiny_text(cx, y - 5, exponent, color);
                }
                cx += width + 1;
            }
            continue;
        }

        char one[2] = {text[i], '\0'};
        if (draw) {
            ui_calc_text(cx, y, one, color);
        }
        cx += ui_calc_text_width(one, 1);
        i++;
    }
    return cx - x;
}

static void ui_text_center(int y, const char *text, uint32_t color, int scale)
{
    int len = text ? (int)strlen(text) : 0;
    int w = len * 6 * scale;
    ui_text((UI_W - w) / 2, y, text, color, scale);
}

static void ui_text_centered_in_box(int x, int y, int w, const char *text, uint32_t color, int scale)
{
    int text_w = ui_text_width(text, 32, scale);
    int text_x = x + (w - text_w) / 2;
    if (text_x < x) {
        text_x = x;
    }
    ui_text(text_x, y, text, color, scale);
}

static void ui_battery_indicator(int x, int y)
{
    int percent = 100;
    bool has_battery_info = board_battery_get_percent(&percent);
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }

    int bars = 0;
    if (percent > 75) {
        bars = 4;
    } else if (percent > 50) {
        bars = 3;
    } else if (percent > 25) {
        bars = 2;
    } else if (percent > 0) {
        bars = 1;
    }

    uint32_t fill = THEME_BATTERY_GREEN;
    if (bars <= 1) {
        fill = THEME_BATTERY_RED;
    } else if (bars == 2) {
        fill = THEME_BATTERY_YELLOW;
    }

    bool flash_off = percent > 0 && percent <= 10 &&
        ((esp_timer_get_time() / 500000) % 2) == 0;

    ui_border(x, y, 24, 10, THEME_HEADER_TEXT);
    ui_rect(x + 24, y + 3, 3, 4, THEME_HEADER_TEXT);
    for (int i = 0; i < 4; i++) {
        ui_rect(x + 3 + i * 5, y + 3, 4, 4,
                (i < bars && !flash_off) ? fill : THEME_SURFACE_2);
    }
    if (!has_battery_info) {
        ui_line(x + 3, y + 2, x + 21, y + 8, THEME_MUTED);
    }
}

static void ui_charge_indicator(int x, int y)
{
    if (!board_battery_is_charging()) {
        return;
    }

    ui_line(x + 5, y, x + 1, y + 6, THEME_CHARGE_YELLOW);
    ui_line(x + 1, y + 6, x + 5, y + 6, THEME_CHARGE_YELLOW);
    ui_line(x + 5, y + 6, x + 2, y + 12, THEME_CHARGE_YELLOW);
    ui_line(x + 7, y + 3, x + 3, y + 8, THEME_CHARGE_YELLOW);
    ui_line(x + 3, y + 8, x + 7, y + 8, THEME_CHARGE_YELLOW);
    ui_line(x + 7, y + 8, x + 3, y + 13, THEME_CHARGE_YELLOW);
}

static void ui_mode_badge(int x, int y, const char *label, uint32_t color)
{
    int w = ui_text_width(label, 8, 1) + 6;
    ui_rect(x, y, w, 12, color);
    ui_text(x + 3, y + 3, label, THEME_BG, 1);
}

static void ui_shift_indicators(int y)
{
    int x = 274;

    if (s_alpha_locked) {
        x -= ui_text_width("LOCK", 8, 1) + 8;
        ui_mode_badge(x, y, "LOCK", THEME_BATTERY_YELLOW);
        x -= 2;
    }

    if (s_alpha_active || s_alpha_locked) {
        x -= ui_text_width("A", 8, 1) + 8;
        ui_mode_badge(x, y, "A", THEME_ACCENT);
        x -= 2;
    }

    if (s_second_active) {
        x -= ui_text_width("2ND", 8, 1) + 8;
        ui_mode_badge(x, y, "2ND", THEME_WHITE);
    }
}

static void ui_header(const app_info_t *app)
{
    const app_info_t *header_app = app ? app : &APPS[s_home_selection];
    app_id_t app_id = app_id_for_info(header_app);
    const int y = UI_TOP_SAFE_Y;
    ui_rect(0, y, UI_W, UI_HEADER_H, THEME_HEADER);
    ui_rect(0, y + UI_HEADER_H - 2, UI_W, 2, THEME_ACCENT);
    ui_rect(7, y + 5, 18, 18, THEME_SURFACE_2);
    ui_app_icon(app_id, 8, y + 6, 16, header_app->color);
    ui_text_center(y + 10, header_app->title, THEME_HEADER_TEXT, 1);
    ui_shift_indicators(y + 7);
    ui_charge_indicator(276, y + 7);
    ui_battery_indicator(288, y + 9);
}

static void ui_present(void)
{
    board_display_lock();
    board_draw_rgb888_frame_320x240(s_frame);
    board_display_unlock();
}

static graph_view_t current_graph_view(void)
{
    return (graph_view_t) {
        .xmin = s_graph_xmin,
        .xmax = s_graph_xmax,
        .ymin = s_graph_ymin,
        .ymax = s_graph_ymax,
        .screen_w = UI_W,
        .screen_top = GRAPH_TOP,
        .screen_bottom = GRAPH_BOTTOM,
    };
}

static const char *graph_poi_label(graph_poi_type_t type)
{
    switch (type) {
    case GRAPH_POI_ZERO: return "zero";
    case GRAPH_POI_Y_INTERCEPT: return "y-int";
    case GRAPH_POI_MIN: return "min";
    case GRAPH_POI_LOCAL_MAX: return "max";
    case GRAPH_POI_INTERSECTION: return "intersect";
    default: return "point";
    }
}

static bool graph_eval_fn_at(int fn, double x, double *y)
{
    return fn >= 0 && fn < 10 && s_graph_enabled[fn] && s_graph_exprs[fn][0] != '\0' &&
        graph_eval_expression(s_graph_exprs[fn], x, y);
}

static bool work_graph_eval_fn_at(const ui_work_job_t *job, int fn, double x, double *y)
{
    return job != NULL && fn >= 0 && fn < 10 && job->graph.enabled[fn] &&
        job->graph.exprs[fn][0] != '\0' && graph_eval_expression(job->graph.exprs[fn], x, y);
}

static bool work_graph_refine_zero(const ui_work_job_t *job, int fn, double lo, double hi, double *x, double *y)
{
    double f_lo = 0.0;
    double f_hi = 0.0;
    if (!work_graph_eval_fn_at(job, fn, lo, &f_lo) || !work_graph_eval_fn_at(job, fn, hi, &f_hi)) {
        return false;
    }

    if (fabs(f_lo) <= 1e-12) {
        *x = fabs(lo) <= 1e-9 ? 0.0 : lo;
        *y = 0.0;
        return true;
    }
    if (fabs(f_hi) <= 1e-12) {
        *x = fabs(hi) <= 1e-9 ? 0.0 : hi;
        *y = 0.0;
        return true;
    }

    for (int i = 0; i < 28; i++) {
        double mid = (lo + hi) * 0.5;
        double f_mid = 0.0;
        if (!work_graph_eval_fn_at(job, fn, mid, &f_mid)) {
            return false;
        }
        if (fabs(f_mid) <= 1e-12) {
            *x = fabs(mid) <= 1e-9 ? 0.0 : mid;
            *y = 0.0;
            return true;
        }
        if ((f_lo <= 0.0 && f_mid >= 0.0) || (f_lo >= 0.0 && f_mid <= 0.0)) {
            hi = mid;
            f_hi = f_mid;
        } else {
            lo = mid;
            f_lo = f_mid;
        }
        (void)f_hi;
    }

    *x = (lo + hi) * 0.5;
    if (!work_graph_eval_fn_at(job, fn, *x, y)) {
        return false;
    }
    if (fabs(*x) <= 1e-9) *x = 0.0;
    if (fabs(*y) <= 1e-9) *y = 0.0;
    return true;
}

static bool work_graph_refine_intersection(const ui_work_job_t *job, int fn_a, int fn_b, double lo, double hi, double *x, double *y)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    double d = 0.0;
    if (!work_graph_eval_fn_at(job, fn_a, lo, &a) || !work_graph_eval_fn_at(job, fn_b, lo, &b) ||
        !work_graph_eval_fn_at(job, fn_a, hi, &c) || !work_graph_eval_fn_at(job, fn_b, hi, &d)) {
        return false;
    }

    double f_lo = a - b;
    for (int i = 0; i < 28; i++) {
        double mid = (lo + hi) * 0.5;
        double y_a = 0.0;
        double y_b = 0.0;
        if (!work_graph_eval_fn_at(job, fn_a, mid, &y_a) || !work_graph_eval_fn_at(job, fn_b, mid, &y_b)) {
            return false;
        }
        double f_mid = y_a - y_b;
        if ((f_lo <= 0.0 && f_mid >= 0.0) || (f_lo >= 0.0 && f_mid <= 0.0)) {
            hi = mid;
        } else {
            lo = mid;
            f_lo = f_mid;
        }
    }

    *x = (lo + hi) * 0.5;
    return work_graph_eval_fn_at(job, fn_a, *x, y);
}

static void work_graph_add_poi(graph_poi_t *pois, int *count, graph_poi_type_t type, int fn, int other_fn, double x, double y)
{
    if (*count >= GRAPH_POI_LIMIT || !isfinite(x) || !isfinite(y)) {
        return;
    }
    for (int i = 0; i < *count; i++) {
        if (pois[i].type == type && pois[i].fn == fn && pois[i].other_fn == other_fn &&
            fabs(pois[i].x - x) < 0.05 && fabs(pois[i].y - y) < 0.05) {
            return;
        }
    }
    pois[*count] = (graph_poi_t){.x = x, .y = y, .fn = fn, .other_fn = other_fn, .type = type};
    (*count)++;
}

static int work_graph_collect_pois(const ui_work_job_t *job, graph_poi_t *pois, int max_count)
{
    int count = 0;
    int enabled[10];
    int enabled_count = 0;
    double step = (job->graph.xmax - job->graph.xmin) / 96.0;
    if (step <= 0.0) {
        return 0;
    }

    for (int fn = 0; fn < 10; fn++) {
        if (!job->graph.enabled[fn] || job->graph.exprs[fn][0] == '\0') {
            continue;
        }
        enabled[enabled_count++] = fn;

        double y0 = 0.0;
        if (job->graph.xmin <= 0.0 && job->graph.xmax >= 0.0 && work_graph_eval_fn_at(job, fn, 0.0, &y0)) {
            work_graph_add_poi(pois, &count, GRAPH_POI_Y_INTERCEPT, fn, -1, 0.0, y0);
        }

        double x_prev2 = job->graph.xmin;
        double y_prev2 = 0.0;
        double x_prev = job->graph.xmin + step;
        double y_prev = 0.0;
        bool have_prev2 = work_graph_eval_fn_at(job, fn, x_prev2, &y_prev2);
        bool have_prev = work_graph_eval_fn_at(job, fn, x_prev, &y_prev);
        for (int i = 2; i <= 96 && count < max_count; i++) {
            double x = job->graph.xmin + step * (double)i;
            double y = 0.0;
            bool have = work_graph_eval_fn_at(job, fn, x, &y);

            if (have_prev && have &&
                ((y_prev <= 0.0 && y >= 0.0) || (y_prev >= 0.0 && y <= 0.0))) {
                double zx = 0.0;
                double zy = 0.0;
                if (work_graph_refine_zero(job, fn, x_prev, x, &zx, &zy)) {
                    work_graph_add_poi(pois, &count, GRAPH_POI_ZERO, fn, -1, zx, zy);
                }
            }

            if (have_prev2 && have_prev && have) {
                if (y_prev < y_prev2 && y_prev < y) {
                    work_graph_add_poi(pois, &count, GRAPH_POI_MIN, fn, -1, x_prev, y_prev);
                } else if (y_prev > y_prev2 && y_prev > y) {
                    work_graph_add_poi(pois, &count, GRAPH_POI_LOCAL_MAX, fn, -1, x_prev, y_prev);
                }
            }

            x_prev2 = x_prev;
            y_prev2 = y_prev;
            have_prev2 = have_prev;
            x_prev = x;
            y_prev = y;
            have_prev = have;
        }
    }

    for (int a = 0; a < enabled_count; a++) {
        for (int b = a + 1; b < enabled_count; b++) {
            int fn_a = enabled[a];
            int fn_b = enabled[b];
            double prev_x = job->graph.xmin;
            double ya = 0.0;
            double yb = 0.0;
            bool have_prev = work_graph_eval_fn_at(job, fn_a, prev_x, &ya) &&
                work_graph_eval_fn_at(job, fn_b, prev_x, &yb);
            double prev_diff = ya - yb;
            for (int i = 1; i <= 96 && count < max_count; i++) {
                double x = job->graph.xmin + step * (double)i;
                bool have = work_graph_eval_fn_at(job, fn_a, x, &ya) &&
                    work_graph_eval_fn_at(job, fn_b, x, &yb);
                double diff = ya - yb;
                if (have_prev && have &&
                    ((prev_diff <= 0.0 && diff >= 0.0) || (prev_diff >= 0.0 && diff <= 0.0))) {
                    double ix = 0.0;
                    double iy = 0.0;
                    if (work_graph_refine_intersection(job, fn_a, fn_b, prev_x, x, &ix, &iy)) {
                        work_graph_add_poi(pois, &count, GRAPH_POI_INTERSECTION, fn_a, fn_b, ix, iy);
                    }
                }
                prev_x = x;
                prev_diff = diff;
                have_prev = have;
            }
        }
    }

    return count;
}

static bool work_graph_first_enabled_fn(const ui_work_job_t *job, int *fn)
{
    for (int i = 0; i < 10; i++) {
        if (job->graph.enabled[i] && job->graph.exprs[i][0] != '\0') {
            *fn = i;
            return true;
        }
    }
    return false;
}

static bool work_graph_ensure_trace(const ui_work_job_t *job, bool *trace, double *trace_x, int *trace_fn)
{
    if (*trace) {
        return true;
    }
    int fn = 0;
    if (!work_graph_first_enabled_fn(job, &fn)) {
        return false;
    }
    *trace = true;
    *trace_fn = fn;
    *trace_x = (job->graph.xmin + job->graph.xmax) * 0.5;
    return true;
}

static bool work_graph_jump_to_poi(const ui_work_job_t *job, graph_poi_type_t type, ui_work_result_t *result)
{
    graph_poi_t pois[GRAPH_POI_LIMIT];
    int count = work_graph_collect_pois(job, pois, GRAPH_POI_LIMIT);
    double target = job->graph.trace ? job->graph.trace_x : (job->graph.xmin + job->graph.xmax) * 0.5;
    int best = -1;
    double best_dist = 0.0;

    for (int i = 0; i < count; i++) {
        if (pois[i].type != type) {
            continue;
        }
        double dist = fabs(pois[i].x - target);
        if (best < 0 || dist < best_dist) {
            best = i;
            best_dist = dist;
        }
    }
    if (best < 0) {
        return false;
    }

    double display_x = fabs(pois[best].x) <= 1e-9 ? 0.0 : pois[best].x;
    double display_y = fabs(pois[best].y) <= 1e-9 ? 0.0 : pois[best].y;
    result->graph.trace = true;
    result->graph.trace_x = display_x;
    result->graph.trace_fn = pois[best].fn;
    snprintf(result->graph.status, sizeof(result->graph.status), "%s Y%d x %.4g y %.4g",
             graph_poi_label(type), pois[best].fn + 1, display_x, display_y);
    return true;
}

static bool work_graph_calc_value(const ui_work_job_t *job, ui_work_result_t *result)
{
    bool trace = job->graph.trace;
    double trace_x = job->graph.trace_x;
    int trace_fn = job->graph.trace_fn;
    if (!work_graph_ensure_trace(job, &trace, &trace_x, &trace_fn)) {
        return false;
    }

    double y = 0.0;
    if (!work_graph_eval_fn_at(job, trace_fn, trace_x, &y)) {
        return false;
    }
    result->graph.trace = trace;
    result->graph.trace_x = trace_x;
    result->graph.trace_fn = trace_fn;
    snprintf(result->graph.status, sizeof(result->graph.status), "value Y%d x %.4g y %.6g",
             trace_fn + 1, trace_x, y);
    return true;
}

static bool work_graph_calc_derivative(const ui_work_job_t *job, ui_work_result_t *result)
{
    bool trace = job->graph.trace;
    double trace_x = job->graph.trace_x;
    int trace_fn = job->graph.trace_fn;
    if (!work_graph_ensure_trace(job, &trace, &trace_x, &trace_fn)) {
        return false;
    }

    double span = job->graph.xmax - job->graph.xmin;
    double h = span > 0.0 ? span / 2000.0 : 0.001;
    if (h < 1e-6) h = 1e-6;

    double y0 = 0.0;
    double y1 = 0.0;
    if (!work_graph_eval_fn_at(job, trace_fn, trace_x - h, &y0) ||
        !work_graph_eval_fn_at(job, trace_fn, trace_x + h, &y1)) {
        return false;
    }

    result->graph.trace = trace;
    result->graph.trace_x = trace_x;
    result->graph.trace_fn = trace_fn;
    snprintf(result->graph.status, sizeof(result->graph.status), "dy/dx Y%d x %.4g = %.6g",
             trace_fn + 1, trace_x, (y1 - y0) / (2.0 * h));
    return true;
}

static bool work_graph_calc_integral(const ui_work_job_t *job, ui_work_result_t *result)
{
    bool trace = job->graph.trace;
    double trace_x = job->graph.trace_x;
    int trace_fn = job->graph.trace_fn;
    if (!work_graph_ensure_trace(job, &trace, &trace_x, &trace_fn)) {
        return false;
    }

    double a = 0.0;
    double b = trace_x;
    double sign = 1.0;
    if (b < a) {
        double tmp = a;
        a = b;
        b = tmp;
        sign = -1.0;
    }

    const int steps = 96;
    double dx = (b - a) / (double)steps;
    double sum = 0.0;
    for (int i = 0; i <= steps; i++) {
        double y = 0.0;
        double x = a + dx * (double)i;
        if (!work_graph_eval_fn_at(job, trace_fn, x, &y)) {
            return false;
        }
        sum += y * (i == 0 || i == steps ? 0.5 : 1.0);
    }

    result->graph.trace = trace;
    result->graph.trace_x = trace_x;
    result->graph.trace_fn = trace_fn;
    snprintf(result->graph.status, sizeof(result->graph.status), "int Y%d 0..%.4g = %.6g",
             trace_fn + 1, trace_x, sum * dx * sign);
    return true;
}

static bool work_graph_calc_run(const ui_work_job_t *job, ui_work_result_t *result)
{
    result->graph.trace = job->graph.trace;
    result->graph.trace_x = job->graph.trace_x;
    result->graph.trace_fn = job->graph.trace_fn;
    result->graph.status[0] = '\0';

    switch (job->graph.selection) {
    case 0: return work_graph_calc_value(job, result);
    case 1: return work_graph_jump_to_poi(job, GRAPH_POI_ZERO, result);
    case 2: return work_graph_jump_to_poi(job, GRAPH_POI_MIN, result);
    case 3: return work_graph_jump_to_poi(job, GRAPH_POI_LOCAL_MAX, result);
    case 4: return work_graph_jump_to_poi(job, GRAPH_POI_INTERSECTION, result);
    case 5: return work_graph_jump_to_poi(job, GRAPH_POI_Y_INTERCEPT, result);
    case 6: return work_graph_calc_derivative(job, result);
    case 7: return work_graph_calc_integral(job, result);
    default: return false;
    }
}

static void inequality_clear_all(void)
{
    memset(s_ineqs, 0, sizeof(s_ineqs));
}

static void inequality_set(int slot, const char *expr, bool greater, bool inclusive)
{
    if (slot < 0 || slot >= INEQ_MAX || expr == NULL) {
        return;
    }

    s_ineqs[slot] = (inequality_t) {
        .enabled = true,
        .vertical = false,
        .greater = greater,
        .inclusive = inclusive,
        .x_value = 0.0,
    };
    snprintf(s_ineqs[slot].expr, sizeof(s_ineqs[slot].expr), "%s", expr);
}

static void inequality_set_vertical(int slot, double x_value, bool greater, bool inclusive)
{
    if (slot < 0 || slot >= INEQ_MAX) {
        return;
    }

    s_ineqs[slot] = (inequality_t) {
        .enabled = true,
        .vertical = true,
        .greater = greater,
        .inclusive = inclusive,
        .x_value = x_value,
    };
    snprintf(s_ineqs[slot].expr, sizeof(s_ineqs[slot].expr), "x=%.10g", x_value);
}

static bool inequality_any_enabled(void)
{
    for (int i = 0; i < INEQ_MAX; i++) {
        if (s_ineqs[i].enabled) {
            return true;
        }
    }
    return false;
}

static void ui_dotted_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int step = 0;

    while (true) {
        if ((step % 8) < 4) {
            ui_pixel(x0, y0, color);
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
        step++;
    }
}

static void draw_inequality_layer(const graph_view_t *view)
{
    if (view == NULL || !inequality_any_enabled()) {
        return;
    }

    for (int px = 0; px < UI_W; px += 2) {
        double x = graph_world_x(view, px);
        for (int py = GRAPH_TOP; py <= GRAPH_BOTTOM; py += 2) {
            double y = s_graph_ymax -
                ((double)(py - GRAPH_TOP) / (double)(GRAPH_BOTTOM - GRAPH_TOP)) *
                (s_graph_ymax - s_graph_ymin);
            int matches = 0;
            for (int i = 0; i < INEQ_MAX; i++) {
                if (!s_ineqs[i].enabled) {
                    continue;
                }
                bool ok = false;
                if (s_ineqs[i].vertical) {
                    ok = s_ineqs[i].greater ? x >= s_ineqs[i].x_value : x <= s_ineqs[i].x_value;
                } else {
                    double boundary = 0.0;
                    if (!graph_eval_expression(s_ineqs[i].expr, x, &boundary)) {
                        continue;
                    }
                    ok = s_ineqs[i].greater ? y >= boundary : y <= boundary;
                }
                if (ok) {
                    matches++;
                }
            }

            if (matches > 0) {
                uint32_t color = matches > 1 ? 0x203a58 : 0x142437;
                ui_rect(px, py, 2, 2, color);
            }
        }
    }

    for (int i = 0; i < INEQ_MAX; i++) {
        if (!s_ineqs[i].enabled) {
            continue;
        }

        uint32_t color = s_graph_colors[i % GRAPH_COLOR_COUNT];
        if (s_ineqs[i].vertical) {
            int sx = graph_screen_x(view, s_ineqs[i].x_value);
            if (s_ineqs[i].inclusive) {
                ui_line(sx, GRAPH_TOP, sx, GRAPH_BOTTOM, color);
            } else {
                ui_dotted_line(sx, GRAPH_TOP, sx, GRAPH_BOTTOM, color);
            }
            continue;
        }

        bool have_prev = false;
        int prev_x = 0;
        int prev_y = 0;
        for (int px = 0; px < UI_W; px++) {
            double x = graph_world_x(view, px);
            double y = 0.0;
            if (!graph_eval_expression(s_ineqs[i].expr, x, &y)) {
                have_prev = false;
                continue;
            }
            int py = graph_screen_y(view, y);
            if (py < GRAPH_TOP - 40 || py > GRAPH_BOTTOM + 40) {
                have_prev = false;
                continue;
            }
            if (have_prev) {
                if (s_ineqs[i].inclusive) {
                    ui_line(prev_x, prev_y, px, py, color);
                } else {
                    ui_dotted_line(prev_x, prev_y, px, py, color);
                }
            }
            prev_x = px;
            prev_y = py;
            have_prev = true;
        }
    }
}

static bool graph_refine_zero(int fn, double lo, double hi, double *x, double *y)
{
    double f_lo = 0.0;
    double f_hi = 0.0;
    if (!graph_eval_fn_at(fn, lo, &f_lo) || !graph_eval_fn_at(fn, hi, &f_hi)) {
        return false;
    }

    if (fabs(f_lo) <= 1e-12) {
        *x = fabs(lo) <= 1e-9 ? 0.0 : lo;
        *y = 0.0;
        return true;
    }
    if (fabs(f_hi) <= 1e-12) {
        *x = fabs(hi) <= 1e-9 ? 0.0 : hi;
        *y = 0.0;
        return true;
    }

    for (int i = 0; i < 28; i++) {
        double mid = (lo + hi) * 0.5;
        double f_mid = 0.0;
        if (!graph_eval_fn_at(fn, mid, &f_mid)) {
            return false;
        }
        if (fabs(f_mid) <= 1e-12) {
            *x = fabs(mid) <= 1e-9 ? 0.0 : mid;
            *y = 0.0;
            return true;
        }
        if ((f_lo <= 0.0 && f_mid >= 0.0) || (f_lo >= 0.0 && f_mid <= 0.0)) {
            hi = mid;
            f_hi = f_mid;
        } else {
            lo = mid;
            f_lo = f_mid;
        }
        (void)f_hi;
    }

    *x = (lo + hi) * 0.5;
    if (!graph_eval_fn_at(fn, *x, y)) {
        return false;
    }
    if (fabs(*x) <= 1e-9) {
        *x = 0.0;
    }
    if (fabs(*y) <= 1e-9) {
        *y = 0.0;
    }
    return true;
}

static bool graph_refine_intersection(int fn_a, int fn_b, double lo, double hi, double *x, double *y)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    double d = 0.0;
    if (!graph_eval_fn_at(fn_a, lo, &a) || !graph_eval_fn_at(fn_b, lo, &b) ||
        !graph_eval_fn_at(fn_a, hi, &c) || !graph_eval_fn_at(fn_b, hi, &d)) {
        return false;
    }

    double f_lo = a - b;
    for (int i = 0; i < 28; i++) {
        double mid = (lo + hi) * 0.5;
        double y_a = 0.0;
        double y_b = 0.0;
        if (!graph_eval_fn_at(fn_a, mid, &y_a) || !graph_eval_fn_at(fn_b, mid, &y_b)) {
            return false;
        }
        double f_mid = y_a - y_b;
        if ((f_lo <= 0.0 && f_mid >= 0.0) || (f_lo >= 0.0 && f_mid <= 0.0)) {
            hi = mid;
        } else {
            lo = mid;
            f_lo = f_mid;
        }
    }

    *x = (lo + hi) * 0.5;
    return graph_eval_fn_at(fn_a, *x, y);
}

static void graph_add_poi(graph_poi_t *pois, int *count, graph_poi_type_t type, int fn, int other_fn, double x, double y)
{
    if (*count >= GRAPH_POI_LIMIT || !isfinite(x) || !isfinite(y)) {
        return;
    }
    for (int i = 0; i < *count; i++) {
        if (pois[i].type == type && pois[i].fn == fn && pois[i].other_fn == other_fn &&
            fabs(pois[i].x - x) < 0.05 && fabs(pois[i].y - y) < 0.05) {
            return;
        }
    }
    pois[*count] = (graph_poi_t){.x = x, .y = y, .fn = fn, .other_fn = other_fn, .type = type};
    (*count)++;
}

static int graph_collect_pois(graph_poi_t *pois, int max_count)
{
    int count = 0;
    int enabled[10];
    int enabled_count = 0;
    double step = (s_graph_xmax - s_graph_xmin) / 96.0;
    if (step <= 0.0) {
        return 0;
    }

    for (int fn = 0; fn < 10; fn++) {
        if (!s_graph_enabled[fn] || s_graph_exprs[fn][0] == '\0') {
            continue;
        }
        enabled[enabled_count++] = fn;

        double y0 = 0.0;
        if (s_graph_xmin <= 0.0 && s_graph_xmax >= 0.0 && graph_eval_fn_at(fn, 0.0, &y0)) {
            graph_add_poi(pois, &count, GRAPH_POI_Y_INTERCEPT, fn, -1, 0.0, y0);
        }

        double x_prev2 = s_graph_xmin;
        double y_prev2 = 0.0;
        double x_prev = s_graph_xmin + step;
        double y_prev = 0.0;
        bool have_prev2 = graph_eval_fn_at(fn, x_prev2, &y_prev2);
        bool have_prev = graph_eval_fn_at(fn, x_prev, &y_prev);
        for (int i = 2; i <= 96 && count < max_count; i++) {
            double x = s_graph_xmin + step * (double)i;
            double y = 0.0;
            bool have = graph_eval_fn_at(fn, x, &y);

            if (have_prev && have &&
                ((y_prev <= 0.0 && y >= 0.0) || (y_prev >= 0.0 && y <= 0.0))) {
                double zx = 0.0;
                double zy = 0.0;
                if (graph_refine_zero(fn, x_prev, x, &zx, &zy)) {
                    graph_add_poi(pois, &count, GRAPH_POI_ZERO, fn, -1, zx, zy);
                }
            }

            if (have_prev2 && have_prev && have) {
                if (y_prev < y_prev2 && y_prev < y) {
                    graph_add_poi(pois, &count, GRAPH_POI_MIN, fn, -1, x_prev, y_prev);
                } else if (y_prev > y_prev2 && y_prev > y) {
                    graph_add_poi(pois, &count, GRAPH_POI_LOCAL_MAX, fn, -1, x_prev, y_prev);
                }
            }

            x_prev2 = x_prev;
            y_prev2 = y_prev;
            have_prev2 = have_prev;
            x_prev = x;
            y_prev = y;
            have_prev = have;
        }
    }

    for (int a = 0; a < enabled_count; a++) {
        for (int b = a + 1; b < enabled_count; b++) {
            int fn_a = enabled[a];
            int fn_b = enabled[b];
            double prev_x = s_graph_xmin;
            double ya = 0.0;
            double yb = 0.0;
            bool have_prev = graph_eval_fn_at(fn_a, prev_x, &ya) && graph_eval_fn_at(fn_b, prev_x, &yb);
            double prev_diff = ya - yb;
            for (int i = 1; i <= 96 && count < max_count; i++) {
                double x = s_graph_xmin + step * (double)i;
                bool have = graph_eval_fn_at(fn_a, x, &ya) && graph_eval_fn_at(fn_b, x, &yb);
                double diff = ya - yb;
                if (have_prev && have &&
                    ((prev_diff <= 0.0 && diff >= 0.0) || (prev_diff >= 0.0 && diff <= 0.0))) {
                    double ix = 0.0;
                    double iy = 0.0;
                    if (graph_refine_intersection(fn_a, fn_b, prev_x, x, &ix, &iy)) {
                        graph_add_poi(pois, &count, GRAPH_POI_INTERSECTION, fn_a, fn_b, ix, iy);
                    }
                }
                prev_x = x;
                prev_diff = diff;
                have_prev = have;
            }
        }
    }

    return count;
}

static bool graph_jump_to_nearest_intersection(void)
{
    graph_poi_t pois[GRAPH_POI_LIMIT];
    int count = graph_collect_pois(pois, GRAPH_POI_LIMIT);
    double target = s_graph_trace ? s_graph_trace_x : (s_graph_xmin + s_graph_xmax) * 0.5;
    int best = -1;
    double best_dist = 0.0;
    for (int i = 0; i < count; i++) {
        if (pois[i].type != GRAPH_POI_INTERSECTION) {
            continue;
        }
        double dist = fabs(pois[i].x - target);
        if (best < 0 || dist < best_dist) {
            best = i;
            best_dist = dist;
        }
    }
    if (best < 0) {
        return false;
    }

    s_graph_trace = true;
    s_graph_trace_x = pois[best].x;
    s_graph_trace_fn = pois[best].fn;
    return true;
}

static void graph_calc_run_selected(void)
{
    submit_graph_calc_job();
}

static bool graph_trace_cycle_line(void)
{
    int first_enabled = -1;
    int next_enabled = -1;

    for (int fn = 0; fn < 10; fn++) {
        if (!s_graph_enabled[fn] || s_graph_exprs[fn][0] == '\0') {
            continue;
        }
        if (first_enabled < 0) {
            first_enabled = fn;
        }
        if (fn > s_graph_trace_fn && next_enabled < 0) {
            next_enabled = fn;
        }
    }

    if (first_enabled < 0) {
        return false;
    }

    if (!s_graph_trace) {
        s_graph_trace_x = (s_graph_xmin + s_graph_xmax) / 2.0;
        s_graph_trace_fn = first_enabled;
    } else {
        s_graph_trace_fn = next_enabled >= 0 ? next_enabled : first_enabled;
    }
    s_graph_trace = true;
    s_page = PAGE_GRAPH;
    s_current_app = APP_GRAPH;
    return true;
}

static void ui_draw_home(void)
{
    ui_clear(THEME_BG);
    ui_header(NULL);

    for (int i = 0; i < UI_APP_COUNT; i++) {
        int col = i % UI_ICON_COLS;
        int row = i / UI_ICON_COLS;
        int x = 8 + col * 78;
        int y = UI_HOME_GRID_Y + row * UI_HOME_GRID_STEP_Y;
        bool selected = i == s_home_selection;

        ui_rect(x, y, 70, 46, selected ? THEME_ACCENT : THEME_BORDER);
        ui_rect(x + 2, y + 2, 66, 42, selected ? THEME_SURFACE_2 : THEME_SURFACE);
        ui_rect(x + 25, y + 6, 20, 18, THEME_BG);
        ui_app_icon((app_id_t)i, x + 26, y + 7, 18, APPS[i].color);
        ui_text_centered_in_box(x + 2, y + 30, 66, HOME_LABELS[i], THEME_TEXT, 1);
    }

    ui_text_center(216, "enter - open", THEME_MUTED, 1);
    ui_present();
}

static const app_tool_t *app_tools_for(app_id_t app_id, int *count)
{
    switch (app_id) {
    case APP_STATS:
        *count = (int)(sizeof(STATS_TOOLS) / sizeof(STATS_TOOLS[0]));
        return STATS_TOOLS;
    case APP_LISTS:
        *count = (int)(sizeof(LIST_TOOLS) / sizeof(LIST_TOOLS[0]));
        return LIST_TOOLS;
    case APP_MATRICES:
        *count = (int)(sizeof(MATRIX_TOOLS) / sizeof(MATRIX_TOOLS[0]));
        return MATRIX_TOOLS;
    case APP_SOLVER:
        *count = (int)(sizeof(SOLVER_TOOLS) / sizeof(SOLVER_TOOLS[0]));
        return SOLVER_TOOLS;
    case APP_FINANCE:
        *count = (int)(sizeof(FINANCE_TOOLS) / sizeof(FINANCE_TOOLS[0]));
        return FINANCE_TOOLS;
    case APP_CONICS:
        *count = (int)(sizeof(CONICS_TOOLS) / sizeof(CONICS_TOOLS[0]));
        return CONICS_TOOLS;
    case APP_INEQUALITY:
        *count = (int)(sizeof(INEQUALITY_TOOLS) / sizeof(INEQUALITY_TOOLS[0]));
        return INEQUALITY_TOOLS;
    default:
        *count = 0;
        return NULL;
    }
}

static void ui_draw_app_page(app_id_t app_id)
{
    const app_info_t *app = &APPS[app_id];
    int tool_count = 0;
    const app_tool_t *tools = app_tools_for(app_id, &tool_count);

    ui_clear(THEME_BG);
    ui_header(app);

    if (tool_count > 0 && tools != NULL) {
        if (s_app_selection >= tool_count) {
            s_app_selection = tool_count - 1;
        }
        int list_y = (app_id == APP_SOLVER || app_id == APP_FINANCE || app_id == APP_MATRICES) ? 78 : 36;
        if (app_id == APP_SOLVER) {
            char line[112];
            snprintf(line, sizeof(line), "E1: %.28s", s_solver_e1);
            ui_text(18, 34, line, THEME_TEXT, 1);
            snprintf(line, sizeof(line), "E2: %.28s", s_solver_e2);
            ui_text(18, 46, line, THEME_TEXT, 1);
            snprintf(line, sizeof(line), "guess %.6g", s_solver_guess);
            ui_text(18, 58, line, THEME_MUTED, 1);
            if (s_solver_has_result) {
                snprintf(line, sizeof(line), "x=%.10g", s_solver_result);
                ui_text(148, 58, line, THEME_ACCENT, 1);
            }
        } else if (app_id == APP_FINANCE) {
            char line[112];
            snprintf(line, sizeof(line), "%sN %.4g  %sI%% %.4g  %s",
                     s_fin_selection == 0 ? ">" : "", s_fin_n,
                     s_fin_selection == 1 ? ">" : "", s_fin_i,
                     s_fin_begin ? "BEGIN" : "END");
            ui_text(18, 34, line, THEME_TEXT, 1);
            snprintf(line, sizeof(line), "%sPV %.4g  %sPMT %.4g  %sFV %.4g",
                     s_fin_selection == 2 ? ">" : "",
                     s_fin_pv,
                     s_fin_selection == 3 ? ">" : "",
                     s_fin_pmt,
                     s_fin_selection == 4 ? ">" : "",
                     s_fin_fv);
            ui_text(18, 46, line, THEME_TEXT, 1);
            snprintf(line, sizeof(line), "%sP/Y %.4g  %sC/Y %.4g  cash L%d",
                     s_fin_selection == 5 ? ">" : "",
                     s_fin_py,
                     s_fin_selection == 6 ? ">" : "",
                     s_fin_cy,
                     s_list_index + 1);
            ui_text(18, 58, line, THEME_MUTED, 1);
        } else if (app_id == APP_MATRICES) {
            char line[96];
            if (s_matrix_rows == 0 || s_matrix_cols == 0) {
                ui_text(18, 38, "A empty", THEME_TEXT, 1);
                ui_text(18, 52, "Calc input format: 1,2;3,4", THEME_MUTED, 1);
            } else {
                snprintf(line, sizeof(line), "A %dx%d", s_matrix_rows, s_matrix_cols);
                ui_text(18, 34, line, THEME_TEXT, 1);
                for (int r = 0; r < s_matrix_rows && r < 2; r++) {
                    char row[96];
                    int used = snprintf(row, sizeof(row), "[");
                    for (int c = 0; c < s_matrix_cols && used < (int)sizeof(row) - 1; c++) {
                        used += snprintf(row + used, sizeof(row) - (size_t)used,
                                         "%s%.4g", c == 0 ? "" : " ", s_matrix_a[r][c]);
                    }
                    if (used < 0 || used >= (int)sizeof(row)) {
                        used = (int)sizeof(row) - 2;
                    }
                    snprintf(row + used, sizeof(row) - (size_t)used, "]");
                    ui_text(18, 46 + r * 12, row, THEME_MUTED, 1);
                }
            }
        }
        for (int i = 0; i < tool_count && i < 7; i++) {
            uint32_t bg = (i == s_app_selection) ? THEME_ACCENT_2 : THEME_SURFACE;
            ui_rect(18, list_y + i * 16, 284, 15, bg);
            ui_text(28, list_y + 4 + i * 16, tools[i].label, THEME_TEXT, 1);
            ui_text(134, list_y + 4 + i * 16, tools[i].detail, THEME_MUTED, 1);
        }
        ui_rect(16, 170, 288, 18, THEME_SURFACE);
        ui_text(24, 176, s_calc_output, THEME_ACCENT, 1);
    } else {
        for (int i = 0; i < 5 && app->lines[i] != NULL; i++) {
            ui_rect(18, 42 + i * 22, 8, 8, app->color);
            ui_text(34, 39 + i * 22, app->lines[i], THEME_TEXT, 1);
        }
    }

    ui_text(16, 220, "up, down - select  enter - run", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_list_editor(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_LISTS]);

    for (int i = 0; i < LIST_COUNT; i++) {
        int x = 12 + i * 50;
        uint32_t bg = i == s_list_index ? THEME_ACCENT_2 : THEME_SURFACE;
        char tab[12];
        snprintf(tab, sizeof(tab), "L%d:%d", i + 1, s_list_counts[i]);
        ui_rect(x, 32, 45, 13, bg);
        ui_text(x + 4, 35, tab, THEME_TEXT, 1);
    }

    char title[32];
    snprintf(title, sizeof(title), "L%d count %d", s_list_index + 1, s_list_counts[s_list_index]);
    ui_text(14, 49, title, THEME_TEXT, 1);

    int visible_start = s_list_cursor > 7 ? s_list_cursor - 7 : 0;
    for (int i = visible_start; i < s_list_counts[s_list_index] && i < visible_start + 8; i++) {
        int y = 66 + (i - visible_start) * 17;
        bool selected = i == s_list_cursor;
        ui_rect(14, y - 2, 292, 15, selected ? THEME_ACCENT_2 : THEME_SURFACE);
        char line[48];
        snprintf(line, sizeof(line), "%02d  %.10g", i + 1, s_lists[s_list_index][i]);
        ui_text(22, y + 3, line, THEME_TEXT, 1);
    }

    if (s_list_cursor == s_list_counts[s_list_index] && s_list_counts[s_list_index] < LIST_MAX_VALUES) {
        int row = s_list_cursor - visible_start;
        if (row >= 0 && row < 8) {
            int y = 66 + row * 17;
            ui_rect(14, y - 2, 292, 15, THEME_ACCENT_2);
            char line[56];
            snprintf(line, sizeof(line), "%02d  %s", s_list_cursor + 1, s_list_entry[0] ? s_list_entry : "_");
            ui_text(22, y + 3, line, THEME_TEXT, 1);
        }
    }

    ui_text(12, 220, "left, right - list  enter - save  del - clear", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_settings(void)
{
    char brightness[40];
    char sleep[40];
    char power_save[40];
    char theme[40];
    char reset[40];

    snprintf(brightness, sizeof(brightness), "Brightness %d%%", board_get_backlight_brightness());
    snprintf(sleep, sizeof(sleep), "Auto sleep %s", s_sleep_enabled ? "on" : "off");
    snprintf(power_save, sizeof(power_save), "Power save %s", s_power_save_enabled ? "on" : "off");
    snprintf(theme, sizeof(theme), "Theme %s", s_light_mode ? "light" : "dark");
    snprintf(reset, sizeof(reset), "Reset to factory");

    const char *items[SETTINGS_COUNT] = {
        brightness,
        sleep,
        power_save,
        theme,
        reset,
    };

    ui_clear(THEME_BG);
    ui_header(&APPS[APP_SETTINGS]);
    for (int i = 0; i < SETTINGS_COUNT; i++) {
        uint32_t bg = (i == s_script_selection) ? THEME_ACCENT_2 : THEME_SURFACE;
        ui_rect(18, 34 + i * 22, 284, 18, bg);
        ui_text(28, 40 + i * 22, items[i], THEME_TEXT, 1);
    }
    const char *footer = "enter - toggle";
    if (s_script_selection == 0) {
        footer = "left, right - brightness";
    } else if (s_script_selection == SETTINGS_COUNT - 1) {
        footer = "enter - reset";
    }
    ui_text(18, 220, footer, THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_mode_menu(void)
{
    static const char *display[] = {"Normal", "Sci", "Eng"};
    static const char *print[] = {"MathPrint", "Classic"};
    static const char *angle[] = {"Degree", "Radian"};
    static const char *graph[] = {"Func", "Par", "Pol", "Seq"};
    static const char *complex[] = {"REAL", "a+bi", "re^ti"};

    char rows[5][40];
    snprintf(rows[0], sizeof(rows[0]), "Display %s", display[s_display_format]);
    snprintf(rows[1], sizeof(rows[1]), "Print %s", print[s_print_mode]);
    snprintf(rows[2], sizeof(rows[2]), "Angle %s", angle[s_angle_mode]);
    snprintf(rows[3], sizeof(rows[3]), "Graph %s", graph[s_graphing_mode]);
    snprintf(rows[4], sizeof(rows[4]), "Complex %s", complex[s_complex_mode]);

    ui_clear(THEME_BG);
    ui_header(&APPS[APP_CALCULATOR]);
    for (int i = 0; i < 5; i++) {
        uint32_t bg = i == s_mode_selection ? THEME_ACCENT_2 : THEME_SURFACE;
        ui_rect(18, 36 + i * 25, 284, 19, bg);
        ui_text(28, 42 + i * 25, rows[i], THEME_TEXT, 1);
    }
    ui_text(18, 220, "up, down - select  left, right - change", THEME_MUTED, 1);
    ui_present();
}

static bool scripts_scan(void)
{
    if (!usb_msc_mount_app()) {
        snprintf(s_script_status, sizeof(s_script_status), "storage busy - eject USB drive");
        return false;
    }
    s_usb_storage_enabled = false;
    s_script_count = 0;
    DIR *dir = opendir("/data/scripts");
    if (dir == NULL) {
        snprintf(s_script_status, sizeof(s_script_status), "scripts unavailable");
        return false;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL && s_script_count < SCRIPT_MAX) {
        size_t len = strlen(entry->d_name);
        if (len > 3 && strcmp(entry->d_name + len - 3, ".py") == 0) {
            strncpy(s_scripts[s_script_count], entry->d_name, sizeof(s_scripts[s_script_count]) - 1);
            s_scripts[s_script_count][sizeof(s_scripts[s_script_count]) - 1] = '\0';
            s_script_count++;
        }
    }
    closedir(dir);
    if (s_script_selection >= s_script_count) {
        s_script_selection = 0;
    }
    return true;
}

static void script_path_for_name(const char *name, char *path, size_t path_size)
{
    snprintf(path, path_size, "/data/scripts/%s", name);
}

static void open_scripts_browser_for(script_action_t action)
{
    if (!usb_msc_mount_app()) {
        snprintf(s_script_status, sizeof(s_script_status), "storage busy - eject USB drive");
        s_page = PAGE_PROGRAM_MENU;
        s_current_app = APP_PYTHON;
        ui_draw_current();
        return;
    }
    s_usb_storage_enabled = false;
    s_script_action = action;
    scripts_scan();
    s_page = PAGE_SCRIPTS;
    s_current_app = APP_PYTHON;
}

static void open_scripts_browser(void)
{
    open_scripts_browser_for(SCRIPT_ACTION_RUN);
}

static void open_program_menu(void)
{
    s_program_selection = 0;
    s_script_action = SCRIPT_ACTION_RUN;
    s_page = PAGE_PROGRAM_MENU;
    s_current_app = APP_PYTHON;
    ui_draw_current();
}

static bool script_editor_insert_text(const char *text)
{
    if (text == NULL || text[0] == '\0') {
        return false;
    }

    size_t add = strlen(text);
    if (s_script_editor_len + add >= sizeof(s_script_editor)) {
        snprintf(s_script_status, sizeof(s_script_status), "editor full");
        return false;
    }

    memmove(s_script_editor + s_script_editor_cursor + add,
            s_script_editor + s_script_editor_cursor,
            s_script_editor_len - s_script_editor_cursor + 1);
    memcpy(s_script_editor + s_script_editor_cursor, text, add);
    s_script_editor_cursor += add;
    s_script_editor_len += add;
    return true;
}

static void script_editor_delete_before_cursor(void)
{
    if (s_script_editor_cursor == 0 || s_script_editor_len == 0) {
        return;
    }

    memmove(s_script_editor + s_script_editor_cursor - 1,
            s_script_editor + s_script_editor_cursor,
            s_script_editor_len - s_script_editor_cursor + 1);
    s_script_editor_cursor--;
    s_script_editor_len--;
}

static void script_editor_clear(void)
{
    s_script_editor[0] = '\0';
    s_script_editor_len = 0;
    s_script_editor_cursor = 0;
    s_script_editor_scroll_line = 0;
}

static void script_editor_cursor_line_col(int *line, int *col)
{
    int out_line = 0;
    int out_col = 0;
    for (size_t i = 0; i < s_script_editor_cursor && i < s_script_editor_len; i++) {
        if (s_script_editor[i] == '\n') {
            out_line++;
            out_col = 0;
        } else {
            out_col++;
        }
    }
    if (line != NULL) *line = out_line;
    if (col != NULL) *col = out_col;
}

static size_t script_editor_line_col_to_index(int target_line, int target_col)
{
    int line = 0;
    int col = 0;
    size_t i = 0;
    for (; i < s_script_editor_len; i++) {
        if (line == target_line && col >= target_col) {
            break;
        }
        if (s_script_editor[i] == '\n') {
            if (line == target_line) {
                break;
            }
            line++;
            col = 0;
        } else {
            col++;
        }
    }
    return i;
}

static void script_editor_move_vertical(int direction)
{
    int line = 0;
    int col = 0;
    script_editor_cursor_line_col(&line, &col);
    int target_line = line + direction;
    if (target_line < 0) {
        target_line = 0;
    }
    s_script_editor_cursor = script_editor_line_col_to_index(target_line, col);
}

static bool script_editor_save(void)
{
    if (s_script_edit_name[0] == '\0') {
        snprintf(s_script_status, sizeof(s_script_status), "no filename");
        return false;
    }

    if (!usb_msc_mount_app()) {
        snprintf(s_script_status, sizeof(s_script_status), "storage busy - eject USB drive");
        return false;
    }
    s_usb_storage_enabled = false;

    char path[80];
    script_path_for_name(s_script_edit_name, path, sizeof(path));
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        snprintf(s_script_status, sizeof(s_script_status), "save failed");
        return false;
    }

    size_t written = fwrite(s_script_editor, 1, s_script_editor_len, file);
    fclose(file);
    if (written != s_script_editor_len) {
        snprintf(s_script_status, sizeof(s_script_status), "save incomplete");
        return false;
    }

    snprintf(s_script_status, sizeof(s_script_status), "saved %.31s", s_script_edit_name);
    scripts_scan();
    return true;
}

static void script_editor_open_new(void)
{
    if (!usb_msc_mount_app()) {
        snprintf(s_script_status, sizeof(s_script_status), "storage busy - eject USB drive");
        s_page = PAGE_PROGRAM_MENU;
        s_current_app = APP_PYTHON;
        ui_draw_current();
        return;
    }
    s_usb_storage_enabled = false;

    for (int i = 1; i <= 99; i++) {
        char path[64];
        char name[32];
        snprintf(name, sizeof(name), "program%02d.py", i);
        script_path_for_name(name, path, sizeof(path));
        FILE *existing = fopen(path, "r");
        if (existing != NULL) {
            fclose(existing);
            continue;
        }

        snprintf(s_script_edit_name, sizeof(s_script_edit_name), "%s", name);
        snprintf(s_script_editor, sizeof(s_script_editor), "print(%d)\n", i);
        s_script_editor_len = strlen(s_script_editor);
        s_script_editor_cursor = s_script_editor_len;
        s_script_editor_scroll_line = 0;
        snprintf(s_script_status, sizeof(s_script_status), "new - 2nd enter saves");
        s_page = PAGE_SCRIPT_EDITOR;
        s_current_app = APP_PYTHON;
        ui_draw_current();
        return;
    }

    status_message("too many programs");
}

static void script_editor_open_selected(void)
{
    if (s_script_count == 0) {
        snprintf(s_script_status, sizeof(s_script_status), "no scripts");
        ui_draw_current();
        return;
    }

    if (!usb_msc_mount_app()) {
        snprintf(s_script_status, sizeof(s_script_status), "storage busy - eject USB drive");
        ui_draw_current();
        return;
    }
    s_usb_storage_enabled = false;

    char path[80];
    script_path_for_name(s_scripts[s_script_selection], path, sizeof(path));
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        snprintf(s_script_status, sizeof(s_script_status), "open failed");
        ui_draw_current();
        return;
    }

    size_t read = fread(s_script_editor, 1, sizeof(s_script_editor) - 1, file);
    fclose(file);
    s_script_editor[read] = '\0';
    s_script_editor_len = read;
    s_script_editor_cursor = 0;
    s_script_editor_scroll_line = 0;
    snprintf(s_script_edit_name, sizeof(s_script_edit_name), "%s", s_scripts[s_script_selection]);
    snprintf(s_script_status, sizeof(s_script_status), "editing %.31s", s_script_edit_name);
    s_page = PAGE_SCRIPT_EDITOR;
    s_current_app = APP_PYTHON;
    ui_draw_current();
}

static void delete_selected_script(void)
{
    if (s_script_count == 0) {
        snprintf(s_script_status, sizeof(s_script_status), "no scripts");
        ui_draw_current();
        return;
    }

    if (!usb_msc_mount_app()) {
        snprintf(s_script_status, sizeof(s_script_status), "storage busy - eject USB drive");
        ui_draw_current();
        return;
    }
    s_usb_storage_enabled = false;

    char deleted[32];
    char path[80];
    snprintf(deleted, sizeof(deleted), "%s", s_scripts[s_script_selection]);
    script_path_for_name(deleted, path, sizeof(path));
    if (remove(path) != 0) {
        snprintf(s_script_status, sizeof(s_script_status), "delete failed");
    } else {
        snprintf(s_script_status, sizeof(s_script_status), "deleted %.31s", deleted);
    }
    scripts_scan();
    s_page = PAGE_SCRIPTS;
    s_current_app = APP_PYTHON;
    ui_draw_current();
}

static void perform_selected_script_action(void)
{
    if (s_script_action == SCRIPT_ACTION_EDIT) {
        script_editor_open_selected();
    } else if (s_script_action == SCRIPT_ACTION_DELETE) {
        delete_selected_script();
    } else {
        run_selected_script();
    }
}

static void ui_draw_scripts(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_PYTHON]);
    const char *title = s_script_action == SCRIPT_ACTION_EDIT ? "Edit script" :
                        s_script_action == SCRIPT_ACTION_DELETE ? "Delete script" :
                        "Run script";
    const char *footer = s_script_action == SCRIPT_ACTION_EDIT ? "enter - edit  back - menu" :
                         s_script_action == SCRIPT_ACTION_DELETE ? "enter - delete  back - menu" :
                         "enter - run  back - menu";
    ui_text(18, 32, title, THEME_ACCENT, 1);

    if (s_script_count == 0) {
        ui_text(22, 54, "No scripts in /scripts", THEME_TEXT, 1);
        ui_text(22, 76, s_script_action == SCRIPT_ACTION_RUN ? "Add .py over USB" : "Use New program first", THEME_TEXT, 1);
    }

    for (int i = 0; i < s_script_count && i < 6; i++) {
        uint32_t bg = (i == s_script_selection) ? THEME_ACCENT_2 : THEME_SURFACE;
        ui_rect(18, 50 + i * 20, 284, 17, bg);
        ui_text(28, 55 + i * 20, s_scripts[i], THEME_TEXT, 1);
    }
    if (s_script_status[0] != '\0') {
        ui_text(18, 196, s_script_status, THEME_MUTED, 1);
    }
    ui_text(18, 220, footer, THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_program_menu(void)
{
    static const char *const items[PROGRAM_MENU_COUNT] = {
        "Run script",
        "Edit script",
        "New program",
        "Delete script",
    };
    static const char *const detail[PROGRAM_MENU_COUNT] = {
        "Open /scripts and run",
        "Choose a file and edit text",
        "Create and edit a new .py",
        "Choose a file and delete it",
    };

    ui_clear(THEME_BG);
    ui_header(&APPS[APP_PYTHON]);
    for (int i = 0; i < PROGRAM_MENU_COUNT; i++) {
        int y = 40 + i * 34;
        uint32_t bg = i == s_program_selection ? THEME_ACCENT_2 : THEME_SURFACE;
        ui_rect(18, y, 284, 28, bg);
        ui_text(28, y + 5, items[i], THEME_TEXT, 1);
        ui_text(134, y + 5, detail[i], THEME_MUTED, 1);
    }
    ui_text(18, 220, "up, down - select  enter - open", THEME_MUTED, 1);
    ui_present();
}

static void script_output_append(const char *text)
{
    if (text == NULL || text[0] == '\0') {
        return;
    }

    size_t used = strlen(s_script_output);
    size_t add = strlen(text);
    if (add >= sizeof(s_script_output)) {
        text += add - (sizeof(s_script_output) - 1);
        add = strlen(text);
    }
    if (used + add >= sizeof(s_script_output)) {
        size_t keep = sizeof(s_script_output) - add - 1;
        memmove(s_script_output, s_script_output + used - keep, keep + 1);
        used = keep;
    }
    memcpy(s_script_output + used, text, add + 1);
}

static void script_output_callback(const char *text, void *user_data)
{
    (void)user_data;
    script_output_append(text);
    vTaskDelay(pdMS_TO_TICKS(1));
}

static bool script_input_append_text(const char *text)
{
    if (text == NULL || text[0] == '\0') {
        return false;
    }

    size_t add = strlen(text);
    if (s_script_input_len + add >= sizeof(s_script_input)) {
        return false;
    }
    memcpy(s_script_input + s_script_input_len, text, add + 1);
    s_script_input_len += add;
    return true;
}

static void script_input_delete_char(void)
{
    if (s_script_input_len > 0) {
        s_script_input[--s_script_input_len] = '\0';
    }
}

static bool script_input_handle_key(int row, int col, bool *submitted, bool *cancelled)
{
    *submitted = false;
    *cancelled = false;

    if (row == 1 && col == 0) {
        s_second_active = !s_second_active;
        return true;
    }
    if (row == 2 && col == 0) {
        if (s_second_active) {
            s_alpha_locked = !s_alpha_locked;
            s_alpha_active = s_alpha_locked;
            s_second_active = false;
        } else {
            s_alpha_active = !s_alpha_active;
        }
        return true;
    }
    if (row == 2 && col == 2) {
        *cancelled = true;
        return true;
    }
    if (row == 9 && col == 4) {
        *submitted = true;
        return true;
    }
    if (row == 3 && col == 4) {
        if (s_second_active) {
            s_script_input[0] = '\0';
            s_script_input_len = 0;
        } else {
            script_input_delete_char();
        }
        s_second_active = false;
        return true;
    }

    const board_key_t *key = board_keypad_key_at(row, col);
    if ((s_alpha_active || s_alpha_locked) && key != NULL && key->alpha != NULL && key->alpha[0] != '\0') {
        script_input_append_text(key->alpha);
        if (!s_alpha_locked) {
            s_alpha_active = false;
        }
        s_second_active = false;
        return true;
    }

    int digit = digit_for_key(row, col);
    if (digit >= 0) {
        char c[2] = {(char)('0' + digit), '\0'};
        script_input_append_text(c);
        s_second_active = false;
        return true;
    }

    if (row == 9 && col == 2) {
        script_input_append_text(".");
    } else if (row == 9 && col == 3) {
        script_input_append_text("-");
    } else if (row == 5 && col == 4) {
        script_input_append_text("+");
    } else if (row == 7 && col == 4) {
        script_input_append_text("-");
    } else if (row == 6 && col == 4) {
        script_input_append_text("*");
    } else if (row == 8 && col == 4) {
        script_input_append_text("/");
    } else if (row == 5 && col == 2) {
        script_input_append_text("(");
    } else if (row == 5 && col == 3) {
        script_input_append_text(")");
    } else if (row == 5 && col == 1) {
        script_input_append_text(",");
    } else if (row == 2 && col == 1) {
        script_input_append_text("x");
    } else {
        return false;
    }

    s_second_active = false;
    return true;
}

static int script_input_callback(char *buffer, size_t buffer_size, void *user_data)
{
    (void)user_data;
    if (buffer == NULL || buffer_size == 0) {
        return 0;
    }

    s_script_input_active = true;
    s_script_input[0] = '\0';
    s_script_input_len = 0;
    snprintf(s_script_status, sizeof(s_script_status), "type input, enter - submit");
    ui_draw_current();

    bool pressed_last[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS] = {0};
    while (true) {
        int queued = 0;
        if (s_serial_button_queue != NULL && xQueueReceive(s_serial_button_queue, &queued, 0) == pdTRUE) {
            if (queued >= 1 && queued <= BOARD_KEYPAD_ROWS * BOARD_KEYPAD_COLS) {
                int index = queued - 1;
                bool submitted = false;
                bool cancelled = false;
                if (script_input_handle_key(index / BOARD_KEYPAD_COLS,
                                            index % BOARD_KEYPAD_COLS,
                                            &submitted,
                                            &cancelled)) {
                    ui_draw_current();
                    if (submitted || cancelled) {
                        s_script_input_active = false;
                        snprintf(s_script_status, sizeof(s_script_status), cancelled ? "input cancelled" : "running...");
                        if (cancelled) {
                            return 0;
                        }
                        snprintf(buffer, buffer_size, "%s", s_script_input);
                        script_output_append(s_script_input);
                        script_output_append("\n");
                        return 1;
                    }
                }
            }
        }

        bool pressed[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS];
        board_keypad_scan_matrix(pressed);
        for (int row = 0; row < BOARD_KEYPAD_ROWS; row++) {
            for (int col = 0; col < BOARD_KEYPAD_COLS; col++) {
                if (!pressed[row][col] || pressed_last[row][col]) {
                    continue;
                }

                bool submitted = false;
                bool cancelled = false;
                if (script_input_handle_key(row, col, &submitted, &cancelled)) {
                    ui_draw_current();
                    if (submitted || cancelled) {
                        s_script_input_active = false;
                        snprintf(s_script_status, sizeof(s_script_status), cancelled ? "input cancelled" : "running...");
                        if (cancelled) {
                            return 0;
                        }
                        snprintf(buffer, buffer_size, "%s", s_script_input);
                        script_output_append(s_script_input);
                        script_output_append("\n");
                        return 1;
                    }
                }
            }
        }
        memcpy(pressed_last, pressed, sizeof(pressed));
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

static void ui_draw_script_io(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_PYTHON]);

    ui_text(10, 32, s_script_title, THEME_ACCENT, 1);
    ui_rect(8, 46, UI_W - 16, 1, THEME_BORDER);

    char lines[13][48];
    int line_count = 1;
    int col = 0;
    memset(lines, 0, sizeof(lines));
    for (size_t i = 0; s_script_output[i] != '\0'; i++) {
        char c = s_script_output[i];
        if (c == '\r') {
            continue;
        }
        if (c == '\n' || col >= 45) {
            if (line_count == 13) {
                memmove(lines, lines + 1, sizeof(lines[0]) * 12);
                memset(lines[12], 0, sizeof(lines[12]));
                line_count = 12;
            }
            line_count++;
            col = 0;
            if (c == '\n') {
                continue;
            }
        }
        lines[line_count - 1][col++] = c;
    }

    for (int i = 0; i < line_count && i < 13; i++) {
        ui_text(10, 54 + i * 12, lines[i], THEME_TEXT, 1);
    }

    if (s_script_input_active) {
        ui_rect(8, 196, UI_W - 16, 18, THEME_SURFACE_2);
        ui_text(12, 202, "> ", THEME_ACCENT, 1);
        ui_text(24, 202, s_script_input, THEME_TEXT, 1);
        ui_draw_calc_cursor(24 + ui_text_width(s_script_input, 80, 1), 202, false);
    }

    ui_rect(0, 220, UI_W, 20, THEME_HEADER);
    ui_text(8, 226, s_script_status, s_script_running ? THEME_BATTERY_YELLOW : THEME_TEXT, 1);
    ui_present();
}

static void ui_draw_script_editor(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_PYTHON]);

    int cursor_line = 0;
    int cursor_col = 0;
    script_editor_cursor_line_col(&cursor_line, &cursor_col);
    if (cursor_line < s_script_editor_scroll_line) {
        s_script_editor_scroll_line = cursor_line;
    }
    if (cursor_line >= s_script_editor_scroll_line + 13) {
        s_script_editor_scroll_line = cursor_line - 12;
    }

    ui_text(10, 32, s_script_edit_name[0] ? s_script_edit_name : "new script", THEME_ACCENT, 1);
    ui_rect(8, 46, UI_W - 16, 1, THEME_BORDER);

    size_t index = 0;
    int line = 0;
    while (line < s_script_editor_scroll_line && index < s_script_editor_len) {
        if (s_script_editor[index++] == '\n') {
            line++;
        }
    }

    char lines[13][48];
    memset(lines, 0, sizeof(lines));
    for (int row = 0; row < 13; row++) {
        int col = 0;
        while (index < s_script_editor_len && s_script_editor[index] != '\n') {
            if (col < (int)sizeof(lines[row]) - 1) {
                lines[row][col++] = s_script_editor[index];
            }
            index++;
        }
        if (index < s_script_editor_len && s_script_editor[index] == '\n') {
            index++;
        }
    }

    for (int row = 0; row < 13; row++) {
        ui_text(10, 54 + row * 12, lines[row], THEME_TEXT, 1);
    }

    int visible_line = cursor_line - s_script_editor_scroll_line;
    if (visible_line >= 0 && visible_line < 13) {
        int visible_col = cursor_col > 45 ? 45 : cursor_col;
        ui_draw_calc_cursor(10 + visible_col * 6, 54 + visible_line * 12, false);
    }

    ui_rect(0, 220, UI_W, 20, THEME_HEADER);
    ui_text(8, 226, "enter-newline  2nd enter-save  back-exit", THEME_MUTED, 1);
    if (s_script_status[0] != '\0') {
        ui_text(10, 208, s_script_status, THEME_MUTED, 1);
    }
    ui_present();
}

static bool script_editor_handle_key(int row, int col)
{
    if (row == 2 && col == 2) {
        scripts_scan();
        s_page = PAGE_SCRIPTS;
        s_current_app = APP_PYTHON;
        ui_draw_current();
        return true;
    }
    if (row == 1 && col == 3) {
        if (s_script_editor_cursor > 0) {
            s_script_editor_cursor--;
        }
        ui_draw_current();
        return true;
    }
    if (row == 2 && col == 4) {
        if (s_script_editor_cursor < s_script_editor_len) {
            s_script_editor_cursor++;
        }
        ui_draw_current();
        return true;
    }
    if (row == 1 && col == 4) {
        script_editor_move_vertical(-1);
        ui_draw_current();
        return true;
    }
    if (row == 2 && col == 3) {
        script_editor_move_vertical(1);
        ui_draw_current();
        return true;
    }
    if (row == 9 && col == 4) {
        if (s_second_active) {
            if (script_editor_save()) {
                s_page = PAGE_SCRIPTS;
                s_current_app = APP_PYTHON;
            }
        } else {
            script_editor_insert_text("\n");
        }
        ui_draw_current();
        return true;
    }
    if (row == 3 && col == 4) {
        if (s_second_active) {
            script_editor_clear();
        } else {
            script_editor_delete_before_cursor();
        }
        ui_draw_current();
        return true;
    }

    const board_key_t *key = board_keypad_key_at(row, col);
    if ((s_alpha_active || s_alpha_locked) && key != NULL && key->alpha != NULL && key->alpha[0] != '\0') {
        script_editor_insert_text(key->alpha);
        if (!s_alpha_locked) {
            s_alpha_active = false;
        }
        ui_draw_current();
        return true;
    }

    int digit = digit_for_key(row, col);
    if (digit >= 0) {
        char text[2] = {(char)('0' + digit), '\0'};
        script_editor_insert_text(text);
        ui_draw_current();
        return true;
    }

    const char *insert = NULL;
    if (row == 9 && col == 2) insert = s_second_active ? "\"" : ".";
    else if (row == 9 && col == 3) insert = s_second_active ? "'" : "-";
    else if (row == 5 && col == 4) insert = "+";
    else if (row == 7 && col == 4) insert = "-";
    else if (row == 6 && col == 4) insert = "*";
    else if (row == 8 && col == 4) insert = "/";
    else if (row == 5 && col == 2) insert = s_second_active ? "[" : "(";
    else if (row == 5 && col == 3) insert = s_second_active ? "]" : ")";
    else if (row == 5 && col == 1) insert = s_second_active ? " " : ",";
    else if (row == 2 && col == 1) insert = "x";
    else if (row == 4 && col == 1) insert = s_second_active ? "csc(" : "sin(";
    else if (row == 4 && col == 2) insert = s_second_active ? "sec(" : "cos(";
    else if (row == 4 && col == 3) insert = s_second_active ? "cot(" : "tan(";
    else if (row == 4 && col == 4) insert = s_second_active ? "e" : "pi";
    else if (row == 6 && col == 0) insert = "log(";
    else if (row == 7 && col == 0) insert = "ln(";
    else if (row == 3 && col == 1) insert = "/";

    if (insert == NULL) {
        return false;
    }

    script_editor_insert_text(insert);
    ui_draw_current();
    return true;
}

static void calc_history_push(const char *expr, const char *result)
{
    if (expr == NULL || expr[0] == '\0' || result == NULL) {
        return;
    }

    if (s_calc_history_count == CALC_HISTORY_MAX) {
        memmove(s_calc_history_expr,
                s_calc_history_expr + 1,
                sizeof(s_calc_history_expr[0]) * (CALC_HISTORY_MAX - 1));
        memmove(s_calc_history_result,
                s_calc_history_result + 1,
                sizeof(s_calc_history_result[0]) * (CALC_HISTORY_MAX - 1));
        s_calc_history_count--;
    }

    snprintf(s_calc_history_expr[s_calc_history_count], sizeof(s_calc_history_expr[0]), "%s", expr);
    snprintf(s_calc_history_result[s_calc_history_count], sizeof(s_calc_history_result[0]), "%s", result);
    s_calc_history_count++;
    s_calc_history_selected = -1;
    s_calc_history_select_answer = false;
}

static void factory_reset_runtime_state(void)
{
    s_current_app = APP_CALCULATOR;
    s_home_selection = APP_CALCULATOR;
    s_script_selection = 0;
    s_app_selection = 0;
    s_math_tab = 0;
    s_math_selection = 0;
    s_calc_input[0] = '\0';
    s_calc_cursor = 0;
    snprintf(s_calc_output, sizeof(s_calc_output), "0");
    snprintf(s_calc_ans, sizeof(s_calc_ans), "0");
    memset(s_calc_history_expr, 0, sizeof(s_calc_history_expr));
    memset(s_calc_history_result, 0, sizeof(s_calc_history_result));
    s_calc_history_count = 0;
    s_calc_history_selected = -1;
    s_calc_history_select_answer = false;
    memset(s_lists, 0, sizeof(s_lists));
    memset(s_list_counts, 0, sizeof(s_list_counts));
    s_list_index = 0;
    s_list_cursor = 0;
    s_list_entry[0] = '\0';
    s_mode_selection = 0;
    s_display_format = 0;
    s_print_mode = 0;
    s_angle_mode = 0;
    s_graphing_mode = 0;
    s_complex_mode = 0;
    memset(s_matrix_a, 0, sizeof(s_matrix_a));
    s_matrix_rows = 0;
    s_matrix_cols = 0;
    snprintf(s_solver_e1, sizeof(s_solver_e1), "x^2");
    snprintf(s_solver_e2, sizeof(s_solver_e2), "4");
    s_solver_guess = 1.0;
    s_solver_result = 0.0;
    s_solver_has_result = false;
    s_fin_n = 12.0;
    s_fin_i = 5.0;
    s_fin_pv = 1000.0;
    s_fin_pmt = 0.0;
    s_fin_fv = 0.0;
    s_fin_py = 12.0;
    s_fin_cy = 12.0;
    s_fin_begin = false;
    s_fin_selection = 3;
    s_conic_type = 0;
    s_conic_h = 0.0;
    s_conic_k = 0.0;
    s_conic_a = 1.0;
    s_conic_b = 1.0;
    s_conic_r = 5.0;
    opencalc_math_set_degrees(true);

    snprintf(s_graph_exprs[0], sizeof(s_graph_exprs[0]), "x");
    for (int i = 1; i < 10; i++) {
        s_graph_exprs[i][0] = '\0';
    }
    for (int i = 0; i < 10; i++) {
        s_graph_enabled[i] = i == 0;
    }
    s_graph_selection = 0;
    s_graph_xmin = -10.0;
    s_graph_xmax = 10.0;
    s_graph_ymin = -6.0;
    s_graph_ymax = 6.0;
    s_graph_xtick = 1.0;
    s_graph_ytick = 1.0;
    s_graph_window_selection = 0;
    s_graph_calc_selection = 0;
    s_graph_status[0] = '\0';
    s_graph_zoom_mode = false;
    s_graph_grid = true;
    inequality_clear_all();
    s_graph_trace = false;
    s_graph_trace_x = 0.0;

    s_second_active = false;
    s_alpha_active = false;
    s_alpha_locked = false;
    s_sleep_enabled = true;
    s_power_save_enabled = false;
    s_light_mode = false;
    s_doom_high_score = 0;
    s_doom_last_saved_high_score = 0;
    opencalc_persist_factory_reset();
    board_set_backlight_brightness(80);
    opencalc_persist_set_u32("brightness", 80);
    opencalc_persist_set_u32("auto_sleep", 1);
    opencalc_persist_set_u32("power_save", 0);
    opencalc_persist_set_u32("light_mode", 0);
    opencalc_power_set_power_save(false);
    s_page = PAGE_CALCULATOR;
    printf("factory reset runtime state\n");
    ui_draw_current();
}

static void ui_draw_calculator(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_CALCULATOR]);

    const int first_y = 36;
    const int row_h = 42;
    const int visible_rows = 4;
    int start = s_calc_history_count > visible_rows ? s_calc_history_count - visible_rows : 0;
    if (s_calc_history_selected >= 0 && s_calc_history_selected < start) {
        start = s_calc_history_selected;
    }
    for (int i = start; i < s_calc_history_count; i++) {
        int visible = i - start;
        int y = first_y + visible * row_h;
        bool selected = i == s_calc_history_selected;
        uint32_t expression_color = selected && !s_calc_history_select_answer ? THEME_ACCENT : THEME_TEXT;
        uint32_t result_color = selected && s_calc_history_select_answer ? THEME_ACCENT : THEME_TEXT;

        int expression_w = ui_math_text(0, 0, s_calc_history_expr[i], expression_color, false);
        if (selected && !s_calc_history_select_answer) {
            ui_rect(8, y - 5, expression_w + 4, 22, THEME_ACCENT_2);
        }
        ui_math_text(10, y + 2, s_calc_history_expr[i], expression_color, true);
        int result_w = ui_math_text(0, 0, s_calc_history_result[i], result_color, false);
        int result_x = UI_W - 10 - result_w;
        if (result_x < 120) {
            result_x = 120;
        }
        if (selected && s_calc_history_select_answer) {
            ui_rect(result_x - 2, y + 15, result_w + 4, 22, THEME_ACCENT_2);
        }
        ui_math_text(result_x, y + 22, s_calc_history_result[i], result_color, true);
    }

    ui_rect(0, 202, UI_W, 1, THEME_BORDER);
    ui_draw_calc_expression(10, 214, s_calc_input, s_calc_cursor);
    ui_present();
}

static int math_menu_count(int tab)
{
    int count = 0;
    while (count < 10 && MATH_MENU[tab][count].label != NULL) {
        count++;
    }
    return count;
}

static void ui_draw_math_menu(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_CALCULATOR]);

    for (int i = 0; i < 4; i++) {
        int x = 14 + i * 74;
        uint32_t bg = (i == s_math_tab) ? THEME_ACCENT : THEME_SURFACE;
        uint32_t fg = THEME_TEXT;
        ui_rect(x, 32, 66, 18, bg);
        ui_text(x + 12, 38, MATH_TAB_NAMES[i], fg, 1);
    }

    int count = math_menu_count(s_math_tab);
    for (int i = 0; i < count; i++) {
        uint32_t bg = (i == s_math_selection) ? THEME_ACCENT_2 : THEME_SURFACE;
        ui_rect(18, 60 + i * 15, 284, 13, bg);
        char line[48];
        snprintf(line, sizeof(line), "%d %s", i + 1, MATH_MENU[s_math_tab][i].label);
        ui_text(26, 62 + i * 15, line, THEME_TEXT, 1);
    }

    ui_text(16, 220, "left, right - tabs  enter - insert", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_graph_calc_menu(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_GRAPH]);

    ui_text(18, 36, "Graph Calc", THEME_TEXT, 1);
    for (int i = 0; i < GRAPH_CALC_COUNT; i++) {
        uint32_t bg = (i == s_graph_calc_selection) ? THEME_ACCENT_2 : THEME_SURFACE;
        ui_rect(18, 56 + i * 17, 284, 14, bg);
        char line[48];
        snprintf(line, sizeof(line), "%d %s", i + 1, GRAPH_CALC_MENU[i]);
        ui_text(26, 59 + i * 17, line, THEME_TEXT, 1);
    }

    ui_text(16, 220, "number/enter - run  del - back", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_graph(void)
{
    ui_clear(THEME_BG);

    const int top = GRAPH_TOP;
    const int bottom = GRAPH_BOTTOM;
    graph_view_t view = current_graph_view();
    double xtick = s_graph_xtick > 0.0 ? s_graph_xtick : 1.0;
    double ytick = s_graph_ytick > 0.0 ? s_graph_ytick : 1.0;

    for (double gx = ceil(s_graph_xmin / xtick) * xtick; gx <= s_graph_xmax + xtick * 0.25; gx += xtick) {
        bool axis = fabs(gx) < xtick * 0.001;
        if (!s_graph_grid && !axis) {
            continue;
        }
        int sx = graph_screen_x(&view, gx);
        uint32_t color = axis ? THEME_BORDER : THEME_GRID;
        ui_line(sx, top, sx, bottom, color);
    }
    for (double gy = ceil(s_graph_ymin / ytick) * ytick; gy <= s_graph_ymax + ytick * 0.25; gy += ytick) {
        bool axis = fabs(gy) < ytick * 0.001;
        if (!s_graph_grid && !axis) {
            continue;
        }
        int sy = graph_screen_y(&view, gy);
        uint32_t color = axis ? THEME_BORDER : THEME_GRID;
        ui_line(0, sy, UI_W - 1, sy, color);
    }

    draw_inequality_layer(&view);

    for (int fn = 0; fn < 10; fn++) {
        if (!s_graph_enabled[fn] || s_graph_exprs[fn][0] == '\0') {
            continue;
        }

        bool have_prev = false;
        int prev_x = 0;
        int prev_y = 0;
        for (int px = 0; px < UI_W; px++) {
            double x = graph_world_x(&view, px);
            double y = 0.0;
            if (!graph_eval_expression(s_graph_exprs[fn], x, &y)) {
                have_prev = false;
                continue;
            }

            int py = graph_screen_y(&view, y);
            if (py < top - 40 || py > bottom + 40) {
                have_prev = false;
                continue;
            }
            if (have_prev) {
                ui_line(prev_x, prev_y, px, py, s_graph_colors[fn]);
            }
            prev_x = px;
            prev_y = py;
            have_prev = true;
        }
    }

    if (s_graph_trace) {
        graph_poi_t pois[GRAPH_POI_LIMIT];
        int poi_count = graph_collect_pois(pois, GRAPH_POI_LIMIT);
        int nearest_poi = -1;
        double nearest_dist = 0.0;
        for (int i = 0; i < poi_count; i++) {
            int px = graph_screen_x(&view, pois[i].x);
            int py = graph_screen_y(&view, pois[i].y);
            if (px < 0 || px >= UI_W || py < top || py > bottom) {
                continue;
            }
            uint32_t color = pois[i].type == GRAPH_POI_INTERSECTION ? THEME_BATTERY_YELLOW : THEME_TEXT;
            ui_line(px - 3, py, px + 3, py, color);
            ui_line(px, py - 3, px, py + 3, color);
            double dist = fabs(pois[i].x - s_graph_trace_x);
            if (nearest_poi < 0 || dist < nearest_dist) {
                nearest_poi = i;
                nearest_dist = dist;
            }
        }

        int tx = graph_screen_x(&view, s_graph_trace_x);
        ui_line(tx, top, tx, bottom, THEME_TEXT);
        for (int offset = 0; offset < 10; offset++) {
            int fn = (s_graph_trace_fn + offset) % 10;
            double y = 0.0;
            if (s_graph_enabled[fn] && graph_eval_expression(s_graph_exprs[fn], s_graph_trace_x, &y)) {
                int ty = graph_screen_y(&view, y);
                ui_rect(tx - 2, ty - 2, 5, 5, THEME_TEXT);
                char buf[64];
                if (nearest_poi >= 0 && fabs(pois[nearest_poi].x - s_graph_trace_x) < (s_graph_xmax - s_graph_xmin) / 40.0) {
                    snprintf(buf, sizeof(buf), "%s x %.2f y %.2f",
                             graph_poi_label(pois[nearest_poi].type),
                             pois[nearest_poi].x,
                             pois[nearest_poi].y);
                } else {
                    snprintf(buf, sizeof(buf), "Y%d x %.2f y %.2f", fn + 1, s_graph_trace_x, y);
                }
                ui_text(6, 6, buf, THEME_TEXT, 1);
                break;
            }
        }
    }

    ui_rect(0, 229, UI_W, 11, THEME_HEADER);
    ui_text(4, 232,
            s_graph_status[0] != '\0' ? s_graph_status :
                (s_graph_zoom_mode ? "zoom: up - in  down - out  zoom - done" :
                    "y= funcs  window - set  zoom - mode  trace - cursor"),
            THEME_TEXT, 1);

    ui_present();
}

static void ui_draw_graph_window(void)
{
    const char *labels[] = {"x max", "y max", "x tick", "y tick"};
    double values[] = {
        fabs(s_graph_xmax),
        fabs(s_graph_ymax),
        s_graph_xtick,
        s_graph_ytick,
    };

    ui_clear(THEME_BG);
    ui_header(&APPS[APP_GRAPH]);

    for (int i = 0; i < 4; i++) {
        char line[48];
        uint32_t bg = i == s_graph_window_selection ? THEME_ACCENT_2 : THEME_SURFACE;
        snprintf(line, sizeof(line), "%s %.2f", labels[i], values[i]);
        ui_rect(18, 42 + i * 24, 284, 18, bg);
        ui_text(28, 48 + i * 24, line, THEME_TEXT, 1);
    }

    ui_text(18, 220, "up, down - select  left, right - set", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_y_equals(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_GRAPH]);
    ui_text(18, 32, "Y=", THEME_TEXT, 2);
    for (int i = 0; i < 10; i++) {
        int y = 55 + i * 14;
        uint32_t bg = (i == s_graph_selection) ? THEME_ACCENT_2 : THEME_SURFACE;
        ui_rect(14, y - 2, 292, 13, bg);
        ui_rect(20, y + 1, 8, 8, s_graph_enabled[i] ? s_graph_colors[i] : THEME_BORDER);
        char line[112];
        snprintf(line, sizeof(line), "Y%d=%s", i + 1, s_graph_exprs[i][0] ? s_graph_exprs[i] : "-");
        ui_text(36, y, line, THEME_TEXT, 1);
    }
    ui_text(16, 220, "enter - toggle  graph - draw", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_table(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_TABLE]);
    ui_text(10, 31, "x", THEME_TEXT, 1);
    static const char *headers[] = {"Y1", "Y2", "Y3", "Y4", "Y5", "Y6", "Y7", "Y8", "Y9", "Y10"};
    for (int fn = 0; fn < 3; fn++) {
        int graph_fn = s_table_func_start + fn;
        if (graph_fn < 10) {
            ui_text(70 + fn * 75, 31, headers[graph_fn], s_graph_colors[graph_fn], 1);
        }
    }

    for (int i = 0; i < 9; i++) {
        double x = s_table_x_start + i;
        int y = 50 + i * 15;
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f", x);
        ui_text(10, y, buf, THEME_TEXT, 1);
        for (int fn = 0; fn < 3; fn++) {
            int graph_fn = s_table_func_start + fn;
            if (graph_fn >= 10) {
                continue;
            }
            double value = 0.0;
            if (s_graph_enabled[graph_fn] && s_graph_exprs[graph_fn][0] != '\0' &&
                graph_eval_expression(s_graph_exprs[graph_fn], x, &value)) {
                snprintf(buf, sizeof(buf), "%.2f", value);
                ui_text(70 + fn * 75, y, buf, THEME_TEXT, 1);
            }
        }
    }
    ui_text(8, 220, "up, down - x scroll  left, right - y cols", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_game_menu(void);

static void ui_draw_current(void)
{
    if (s_active_game != GAME_NONE) {
        return;
    }

    switch (s_page) {
    case PAGE_HOME: ui_draw_home(); break;
    case PAGE_SETTINGS: ui_draw_settings(); break;
    case PAGE_SCRIPTS: ui_draw_scripts(); break;
    case PAGE_CALCULATOR: ui_draw_calculator(); break;
    case PAGE_MATH_MENU: ui_draw_math_menu(); break;
    case PAGE_GRAPH: ui_draw_graph(); break;
    case PAGE_Y_EQUALS: ui_draw_y_equals(); break;
    case PAGE_TABLE: ui_draw_table(); break;
    case PAGE_GRAPH_WINDOW: ui_draw_graph_window(); break;
    case PAGE_GRAPH_CALC: ui_draw_graph_calc_menu(); break;
    case PAGE_LIST_EDITOR: ui_draw_list_editor(); break;
    case PAGE_MODE_MENU: ui_draw_mode_menu(); break;
    case PAGE_PROGRAM_MENU: ui_draw_program_menu(); break;
    case PAGE_GAME_MENU: ui_draw_game_menu(); break;
    case PAGE_SCRIPT_IO: ui_draw_script_io(); break;
    case PAGE_SCRIPT_EDITOR: ui_draw_script_editor(); break;
    case PAGE_APP: ui_draw_app_page(s_current_app); break;
    }
}

static const char *game_name(game_id_t game)
{
    switch (game) {
    case GAME_TETRIS: return "Tetris";
    case GAME_DOOM: return "Doom";
    case GAME_SNAKE: return "Snake";
    case GAME_BREAKOUT: return "Breakout";
    case GAME_MARIO: return "Mario";
    default: return "";
    }
}

static const char *game_key(game_id_t game)
{
    switch (game) {
    case GAME_TETRIS: return "hs_tetris";
    case GAME_DOOM: return "hs_doom";
    case GAME_SNAKE: return "hs_snake";
    case GAME_BREAKOUT: return "hs_breakout";
    case GAME_MARIO: return "hs_mario";
    default: return "";
    }
}

static game_id_t selected_game_id(void)
{
    static const game_id_t games[] = {
        GAME_TETRIS,
        GAME_DOOM,
        GAME_SNAKE,
        GAME_BREAKOUT,
        GAME_MARIO,
    };
    if (s_game_selection < 0) s_game_selection = 0;
    if (s_game_selection >= (int)(sizeof(games) / sizeof(games[0]))) {
        s_game_selection = (int)(sizeof(games) / sizeof(games[0])) - 1;
    }
    return games[s_game_selection];
}

static void ui_draw_game_menu(void)
{
    static const game_id_t games[] = {
        GAME_TETRIS,
        GAME_DOOM,
        GAME_SNAKE,
        GAME_BREAKOUT,
        GAME_MARIO,
    };

    ui_clear(THEME_BG);
    ui_rect(0, 0, UI_W, 22, THEME_HEADER);
    ui_text(8, 7, "Games", THEME_HEADER_TEXT, 1);
    ui_text(225, 7, "high score", THEME_HEADER_TEXT, 1);

    for (int i = 0; i < (int)(sizeof(games) / sizeof(games[0])); i++) {
        int y = 34 + i * 34;
        uint32_t bg = i == s_game_selection ? THEME_ACCENT_2 : THEME_SURFACE;
        uint32_t border = i == s_game_selection ? THEME_ACCENT : THEME_BORDER;
        ui_rect(12, y, 296, 27, bg);
        ui_border(12, y, 296, 27, border);
        ui_text(22, y + 9, game_name(games[i]), THEME_TEXT, 1);

        char score[24];
        snprintf(score, sizeof(score), "%lu",
                 (unsigned long)opencalc_persist_get_u32(game_key(games[i]), 0));
        ui_text(236, y + 9, score, THEME_TEXT, 1);
    }

    ui_text(12, 218, "enter - play  back - home", THEME_MUTED, 1);
    ui_present();
}

static void ui_open_selected_app(void)
{
    s_current_app = (app_id_t)s_home_selection;
    s_app_selection = 0;
    if (s_current_app == APP_SETTINGS) {
        s_page = PAGE_SETTINGS;
        s_script_selection = 0;
    } else if (s_current_app == APP_PYTHON) {
        s_program_selection = 0;
        s_page = PAGE_PROGRAM_MENU;
    } else if (s_current_app == APP_CALCULATOR) {
        s_page = PAGE_CALCULATOR;
    } else if (s_current_app == APP_GRAPH) {
        reset_graph_view_defaults();
        s_page = PAGE_GRAPH;
    } else if (s_current_app == APP_TABLE) {
        s_page = PAGE_TABLE;
    } else {
        s_page = PAGE_APP;
    }
    ui_draw_current();
}

static void save_doom_high_score(void)
{
    long score = opencalc_doom_score();
    if (score <= 0) {
        return;
    }
    if ((uint32_t)score > s_doom_high_score) {
        s_doom_high_score = (uint32_t)score;
    }
    if (s_doom_high_score > s_doom_last_saved_high_score) {
        opencalc_persist_set_u32("hs_doom", s_doom_high_score);
        s_doom_last_saved_high_score = s_doom_high_score;
    }
}

static void close_active_game_to_menu(void)
{
    if (s_active_game == GAME_DOOM) {
        save_doom_high_score();
    }
    if (s_active_game == GAME_TETRIS) opencalc_tetris_press_button_number(46);
    if (s_active_game == GAME_SNAKE) opencalc_snake_press_button_number(46);
    if (s_active_game == GAME_BREAKOUT) opencalc_breakout_press_button_number(46);
    if (s_active_game == GAME_MARIO) opencalc_mario_press_button_number(46);

    s_active_game = GAME_NONE;
    vTaskDelay(pdMS_TO_TICKS(30));
#if OPENCALC_EXPORT_USB_STORAGE_TO_HOST
    usb_msc_mount_usb();
#endif
    s_page = PAGE_GAME_MENU;
    ui_draw_current();
}

static void open_game_menu(void)
{
    if (s_active_game != GAME_NONE) {
        close_active_game_to_menu();
        return;
    }
    s_game_selection = 0;
    s_page = PAGE_GAME_MENU;
    ui_draw_current();
}

static void launch_selected_game(void)
{
    game_id_t game = selected_game_id();

    if (game == GAME_TETRIS) {
        opencalc_tetris_enter();
        s_active_game = GAME_TETRIS;
        printf("tetris on\n");
        return;
    }
    if (game == GAME_SNAKE) {
        opencalc_snake_enter();
        s_active_game = GAME_SNAKE;
        printf("snake on\n");
        return;
    }
    if (game == GAME_BREAKOUT) {
        opencalc_breakout_enter();
        s_active_game = GAME_BREAKOUT;
        printf("breakout on\n");
        return;
    }

    if (game == GAME_DOOM) {
#if OPENCALC_ENABLE_DOOM
        if (!usb_msc_mount_app()) {
            printf("Doom could not take ownership of USB storage\n");
            s_page = PAGE_GAME_MENU;
            ui_draw_current();
            return;
        }
        if (!opencalc_doom_wad_available()) {
            printf("Doom WAD missing: copy doom1.wad to the USB drive root\n");
            s_page = PAGE_GAME_MENU;
            ui_draw_current();
#if OPENCALC_EXPORT_USB_STORAGE_TO_HOST
            usb_msc_mount_usb();
#endif
            return;
        }
        if (opencalc_doom_start()) {
            s_active_game = GAME_DOOM;
            printf("doom on\n");
            return;
        }
        printf("Doom initialization failed; see resource logs above\n");
        s_page = PAGE_GAME_MENU;
        ui_draw_current();
#if OPENCALC_EXPORT_USB_STORAGE_TO_HOST
        usb_msc_mount_usb();
#endif
#else
        printf("doom disabled\n");
#endif
        return;
    }

    if (game == GAME_MARIO) {
        usb_msc_mount_app();
        if (!opencalc_mario_rom_available()) {
            printf("Mario ROM missing: copy mario.nes to the USB drive root\n");
            s_page = PAGE_GAME_MENU;
            ui_draw_current();
#if OPENCALC_EXPORT_USB_STORAGE_TO_HOST
            usb_msc_mount_usb();
#endif
            return;
        }
        opencalc_mario_enter();
        s_active_game = GAME_MARIO;
        printf("mario on\n");
        return;
    }
}

static void power_off_calculator(void)
{
    printf("software off\n");
    board_enter_deep_sleep();
    ui_draw_current();
}

static void apply_power_save_mode(bool enabled)
{
    if (enabled == s_power_save_enabled && opencalc_power_get_power_save() == enabled) {
        return;
    }

    if (enabled) {
        s_power_save_saved_brightness = board_get_backlight_brightness();
        if (s_power_save_saved_brightness <= 0) {
            s_power_save_saved_brightness = (int)opencalc_persist_get_u32("brightness", 80);
        }
        if (board_get_backlight_brightness() > OPENCALC_POWER_SAVE_BRIGHTNESS_CAP_PERCENT) {
            board_set_backlight_brightness(OPENCALC_POWER_SAVE_BRIGHTNESS_CAP_PERCENT);
        }
    } else {
        board_set_backlight_brightness(s_power_save_saved_brightness);
        opencalc_persist_set_u32("brightness", (uint32_t)board_get_backlight_brightness());
    }

    s_power_save_enabled = enabled;
    opencalc_power_set_power_save(enabled);
    opencalc_persist_set_u32("power_save", enabled ? 1 : 0);
}

static void adjust_brightness(int delta)
{
    int next = board_get_backlight_brightness() + delta;
    board_set_backlight_brightness(next);
    if (s_power_save_enabled && board_get_backlight_brightness() > OPENCALC_POWER_SAVE_BRIGHTNESS_CAP_PERCENT) {
        board_set_backlight_brightness(OPENCALC_POWER_SAVE_BRIGHTNESS_CAP_PERCENT);
    }
    if (s_power_save_enabled) {
        s_power_save_saved_brightness = board_get_backlight_brightness();
    }
    opencalc_persist_set_u32("brightness", (uint32_t)board_get_backlight_brightness());
    printf("brightness %d%%\n", board_get_backlight_brightness());
    ui_draw_current();
}

static void status_message(const char *text)
{
    snprintf(s_calc_output, sizeof(s_calc_output), "%s", text);
    printf("%s\n", s_calc_output);
    ui_draw_current();
}

static void reset_graph_view_defaults(void)
{
    s_graph_xmin = -10.0;
    s_graph_xmax = 10.0;
    s_graph_ymin = -6.0;
    s_graph_ymax = 6.0;
    s_graph_xtick = 1.0;
    s_graph_ytick = 1.0;
    s_graph_trace = false;
    s_graph_trace_x = 0.0;
    s_graph_zoom_mode = false;
    s_graph_window_selection = 0;
    s_graph_calc_selection = 0;
    s_graph_status[0] = '\0';
    s_table_x_start = 0.0;
    s_table_func_start = 0;
}

static void open_graph_window_menu(void)
{
    s_page = PAGE_GRAPH_WINDOW;
    s_current_app = APP_GRAPH;
    s_graph_zoom_mode = false;
    ui_draw_current();
}

static void open_graph_calc_menu(void)
{
    s_page = PAGE_GRAPH_CALC;
    s_current_app = APP_GRAPH;
    s_graph_zoom_mode = false;
    ui_draw_current();
}

static void apply_symmetric_graph_window(double xmax, double ymax)
{
    if (xmax < 1.0) {
        xmax = 1.0;
    }
    if (ymax < 1.0) {
        ymax = 1.0;
    }

    s_graph_xmin = -xmax;
    s_graph_xmax = xmax;
    s_graph_ymin = -ymax;
    s_graph_ymax = ymax;
}

static void adjust_graph_window_value(double delta)
{
    double xmax = fabs(s_graph_xmax);
    double ymax = fabs(s_graph_ymax);

    switch (s_graph_window_selection) {
    case 0:
        apply_symmetric_graph_window(xmax + delta, ymax);
        break;
    case 1:
        apply_symmetric_graph_window(xmax, ymax + delta);
        break;
    case 2:
        s_graph_xtick += delta;
        if (s_graph_xtick < 0.25) {
            s_graph_xtick = 0.25;
        }
        break;
    case 3:
        s_graph_ytick += delta;
        if (s_graph_ytick < 0.25) {
            s_graph_ytick = 0.25;
        }
        break;
    default:
        break;
    }
}

static void adjust_mode_value(int delta)
{
    int *value = NULL;
    int count = 0;
    switch (s_mode_selection) {
    case 0: value = &s_display_format; count = 3; break;
    case 1: value = &s_print_mode; count = 2; break;
    case 2: value = &s_angle_mode; count = 2; break;
    case 3: value = &s_graphing_mode; count = 4; break;
    case 4: value = &s_complex_mode; count = 3; break;
    default: break;
    }
    if (value == NULL || count <= 0) {
        return;
    }
    *value = (*value + delta + count) % count;
    opencalc_math_set_degrees(s_angle_mode == 0);
}

static void zoom_graph(double factor)
{
    double cx = (s_graph_xmin + s_graph_xmax) / 2.0;
    double cy = (s_graph_ymin + s_graph_ymax) / 2.0;
    double xs = (s_graph_xmax - s_graph_xmin) * factor;
    double ys = (s_graph_ymax - s_graph_ymin) * factor;
    s_graph_xmin = cx - xs / 2.0;
    s_graph_xmax = cx + xs / 2.0;
    s_graph_ymin = cy - ys / 2.0;
    s_graph_ymax = cy + ys / 2.0;
    s_page = PAGE_GRAPH;
    s_current_app = APP_GRAPH;
    ui_draw_current();
}

static void enter_graph_zoom_mode(void)
{
    s_graph_zoom_mode = !s_graph_zoom_mode;
    s_page = PAGE_GRAPH;
    s_current_app = APP_GRAPH;
    ui_draw_current();
}

static void app_output(const char *text)
{
    snprintf(s_calc_output, sizeof(s_calc_output), "%s", text);
    printf("%s\n", s_calc_output);
    ui_draw_current();
}

static void matrix_print_a(void)
{
    printf("Matrix A %dx%d\n", s_matrix_rows, s_matrix_cols);
    for (int r = 0; r < s_matrix_rows; r++) {
        printf("[");
        for (int c = 0; c < s_matrix_cols; c++) {
            printf("%s%.10g", c == 0 ? "" : " ", s_matrix_a[r][c]);
        }
        printf("]\n");
    }
}

static bool matrix_parse_calc_input(void)
{
    if (s_calc_input[0] == '\0') {
        return false;
    }

    double temp[MATRIX_MAX_N][MATRIX_MAX_N] = {0};
    int rows = 0;
    int cols = -1;
    int current_cols = 0;
    const char *p = s_calc_input;

    while (*p != '\0') {
        while (*p == ' ' || *p == '\t' || *p == '[' || *p == ']' || *p == ',') {
            p++;
        }
        if (*p == ';') {
            if (current_cols == 0 || rows >= MATRIX_MAX_N) {
                return false;
            }
            if (cols < 0) {
                cols = current_cols;
            } else if (current_cols != cols) {
                return false;
            }
            rows++;
            current_cols = 0;
            p++;
            continue;
        }
        if (*p == '\0') {
            break;
        }
        if (rows >= MATRIX_MAX_N || current_cols >= MATRIX_MAX_N) {
            return false;
        }

        char *end = NULL;
        double value = strtod(p, &end);
        if (end == p || !isfinite(value)) {
            return false;
        }
        temp[rows][current_cols++] = value;
        p = end;
    }

    if (current_cols > 0) {
        if (cols < 0) {
            cols = current_cols;
        } else if (current_cols != cols) {
            return false;
        }
        rows++;
    }

    if (rows <= 0 || cols <= 0 || rows > MATRIX_MAX_N || cols > MATRIX_MAX_N) {
        return false;
    }

    memset(s_matrix_a, 0, sizeof(s_matrix_a));
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            s_matrix_a[r][c] = temp[r][c];
        }
    }
    s_matrix_rows = rows;
    s_matrix_cols = cols;
    return true;
}

static bool matrix_det_a(double *det)
{
    if (det == NULL || s_matrix_rows == 0 || s_matrix_rows != s_matrix_cols) {
        return false;
    }

    int n = s_matrix_rows;
    double m[MATRIX_MAX_N][MATRIX_MAX_N];
    memcpy(m, s_matrix_a, sizeof(m));
    double result = 1.0;
    int sign = 1;

    for (int col = 0; col < n; col++) {
        int pivot = col;
        for (int r = col + 1; r < n; r++) {
            if (fabs(m[r][col]) > fabs(m[pivot][col])) {
                pivot = r;
            }
        }
        if (fabs(m[pivot][col]) < 1e-12) {
            *det = 0.0;
            return true;
        }
        if (pivot != col) {
            for (int c = 0; c < n; c++) {
                double tmp = m[col][c];
                m[col][c] = m[pivot][c];
                m[pivot][c] = tmp;
            }
            sign = -sign;
        }
        double pivot_value = m[col][col];
        result *= pivot_value;
        for (int r = col + 1; r < n; r++) {
            double factor = m[r][col] / pivot_value;
            for (int c = col; c < n; c++) {
                m[r][c] -= factor * m[col][c];
            }
        }
    }

    *det = result * (double)sign;
    return true;
}

static bool matrix_inverse_a(void)
{
    if (s_matrix_rows == 0 || s_matrix_rows != s_matrix_cols) {
        return false;
    }

    int n = s_matrix_rows;
    double aug[MATRIX_MAX_N][MATRIX_MAX_N * 2] = {0};
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            aug[r][c] = s_matrix_a[r][c];
        }
        aug[r][n + r] = 1.0;
    }

    for (int col = 0; col < n; col++) {
        int pivot = col;
        for (int r = col + 1; r < n; r++) {
            if (fabs(aug[r][col]) > fabs(aug[pivot][col])) {
                pivot = r;
            }
        }
        if (fabs(aug[pivot][col]) < 1e-12) {
            return false;
        }
        if (pivot != col) {
            for (int c = 0; c < n * 2; c++) {
                double tmp = aug[col][c];
                aug[col][c] = aug[pivot][c];
                aug[pivot][c] = tmp;
            }
        }
        double pivot_value = aug[col][col];
        for (int c = 0; c < n * 2; c++) {
            aug[col][c] /= pivot_value;
        }
        for (int r = 0; r < n; r++) {
            if (r == col) {
                continue;
            }
            double factor = aug[r][col];
            for (int c = 0; c < n * 2; c++) {
                aug[r][c] -= factor * aug[col][c];
            }
        }
    }

    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            s_matrix_a[r][c] = aug[r][n + c];
        }
    }
    return true;
}

static bool matrix_rref_a(void)
{
    if (s_matrix_rows == 0 || s_matrix_cols == 0) {
        return false;
    }

    int lead = 0;
    for (int r = 0; r < s_matrix_rows && lead < s_matrix_cols; r++) {
        int pivot = r;
        while (pivot < s_matrix_rows && fabs(s_matrix_a[pivot][lead]) < 1e-12) {
            pivot++;
        }
        if (pivot == s_matrix_rows) {
            lead++;
            r--;
            continue;
        }
        if (pivot != r) {
            for (int c = 0; c < s_matrix_cols; c++) {
                double tmp = s_matrix_a[r][c];
                s_matrix_a[r][c] = s_matrix_a[pivot][c];
                s_matrix_a[pivot][c] = tmp;
            }
        }
        double pivot_value = s_matrix_a[r][lead];
        for (int c = 0; c < s_matrix_cols; c++) {
            s_matrix_a[r][c] /= pivot_value;
        }
        for (int rr = 0; rr < s_matrix_rows; rr++) {
            if (rr == r) {
                continue;
            }
            double factor = s_matrix_a[rr][lead];
            for (int c = 0; c < s_matrix_cols; c++) {
                s_matrix_a[rr][c] -= factor * s_matrix_a[r][c];
                if (fabs(s_matrix_a[rr][c]) < 1e-10) {
                    s_matrix_a[rr][c] = 0.0;
                }
            }
        }
        lead++;
    }
    return true;
}

static bool matrix_transpose_a(void)
{
    if (s_matrix_rows == 0 || s_matrix_cols == 0) {
        return false;
    }

    double temp[MATRIX_MAX_N][MATRIX_MAX_N] = {0};
    for (int r = 0; r < s_matrix_rows; r++) {
        for (int c = 0; c < s_matrix_cols; c++) {
            temp[c][r] = s_matrix_a[r][c];
        }
    }
    int old_rows = s_matrix_rows;
    s_matrix_rows = s_matrix_cols;
    s_matrix_cols = old_rows;
    memcpy(s_matrix_a, temp, sizeof(s_matrix_a));
    return true;
}

static void matrix_set_identity(void)
{
    memset(s_matrix_a, 0, sizeof(s_matrix_a));
    s_matrix_rows = 3;
    s_matrix_cols = 3;
    for (int i = 0; i < 3; i++) {
        s_matrix_a[i][i] = 1.0;
    }
}

static bool solver_set_guess_from_calc(void)
{
    const char *source = s_calc_input[0] ? s_calc_input : s_calc_ans;
    char eval_expr[128];
    double value = 0.0;
    calc_expand_ans(source, eval_expr, sizeof(eval_expr));
    if (!opencalc_math_eval_expression(eval_expr, &value)) {
        return false;
    }
    s_solver_guess = value;
    return true;
}

static double *finance_selected_value(void)
{
    switch (s_fin_selection) {
    case 0: return &s_fin_n;
    case 1: return &s_fin_i;
    case 2: return &s_fin_pv;
    case 3: return &s_fin_pmt;
    case 4: return &s_fin_fv;
    case 5: return &s_fin_py;
    case 6: return &s_fin_cy;
    default: return NULL;
    }
}

static const char *finance_selected_name(void)
{
    static const char *names[] = {"N", "I%", "PV", "PMT", "FV", "P/Y", "C/Y"};
    return names[s_fin_selection >= 0 && s_fin_selection < 7 ? s_fin_selection : 0];
}

static bool finance_set_selected_from_calc(void)
{
    double *field = finance_selected_value();
    if (field == NULL) {
        return false;
    }

    const char *source = s_calc_input[0] ? s_calc_input : s_calc_ans;
    char eval_expr[128];
    double value = 0.0;
    calc_expand_ans(source, eval_expr, sizeof(eval_expr));
    if (!opencalc_math_eval_expression(eval_expr, &value) || !isfinite(value)) {
        return false;
    }

    *field = value;
    if (s_fin_n < 0.0) s_fin_n = 0.0;
    if (s_fin_py <= 0.0) s_fin_py = 1.0;
    if (s_fin_cy <= 0.0) s_fin_cy = s_fin_py;
    return true;
}

static double finance_periodic_rate(void)
{
    double periods = s_fin_py > 0.0 ? s_fin_py : 1.0;
    return (s_fin_i / 100.0) / periods;
}

static double finance_solve_pmt(void)
{
    double r = finance_periodic_rate();
    if (fabs(r) < 1e-12) {
        return -(s_fin_pv + s_fin_fv) / s_fin_n;
    }

    double compound = pow(1.0 + r, s_fin_n);
    double due = s_fin_begin ? (1.0 + r) : 1.0;
    return -(s_fin_pv * compound + s_fin_fv) * r / ((compound - 1.0) * due);
}

static double finance_solve_fv(void)
{
    double r = finance_periodic_rate();
    if (fabs(r) < 1e-12) {
        return -(s_fin_pv + s_fin_pmt * s_fin_n);
    }

    double compound = pow(1.0 + r, s_fin_n);
    double due = s_fin_begin ? (1.0 + r) : 1.0;
    return -(s_fin_pv * compound + s_fin_pmt * due * (compound - 1.0) / r);
}

static double finance_solve_pv(void)
{
    double r = finance_periodic_rate();
    if (fabs(r) < 1e-12) {
        return -(s_fin_fv + s_fin_pmt * s_fin_n);
    }

    double compound = pow(1.0 + r, s_fin_n);
    double due = s_fin_begin ? (1.0 + r) : 1.0;
    return -(s_fin_fv + s_fin_pmt * due * (compound - 1.0) / r) / compound;
}

static bool finance_solve_n(double *n)
{
    double r = finance_periodic_rate();
    if (n == NULL || fabs(s_fin_pmt) < 1e-12) {
        return false;
    }
    if (fabs(r) < 1e-12) {
        *n = -(s_fin_pv + s_fin_fv) / s_fin_pmt;
        return *n >= 0.0 && isfinite(*n);
    }

    double due = s_fin_begin ? (1.0 + r) : 1.0;
    double ratio = (s_fin_pmt * due - s_fin_fv * r) / (s_fin_pmt * due + s_fin_pv * r);
    if (ratio <= 0.0 || fabs(1.0 + r) <= 1e-12) {
        return false;
    }
    *n = log(ratio) / log(1.0 + r);
    return *n >= 0.0 && isfinite(*n);
}

static double finance_npv_from_list(int list, double rate)
{
    double total = 0.0;
    for (int i = 0; i < s_list_counts[list]; i++) {
        total += s_lists[list][i] / pow(1.0 + rate, (double)i);
    }
    return total;
}

static bool finance_irr_from_list(int list, double *irr)
{
    double lo = -0.95;
    double hi = 2.0;
    double f_lo = finance_npv_from_list(list, lo);
    double f_hi = finance_npv_from_list(list, hi);

    for (int expand = 0; expand < 8 && f_lo * f_hi > 0.0; expand++) {
        hi *= 2.0;
        f_hi = finance_npv_from_list(list, hi);
    }
    if (f_lo * f_hi > 0.0) {
        return false;
    }

    for (int i = 0; i < 80; i++) {
        double mid = (lo + hi) * 0.5;
        double f_mid = finance_npv_from_list(list, mid);
        if (fabs(f_mid) < 1e-7) {
            *irr = mid;
            return true;
        }
        if (f_lo * f_mid <= 0.0) {
            hi = mid;
            f_hi = f_mid;
        } else {
            lo = mid;
            f_lo = f_mid;
        }
        (void)f_hi;
    }

    *irr = (lo + hi) * 0.5;
    return true;
}

static double list_sum(int list)
{
    double sum = 0.0;
    for (int i = 0; i < s_list_counts[list]; i++) {
        sum += s_lists[list][i];
    }
    return sum;
}

static double list_mean(int list)
{
    return s_list_counts[list] > 0 ? list_sum(list) / (double)s_list_counts[list] : 0.0;
}

static double list_stdev(int list)
{
    if (s_list_counts[list] < 2) {
        return 0.0;
    }
    double mean = list_mean(list);
    double sumsq = 0.0;
    for (int i = 0; i < s_list_counts[list]; i++) {
        double d = s_lists[list][i] - mean;
        sumsq += d * d;
    }
    return sqrt(sumsq / (double)(s_list_counts[list] - 1));
}

static void list_sorted_copy(int list, double *values, int *count)
{
    *count = s_list_counts[list];
    memcpy(values, s_lists[list], sizeof(double) * (size_t)(*count));
    for (int i = 0; i < *count - 1; i++) {
        for (int j = i + 1; j < *count; j++) {
            if (values[j] < values[i]) {
                double tmp = values[i];
                values[i] = values[j];
                values[j] = tmp;
            }
        }
    }
}

static double median_of_sorted_values(const double *values, int count)
{
    if (count <= 0) {
        return 0.0;
    }
    if ((count % 2) == 1) {
        return values[count / 2];
    }
    return (values[count / 2 - 1] + values[count / 2]) / 2.0;
}

static double list_median(int list)
{
    double values[LIST_MAX_VALUES];
    int count = 0;
    list_sorted_copy(list, values, &count);
    return median_of_sorted_values(values, count);
}

static void list_quartiles(int list, double *q1, double *q3)
{
    double values[LIST_MAX_VALUES];
    int count = 0;
    list_sorted_copy(list, values, &count);
    if (count < 2) {
        *q1 = count == 1 ? values[0] : 0.0;
        *q3 = *q1;
        return;
    }

    int upper_start = (count + 1) / 2;
    int lower_count = count / 2;
    int upper_count = count / 2;
    *q1 = median_of_sorted_values(values, lower_count);
    *q3 = median_of_sorted_values(values + upper_start, upper_count);
}

static double list_median_l1(void)
{
    return list_median(0);
}

static void sort_list(int list, bool ascending)
{
    int count = s_list_counts[list];
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            bool swap = ascending ?
                (s_lists[list][j] < s_lists[list][i]) :
                (s_lists[list][j] > s_lists[list][i]);
            if (swap) {
                double tmp = s_lists[list][i];
                s_lists[list][i] = s_lists[list][j];
                s_lists[list][j] = tmp;
            }
        }
    }
}

static void select_list_index(int index)
{
    if (index < 0) {
        index = LIST_COUNT - 1;
    } else if (index >= LIST_COUNT) {
        index = 0;
    }
    s_list_index = index;
    s_list_cursor = s_list_counts[s_list_index];
    s_list_entry[0] = '\0';
}

static void select_next_empty_list(void)
{
    for (int offset = 1; offset <= LIST_COUNT; offset++) {
        int index = (s_list_index + offset) % LIST_COUNT;
        if (s_list_counts[index] == 0) {
            select_list_index(index);
            return;
        }
    }
    select_list_index((s_list_index + 1) % LIST_COUNT);
}

static bool linreg_l1_l2(double *intercept, double *slope, double *r)
{
    int count = s_list_counts[0] < s_list_counts[1] ? s_list_counts[0] : s_list_counts[1];
    if (count < 2) {
        return false;
    }

    double sx = 0.0;
    double sy = 0.0;
    double sxx = 0.0;
    double syy = 0.0;
    double sxy = 0.0;
    for (int i = 0; i < count; i++) {
        double x = s_lists[0][i];
        double y = s_lists[1][i];
        sx += x;
        sy += y;
        sxx += x * x;
        syy += y * y;
        sxy += x * y;
    }

    double denom = (double)count * sxx - sx * sx;
    if (fabs(denom) < 1e-12) {
        return false;
    }

    *slope = ((double)count * sxy - sx * sy) / denom;
    *intercept = (sy - *slope * sx) / (double)count;

    double r_denom = sqrt(((double)count * sxx - sx * sx) * ((double)count * syy - sy * sy));
    *r = r_denom > 1e-12 ? (((double)count * sxy - sx * sy) / r_denom) : 0.0;
    return true;
}

static bool two_var_stats_l1_l2(int *count, double *xbar, double *ybar, double *sx, double *sy)
{
    int n = s_list_counts[0] < s_list_counts[1] ? s_list_counts[0] : s_list_counts[1];
    if (n < 2) {
        return false;
    }

    double sum_x = 0.0;
    double sum_y = 0.0;
    for (int i = 0; i < n; i++) {
        sum_x += s_lists[0][i];
        sum_y += s_lists[1][i];
    }

    double mean_x = sum_x / (double)n;
    double mean_y = sum_y / (double)n;
    double ss_x = 0.0;
    double ss_y = 0.0;
    for (int i = 0; i < n; i++) {
        double dx = s_lists[0][i] - mean_x;
        double dy = s_lists[1][i] - mean_y;
        ss_x += dx * dx;
        ss_y += dy * dy;
    }

    *count = n;
    *xbar = mean_x;
    *ybar = mean_y;
    *sx = sqrt(ss_x / (double)(n - 1));
    *sy = sqrt(ss_y / (double)(n - 1));
    return true;
}

static bool expression_entry_active(void)
{
    return s_page == PAGE_CALCULATOR || s_page == PAGE_Y_EQUALS;
}

static void open_app(app_id_t app)
{
    s_current_app = app;
    s_app_selection = 0;
    if (app == APP_CALCULATOR) {
        s_page = PAGE_CALCULATOR;
    } else if (app == APP_GRAPH) {
        reset_graph_view_defaults();
        s_page = PAGE_GRAPH;
    } else if (app == APP_TABLE) {
        s_page = PAGE_TABLE;
    } else if (app == APP_PYTHON) {
        s_program_selection = 0;
        s_page = PAGE_PROGRAM_MENU;
    } else if (app == APP_SETTINGS) {
        s_script_selection = 0;
        s_page = PAGE_SETTINGS;
    } else {
        s_page = PAGE_APP;
    }
    ui_draw_current();
}

static void clear_graph_expressions(void)
{
    for (int i = 0; i < 10; i++) {
        s_graph_exprs[i][0] = '\0';
        s_graph_enabled[i] = false;
    }
    s_graph_selection = 0;
}

static void select_conic_template(int type)
{
    s_conic_type = type;
    switch (s_conic_type) {
    case 0:
        s_conic_h = 0.0;
        s_conic_k = 0.0;
        s_conic_a = 1.0;
        snprintf(s_calc_output, sizeof(s_calc_output), "line m=1 h=0 k=0");
        break;
    case 1:
        s_conic_h = 0.0;
        s_conic_k = 0.0;
        s_conic_r = 5.0;
        snprintf(s_calc_output, sizeof(s_calc_output), "circle h=0 k=0 r=5");
        break;
    case 2:
        s_conic_h = 0.0;
        s_conic_k = 0.0;
        s_conic_a = 1.0;
        snprintf(s_calc_output, sizeof(s_calc_output), "parabola h=0 k=0 a=1");
        break;
    case 3:
        s_conic_h = 0.0;
        s_conic_k = 0.0;
        s_conic_a = 6.0;
        s_conic_b = 4.0;
        snprintf(s_calc_output, sizeof(s_calc_output), "ellipse h=0 k=0 a=6 b=4");
        break;
    case 4:
    default:
        s_conic_type = 4;
        s_conic_h = 0.0;
        s_conic_k = 0.0;
        s_conic_a = 4.0;
        s_conic_b = 2.0;
        snprintf(s_calc_output, sizeof(s_calc_output), "hyperbola h=0 k=0 a=4 b=2");
        break;
    }
    printf("%s\n", s_calc_output);
    ui_draw_current();
}

static int graph_first_free_slot(int needed)
{
    if (needed <= 0 || needed > 10) {
        return -1;
    }
    for (int i = 0; i <= 10 - needed; i++) {
        bool free = true;
        for (int j = 0; j < needed; j++) {
            if (s_graph_exprs[i + j][0] != '\0') {
                free = false;
                break;
            }
        }
        if (free) {
            return i;
        }
    }
    return -1;
}

static void graph_selected_conic(void)
{
    int needed = (s_conic_type == 0 || s_conic_type == 2) ? 1 : 2;
    int slot = graph_first_free_slot(needed);
    if (slot < 0) {
        clear_graph_expressions();
        slot = 0;
    }

    switch (s_conic_type) {
    case 0:
        snprintf(s_graph_exprs[slot], sizeof(s_graph_exprs[slot]),
                 "%.10g*(x-%.10g)+%.10g", s_conic_a, s_conic_h, s_conic_k);
        s_graph_enabled[slot] = true;
        break;
    case 1: {
        double rr = s_conic_r * s_conic_r;
        snprintf(s_graph_exprs[slot], sizeof(s_graph_exprs[slot]),
                 "sqrt(%.10g-(x-%.10g)^2)+%.10g", rr, s_conic_h, s_conic_k);
        snprintf(s_graph_exprs[slot + 1], sizeof(s_graph_exprs[slot + 1]),
                 "-sqrt(%.10g-(x-%.10g)^2)+%.10g", rr, s_conic_h, s_conic_k);
        s_graph_enabled[slot] = true;
        s_graph_enabled[slot + 1] = true;
        break;
    }
    case 2:
        snprintf(s_graph_exprs[slot], sizeof(s_graph_exprs[slot]),
                 "%.10g*(x-%.10g)^2+%.10g", s_conic_a, s_conic_h, s_conic_k);
        s_graph_enabled[slot] = true;
        break;
    case 3: {
        double aa = s_conic_a * s_conic_a;
        snprintf(s_graph_exprs[slot], sizeof(s_graph_exprs[slot]),
                 "%.10g*sqrt(1-((x-%.10g)^2)/(%.10g))+%.10g",
                 s_conic_b, s_conic_h, aa, s_conic_k);
        snprintf(s_graph_exprs[slot + 1], sizeof(s_graph_exprs[slot + 1]),
                 "-%.10g*sqrt(1-((x-%.10g)^2)/(%.10g))+%.10g",
                 s_conic_b, s_conic_h, aa, s_conic_k);
        s_graph_enabled[slot] = true;
        s_graph_enabled[slot + 1] = true;
        break;
    }
    case 4:
    default: {
        double aa = s_conic_a * s_conic_a;
        snprintf(s_graph_exprs[slot], sizeof(s_graph_exprs[slot]),
                 "%.10g*sqrt(((x-%.10g)^2)/(%.10g)-1)+%.10g",
                 s_conic_b, s_conic_h, aa, s_conic_k);
        snprintf(s_graph_exprs[slot + 1], sizeof(s_graph_exprs[slot + 1]),
                 "-%.10g*sqrt(((x-%.10g)^2)/(%.10g)-1)+%.10g",
                 s_conic_b, s_conic_h, aa, s_conic_k);
        s_graph_enabled[slot] = true;
        s_graph_enabled[slot + 1] = true;
        break;
    }
    }

    open_app(APP_GRAPH);
}

static void conic_points_output(void)
{
    char line[96];
    switch (s_conic_type) {
    case 0:
        snprintf(line, sizeof(line), "line slope %.4g through (%.4g,%.4g)",
                 s_conic_a, s_conic_h, s_conic_k);
        break;
    case 1:
        snprintf(line, sizeof(line), "center(%.4g,%.4g) r=%.4g",
                 s_conic_h, s_conic_k, s_conic_r);
        break;
    case 2:
        snprintf(line, sizeof(line), "vertex(%.4g,%.4g) a=%.4g",
                 s_conic_h, s_conic_k, s_conic_a);
        break;
    case 3:
        snprintf(line, sizeof(line), "center(%.4g,%.4g) vx=%.4g,%.4g",
                 s_conic_h, s_conic_k, s_conic_h - s_conic_a, s_conic_h + s_conic_a);
        break;
    case 4:
    default:
        snprintf(line, sizeof(line), "center(%.4g,%.4g) vx=%.4g,%.4g",
                 s_conic_h, s_conic_k, s_conic_h - s_conic_a, s_conic_h + s_conic_a);
        break;
    }
    app_output(line);
}

static void open_math_menu(int tab)
{
    s_math_return_page = expression_entry_active() ? s_page : PAGE_CALCULATOR;
    s_page = PAGE_MATH_MENU;
    s_current_app = APP_CALCULATOR;
    s_math_tab = tab;
    s_math_selection = 0;
    ui_draw_current();
}

static void prepare_calculator_entry(void)
{
    s_calc_history_selected = -1;
    s_calc_history_select_answer = false;

    if (s_page == PAGE_MATH_MENU) {
        return;
    }
    if (!expression_entry_active()) {
        s_page = PAGE_CALCULATOR;
        s_current_app = APP_CALCULATOR;
        if (s_calc_cursor > strlen(s_calc_input)) {
            s_calc_cursor = strlen(s_calc_input);
        }
    }
}

static char *active_expression_buffer(size_t *size)
{
    page_id_t page = s_page == PAGE_MATH_MENU ? s_math_return_page : s_page;
    if (page == PAGE_Y_EQUALS) {
        if (size != NULL) {
            *size = sizeof(s_graph_exprs[s_graph_selection]);
        }
        return s_graph_exprs[s_graph_selection];
    }

    if (size != NULL) {
        *size = sizeof(s_calc_input);
    }
    return s_calc_input;
}

static void expression_append(const char *text)
{
    prepare_calculator_entry();

    size_t size = 0;
    char *buffer = active_expression_buffer(&size);
    page_id_t page = s_page == PAGE_MATH_MENU ? s_math_return_page : s_page;
    size_t cur = strlen(buffer);
    size_t add = strlen(text);
    if (cur + add + 1 >= size) {
        return;
    }

    if (page == PAGE_CALCULATOR) {
        if (s_calc_cursor > cur) {
            s_calc_cursor = cur;
        }
        memmove(buffer + s_calc_cursor + add, buffer + s_calc_cursor, cur - s_calc_cursor + 1);
        memcpy(buffer + s_calc_cursor, text, add);
        s_calc_cursor += add;
    } else {
        memcpy(buffer + cur, text, add + 1);
    }
}

static bool expression_delete_range(char *buffer, size_t len, size_t start, size_t end)
{
    if (buffer == NULL || start >= end || end > len) {
        return false;
    }

    memmove(buffer + start, buffer + end, len - end + 1);
    s_calc_cursor = start;
    return true;
}

static bool expression_token_matches_at(const char *buffer, size_t len, size_t start, const char *token)
{
    size_t token_len = strlen(token);
    return start + token_len <= len && strncasecmp(buffer + start, token, token_len) == 0;
}

static bool expression_delete_token_near_cursor(char *buffer, size_t len)
{
    static const char *const template_tokens[] = {
        "nroot(,)",
        "frac(,)",
        "^()",
    };
    static const char *const word_tokens[] = {
        "nDeriv(",
        "fnInt(",
        "randInt(",
        "randNorm(",
        "remainder(",
        "convert(",
        "nroot(",
        "frac(",
        "sqrt(",
        "cbrt(",
        "asin(",
        "acos(",
        "atan(",
        "round(",
        "iPart(",
        "fPart(",
        "conj(",
        "real(",
        "imag(",
        "angle(",
        "sin(",
        "cos(",
        "tan(",
        "abs(",
        "min(",
        "max(",
        "gcd(",
        "lcm(",
        "log(",
        "ln(",
        "ANS",
        "pi",
    };

    for (size_t start = 0; start <= len; start++) {
        for (size_t i = 0; i < sizeof(template_tokens) / sizeof(template_tokens[0]); i++) {
            size_t token_len = strlen(template_tokens[i]);
            if (!expression_token_matches_at(buffer, len, start, template_tokens[i])) {
                continue;
            }
            size_t end = start + token_len;
            if (s_calc_cursor >= start && s_calc_cursor <= end) {
                return expression_delete_range(buffer, len, start, end);
            }
        }

        for (size_t i = 0; i < sizeof(word_tokens) / sizeof(word_tokens[0]); i++) {
            size_t token_len = strlen(word_tokens[i]);
            if (!expression_token_matches_at(buffer, len, start, word_tokens[i])) {
                continue;
            }
            size_t end = start + token_len;
            if (s_calc_cursor > start && s_calc_cursor <= end) {
                return expression_delete_range(buffer, len, start, end);
            }
            if (s_calc_cursor == start && start < len) {
                return expression_delete_range(buffer, len, start, end);
            }
        }
    }

    return false;
}

static void expression_delete(void)
{
    prepare_calculator_entry();

    size_t size = 0;
    char *buffer = active_expression_buffer(&size);
    page_id_t page = s_page == PAGE_MATH_MENU ? s_math_return_page : s_page;
    (void)size;
    size_t len = strlen(buffer);
    if (page == PAGE_CALCULATOR) {
        if (s_calc_cursor > len) {
            s_calc_cursor = len;
        }
        if (expression_delete_token_near_cursor(buffer, len)) {
            return;
        }
        if (s_calc_cursor < len) {
            memmove(buffer + s_calc_cursor, buffer + s_calc_cursor + 1, len - s_calc_cursor);
        }
    } else if (len > 0) {
        buffer[len - 1] = '\0';
    }
}

static void expression_insert_power_box(void)
{
    prepare_calculator_entry();

    page_id_t page = s_page == PAGE_MATH_MENU ? s_math_return_page : s_page;
    if (page == PAGE_CALCULATOR) {
        expression_append("^()");
        if (s_calc_cursor > 0) {
            s_calc_cursor--;
        }
    } else {
        expression_append("^(");
    }
}

static void expression_insert_fraction_box(void)
{
    prepare_calculator_entry();

    page_id_t page = s_page == PAGE_MATH_MENU ? s_math_return_page : s_page;
    if (page == PAGE_CALCULATOR) {
        expression_append("frac(,)");
        if (s_calc_cursor >= 2) {
            s_calc_cursor -= 2;
        }
    } else {
        expression_append("frac(");
    }
}

static void expression_insert_nroot_box(void)
{
    prepare_calculator_entry();

    page_id_t page = s_page == PAGE_MATH_MENU ? s_math_return_page : s_page;
    if (page == PAGE_CALCULATOR) {
        expression_append("nroot(,)");
        if (s_calc_cursor >= 2) {
            s_calc_cursor -= 2;
        }
    } else {
        expression_append("nroot(");
    }
}

static void expression_append_power_base(const char *base)
{
    prepare_calculator_entry();
    expression_append(base);
    expression_insert_power_box();
}

static void expression_clear(void)
{
    prepare_calculator_entry();

    size_t size = 0;
    char *buffer = active_expression_buffer(&size);
    (void)size;
    buffer[0] = '\0';
    if ((s_page == PAGE_MATH_MENU ? s_math_return_page : s_page) == PAGE_CALCULATOR) {
        s_calc_cursor = 0;
    }
}

static void store_answer(void)
{
    if (s_calc_output[0] != '\0' && strcmp(s_calc_output, "error") != 0) {
        strncpy(s_calc_ans, s_calc_output, sizeof(s_calc_ans) - 1);
        s_calc_ans[sizeof(s_calc_ans) - 1] = '\0';
    }
    snprintf(s_calc_output, sizeof(s_calc_output), "Ans=%s", s_calc_ans);
    printf("stored Ans=%s\n", s_calc_ans);
    ui_draw_current();
}

static void run_home_app_tool(void)
{
    char line[96];

    switch (s_current_app) {
    case APP_STATS:
        if (s_app_selection == 0) {
            s_page = PAGE_LIST_EDITOR;
            s_current_app = APP_LISTS;
            s_list_index = 0;
            s_list_cursor = s_list_counts[s_list_index];
            s_list_entry[0] = '\0';
            ui_draw_current();
        } else if (s_app_selection == 1) {
            if (s_list_counts[0] == 0) {
                app_output("SortA L1 empty");
            } else {
                sort_list(0, true);
                app_output("SortA L1 done");
            }
        } else if (s_app_selection == 2) {
            if (s_list_counts[0] == 0) {
                app_output("SortD L1 empty");
            } else {
                sort_list(0, false);
                app_output("SortD L1 done");
            }
        } else if (s_app_selection == 3) {
            memset(s_lists, 0, sizeof(s_lists));
            memset(s_list_counts, 0, sizeof(s_list_counts));
            s_list_index = 0;
            s_list_cursor = 0;
            s_list_entry[0] = '\0';
            app_output("ClearList done");
        } else if (s_app_selection == 4) {
            if (s_list_counts[0] == 0) {
                app_output("1-var needs L1");
            } else {
                double q1 = 0.0;
                double q3 = 0.0;
                list_quartiles(0, &q1, &q3);
                snprintf(line, sizeof(line), "xbar=%.3g med=%.3g sx=%.3g", list_mean(0), list_median_l1(), list_stdev(0));
                app_output(line);
                printf("1-var L1: n=%d sum=%.10g min/q1/med/q3/max available q1=%.10g q3=%.10g\n",
                       s_list_counts[0], list_sum(0), q1, q3);
            }
        } else if (s_app_selection == 5) {
            int count = 0;
            double xbar = 0.0;
            double ybar = 0.0;
            double sx = 0.0;
            double sy = 0.0;
            if (two_var_stats_l1_l2(&count, &xbar, &ybar, &sx, &sy)) {
                snprintf(line, sizeof(line), "2-var n=%d xbar=%.3g ybar=%.3g", count, xbar, ybar);
                app_output(line);
                printf("2-var L1,L2: n=%d xbar=%.10g ybar=%.10g sx=%.10g sy=%.10g\n",
                       count, xbar, ybar, sx, sy);
            } else {
                app_output("2-var needs L1,L2");
            }
        } else {
            double a = 0.0;
            double b = 0.0;
            double r = 0.0;
            if (linreg_l1_l2(&a, &b, &r)) {
                snprintf(line, sizeof(line), "LinReg y=%.3g+%.3gx", a, b);
                app_output(line);
                printf("LinReg L1,L2: a=%.10g b=%.10g r=%.10g\n", a, b, r);
            } else {
                app_output("LinReg needs L1,L2");
            }
        }
        break;
    case APP_LISTS:
        if (s_app_selection == 0) {
            s_page = PAGE_LIST_EDITOR;
            s_current_app = APP_LISTS;
            s_list_entry[0] = '\0';
            if (s_list_cursor > s_list_counts[s_list_index]) {
                s_list_cursor = s_list_counts[s_list_index];
            }
            ui_draw_current();
        } else if (s_app_selection == 1) {
            select_next_empty_list();
            snprintf(s_calc_output, sizeof(s_calc_output), "new/edit L%d", s_list_index + 1);
            s_page = PAGE_LIST_EDITOR;
            s_current_app = APP_LISTS;
            ui_draw_current();
        } else if (s_app_selection == 2) {
            select_list_index(s_list_index + 1);
            snprintf(s_calc_output, sizeof(s_calc_output), "selected L%d", s_list_index + 1);
            ui_draw_current();
        } else if (s_app_selection == 3) {
            select_list_index(s_list_index - 1);
            snprintf(s_calc_output, sizeof(s_calc_output), "selected L%d", s_list_index + 1);
            ui_draw_current();
        } else if (s_app_selection == 4) {
            if (s_list_counts[s_list_index] == 0) {
                snprintf(line, sizeof(line), "sum L%d empty", s_list_index + 1);
                app_output(line);
            } else {
                snprintf(line, sizeof(line), "sum L%d %.10g", s_list_index + 1, list_sum(s_list_index));
                app_output(line);
            }
        } else if (s_app_selection == 5) {
            if (s_list_counts[s_list_index] == 0) {
                snprintf(line, sizeof(line), "minmax L%d empty", s_list_index + 1);
                app_output(line);
            } else {
                double min = s_lists[s_list_index][0];
                double max = s_lists[s_list_index][0];
                for (int i = 1; i < s_list_counts[s_list_index]; i++) {
                    if (s_lists[s_list_index][i] < min) min = s_lists[s_list_index][i];
                    if (s_lists[s_list_index][i] > max) max = s_lists[s_list_index][i];
                }
                snprintf(line, sizeof(line), "L%d min %.10g max %.10g", s_list_index + 1, min, max);
                app_output(line);
            }
        } else {
            memset(s_lists[s_list_index], 0, sizeof(s_lists[s_list_index]));
            s_list_counts[s_list_index] = 0;
            s_list_cursor = 0;
            s_list_entry[0] = '\0';
            snprintf(line, sizeof(line), "L%d cleared", s_list_index + 1);
            app_output(line);
        }
        break;
    case APP_MATRICES:
        if (s_app_selection == 0) {
            if (matrix_parse_calc_input()) {
                snprintf(line, sizeof(line), "A set %dx%d", s_matrix_rows, s_matrix_cols);
                app_output(line);
            } else {
                app_output("use calc: 1,2;3,4");
            }
        } else if (s_app_selection == 1) {
            if (s_matrix_rows == 0) {
                app_output("A empty");
            } else {
                matrix_print_a();
                snprintf(line, sizeof(line), "A %dx%d printed", s_matrix_rows, s_matrix_cols);
                app_output(line);
            }
        } else if (s_app_selection == 2) {
            double det = 0.0;
            if (matrix_det_a(&det)) {
                snprintf(line, sizeof(line), "det A %.10g", det);
                app_output(line);
            } else {
                app_output("det needs square A");
            }
        } else if (s_app_selection == 3) {
            if (matrix_inverse_a()) {
                matrix_print_a();
                app_output("A inverse stored");
            } else {
                app_output("inverse unavailable");
            }
        } else if (s_app_selection == 4) {
            if (matrix_rref_a()) {
                matrix_print_a();
                app_output("A rref stored");
            } else {
                app_output("A empty");
            }
        } else if (s_app_selection == 5) {
            if (matrix_transpose_a()) {
                matrix_print_a();
                app_output("A transposed");
            } else {
                app_output("A empty");
            }
        } else {
            matrix_set_identity();
            matrix_print_a();
            app_output("A=identity 3x3");
        }
        break;
    case APP_SOLVER:
        if (s_app_selection == 0) {
            if (s_calc_input[0] == '\0') {
                app_output("type E1 in Calc first");
            } else {
                snprintf(s_solver_e1, sizeof(s_solver_e1), "%s", s_calc_input);
                s_solver_has_result = false;
                snprintf(line, sizeof(line), "E1=%.40s", s_solver_e1);
                app_output(line);
            }
        } else if (s_app_selection == 1) {
            if (s_calc_input[0] == '\0') {
                app_output("type E2 in Calc first");
            } else {
                snprintf(s_solver_e2, sizeof(s_solver_e2), "%s", s_calc_input);
                s_solver_has_result = false;
                snprintf(line, sizeof(line), "E2=%.40s", s_solver_e2);
                app_output(line);
            }
        } else if (s_app_selection == 2) {
            if (solver_set_guess_from_calc()) {
                snprintf(line, sizeof(line), "guess %.10g", s_solver_guess);
                app_output(line);
            } else {
                app_output("guess must be number");
            }
        } else if (s_app_selection == 3) {
            submit_solver_solve_job();
        } else if (s_app_selection == 4) {
            if (s_solver_has_result && calc_format_fraction_value(s_solver_result, line, sizeof(line))) {
                char out[96];
                snprintf(out, sizeof(out), "x=%.90s", line);
                app_output(out);
            } else {
                app_output("solve first");
            }
        } else if (s_app_selection == 5) {
            snprintf(s_solver_e1, sizeof(s_solver_e1), "x^2");
            snprintf(s_solver_e2, sizeof(s_solver_e2), "4");
            s_solver_guess = 1.0;
            s_solver_result = 0.0;
            s_solver_has_result = false;
            app_output("example E1 x^2 E2 4");
        } else {
            snprintf(s_solver_e1, sizeof(s_solver_e1), "x");
            snprintf(s_solver_e2, sizeof(s_solver_e2), "0");
            s_solver_guess = 0.0;
            s_solver_result = 0.0;
            s_solver_has_result = false;
            app_output("solver cleared");
        }
        break;
    case APP_FINANCE:
        if (s_app_selection == 0) {
            if (finance_set_selected_from_calc()) {
                snprintf(line, sizeof(line), "%s=%.10g", finance_selected_name(), *finance_selected_value());
                app_output(line);
            } else {
                app_output("type value in Calc first");
            }
        } else if (s_app_selection == 1) {
            s_fin_selection = (s_fin_selection + 1) % 7;
            snprintf(s_calc_output, sizeof(s_calc_output), "field %s", finance_selected_name());
            ui_draw_current();
        } else if (s_app_selection == 2) {
            s_fin_selection = (s_fin_selection + 6) % 7;
            snprintf(s_calc_output, sizeof(s_calc_output), "field %s", finance_selected_name());
            ui_draw_current();
        } else if (s_app_selection == 3) {
            if (s_fin_selection == 0) {
                double n = 0.0;
                if (finance_solve_n(&n)) {
                    s_fin_n = n;
                    snprintf(line, sizeof(line), "N %.6g", s_fin_n);
                    app_output(line);
                } else {
                    app_output("N solve unavailable");
                }
            } else if (s_fin_selection == 2) {
                s_fin_pv = finance_solve_pv();
                snprintf(line, sizeof(line), "PV %.2f", s_fin_pv);
                app_output(line);
            } else if (s_fin_selection == 3) {
                s_fin_pmt = finance_solve_pmt();
                snprintf(line, sizeof(line), "PMT %.2f %s", s_fin_pmt, s_fin_begin ? "BEGIN" : "END");
                app_output(line);
            } else if (s_fin_selection == 4) {
                s_fin_fv = finance_solve_fv();
                snprintf(line, sizeof(line), "FV %.2f", s_fin_fv);
                app_output(line);
            } else {
                app_output("solve N/PV/PMT/FV");
            }
        } else if (s_app_selection == 4) {
            s_fin_begin = !s_fin_begin;
            app_output(s_fin_begin ? "payments BEGIN" : "payments END");
        } else if (s_app_selection == 5) {
            if (s_list_counts[s_list_index] == 0) {
                snprintf(line, sizeof(line), "L%d empty", s_list_index + 1);
                app_output(line);
            } else {
                double npv = finance_npv_from_list(s_list_index, finance_periodic_rate());
                snprintf(line, sizeof(line), "NPV L%d %.2f", s_list_index + 1, npv);
                app_output(line);
            }
        } else {
            if (s_list_counts[s_list_index] < 2) {
                snprintf(line, sizeof(line), "IRR needs L%d flows", s_list_index + 1);
                app_output(line);
            } else {
                double irr = 0.0;
                if (finance_irr_from_list(s_list_index, &irr)) {
                    snprintf(line, sizeof(line), "IRR L%d %.4g%%", s_list_index + 1, irr * 100.0);
                    app_output(line);
                } else {
                    app_output("IRR no sign change");
                }
            }
        }
        break;
    case APP_CONICS:
        if (s_app_selection >= 0 && s_app_selection <= 4) {
            select_conic_template(s_app_selection);
        }
        else if (s_app_selection == 5) {
            graph_selected_conic();
        } else {
            conic_points_output();
        }
        break;
    case APP_INEQUALITY:
        if (s_app_selection == 0) {
            clear_graph_expressions();
            inequality_clear_all();
            inequality_set(0, "x", true, false);
            snprintf(s_graph_exprs[0], sizeof(s_graph_exprs[0]), "x");
            s_graph_enabled[0] = true;
            app_output("y>x dotted shade up");
        } else if (s_app_selection == 1) {
            clear_graph_expressions();
            inequality_clear_all();
            inequality_set(0, "x^2", false, false);
            snprintf(s_graph_exprs[0], sizeof(s_graph_exprs[0]), "x^2");
            s_graph_enabled[0] = true;
            app_output("y<x^2 dotted shade down");
        } else if (s_app_selection == 2) {
            clear_graph_expressions();
            inequality_clear_all();
            inequality_set_vertical(0, 0.0, true, true);
            app_output("x>=0 solid shade right");
        } else if (s_app_selection == 3) {
            clear_graph_expressions();
            inequality_clear_all();
            inequality_set(0, "0", true, true);
            snprintf(s_graph_exprs[0], sizeof(s_graph_exprs[0]), "0");
            s_graph_enabled[0] = true;
            app_output("y>=0 solid shade up");
        } else if (s_app_selection == 4) {
            clear_graph_expressions();
            inequality_clear_all();
            inequality_set(0, "x", true, false);
            inequality_set(1, "x^2", false, false);
            snprintf(s_graph_exprs[0], sizeof(s_graph_exprs[0]), "x");
            snprintf(s_graph_exprs[1], sizeof(s_graph_exprs[1]), "x^2");
            s_graph_enabled[0] = true;
            s_graph_enabled[1] = true;
            app_output("overlap shading loaded");
        } else if (s_app_selection == 5) {
            open_app(APP_GRAPH);
        } else {
            clear_graph_expressions();
            inequality_clear_all();
            app_output("inequalities cleared");
        }
        break;
    default:
        break;
    }
}

static bool handle_alpha_insert(int row, int col)
{
    if (!s_alpha_active && !s_alpha_locked) {
        return false;
    }

    const board_key_t *key = board_keypad_key_at(row, col);
    if (key == NULL || key->alpha == NULL || key->alpha[0] == '\0') {
        return false;
    }

    expression_append(key->alpha);
    ui_draw_current();
    return true;
}

static bool calc_take_wrapped_expression(const char *input, const char *name, char *out, size_t out_size)
{
    size_t name_len = strlen(name);
    if (strncmp(input, name, name_len) != 0 || input[name_len] != '(' || out_size == 0) {
        return false;
    }

    const char *start = input + name_len + 1;
    const char *end = strrchr(start, ')');
    if (end == NULL || end <= start) {
        return false;
    }

    size_t len = (size_t)(end - start);
    if (len >= out_size) {
        len = out_size - 1;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return true;
}

static void calc_expand_ans_value(const char *input, const char *ans, char *out, size_t out_size)
{
    size_t out_len = 0;
    if (out_size == 0) {
        return;
    }

    for (size_t i = 0; input[i] != '\0' && out_len + 1 < out_size;) {
        if ((input[i] == 'A' || input[i] == 'a') &&
            (input[i + 1] == 'N' || input[i + 1] == 'n') &&
            (input[i + 2] == 'S' || input[i + 2] == 's')) {
            size_t ans_len = strlen(ans);
            if (out_len + ans_len >= out_size) {
                break;
            }
            memcpy(out + out_len, ans, ans_len);
            out_len += ans_len;
            i += 3;
        } else {
            out[out_len++] = input[i++];
        }
    }
    out[out_len] = '\0';
}

static void calc_expand_ans(const char *input, char *out, size_t out_size)
{
    calc_expand_ans_value(input, s_calc_ans, out, out_size);
}

static bool calc_format_fraction_value(double value, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0 || !isfinite(value)) {
        return false;
    }

    double sign = value < 0.0 ? -1.0 : 1.0;
    double x = fabs(value);
    long long best_den = 1;
    long long best_num = (long long)llround(x);
    double best_err = fabs(x - (double)best_num);

    for (long long den = 1; den <= 100000; den++) {
        long long num = (long long)llround(x * (double)den);
        double err = fabs(x - (double)num / (double)den);
        if (err < best_err) {
            best_err = err;
            best_num = num;
            best_den = den;
            if (err < 1e-10) {
                break;
            }
        }
    }

    if (best_err > 1e-7) {
        snprintf(out, out_size, "%.10g", value);
        return true;
    }

    long long a = llabs(best_num);
    long long b = llabs(best_den);
    while (b != 0) {
        long long r = a % b;
        a = b;
        b = r;
    }
    if (a > 1) {
        best_num /= a;
        best_den /= a;
    }
    if (sign < 0.0) {
        best_num = -best_num;
    }

    if (best_den == 1) {
        snprintf(out, out_size, "%lld", best_num);
    } else {
        snprintf(out, out_size, "%lld/%lld", best_num, best_den);
    }
    return true;
}

static bool work_solver_eval_difference(const ui_work_job_t *job, double x, double *out)
{
    double left = 0.0;
    double right = 0.0;
    if (!graph_eval_expression(job->solver.e1, x, &left) ||
        !graph_eval_expression(job->solver.e2, x, &right)) {
        return false;
    }
    *out = left - right;
    return isfinite(*out);
}

static bool work_solver_solve_near_guess(const ui_work_job_t *job, double *root)
{
    double guess = job->solver.guess;
    double f_guess = 0.0;
    if (!work_solver_eval_difference(job, guess, &f_guess)) {
        return false;
    }
    if (fabs(f_guess) < 1e-10) {
        *root = guess;
        return true;
    }

    double best_x = guess;
    double best_abs = fabs(f_guess);
    double lo = guess;
    double hi = guess;
    double f_lo = f_guess;
    double f_hi = f_guess;
    bool bracketed = false;

    double step = fmax(0.25, fabs(guess) * 0.1);
    for (int i = 0; i < 48 && !bracketed; i++) {
        double left_x = guess - step;
        double right_x = guess + step;
        double f_left = 0.0;
        double f_right = 0.0;
        bool left_ok = work_solver_eval_difference(job, left_x, &f_left);
        bool right_ok = work_solver_eval_difference(job, right_x, &f_right);

        if (left_ok && fabs(f_left) < best_abs) {
            best_abs = fabs(f_left);
            best_x = left_x;
        }
        if (right_ok && fabs(f_right) < best_abs) {
            best_abs = fabs(f_right);
            best_x = right_x;
        }

        if (left_ok && ((f_left <= 0.0 && f_guess >= 0.0) || (f_left >= 0.0 && f_guess <= 0.0))) {
            lo = left_x;
            hi = guess;
            f_lo = f_left;
            f_hi = f_guess;
            bracketed = true;
        } else if (right_ok && ((f_guess <= 0.0 && f_right >= 0.0) || (f_guess >= 0.0 && f_right <= 0.0))) {
            lo = guess;
            hi = right_x;
            f_lo = f_guess;
            f_hi = f_right;
            bracketed = true;
        } else if (left_ok && right_ok && ((f_left <= 0.0 && f_right >= 0.0) || (f_left >= 0.0 && f_right <= 0.0))) {
            lo = left_x;
            hi = right_x;
            f_lo = f_left;
            f_hi = f_right;
            bracketed = true;
        }
        step *= 1.6;
    }

    if (bracketed) {
        for (int i = 0; i < 64; i++) {
            double mid = (lo + hi) * 0.5;
            double f_mid = 0.0;
            if (!work_solver_eval_difference(job, mid, &f_mid)) {
                break;
            }
            if (fabs(f_mid) < 1e-10) {
                *root = mid;
                return true;
            }
            if ((f_lo <= 0.0 && f_mid >= 0.0) || (f_lo >= 0.0 && f_mid <= 0.0)) {
                hi = mid;
                f_hi = f_mid;
            } else {
                lo = mid;
                f_lo = f_mid;
            }
            (void)f_hi;
        }
        *root = (lo + hi) * 0.5;
        return true;
    }

    double x = best_x;
    for (int i = 0; i < 24; i++) {
        double fx = 0.0;
        if (!work_solver_eval_difference(job, x, &fx)) {
            return false;
        }
        if (fabs(fx) < 1e-9) {
            *root = x;
            return true;
        }
        double h = fmax(1e-5, fabs(x) * 1e-5);
        double fp = 0.0;
        double fm = 0.0;
        if (!work_solver_eval_difference(job, x + h, &fp) ||
            !work_solver_eval_difference(job, x - h, &fm)) {
            return false;
        }
        double slope = (fp - fm) / (2.0 * h);
        if (fabs(slope) < 1e-12) {
            break;
        }
        x -= fx / slope;
        if (!isfinite(x)) {
            return false;
        }
    }

    double final_f = 0.0;
    if (work_solver_eval_difference(job, x, &final_f) && fabs(final_f) < 1e-6) {
        *root = x;
        return true;
    }
    return false;
}

static void work_calc_eval(const ui_work_job_t *job, ui_work_result_t *result)
{
    char inner[96];
    char eval_expr[128];
    result->ok = true;
    snprintf(result->calc.expr, sizeof(result->calc.expr), "%s", job->calc.expr);
    result->calc.update_ans = false;

    if (calc_take_wrapped_expression(job->calc.expr, "deriv", inner, sizeof(inner))) {
        if (!opencalc_math_derivative_expression(inner, result->calc.output, sizeof(result->calc.output))) {
            snprintf(result->calc.output, sizeof(result->calc.output), "unsupported deriv");
        }
    } else if (calc_take_wrapped_expression(job->calc.expr, "int", inner, sizeof(inner))) {
        if (!opencalc_math_integral_expression(inner, result->calc.output, sizeof(result->calc.output))) {
            snprintf(result->calc.output, sizeof(result->calc.output), "unsupported int");
        }
    } else {
        double value = 0.0;
        calc_expand_ans_value(job->calc.expr, job->calc.ans, eval_expr, sizeof(eval_expr));
        bool ok = opencalc_math_eval_expression(eval_expr, &value);
        if (ok) {
            if (calc_take_wrapped_expression(job->calc.expr, "frac", inner, sizeof(inner)) ||
                calc_take_wrapped_expression(job->calc.expr, "Frac", inner, sizeof(inner)) ||
                calc_take_wrapped_expression(job->calc.expr, "FRAC", inner, sizeof(inner))) {
                calc_format_fraction_value(value, result->calc.output, sizeof(result->calc.output));
            } else {
                snprintf(result->calc.output, sizeof(result->calc.output), "%.10g", value);
            }
            result->calc.update_ans = true;
        } else {
            snprintf(result->calc.output, sizeof(result->calc.output), "error");
        }
    }
}

static void ui_work_task(void *arg)
{
    (void)arg;

    ui_work_job_t job;
    while (true) {
        if (xQueueReceive(s_work_queue, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        opencalc_math_set_degrees(job.degrees);

        ui_work_result_t result;
        memset(&result, 0, sizeof(result));
        result.type = job.type;

        switch (job.type) {
        case UI_WORK_CALC_EVAL:
            work_calc_eval(&job, &result);
            break;
        case UI_WORK_SOLVER_SOLVE:
            result.ok = work_solver_solve_near_guess(&job, &result.solver.root);
            break;
        case UI_WORK_GRAPH_CALC:
            result.ok = work_graph_calc_run(&job, &result);
            if (!result.ok) {
                snprintf(result.graph.status, sizeof(result.graph.status), "graph calc: no result");
            }
            break;
        default:
            result.ok = false;
            break;
        }

        xQueueSend(s_work_result_queue, &result, portMAX_DELAY);
    }
}

static bool ui_work_submit(const ui_work_job_t *job)
{
    if (s_work_queue == NULL || s_work_task == NULL || job == NULL) {
        return false;
    }
    return xQueueSend(s_work_queue, job, 0) == pdTRUE;
}

static void run_calc_eval_synchronously(const ui_work_job_t *job)
{
    ui_work_result_t result;
    memset(&result, 0, sizeof(result));
    result.type = UI_WORK_CALC_EVAL;
    opencalc_math_set_degrees(job->degrees);
    work_calc_eval(job, &result);
    ui_work_apply_result(&result);
}

static bool submit_calc_eval_job(void)
{
    if (s_calc_eval_pending) {
        snprintf(s_calc_output, sizeof(s_calc_output), "busy");
        ui_draw_current();
        return false;
    }

    ui_work_job_t job;
    memset(&job, 0, sizeof(job));
    job.type = UI_WORK_CALC_EVAL;
    job.degrees = s_angle_mode == 0;
    snprintf(job.calc.expr, sizeof(job.calc.expr), "%s", s_calc_input);
    snprintf(job.calc.ans, sizeof(job.calc.ans), "%s", s_calc_ans);

    if (!ui_work_submit(&job)) {
        run_calc_eval_synchronously(&job);
        return true;
    }

    s_calc_eval_pending = true;
    snprintf(s_calc_output, sizeof(s_calc_output), "working...");
    ui_draw_current();
    return true;
}

static bool submit_solver_solve_job(void)
{
    if (s_solver_solve_pending) {
        app_output("solver busy");
        return false;
    }

    ui_work_job_t job;
    memset(&job, 0, sizeof(job));
    job.type = UI_WORK_SOLVER_SOLVE;
    job.degrees = s_angle_mode == 0;
    snprintf(job.solver.e1, sizeof(job.solver.e1), "%s", s_solver_e1);
    snprintf(job.solver.e2, sizeof(job.solver.e2), "%s", s_solver_e2);
    job.solver.guess = s_solver_guess;

    if (!ui_work_submit(&job)) {
        app_output("worker unavailable");
        return false;
    }

    s_solver_solve_pending = true;
    app_output("solving...");
    return true;
}

static bool submit_graph_calc_job(void)
{
    if (s_graph_calc_pending) {
        snprintf(s_graph_status, sizeof(s_graph_status), "graph calc: busy");
        ui_draw_current();
        return false;
    }

    ui_work_job_t job;
    memset(&job, 0, sizeof(job));
    job.type = UI_WORK_GRAPH_CALC;
    job.degrees = s_angle_mode == 0;
    job.graph.selection = s_graph_calc_selection;
    memcpy(job.graph.exprs, s_graph_exprs, sizeof(job.graph.exprs));
    memcpy(job.graph.enabled, s_graph_enabled, sizeof(job.graph.enabled));
    job.graph.xmin = s_graph_xmin;
    job.graph.xmax = s_graph_xmax;
    job.graph.trace = s_graph_trace;
    job.graph.trace_x = s_graph_trace_x;
    job.graph.trace_fn = s_graph_trace_fn;

    if (!ui_work_submit(&job)) {
        snprintf(s_graph_status, sizeof(s_graph_status), "graph calc: worker unavailable");
        ui_draw_current();
        return false;
    }

    s_graph_calc_pending = true;
    snprintf(s_graph_status, sizeof(s_graph_status), "graph calc: working...");
    s_page = PAGE_GRAPH;
    s_current_app = APP_GRAPH;
    s_graph_zoom_mode = false;
    ui_draw_current();
    return true;
}

static void ui_work_apply_result(const ui_work_result_t *result)
{
    if (result == NULL) {
        return;
    }

    switch (result->type) {
    case UI_WORK_CALC_EVAL:
        s_calc_eval_pending = false;
        snprintf(s_calc_output, sizeof(s_calc_output), "%s", result->calc.output);
        if (result->calc.update_ans) {
            strncpy(s_calc_ans, result->calc.output, sizeof(s_calc_ans) - 1);
            s_calc_ans[sizeof(s_calc_ans) - 1] = '\0';
        }
        calc_history_push(result->calc.expr, s_calc_output);
        printf("calc %s => %s\n", result->calc.expr, s_calc_output);
        s_calc_input[0] = '\0';
        s_calc_cursor = 0;
        ui_draw_current();
        break;
    case UI_WORK_SOLVER_SOLVE:
        s_solver_solve_pending = false;
        if (result->ok) {
            s_solver_result = result->solver.root;
            s_solver_guess = result->solver.root;
            s_solver_has_result = true;
            char line[96];
            snprintf(line, sizeof(line), "x=%.10g", result->solver.root);
            app_output(line);
        } else {
            s_solver_has_result = false;
            app_output("no solution near guess");
        }
        break;
    case UI_WORK_GRAPH_CALC:
        s_graph_calc_pending = false;
        snprintf(s_graph_status, sizeof(s_graph_status), "%s", result->graph.status);
        if (result->ok) {
            s_graph_trace = result->graph.trace;
            s_graph_trace_x = result->graph.trace_x;
            s_graph_trace_fn = result->graph.trace_fn;
        }
        s_page = PAGE_GRAPH;
        s_current_app = APP_GRAPH;
        s_graph_zoom_mode = false;
        ui_draw_current();
        break;
    default:
        break;
    }
}

static void ui_work_poll_results(void)
{
    if (s_work_result_queue == NULL) {
        return;
    }

    ui_work_result_t result;
    while (xQueueReceive(s_work_result_queue, &result, 0) == pdTRUE) {
        ui_work_apply_result(&result);
    }
}

static void calc_eval(void)
{
    char expr[96];
    snprintf(expr, sizeof(expr), "%s", s_calc_input);
    if (expr[0] == '\0') {
        return;
    }
    submit_calc_eval_job();
}

static void math_menu_insert_selected(void)
{
    int count = math_menu_count(s_math_tab);
    if (s_math_selection < 0 || s_math_selection >= count) {
        return;
    }

    const char *insert = MATH_MENU[s_math_tab][s_math_selection].insert;
    if (strcmp(insert, "nroot(") == 0) {
        expression_insert_nroot_box();
    } else {
        expression_append(insert);
    }
    s_page = s_math_return_page == PAGE_Y_EQUALS ? PAGE_Y_EQUALS : PAGE_CALCULATOR;
    s_current_app = s_page == PAGE_Y_EQUALS ? APP_GRAPH : APP_CALCULATOR;
    ui_draw_current();
}

static void run_selected_script(void)
{
    if (s_script_count == 0 || s_script_running) {
        return;
    }

    char path[80];
    snprintf(path, sizeof(path), "/data/scripts/%s", s_scripts[s_script_selection]);
    printf("Running %s\n", path);
    s_script_output[0] = '\0';
    snprintf(s_script_title, sizeof(s_script_title), "Running %.36s", s_scripts[s_script_selection]);
    snprintf(s_script_status, sizeof(s_script_status), "running...");
    s_page = PAGE_SCRIPT_IO;
    s_current_app = APP_PYTHON;
    ui_draw_current();

    s_script_running = true;
    printf("python memory: stack_free=%u internal_free=%u psram_free=%u\n",
           (unsigned)uxTaskGetStackHighWaterMark(NULL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    py_init(&s_script_py);
    py_set_output_callback(&s_script_py, script_output_callback, NULL);
    py_set_input_callback(&s_script_py, script_input_callback, NULL);
    int ok = py_run_file(&s_script_py, path, NULL, 0);
    char error[PY_MAX_ERROR];
    snprintf(error, sizeof(error), "%s", s_script_py.error);
    printf("Python finished: %s%s%s\n",
           ok ? "ok" : "error",
           error[0] != '\0' ? " - " : "",
           error);
    py_deinit(&s_script_py);
    s_script_running = false;

    if (!ok && error[0] != '\0') {
        script_output_append("\npython error: ");
        script_output_append(error);
        script_output_append("\n");
    } else if (s_script_output[0] == '\0') {
        script_output_append("Done\n");
    }
    snprintf(s_script_title, sizeof(s_script_title), "%.47s", s_scripts[s_script_selection]);
    snprintf(s_script_status, sizeof(s_script_status), ok ? "done - enter/back to scripts" : "error - enter/back to scripts");
    snprintf(s_calc_output, sizeof(s_calc_output), ok ? "ran %s" : "script error", s_scripts[s_script_selection]);
    ui_draw_current();
}

static void list_editor_save_entry(void)
{
    if (s_list_entry[0] == '\0' || s_list_cursor >= LIST_MAX_VALUES) {
        return;
    }

    char *end = NULL;
    double value = strtod(s_list_entry, &end);
    if (end == s_list_entry) {
        snprintf(s_calc_output, sizeof(s_calc_output), "bad number");
        ui_draw_current();
        return;
    }

    s_lists[s_list_index][s_list_cursor] = value;
    if (s_list_cursor == s_list_counts[s_list_index]) {
        s_list_counts[s_list_index]++;
    }
    if (s_list_cursor + 1 <= s_list_counts[s_list_index] && s_list_cursor + 1 < LIST_MAX_VALUES) {
        s_list_cursor++;
    }
    s_list_entry[0] = '\0';
    ui_draw_current();
}

static void list_editor_delete(void)
{
    if (s_list_entry[0] != '\0') {
        size_t len = strlen(s_list_entry);
        s_list_entry[len - 1] = '\0';
    } else if (s_list_cursor < s_list_counts[s_list_index]) {
        for (int i = s_list_cursor + 1; i < s_list_counts[s_list_index]; i++) {
            s_lists[s_list_index][i - 1] = s_lists[s_list_index][i];
        }
        s_list_counts[s_list_index]--;
        if (s_list_cursor > s_list_counts[s_list_index]) {
            s_list_cursor = s_list_counts[s_list_index];
        }
    }
    ui_draw_current();
}

static bool list_editor_append_char(char c)
{
    size_t len = strlen(s_list_entry);
    if (len + 1 >= sizeof(s_list_entry)) {
        return false;
    }
    s_list_entry[len] = c;
    s_list_entry[len + 1] = '\0';
    ui_draw_current();
    return true;
}

static void key_home(void)
{
    if (s_active_game != GAME_NONE) {
        close_active_game_to_menu();
        return;
    }
    s_home_selection = APP_CALCULATOR;
    s_page = PAGE_HOME;
#if OPENCALC_EXPORT_USB_STORAGE_TO_HOST
    s_usb_storage_enabled = usb_msc_mount_usb();
#else
    s_usb_storage_enabled = false;
#endif
    ui_draw_current();
}

static void key_back(void)
{
    if (s_page == PAGE_GRAPH && (s_graph_zoom_mode || s_graph_trace)) {
        s_graph_zoom_mode = false;
        s_graph_trace = false;
        ui_draw_current();
        return;
    }

    if (s_page == PAGE_HOME) {
        s_page = PAGE_CALCULATOR;
        s_current_app = APP_CALCULATOR;
    } else if (s_page == PAGE_CALCULATOR) {
        s_page = PAGE_HOME;
        s_home_selection = APP_CALCULATOR;
    } else if (s_page == PAGE_MATH_MENU) {
        s_page = s_math_return_page;
        s_current_app = s_page == PAGE_Y_EQUALS ? APP_GRAPH : APP_CALCULATOR;
    } else if (s_page == PAGE_GRAPH_WINDOW || s_page == PAGE_GRAPH_CALC || s_page == PAGE_Y_EQUALS) {
        s_page = PAGE_GRAPH;
        s_current_app = APP_GRAPH;
    } else if (s_page == PAGE_LIST_EDITOR) {
        s_page = PAGE_APP;
        s_current_app = APP_LISTS;
    } else if (s_page == PAGE_SCRIPT_IO) {
        scripts_scan();
        s_page = PAGE_SCRIPTS;
        s_current_app = APP_PYTHON;
    } else if (s_page == PAGE_SCRIPT_EDITOR) {
        scripts_scan();
        s_page = PAGE_SCRIPTS;
        s_current_app = APP_PYTHON;
    } else if (s_page == PAGE_SCRIPTS) {
        s_page = PAGE_PROGRAM_MENU;
        s_current_app = APP_PYTHON;
    } else if (s_page == PAGE_PROGRAM_MENU) {
        s_page = PAGE_HOME;
        s_current_app = APP_PYTHON;
    } else if (s_page == PAGE_GAME_MENU) {
        s_page = PAGE_HOME;
        s_current_app = APP_CALCULATOR;
    } else if (s_page == PAGE_MODE_MENU) {
        s_page = PAGE_CALCULATOR;
        s_current_app = APP_CALCULATOR;
    } else {
        s_page = PAGE_HOME;
    }

    ui_draw_current();
}

static void key_enter(void)
{
    if (s_page == PAGE_HOME) {
        ui_open_selected_app();
    } else if (s_page == PAGE_GAME_MENU) {
        launch_selected_game();
    } else if (s_page == PAGE_PROGRAM_MENU) {
        if (s_program_selection == 0) {
            open_scripts_browser_for(SCRIPT_ACTION_RUN);
            ui_draw_current();
        } else if (s_program_selection == 1) {
            open_scripts_browser_for(SCRIPT_ACTION_EDIT);
            ui_draw_current();
        } else if (s_program_selection == 2) {
            script_editor_open_new();
        } else {
            open_scripts_browser_for(SCRIPT_ACTION_DELETE);
            ui_draw_current();
        }
    } else if (s_page == PAGE_SCRIPTS) {
        perform_selected_script_action();
    } else if (s_page == PAGE_SCRIPT_IO) {
        if (!s_script_running) {
            scripts_scan();
            s_page = PAGE_SCRIPTS;
            s_current_app = APP_PYTHON;
            ui_draw_current();
        }
    } else if (s_page == PAGE_SCRIPT_EDITOR) {
        script_editor_insert_text("\n");
        ui_draw_current();
    } else if (s_page == PAGE_CALCULATOR) {
        if (s_calc_history_selected >= 0 && s_calc_history_selected < s_calc_history_count) {
            const char *copy = s_calc_history_select_answer ?
                s_calc_history_result[s_calc_history_selected] :
                s_calc_history_expr[s_calc_history_selected];
            snprintf(s_calc_input, sizeof(s_calc_input), "%s", copy);
            s_calc_cursor = strlen(s_calc_input);
            s_calc_history_selected = -1;
            s_calc_history_select_answer = false;
            ui_draw_current();
            return;
        }
        calc_eval();
    } else if (s_page == PAGE_MATH_MENU) {
        math_menu_insert_selected();
    } else if (s_page == PAGE_Y_EQUALS) {
        s_graph_enabled[s_graph_selection] = !s_graph_enabled[s_graph_selection];
        ui_draw_current();
    } else if (s_page == PAGE_GRAPH) {
        if (!graph_jump_to_nearest_intersection()) {
            s_graph_trace = !s_graph_trace;
            s_graph_trace_x = (s_graph_xmin + s_graph_xmax) / 2.0;
        }
        ui_draw_current();
    } else if (s_page == PAGE_GRAPH_CALC) {
        graph_calc_run_selected();
    } else if (s_page == PAGE_GRAPH_WINDOW) {
        s_page = PAGE_GRAPH;
        s_current_app = APP_GRAPH;
        ui_draw_current();
    } else if (s_page == PAGE_LIST_EDITOR) {
        list_editor_save_entry();
    } else if (s_page == PAGE_MODE_MENU) {
        adjust_mode_value(1);
        ui_draw_current();
    } else if (s_page == PAGE_SETTINGS) {
        if (s_script_selection == 0) {
            adjust_brightness(10);
        } else if (s_script_selection == 1) {
            s_sleep_enabled = !s_sleep_enabled;
            opencalc_persist_set_u32("auto_sleep", s_sleep_enabled ? 1 : 0);
            ui_draw_current();
        } else if (s_script_selection == 2) {
            apply_power_save_mode(!s_power_save_enabled);
            ui_draw_current();
        } else if (s_script_selection == 3) {
            s_light_mode = !s_light_mode;
            opencalc_persist_set_u32("light_mode", s_light_mode ? 1 : 0);
            ui_draw_current();
        } else if (s_script_selection == 4) {
            factory_reset_runtime_state();
        }
    } else if (s_page == PAGE_APP) {
        run_home_app_tool();
    }
}

static int digit_for_key(int row, int col)
{
    if (row == 8 && col >= 1 && col <= 3) {
        return col;
    }
    if (row == 7 && col >= 1 && col <= 3) {
        return col + 3;
    }
    if (row == 6 && col >= 1 && col <= 3) {
        return col + 6;
    }
    if (row == 9 && col == 1) {
        return 0;
    }
    return -1;
}

static bool handle_numbered_menu_key(int row, int col)
{
    int digit = digit_for_key(row, col);
    if (digit <= 0) {
        return false;
    }

    if (s_page == PAGE_MATH_MENU) {
        int index = digit - 1;
        if (index < math_menu_count(s_math_tab)) {
            s_math_selection = index;
            math_menu_insert_selected();
            return true;
        }
    } else if (s_page == PAGE_GRAPH_CALC) {
        int index = digit - 1;
        if (index >= 0 && index < GRAPH_CALC_COUNT) {
            s_graph_calc_selection = index;
            graph_calc_run_selected();
            return true;
        }
    } else if (s_page == PAGE_APP) {
        int count = 0;
        (void)app_tools_for(s_current_app, &count);
        int index = digit - 1;
        if (index < count) {
            s_app_selection = index;
            run_home_app_tool();
            return true;
        }
    } else if (s_page == PAGE_SCRIPTS) {
        int index = digit - 1;
        if (index < s_script_count) {
            s_script_selection = index;
            perform_selected_script_action();
            return true;
        }
    } else if (s_page == PAGE_SETTINGS) {
        int index = digit - 1;
        if (index < SETTINGS_COUNT) {
            s_script_selection = index;
            key_enter();
            return true;
        }
    } else if (s_page == PAGE_PROGRAM_MENU) {
        int index = digit - 1;
        if (index < PROGRAM_MENU_COUNT) {
            s_program_selection = index;
            key_enter();
            return true;
        }
    } else if (s_page == PAGE_LIST_EDITOR) {
        list_editor_append_char((char)('0' + digit));
        return true;
    }

    return false;
}

static bool expression_fraction_move_left(void)
{
    if (s_page != PAGE_CALCULATOR) {
        return false;
    }

    for (size_t i = 0; s_calc_input[i] != '\0'; i++) {
        size_t index_start = 0;
        size_t root_comma = 0;
        size_t root_close = 0;
        if (expression_find_nroot_parts(s_calc_input, i, &index_start, &root_comma, &root_close)) {
            if (s_calc_cursor == root_close + 1) {
                s_calc_cursor = root_close;
                return true;
            }
            if (s_calc_cursor > root_comma && s_calc_cursor <= root_close) {
                s_calc_cursor = root_comma;
                return true;
            }
            i = root_close;
            continue;
        }

        size_t num_start = 0;
        size_t comma = 0;
        size_t close = 0;
        if (!expression_find_frac_parts(s_calc_input, i, &num_start, &comma, &close)) {
            continue;
        }
        if (s_calc_cursor == close + 1) {
            s_calc_cursor = close;
            return true;
        }
        if (s_calc_cursor > comma && s_calc_cursor <= close) {
            s_calc_cursor = comma;
            return true;
        }
        i = close;
    }

    return false;
}

static bool expression_fraction_move_right(void)
{
    if (s_page != PAGE_CALCULATOR) {
        return false;
    }

    for (size_t i = 0; s_calc_input[i] != '\0'; i++) {
        size_t index_start = 0;
        size_t root_comma = 0;
        size_t root_close = 0;
        if (expression_find_nroot_parts(s_calc_input, i, &index_start, &root_comma, &root_close)) {
            if (s_calc_cursor >= index_start && s_calc_cursor <= root_comma) {
                s_calc_cursor = root_comma + 1;
                return true;
            }
            if (s_calc_cursor > root_comma && s_calc_cursor <= root_close) {
                s_calc_cursor = root_close + 1;
                return true;
            }
            i = root_close;
            continue;
        }

        size_t num_start = 0;
        size_t comma = 0;
        size_t close = 0;
        if (!expression_find_frac_parts(s_calc_input, i, &num_start, &comma, &close)) {
            continue;
        }
        if (s_calc_cursor >= num_start && s_calc_cursor <= comma) {
            s_calc_cursor = comma + 1;
            return true;
        }
        if (s_calc_cursor > comma && s_calc_cursor <= close) {
            s_calc_cursor = close + 1;
            return true;
        }
        i = close;
    }

    return false;
}

static void key_left(void)
{
    if (s_page == PAGE_GRAPH) {
        double span = s_graph_xmax - s_graph_xmin;
        if (s_graph_trace) {
            s_graph_trace_x -= span / 40.0;
        } else {
            s_graph_xmin -= span / 10.0;
            s_graph_xmax -= span / 10.0;
        }
        ui_draw_current();
    } else if (s_page == PAGE_GRAPH_WINDOW) {
        adjust_graph_window_value(-0.5);
        ui_draw_current();
    } else if (s_page == PAGE_MODE_MENU) {
        adjust_mode_value(-1);
        ui_draw_current();
    } else if (s_page == PAGE_TABLE) {
        if (s_table_func_start >= 3) {
            s_table_func_start -= 3;
        } else {
            s_table_func_start = 0;
        }
        ui_draw_current();
    } else if (s_page == PAGE_HOME && s_home_selection % UI_ICON_COLS > 0) {
        s_home_selection--;
        ui_draw_current();
    } else if (s_page == PAGE_MATH_MENU && s_math_tab > 0) {
        s_math_tab--;
        s_math_selection = 0;
        ui_draw_current();
    } else if (s_page == PAGE_SETTINGS && s_script_selection == 0) {
        adjust_brightness(-10);
    } else if (s_page == PAGE_LIST_EDITOR) {
        s_list_index = (s_list_index + LIST_COUNT - 1) % LIST_COUNT;
        if (s_list_cursor > s_list_counts[s_list_index]) s_list_cursor = s_list_counts[s_list_index];
        s_list_entry[0] = '\0';
        ui_draw_current();
    } else if (s_page == PAGE_CALCULATOR && s_calc_history_selected >= 0) {
        s_calc_history_select_answer = false;
        ui_draw_current();
    } else if (s_page == PAGE_CALCULATOR && s_calc_cursor > 0) {
        if (!expression_fraction_move_left()) {
            s_calc_cursor--;
        }
        ui_draw_current();
    } else if (s_page == PAGE_APP && s_app_selection > 0) {
        s_app_selection--;
        ui_draw_current();
    }
}

static void key_right(void)
{
    if (s_page == PAGE_GRAPH) {
        double span = s_graph_xmax - s_graph_xmin;
        if (s_graph_trace) {
            s_graph_trace_x += span / 40.0;
        } else {
            s_graph_xmin += span / 10.0;
            s_graph_xmax += span / 10.0;
        }
        ui_draw_current();
    } else if (s_page == PAGE_GRAPH_WINDOW) {
        adjust_graph_window_value(0.5);
        ui_draw_current();
    } else if (s_page == PAGE_MODE_MENU) {
        adjust_mode_value(1);
        ui_draw_current();
    } else if (s_page == PAGE_TABLE) {
        if (s_table_func_start + 3 < 10) {
            s_table_func_start += 3;
            if (s_table_func_start > 7) {
                s_table_func_start = 7;
            }
        }
        ui_draw_current();
    } else if (s_page == PAGE_HOME && s_home_selection % UI_ICON_COLS < UI_ICON_COLS - 1 && s_home_selection + 1 < UI_APP_COUNT) {
        s_home_selection++;
        ui_draw_current();
    } else if (s_page == PAGE_MATH_MENU && s_math_tab < 3) {
        s_math_tab++;
        s_math_selection = 0;
        ui_draw_current();
    } else if (s_page == PAGE_SETTINGS && s_script_selection == 0) {
        adjust_brightness(10);
    } else if (s_page == PAGE_LIST_EDITOR) {
        s_list_index = (s_list_index + 1) % LIST_COUNT;
        if (s_list_cursor > s_list_counts[s_list_index]) s_list_cursor = s_list_counts[s_list_index];
        s_list_entry[0] = '\0';
        ui_draw_current();
    } else if (s_page == PAGE_CALCULATOR && s_calc_history_selected >= 0) {
        s_calc_history_select_answer = true;
        ui_draw_current();
    } else if (s_page == PAGE_CALCULATOR && s_calc_cursor < strlen(s_calc_input)) {
        if (!expression_fraction_move_right()) {
            s_calc_cursor++;
        }
        ui_draw_current();
    } else if (s_page == PAGE_APP) {
        int count = 0;
        (void)app_tools_for(s_current_app, &count);
        if (s_app_selection + 1 < count) {
            s_app_selection++;
            ui_draw_current();
        }
    }
}

static void key_up(void)
{
    if (s_page == PAGE_GRAPH) {
        if (s_graph_zoom_mode) {
            zoom_graph(0.8);
            return;
        } else {
            double span = s_graph_ymax - s_graph_ymin;
            s_graph_ymin += span / 10.0;
            s_graph_ymax += span / 10.0;
        }
    } else if (s_page == PAGE_GRAPH_WINDOW && s_graph_window_selection > 0) {
        s_graph_window_selection--;
    } else if (s_page == PAGE_MODE_MENU && s_mode_selection > 0) {
        s_mode_selection--;
    } else if (s_page == PAGE_TABLE) {
        s_table_x_start -= 1.0;
    } else if (s_page == PAGE_HOME && s_home_selection >= UI_ICON_COLS) {
        s_home_selection -= UI_ICON_COLS;
    } else if (s_page == PAGE_CALCULATOR && s_calc_history_count > 0) {
        if (s_calc_history_selected < 0) {
            s_calc_history_selected = s_calc_history_count - 1;
            s_calc_history_select_answer = true;
        } else if (s_calc_history_select_answer) {
            s_calc_history_select_answer = false;
        } else if (s_calc_history_selected > 0) {
            s_calc_history_selected--;
            s_calc_history_select_answer = true;
        }
    } else if (s_page == PAGE_Y_EQUALS && s_graph_selection > 0) {
        s_graph_selection--;
    } else if (s_page == PAGE_MATH_MENU && s_math_selection > 0) {
        s_math_selection--;
    } else if (s_page == PAGE_GRAPH_CALC && s_graph_calc_selection > 0) {
        s_graph_calc_selection--;
    } else if (s_page == PAGE_APP && s_app_selection > 0) {
        s_app_selection--;
    } else if (s_page == PAGE_LIST_EDITOR && s_list_cursor > 0) {
        s_list_cursor--;
        s_list_entry[0] = '\0';
    } else if ((s_page == PAGE_SCRIPTS || s_page == PAGE_SETTINGS) && s_script_selection > 0) {
        s_script_selection--;
    } else if (s_page == PAGE_PROGRAM_MENU && s_program_selection > 0) {
        s_program_selection--;
    } else if (s_page == PAGE_GAME_MENU && s_game_selection > 0) {
        s_game_selection--;
    }
    ui_draw_current();
}

static void key_down(void)
{
    if (s_page == PAGE_GRAPH) {
        if (s_graph_zoom_mode) {
            zoom_graph(1.25);
            return;
        } else {
            double span = s_graph_ymax - s_graph_ymin;
            s_graph_ymin -= span / 10.0;
            s_graph_ymax -= span / 10.0;
        }
    } else if (s_page == PAGE_GRAPH_WINDOW && s_graph_window_selection < 3) {
        s_graph_window_selection++;
    } else if (s_page == PAGE_MODE_MENU && s_mode_selection < 4) {
        s_mode_selection++;
    } else if (s_page == PAGE_TABLE) {
        s_table_x_start += 1.0;
    } else if (s_page == PAGE_HOME && s_home_selection + UI_ICON_COLS < UI_APP_COUNT) {
        s_home_selection += UI_ICON_COLS;
    } else if (s_page == PAGE_CALCULATOR && s_calc_history_selected >= 0) {
        if (!s_calc_history_select_answer) {
            s_calc_history_select_answer = true;
        } else if (s_calc_history_selected + 1 < s_calc_history_count) {
            s_calc_history_selected++;
            s_calc_history_select_answer = false;
        } else {
            s_calc_history_selected = -1;
            s_calc_history_select_answer = false;
        }
    } else if (s_page == PAGE_Y_EQUALS && s_graph_selection < 9) {
        s_graph_selection++;
    } else if (s_page == PAGE_MATH_MENU && s_math_selection + 1 < math_menu_count(s_math_tab)) {
        s_math_selection++;
    } else if (s_page == PAGE_GRAPH_CALC && s_graph_calc_selection + 1 < GRAPH_CALC_COUNT) {
        s_graph_calc_selection++;
    } else if (s_page == PAGE_APP) {
        int count = 0;
        (void)app_tools_for(s_current_app, &count);
        if (s_app_selection + 1 < count) {
            s_app_selection++;
        }
    } else if (s_page == PAGE_LIST_EDITOR && s_list_cursor < s_list_counts[s_list_index] && s_list_cursor + 1 < LIST_MAX_VALUES) {
        s_list_cursor++;
        s_list_entry[0] = '\0';
    } else if (s_page == PAGE_SCRIPTS && s_script_selection + 1 < s_script_count) {
        s_script_selection++;
    } else if (s_page == PAGE_SETTINGS && s_script_selection + 1 < SETTINGS_COUNT) {
        s_script_selection++;
    } else if (s_page == PAGE_PROGRAM_MENU && s_program_selection + 1 < PROGRAM_MENU_COUNT) {
        s_program_selection++;
    } else if (s_page == PAGE_GAME_MENU && s_game_selection < 4) {
        s_game_selection++;
    }
    ui_draw_current();
}

static void dispatch_key(int row, int col)
{
    printf("key r%d c%d\n", row, col);
    bool had_second_active = s_second_active;

    if (row == 1 && col == 0) {
        if (s_alpha_locked) {
            s_second_active = true;
            s_alpha_active = false;
            s_alpha_locked = false;
        } else if (s_alpha_active) {
            s_second_active = false;
            s_alpha_active = false;
            open_game_menu();
        } else {
            s_second_active = !s_second_active;
            s_alpha_active = false;
        }
        ui_draw_current();
        return;
    }

    if (row == 2 && col == 0) {
        if (s_second_active) {
            s_alpha_locked = !s_alpha_locked;
            s_alpha_active = s_alpha_locked;
            s_second_active = false;
        } else {
            s_alpha_active = !s_alpha_active;
            s_second_active = false;
        }
        ui_draw_current();
        return;
    }

    if (row == 9 && col == 0) {
        if (s_second_active) {
            s_second_active = false;
            power_off_calculator();
        } else {
            key_home();
        }
        return;
    }

    if (s_second_active && row == 1 && col == 4) {
        s_second_active = false;
        adjust_brightness(10);
        return;
    }

    if (s_second_active && row == 2 && col == 3) {
        s_second_active = false;
        adjust_brightness(-10);
        return;
    }

    if (s_page == PAGE_SCRIPT_EDITOR && script_editor_handle_key(row, col)) {
        goto finish_key;
    }

    if (handle_alpha_insert(row, col)) {
        goto finish_key;
    }

    if (handle_numbered_menu_key(row, col)) {
        goto finish_key;
    }

    if (s_page == PAGE_LIST_EDITOR) {
        if (row == 9 && col == 2) {
            list_editor_append_char('.');
        } else if (row == 9 && col == 3) {
            if (s_list_entry[0] == '-') {
                memmove(s_list_entry, s_list_entry + 1, strlen(s_list_entry));
                ui_draw_current();
            } else if (strlen(s_list_entry) + 1 < sizeof(s_list_entry)) {
                memmove(s_list_entry + 1, s_list_entry, strlen(s_list_entry) + 1);
                s_list_entry[0] = '-';
                ui_draw_current();
            }
        } else if (row == 2 && col == 2) {
            key_back();
        } else if (row == 3 && col == 4) {
            if (s_second_active) {
                s_list_entry[0] = '\0';
                ui_draw_current();
            } else {
                list_editor_delete();
            }
        } else {
            printf("unhandled list key r%d c%d\n", row, col);
        }
        goto finish_key;
    }

    if (row == 0 && col == 0) {
        if (s_second_active) {
            open_app(APP_STATS);
            status_message("plot setup");
            goto finish_key;
        }
        s_page = PAGE_Y_EQUALS;
        s_current_app = APP_GRAPH;
        ui_draw_current();
    } else if (row == 0 && col == 1) {
        if (s_second_active) {
            open_app(APP_TABLE);
            status_message("table setup");
            goto finish_key;
        }
        open_graph_window_menu();
    } else if (row == 0 && col == 2) {
        if (s_second_active) {
            s_graph_grid = !s_graph_grid;
            s_page = PAGE_GRAPH;
            s_current_app = APP_GRAPH;
            status_message(s_graph_grid ? "format: grid on" : "format: grid off");
            goto finish_key;
        }
        enter_graph_zoom_mode();
    } else if (row == 0 && col == 3) {
        if (s_second_active) {
            s_second_active = false;
            open_graph_calc_menu();
            goto finish_key;
        }
        if (!graph_trace_cycle_line()) {
            s_graph_trace = false;
        }
        ui_draw_current();
    } else if (row == 0 && col == 4) {
        open_app(s_second_active ? APP_TABLE : APP_GRAPH);
    } else if (s_page == PAGE_GRAPH && row == 1 && col == 2) {
        open_app(s_second_active ? APP_LISTS : APP_STATS);
    } else
    if (row == 1 && col == 3) key_left();
    else if (row == 1 && col == 4) key_up();
    else if (row == 2 && col == 3) key_down();
    else if (row == 2 && col == 4) key_right();
    else if (row == 9 && col == 4) key_enter();
    else if (row == 1 && col == 1) {
        if (s_second_active) {
            key_home();
            goto finish_key;
        }
        s_page = PAGE_MODE_MENU;
        s_current_app = APP_CALCULATOR;
        ui_draw_current();
    }
    else if (row == 1 && col == 2) open_app(s_second_active ? APP_LISTS : APP_STATS);
    else if (row == 3 && col == 2) {
        if (s_second_active) {
            open_scripts_browser();
            status_message("scripts");
        } else {
            open_program_menu();
        }
    }
    else if (row == 3 && col == 0) {
        if (s_second_active) {
            expression_append("=");
            ui_draw_current();
        } else {
            open_math_menu(0);
        }
    }
    else {
        if (row == 8 && col >= 1 && col <= 3) expression_append((const char *[]){"1","2","3"}[col - 1]);
        else if (row == 7 && col >= 1 && col <= 3) expression_append((const char *[]){"4","5","6"}[col - 1]);
        else if (row == 6 && col >= 1 && col <= 3) expression_append((const char *[]){"7","8","9"}[col - 1]);
        else if (row == 9 && col == 1) expression_append("0");
        else if (row == 9 && col == 2) expression_append(".");
        else if (row == 5 && col == 4) expression_append("+");
        else if (row == 7 && col == 4) expression_append("-");
        else if (row == 6 && col == 4) expression_append("*");
        else if (row == 8 && col == 4) expression_append(s_second_active ? "%" : "/");
        else if (row == 2 && col == 2) key_back();
        else if (row == 3 && col == 4) {
            if (s_second_active) {
                expression_clear();
            } else {
                expression_delete();
            }
        }
        else if (row == 2 && col == 1) expression_append("x");
        else if (row == 4 && col == 0) {
            if (s_second_active) {
                expression_insert_nroot_box();
            } else {
                expression_append("sqrt(");
            }
        }
        else if (row == 4 && col == 1) expression_append(s_second_active ? "asin(" : "sin(");
        else if (row == 4 && col == 2) expression_append(s_second_active ? "acos(" : "cos(");
        else if (row == 4 && col == 3) expression_append(s_second_active ? "atan(" : "tan(");
        else if (row == 4 && col == 4) expression_append(s_second_active ? "e" : "pi");
        else if (row == 5 && col == 0) {
            if (s_second_active) {
                expression_insert_power_box();
            } else {
                expression_append("^2");
            }
        }
        else if (row == 5 && col == 1) expression_append(s_second_active ? "E" : ",");
        else if (row == 5 && col == 2) expression_append(s_second_active ? "{" : "(");
        else if (row == 5 && col == 3) expression_append(s_second_active ? "}" : ")");
        else if (row == 6 && col == 0) {
            if (s_second_active) {
                expression_append_power_base("10");
            } else {
                expression_append("log(");
            }
        }
        else if (row == 7 && col == 0) {
            if (s_second_active) {
                expression_append_power_base("e");
            } else {
                expression_append("ln(");
            }
        }
        else if (row == 8 && col == 0) {
            if (s_second_active) {
                expression_append("ANS");
            } else {
                store_answer();
            }
        }
        else if (row == 9 && col == 3) expression_append(s_second_active ? "ANS" : "-");
        else if (row == 3 && col == 1) {
            if (s_second_active) {
                expression_append("/");
            } else {
                expression_insert_fraction_box();
            }
        }
        else if (row == 3 && col == 3) expression_append(s_second_active ? "convert(" : "ANS");
        else {
            printf("unhandled key r%d c%d\n", row, col);
        }
        if (s_page == PAGE_CALCULATOR || s_page == PAGE_Y_EQUALS) {
            ui_draw_current();
        }
    }

finish_key:
    if (!(row == 1 && col == 0) && !(row == 2 && col == 0)) {
        s_second_active = false;
        s_alpha_active = false;
        if (had_second_active) {
            ui_draw_current();
        }
    }
}

static bool active_game_press_button_number(int number)
{
    if (s_active_game == GAME_NONE) {
        return false;
    }

    if (number == 46) {
        close_active_game_to_menu();
        return true;
    }

    switch (s_active_game) {
    case GAME_TETRIS: return opencalc_tetris_press_button_number(number);
    case GAME_DOOM: return opencalc_doom_press_button_number(number);
    case GAME_SNAKE: return opencalc_snake_press_button_number(number);
    case GAME_BREAKOUT: return opencalc_breakout_press_button_number(number);
    case GAME_MARIO: return opencalc_mario_press_button_number(number);
    default: return false;
    }
}

static bool active_game_physical_press_button_number(int number)
{
    if (s_active_game == GAME_NONE) {
        return false;
    }

    if (number == 46) {
        close_active_game_to_menu();
        return true;
    }

    switch (s_active_game) {
    case GAME_DOOM:
    case GAME_MARIO:
        /* These games read the complete physical matrix every frame. */
        return true;
    case GAME_BREAKOUT:
        /* Left/right are level-based; pause and launch remain edge-based. */
        if (number == 9 || number == 15) {
            return true;
        }
        return opencalc_breakout_press_button_number(number);
    case GAME_TETRIS:
        return opencalc_tetris_press_button_number(number);
    case GAME_SNAKE:
        return opencalc_snake_press_button_number(number);
    default:
        return false;
    }
}

static void debug_log_keypad_press(int number, int row, int col)
{
#if OPENCALC_DEBUG_LOG_KEYPAD_PRESSES
    const board_key_t *key = board_keypad_key_at(row, col);
    printf("keypad button %d -> r%d c%d %s\n",
           number,
           row,
           col,
           key && key->normal ? key->normal : "unmapped");
    fflush(stdout);
#else
    (void)number;
    (void)row;
    (void)col;
#endif
}

static void debug_log_raw_keypad_levels(void)
{
#if OPENCALC_DEBUG_LOG_RAW_KEYPAD_LEVELS
    static TickType_t last_log_tick = 0;
    TickType_t now = xTaskGetTickCount();
    if (last_log_tick != 0 && now - last_log_tick < pdMS_TO_TICKS(1000)) {
        return;
    }

    char rows[BOARD_KEYPAD_ROWS + 1];
    char cols[BOARD_KEYPAD_COLS + 1];
    if (board_keypad_read_raw_levels(rows, cols)) {
        printf("keypad raw rows=%s cols=%s\n", rows, cols);
        fflush(stdout);
    }
    last_log_tick = now;
#endif
}

bool opencalc_ui_press_button_number(int number)
{
    if (number < 1 || number > BOARD_KEYPAD_ROWS * BOARD_KEYPAD_COLS) {
        printf("button number must be 1-%d\n", BOARD_KEYPAD_ROWS * BOARD_KEYPAD_COLS);
        return false;
    }

    int index = number - 1;
    int row = index / BOARD_KEYPAD_COLS;
    int col = index % BOARD_KEYPAD_COLS;
    const board_key_t *key = board_keypad_key_at(row, col);
    if (key == NULL) {
        printf("button %d has no mapped key\n", number);
        return false;
    }

    if (s_active_game != GAME_NONE) {
        return active_game_press_button_number(number);
    }

    printf("serial button %d -> r%d c%d %s\n", number, row, col, key->normal);
    dispatch_key(row, col);
    return true;
}

bool opencalc_ui_queue_button_number(int number)
{
    if (s_serial_button_queue == NULL) {
        return false;
    }
    if (xQueueSend(s_serial_button_queue, &number, 0) != pdTRUE) {
        printf("serial button queue full\n");
        return false;
    }
    return true;
}

void opencalc_ui_handle_serial_buttons(void)
{
    if (s_serial_button_queue == NULL) {
        return;
    }

    int number = 0;
    while (xQueueReceive(s_serial_button_queue, &number, 0) == pdTRUE) {
        opencalc_ui_press_button_number(number);
    }
}

void opencalc_ui_handle_keypad_interrupt(void)
{
    static bool game_prev[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS] = {0};
    static bool ui_prev[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS] = {0};

    debug_log_raw_keypad_levels();

    if (s_active_game != GAME_NONE) {
        bool matrix[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS];
        if (!board_keypad_scan_matrix(matrix)) {
            memset(game_prev, 0, sizeof(game_prev));
            vTaskDelay(pdMS_TO_TICKS(1));
            return;
        }
        for (int row = 0; row < BOARD_KEYPAD_ROWS; row++) {
            for (int col = 0; col < BOARD_KEYPAD_COLS; col++) {
                if (matrix[row][col] && !game_prev[row][col]) {
                    int number = row * BOARD_KEYPAD_COLS + col + 1;
                    debug_log_keypad_press(number, row, col);
                    active_game_physical_press_button_number(number);
                }
                game_prev[row][col] = matrix[row][col];
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        return;
    }

#if OPENCALC_KEYPAD_POLL_WHEN_NO_INTERRUPT
    (void)board_keypad_take_interrupt();
#else
    bool had_interrupt = board_keypad_take_interrupt();
    bool had_pressed_key = false;
    for (int row = 0; row < BOARD_KEYPAD_ROWS && !had_pressed_key; row++) {
        for (int col = 0; col < BOARD_KEYPAD_COLS; col++) {
            if (ui_prev[row][col]) {
                had_pressed_key = true;
                break;
            }
        }
    }
    if (!had_interrupt && !had_pressed_key) {
        return;
    }
#endif

    bool matrix[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS];
    if (!board_keypad_scan_matrix(matrix)) {
        memset(ui_prev, 0, sizeof(ui_prev));
        return;
    }

    for (int row = 0; row < BOARD_KEYPAD_ROWS; row++) {
        for (int col = 0; col < BOARD_KEYPAD_COLS; col++) {
            if (matrix[row][col] && !ui_prev[row][col]) {
                debug_log_keypad_press(row * BOARD_KEYPAD_COLS + col + 1, row, col);
                dispatch_key(row, col);
            }
            ui_prev[row][col] = matrix[row][col];
        }
    }
}

static void handle_touch_tap(int raw_x, int raw_y)
{
    enum {
        TOUCH_MIN_X = 200,
        TOUCH_MAX_X = 3900,
        TOUCH_MIN_Y = 200,
        TOUCH_MAX_Y = 3900,
        TOUCH_SWAP_XY = 0,
        TOUCH_INVERT_X = 0,
        TOUCH_INVERT_Y = 0,
    };

    int tx = raw_x;
    int ty = raw_y;
    if (TOUCH_SWAP_XY) {
        int tmp = tx;
        tx = ty;
        ty = tmp;
    }
    if (tx < TOUCH_MIN_X) tx = TOUCH_MIN_X;
    if (tx > TOUCH_MAX_X) tx = TOUCH_MAX_X;
    if (ty < TOUCH_MIN_Y) ty = TOUCH_MIN_Y;
    if (ty > TOUCH_MAX_Y) ty = TOUCH_MAX_Y;

    int x = (tx - TOUCH_MIN_X) * UI_W / (TOUCH_MAX_X - TOUCH_MIN_X);
    int y = (ty - TOUCH_MIN_Y) * UI_H / (TOUCH_MAX_Y - TOUCH_MIN_Y);
    if (TOUCH_INVERT_X) {
        x = UI_W - 1 - x;
    }
    if (TOUCH_INVERT_Y) {
        y = UI_H - 1 - y;
    }
    if (x < 0) {
        x = 0;
    } else if (x >= UI_W) {
        x = UI_W - 1;
    }
    if (y < 0) {
        y = 0;
    } else if (y >= UI_H) {
        y = UI_H - 1;
    }

    printf("touch raw=%d,%d screen=%d,%d\n", raw_x, raw_y, x, y);

    if (s_page != PAGE_HOME) {
        return;
    }

    for (int i = 0; i < UI_APP_COUNT; i++) {
        int col = i % UI_ICON_COLS;
        int row = i / UI_ICON_COLS;
        int ix = 8 + col * 78;
        int iy = UI_HOME_GRID_Y + row * UI_HOME_GRID_STEP_Y;
        if (x >= ix && x < ix + 70 && y >= iy && y < iy + 46) {
            s_home_selection = i;
            ui_open_selected_app();
            return;
        }
    }

    int grid_top = UI_HOME_GRID_Y - 8;
    int grid_bottom = UI_HOME_GRID_Y + 2 * UI_HOME_GRID_STEP_Y + 54;
    if (y >= grid_top && y < grid_bottom) {
        int col = (x - 8 + 39) / 78;
        int row = (y - UI_HOME_GRID_Y + UI_HOME_GRID_STEP_Y / 2) / UI_HOME_GRID_STEP_Y;
        if (col < 0) col = 0;
        if (col >= UI_ICON_COLS) col = UI_ICON_COLS - 1;
        if (row < 0) row = 0;
        if (row >= 3) row = 2;

        int app = row * UI_ICON_COLS + col;
        if (app >= 0 && app < UI_APP_COUNT) {
            s_home_selection = app;
            ui_open_selected_app();
        }
    }
}

void opencalc_ui_handle_touch_interrupt(void)
{
    static bool was_touched = false;
    bool had_interrupt = board_touch_take_interrupt();

    if (s_page != PAGE_HOME) {
        was_touched = false;
        if (!had_interrupt) {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        return;
    }

    if (had_interrupt) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    int x = 0;
    int y = 0;
    bool touched = board_touch_scan(&x, &y);

    if (touched && !was_touched) {
        handle_touch_tap(x, y);
    }

    was_touched = touched;

    if (!touched) {
        (void)board_touch_take_interrupt();
    }

    if (!had_interrupt) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void opencalc_ui_start_worker(void)
{
    if (s_work_queue == NULL) {
        s_work_queue = xQueueCreate(4, sizeof(ui_work_job_t));
    }
    if (s_work_result_queue == NULL) {
        s_work_result_queue = xQueueCreate(4, sizeof(ui_work_result_t));
    }
    if (s_work_task == NULL && s_work_queue != NULL && s_work_result_queue != NULL) {
        BaseType_t ok = xTaskCreatePinnedToCore(ui_work_task,
                                                "opencalc_work",
                                                24576,
                                                NULL,
                                                5,
                                                &s_work_task,
                                                OPENCALC_WORKER_CORE);
        if (ok != pdPASS) {
            s_work_task = NULL;
            printf("ERROR: failed to start math worker; calculator will run eval inline\n");
        }
    }
}

void opencalc_ui_init(void)
{
    if (s_serial_button_queue == NULL) {
        s_serial_button_queue = xQueueCreate(8, sizeof(int));
    }
    opencalc_ui_start_worker();
    s_light_mode = opencalc_persist_get_u32("light_mode", 0) != 0;
    s_sleep_enabled = opencalc_persist_get_u32("auto_sleep", 1) != 0;
    board_set_backlight_brightness((int)opencalc_persist_get_u32("brightness", 80));
    s_power_save_saved_brightness = board_get_backlight_brightness();
    apply_power_save_mode(opencalc_persist_get_u32("power_save", 0) != 0);
    s_doom_high_score = opencalc_persist_get_u32("hs_doom", 0);
    s_doom_last_saved_high_score = s_doom_high_score;
    opencalc_tetris_init();
    opencalc_snake_init();
    opencalc_breakout_init();
    opencalc_mario_init();
}

void opencalc_ui_draw(void)
{
    ui_draw_current();
}

void opencalc_ui_tick(void)
{
    ui_work_poll_results();

    s_cursor_blink_visible = ((esp_timer_get_time() / 450000) % 2) == 0;
    if (s_cursor_blink_visible == s_cursor_blink_last_visible) {
        return;
    }
    s_cursor_blink_last_visible = s_cursor_blink_visible;

    if (s_page == PAGE_CALCULATOR || s_page == PAGE_Y_EQUALS) {
        ui_draw_current();
    }
}

bool opencalc_ui_doom_active(void)
{
    return s_active_game != GAME_NONE;
}

void opencalc_ui_tick_doom(void)
{
    switch (s_active_game) {
    case GAME_TETRIS:
        opencalc_tetris_tick();
        if (!opencalc_tetris_active()) close_active_game_to_menu();
        break;
    case GAME_DOOM:
        opencalc_doom_tick();
        break;
    case GAME_SNAKE:
        opencalc_snake_tick();
        if (!opencalc_snake_active()) close_active_game_to_menu();
        break;
    case GAME_BREAKOUT:
        opencalc_breakout_tick();
        if (!opencalc_breakout_active()) close_active_game_to_menu();
        break;
    case GAME_MARIO:
        opencalc_mario_tick();
        if (!opencalc_mario_active()) close_active_game_to_menu();
        break;
    default:
        break;
    }
}
