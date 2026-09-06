// Cory Pearl
// 05/22/26

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <dirent.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <ctype.h>
#include <complex.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "board_init.h"
#include "opencalc_breakout.h"
#include "opencalc_calc.h"
#include "opencalc_cas.h"
#include "opencalc_conics.h"
#include "opencalc_config.h"
#include "opencalc_doom.h"
#include "opencalc_audio.h"
#include "opencalc_giac.h"
#include "opencalc_graph_model.h"
#include "opencalc_inequality.h"
#include "opencalc_mario.h"
#include "opencalc_math.h"
#include "opencalc_persist.h"
#include "opencalc_power.h"
#include "opencalc_snake.h"
#include "opencalc_stats.h"
#include "opencalc_units.h"
#include "opencalc_tetris.h"
#include "opencalc_ui.h"
#include "opencalc_ui_canvas.h"
#include "opencalc_ui_navigation.h"
#include "opencalc_ui_work.h"
#include "opencalc_worksheet_model.h"
#include "opencalc_workspace_io.h"
#include "opencalc_reference.h"
#include "opencalc_sensor_hub.h"
#include "opencalc_script_model.h"
#include "tiny-python.h"
#include "usb_msc.h"

static bool s_light_mode = false;

#define UI_W OPENCALC_UI_WIDTH
#define UI_H OPENCALC_UI_HEIGHT
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
_Static_assert(SCRIPT_EDITOR_MAX == OPENCALC_SCRIPT_EDITOR_CAPACITY,
               "script editor capacity mismatch");
#define SCRIPT_EDITOR_VISIBLE_LINES 13
#define SCRIPT_EDITOR_VISIBLE_COLS 48
#define SCRIPT_BREAKPOINT_MAX 256
#define SCRIPT_GRAPHICS_MAX 64
#define SETTINGS_COUNT 6
#define PROGRAM_MENU_COUNT 5
#define CALC_HISTORY_MAX OPENCALC_CALC_HISTORY_MAX
#define CALC_EXPR_MAX OPENCALC_CALC_EXPR_MAX
#define CALC_RESULT_MAX OPENCALC_CALC_RESULT_MAX
#define LIST_COUNT 6
#define LIST_MAX_VALUES 999
#define MATRIX_COUNT 10
#define MATRIX_MAX_N 99
#define worksheet_crc32_update opencalc_workspace_crc32_update
#define worksheet_write_payload opencalc_workspace_write_payload
#define worksheet_read_payload opencalc_workspace_read_payload

_Static_assert(LIST_COUNT == OPENCALC_WORKSHEET_LIST_COUNT, "worksheet list count mismatch");
_Static_assert(LIST_MAX_VALUES == OPENCALC_WORKSHEET_LIST_CAPACITY, "worksheet list capacity mismatch");
_Static_assert(MATRIX_COUNT == OPENCALC_WORKSHEET_MATRIX_COUNT, "worksheet matrix count mismatch");
_Static_assert(MATRIX_MAX_N == OPENCALC_WORKSHEET_MATRIX_MAX_N, "worksheet matrix size mismatch");
#define SOLVER_POLY_ROOT_MAX 10
#define SOLVER_SAVED_MAX 5
#define INEQ_MAX 6
#define GRAPH_FUNC_COUNT 10
#define GRAPH_PARAM_COUNT 6
#define GRAPH_POLAR_COUNT 6
#define GRAPH_SEQ_COUNT 3
#define GRAPH_POI_LIMIT 16
#define GRAPH_COLOR_COUNT 15
#define GRAPH_FORMAT_COUNT 6
_Static_assert(GRAPH_FUNC_COUNT == OPENCALC_GRAPH_FUNCTION_COUNT, "graph function count mismatch");
_Static_assert(GRAPH_PARAM_COUNT == OPENCALC_GRAPH_PARAM_COUNT, "graph param count mismatch");
_Static_assert(GRAPH_POLAR_COUNT == OPENCALC_GRAPH_POLAR_COUNT, "graph polar count mismatch");
_Static_assert(GRAPH_SEQ_COUNT == OPENCALC_GRAPH_SEQUENCE_COUNT, "graph sequence count mismatch");
#define GRAPH_BACKGROUND_PATH "/data/graph.bmp"
#define WORKSHEET_DIR "/data/.opencalc"
#define WORKSHEET_PATH WORKSHEET_DIR "/worksheet.bin"
#define WORKSHEET_TEMP_PATH WORKSHEET_DIR "/worksheet.tmp"
#define WORKSHEET_BACKUP_PATH WORKSHEET_DIR "/worksheet.bak"
#define WORKSHEET_JOURNAL_PATH WORKSHEET_DIR "/worksheet.journal"
#define WORKSHEET_MAGIC 0x4f435753u
#define WORKSHEET_VERSION 2u
#define WORKSHEET_JOURNAL_MAGIC 0x4f434a52u
#define WORKSHEET_JOURNAL_VERSION 1u
#define WORKSHEET_CHECKPOINT_BYTES (1536U * 1024U)
#define WORKSHEET_SAVE_DELAY_US 1500000LL

enum {
    WORKSHEET_DIRTY_FIXED = 1u << 0,
    WORKSHEET_DIRTY_LISTS = 1u << 1,
    WORKSHEET_DIRTY_MATRICES = 1u << 2,
    WORKSHEET_DIRTY_ALL = WORKSHEET_DIRTY_FIXED | WORKSHEET_DIRTY_LISTS |
                          WORKSHEET_DIRTY_MATRICES,
};

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
    PAGE_CALC_RESULT,
    PAGE_MATH_MENU,
    PAGE_GRAPH,
    PAGE_Y_EQUALS,
    PAGE_TABLE,
    PAGE_TABLE_SETUP,
    PAGE_GRAPH_WINDOW,
    PAGE_GRAPH_CALC,
    PAGE_GRAPH_SYMBOLIC,
    PAGE_GRAPH_FORMAT,
    PAGE_LIST_EDITOR,
    PAGE_MATRIX_EDITOR,
    PAGE_MATRIX_VIEWER,
    PAGE_MATRIX_SIZE,
    PAGE_FINANCE_TVM,
    PAGE_FINANCE_CASHFLOW,
    PAGE_FINANCE_RESULT,
    PAGE_MODE_MENU,
    PAGE_PROGRAM_MENU,
    PAGE_GAME_MENU,
    PAGE_SCRIPT_IO,
    PAGE_SCRIPT_EDITOR,
    PAGE_SOLVER_WORKFLOW,
    PAGE_SOLVER_SYSTEM_RESULT,
    PAGE_SOLVER_SYMBOLIC_RESULT,
    PAGE_SOLVER_SAVED,
    PAGE_SOLVER_ROOTS,
    PAGE_STATS_CATEGORY,
    PAGE_STATS_ONE_VAR,
    PAGE_STATS_SETUP,
    PAGE_STATS_RESULT,
    PAGE_STATS_PLOT,
    PAGE_REFERENCE_HOME,
    PAGE_PERIODIC_TABLE,
    PAGE_ELEMENT_DETAIL,
    PAGE_REFERENCE_LIST,
    PAGE_REFERENCE_DETAIL,
    PAGE_CONIC_EDITOR,
    PAGE_CONIC_RESULT,
    PAGE_CONIC_GRAPHS,
    PAGE_INEQ_EDITOR,
    PAGE_INEQ_RESULT,
    PAGE_VARIABLES,
    PAGE_VARIABLE_NAME,
} page_id_t;

typedef enum {
    VARIABLE_ACTION_INSERT = 0,
    VARIABLE_ACTION_GET,
    VARIABLE_ACTION_STORE,
} variable_action_t;

typedef enum {
    VARIABLE_CATEGORY_USER = 0,
    VARIABLE_CATEGORY_LISTS,
    VARIABLE_CATEGORY_MATRICES,
    VARIABLE_CATEGORY_FUNCTIONS,
    VARIABLE_CATEGORY_STRINGS,
    VARIABLE_CATEGORY_STATISTICS,
    VARIABLE_CATEGORY_GRAPH,
    VARIABLE_CATEGORY_SYSTEM,
    VARIABLE_CATEGORY_COUNT,
} variable_category_t;

typedef struct {
    char name[OPENCALC_VARIABLE_NAME_MAX];
    char type[16];
    char preview[72];
    char reference[CALC_EXPR_MAX];
    char value[CALC_EXPR_MAX];
    bool selectable;
    bool user_variable;
} variable_browser_item_t;

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
    {"Statistics", "S", 0x6f9ee8, {"Guided summaries", "Regression and plots", "Intervals and tests", "Distributions", NULL}},
    {"Lists", "L", 0x8f9db4, {"L1-L6 lists", "999 values each", "Sum min max", NULL}},
    {"Matrices", "M", 0xc1a6ff, {"A-J matrices", "Det inverse rref", "Augment rows", "List convert", NULL}},
    {"Solver", "=", 0x5d8fd6, {"Exact symbolic equations", "Linear and nonlinear systems", "Polynomial roots", "Numeric search", NULL}},
    {"Settings", "*", 0x6d7d91, {"Brightness", "Auto sleep", "Dark / light theme", NULL}},
    {"Finance", "$", 0x8aa4c4, {"TVM solver", "NPV / IRR", "Begin / end", NULL}},
    {"Conics", "O", 0xd0d7e2, {"Coefficient worksheets", "Geometry analysis", "Construction and tangent", "Multi-conic graphs", NULL}},
    {"Inequality", "<", 0xa9b4c4, {"One-variable solving", "Systems and regions", "Sign charts", "Linear optimization", NULL}},
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
    GRAPH_STYLE_LINE = 0,
    GRAPH_STYLE_THICK,
    GRAPH_STYLE_DOTTED,
    GRAPH_STYLE_POINTS,
    GRAPH_STYLE_COUNT,
} graph_style_t;

typedef enum {
    SCRIPT_ACTION_RUN = 0,
    SCRIPT_ACTION_DEBUG,
    SCRIPT_ACTION_EDIT,
    SCRIPT_ACTION_DELETE,
} script_action_t;

typedef enum {
    SCRIPT_VIEW_CONSOLE = 0,
    SCRIPT_VIEW_VARIABLES,
    SCRIPT_VIEW_TRACEBACK,
    SCRIPT_VIEW_PROFILE,
    SCRIPT_VIEW_COUNT,
} script_view_t;

typedef enum {
    SCRIPT_GFX_CLEAR = 0,
    SCRIPT_GFX_PIXEL,
    SCRIPT_GFX_LINE,
    SCRIPT_GFX_RECT,
    SCRIPT_GFX_TEXT,
} script_graphics_kind_t;

typedef struct {
    script_graphics_kind_t kind;
    int x0;
    int y0;
    int x1;
    int y1;
    uint32_t color;
    char text[48];
} script_graphics_command_t;

typedef struct {
    double input;
    double x;
    double y;
    int fn;
    int other_fn;
    graph_poi_type_t type;
} graph_poi_t;

typedef enum {
    UI_WORK_CALC_EVAL = 0,
    UI_WORK_SOLVER_SOLVE,
    UI_WORK_SOLVER_SYMBOLIC,
    UI_WORK_GRAPH_CALC,
    UI_WORK_GRAPH_SYMBOLIC,
} ui_work_type_t;

typedef struct {
    ui_work_type_t type;
    bool degrees;
    uint32_t request_id;
    union {
        struct {
            char expr[CALC_EXPR_MAX];
            char ans[CALC_RESULT_MAX];
            int complex_mode;
            int display_format;
            int print_mode;
        } calc;
        struct {
            char e1[96];
            char e2[96];
            double guess;
            double lower;
            double upper;
            double tolerance;
            int complex_mode;
            bool scan_all;
        } solver;
        struct {
            char expression[256];
            char title[32];
        } symbolic;
        struct {
            int graphing_mode;
            int series;
            uint32_t fingerprint;
            char primary[96];
            char secondary[96];
        } graph_symbolic;
        struct {
            int selection;
            int graphing_mode;
            char exprs[10][96];
            bool enabled[10];
            char param_x[GRAPH_PARAM_COUNT][96];
            char param_y[GRAPH_PARAM_COUNT][96];
            bool param_enabled[GRAPH_PARAM_COUNT];
            char polar_exprs[GRAPH_POLAR_COUNT][96];
            bool polar_enabled[GRAPH_POLAR_COUNT];
            char seq_exprs[GRAPH_SEQ_COUNT][96];
            bool seq_enabled[GRAPH_SEQ_COUNT];
            double xmin;
            double xmax;
            double ymin;
            double ymax;
            double tmin;
            double tmax;
            double nmin;
            double nmax;
            bool trace;
            double trace_x;
            int trace_fn;
        } graph;
    };
} ui_work_job_t;

typedef struct {
    ui_work_type_t type;
    bool ok;
    uint32_t request_id;
    union {
        struct {
            char expr[CALC_EXPR_MAX];
            char output[CALC_RESULT_MAX];
            bool update_ans;
        } calc;
        struct {
            double root;
            double root_imag;
            bool complex_root;
            bool multiple;
            int root_count;
            double roots[SOLVER_POLY_ROOT_MAX];
        } solver;
        struct {
            char output[768];
            char title[32];
        } symbolic;
        struct {
            char status[72];
            bool trace;
            double trace_x;
            int trace_fn;
        } graph;
        struct {
            int graphing_mode;
            int series;
            uint32_t fingerprint;
            char derivative[160];
            char integral[160];
            char roots[160];
            char asymptotes[160];
        } graph_symbolic;
    };
} ui_work_result_t;

typedef struct {
    bool enabled;
    char text[160];
    opencalc_inequality_problem_t problem;
} inequality_t;

#define MATH_TAB_COUNT 6

static const char *MATH_TAB_NAMES[] = {"MATH", "NUM", "CPX", "PRB", "CAS", "ADV"};

static const math_menu_item_t MATH_MENU[][10] = {
    {
        {"Frac", "frac("},
        {"Dec", "dec("},
        {"solve", "solve("},
        {"expand", "expand("},
        {"factor", "factor("},
        {"xroot", "nroot("},
        {"deriv", "deriv("},
        {"nDeriv", "nDeriv("},
        {"int", "int("},
        {"fnInt", "fnInt("},
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
    {
        {"simplify", "simplify("},
        {"roots", "roots("},
        {"derivative", "deriv("},
        {"integral", "int("},
        {"definite int", "defint("},
        {"sum", "sum("},
        {"product", "product("},
        {"Taylor", "taylor("},
        {"numerator", "numerator("},
        {"denominator", "denominator("},
    },
    {
        {"limit", "limit("},
        {"rationalize", "rationalize("},
        {"complex rect", "rect("},
        {"complex polar", "polar("},
        {"gamma", "gamma("},
        {"erf", "erf("},
        {"erfc", "erfc("},
        {"eigenvectors", "eigenvec("},
        {"matrix rank", "rank("},
        {"transpose", "transpose("},
    },
};

static const app_tool_t STATS_TOOLS[] = {
    {"edit lists", "EDIT spreadsheet"},
    {"SortA L1", "ascending data"},
    {"SortD L1", "descending data"},
    {"ClearList", "wipe all lists"},
    {"One-Variable Stats", "mean med sx quartiles"},
    {"Two-Variable Stats", "L1/L2 summary"},
    {"LinReg L1,L2", "y=a+bx, r"},
    {"QuadReg", "quadratic best fit"},
    {"ExpReg", "exponential best fit"},
    {"LogReg", "a+b ln(x)"},
    {"PwrReg", "a*x^b"},
    {"CubicReg", "degree 3 poly"},
    {"QuartReg", "degree 4 poly"},
    {"Med-Med", "median regression"},
    {"Z-Test", "mean vs mu0"},
    {"T-Test", "sample mean"},
    {"Chi-square", "goodness fit"},
    {"1-Prop Z-Test", "one proportion"},
    {"2-Prop Z-Test", "compare proportions"},
    {"2-Samp T-Test", "compare means"},
    {"Z-Interval", "mean confidence"},
    {"T-Interval", "mean confidence"},
    {"1-Prop Z-Int", "proportion confidence"},
    {"2-Prop Z-Int", "difference confidence"},
    {"2-Samp Z-Int", "known sigmas"},
    {"2-Samp T-Int", "Welch confidence"},
    {"ANOVA", "L1,L2,L3 groups"},
    {"normalpdf", "x,mu,sigma"},
    {"normalcdf", "low,high,mu,sigma"},
    {"invNorm", "area,mu,sigma"},
    {"tpdf", "x,df"},
    {"tcdf", "low,high,df"},
    {"chi2pdf", "x,df"},
    {"chi2cdf", "low,high,df"},
    {"Fpdf", "x,df1,df2"},
    {"Fcdf", "low,high,df1,df2"},
    {"binompdf", "n,p,x"},
    {"binomcdf", "n,p,x"},
    {"poissonpdf", "lambda,x"},
    {"poissoncdf", "lambda,x"},
    {"stat plot", "scatter line hist box normal"},
};

static const app_tool_t STATS_HOME_TOOLS[] = {
    {"Summary Statistics", "describe list data"},
    {"Regression", "fit and compare models"},
    {"Statistical Plots", "inspect data visually"},
    {"Distributions", "pdf, cdf, inverse"},
    {"Confidence Intervals", "estimate parameters"},
    {"Hypothesis Tests", "test statistical claims"},
    {"One-Variable Stats", "guided L1-L6 worksheet"},
};

enum {
    STATS_TOOL_Z_TEST = 14,
    STATS_TOOL_T_TEST,
    STATS_TOOL_CHI_GOF,
    STATS_TOOL_ONE_PROP_TEST,
    STATS_TOOL_TWO_PROP_TEST,
    STATS_TOOL_TWO_SAMPLE_T_TEST,
    STATS_TOOL_Z_INTERVAL,
    STATS_TOOL_T_INTERVAL,
    STATS_TOOL_ONE_PROP_INTERVAL,
    STATS_TOOL_TWO_PROP_INTERVAL,
    STATS_TOOL_TWO_SAMPLE_Z_INTERVAL,
    STATS_TOOL_TWO_SAMPLE_T_INTERVAL,
    STATS_TOOL_ANOVA,
    STATS_TOOL_NORMAL_PDF,
    STATS_TOOL_NORMAL_CDF,
    STATS_TOOL_INV_NORMAL,
    STATS_TOOL_T_PDF,
    STATS_TOOL_T_CDF,
    STATS_TOOL_CHI_PDF,
    STATS_TOOL_CHI_CDF,
    STATS_TOOL_F_PDF,
    STATS_TOOL_F_CDF,
    STATS_TOOL_BINOM_PDF,
    STATS_TOOL_BINOM_CDF,
    STATS_TOOL_POISSON_PDF,
    STATS_TOOL_POISSON_CDF,
    STATS_TOOL_PLOT,
};

typedef enum {
    STATS_CATEGORY_SUMMARY = 0,
    STATS_CATEGORY_REGRESSION,
    STATS_CATEGORY_DISTRIBUTIONS,
    STATS_CATEGORY_INTERVALS,
    STATS_CATEGORY_TESTS,
} stats_category_t;

typedef struct {
    double value;
    int64_t frequency;
} stats_weighted_item_t;

static const int STATS_SUMMARY_TOOLS[] = {4, 5, 0, 1, 2, 3};
static const int STATS_REGRESSION_TOOLS[] = {6, 7, 8, 9, 10, 11, 12, 13};
static const int STATS_DISTRIBUTION_TOOLS[] = {
    STATS_TOOL_NORMAL_PDF, STATS_TOOL_NORMAL_CDF, STATS_TOOL_INV_NORMAL,
    STATS_TOOL_T_PDF, STATS_TOOL_T_CDF, STATS_TOOL_CHI_PDF,
    STATS_TOOL_CHI_CDF, STATS_TOOL_F_PDF, STATS_TOOL_F_CDF,
    STATS_TOOL_BINOM_PDF, STATS_TOOL_BINOM_CDF,
    STATS_TOOL_POISSON_PDF, STATS_TOOL_POISSON_CDF,
};
static const int STATS_INTERVAL_TOOLS[] = {
    STATS_TOOL_Z_INTERVAL, STATS_TOOL_T_INTERVAL,
    STATS_TOOL_ONE_PROP_INTERVAL, STATS_TOOL_TWO_PROP_INTERVAL,
    STATS_TOOL_TWO_SAMPLE_Z_INTERVAL, STATS_TOOL_TWO_SAMPLE_T_INTERVAL,
};
static const int STATS_TEST_TOOLS[] = {
    STATS_TOOL_Z_TEST, STATS_TOOL_T_TEST, STATS_TOOL_CHI_GOF,
    STATS_TOOL_ONE_PROP_TEST, STATS_TOOL_TWO_PROP_TEST,
    STATS_TOOL_TWO_SAMPLE_T_TEST, STATS_TOOL_ANOVA,
};

typedef enum {
    STATS_FIELD_NUMBER = 0,
    STATS_FIELD_INTEGER,
    STATS_FIELD_LIST,
    STATS_FIELD_TAIL,
    STATS_FIELD_CONFIDENCE,
} stats_field_kind_t;

static const app_tool_t LIST_TOOLS[] = {
    {"edit list", "enter values"},
    {"new list", "next empty L"},
    {"sum list", "selected total"},
    {"min max", "selected range"},
    {"clear list", "selected only"},
};

static const app_tool_t MATRIX_TOOLS[] = {
    {"edit cells", "grid editor"},
    {"browse", "full matrix"},
    {"dimensions", "rows and columns"},
    {"add next", "selected + next"},
    {"subtract next", "selected - next"},
    {"multiply next", "selected x next"},
    {"scalar multiply", "scalar in Calc/Ans"},
    {"matrix power", "integer in Calc/Ans"},
    {"transpose", "swap rows/cols"},
    {"determinant", "square matrix"},
    {"inverse", "square matrix"},
    {"rref", "reduced row echelon"},
    {"ref", "row echelon form"},
    {"identity", "square size or 3x3"},
    {"zero matrix", "keep dimensions"},
    {"augment next", "append next matrix"},
    {"extract", "row,2 col,3 sub,1,2,3,4"},
    {"import Calc", "1,2;3,4"},
    {"row op", "swap/scale/add"},
    {"to list", "first col -> list"},
    {"from list", "list -> column"},
};

enum {
    MATRIX_TOOL_EDIT = 0,
    MATRIX_TOOL_BROWSE,
    MATRIX_TOOL_DIMENSIONS,
    MATRIX_TOOL_ADD,
    MATRIX_TOOL_SUBTRACT,
    MATRIX_TOOL_MULTIPLY,
    MATRIX_TOOL_SCALAR,
    MATRIX_TOOL_POWER,
    MATRIX_TOOL_TRANSPOSE,
    MATRIX_TOOL_DETERMINANT,
    MATRIX_TOOL_INVERSE,
    MATRIX_TOOL_RREF,
    MATRIX_TOOL_REF,
    MATRIX_TOOL_IDENTITY,
    MATRIX_TOOL_ZERO,
    MATRIX_TOOL_AUGMENT,
    MATRIX_TOOL_EXTRACT,
    MATRIX_TOOL_IMPORT,
    MATRIX_TOOL_ROW_OP,
    MATRIX_TOOL_TO_LIST,
    MATRIX_TOOL_FROM_LIST,
};

static const app_tool_t SOLVER_TOOLS[] = {
    {"Equation Solver", "exact symbolic equations"},
    {"System Solver", "linear and symbolic systems"},
    {"Polynomial Solver", "factor and find roots"},
    {"Numeric Solver", "bounded or nearby root"},
    {"Saved Problems", "persistent equation library"},
};

typedef enum {
    SOLVER_WORKFLOW_EQUATION = 0,
    SOLVER_WORKFLOW_SYSTEM,
    SOLVER_WORKFLOW_POLYNOMIAL,
    SOLVER_WORKFLOW_NUMERIC,
} solver_workflow_t;

typedef enum {
    SOLVER_SYSTEM_NONE = 0,
    SOLVER_SYSTEM_UNIQUE,
    SOLVER_SYSTEM_DEPENDENT,
    SOLVER_SYSTEM_INCONSISTENT,
} solver_system_status_t;

static const app_tool_t SOLVER_EQUATION_ACTIONS[] = {
    {"Set left side", "use Calculator input"},
    {"Set right side", "use Calculator input"},
    {"Solve exactly", "symbolic CAS"},
    {"Advanced CAS command", "run Calculator solve(...)"},
    {"Approximate", "root near current guess"},
    {"Plot both sides", "send equation to Graph"},
    {"Save problem", "store in selected slot"},
};

static const app_tool_t SOLVER_SYSTEM_ACTIONS[] = {
    {"Edit augmented matrix", "rows x (variables+1)"},
    {"Solve matrix system", "RREF with classification"},
    {"Symbolic system", "solve Calculator input"},
    {"View last result", "browse up to 99 variables"},
    {"Solutions to list", "unique result to selected list"},
    {"Save symbolic problem", "store Calculator input"},
};

static const app_tool_t SOLVER_POLYNOMIAL_ACTIONS[] = {
    {"Set polynomial", "use Calculator expression"},
    {"Exact roots", "symbolic solve over C"},
    {"Factor", "exact polynomial factors"},
    {"Numeric roots", "coefficients high to constant"},
    {"View numeric roots", "residual and complex parts"},
    {"Save polynomial", "store expression"},
};

static const app_tool_t SOLVER_NUMERIC_ACTIONS[] = {
    {"Set left side", "use Calculator input"},
    {"Set right side", "use Calculator input"},
    {"Set initial guess", "use Calculator/Ans"},
    {"Set lower bound", "use Calculator/Ans"},
    {"Set upper bound", "use Calculator/Ans"},
    {"Set precision", "digits 4 through 12"},
    {"Find nearby root", "bracket then Newton"},
    {"Find all in bounds", "detect multiple real roots"},
    {"Plot both sides", "inspect intersections"},
    {"Save problem", "store in selected slot"},
};

static const app_tool_t FINANCE_TOOLS[] = {
    {"TVM worksheet", "edit and solve"},
    {"cash flows", "period ledger"},
    {"NPV", "discounted value"},
    {"IRR", "cash-flow rate"},
    {"payment timing", "begin or end"},
    {"next cash list", "L1 through L6"},
    {"previous list", "L1 through L6"},
    {"clear TVM", "reset worksheet"},
};

static const app_tool_t CONICS_TOOLS[] = {
    {"Circle", "center, radius, construction"},
    {"Parabola", "vertex, focus, directrix"},
    {"Ellipse", "axes, vertices, foci"},
    {"Hyperbola", "axes, foci, asymptotes"},
    {"General Conic", "Ax2+Bxy+Cy2+Dx+Ey+F"},
    {"Conic Graphs", "overlays and graph export"},
};

static const app_tool_t INEQUALITY_TOOLS[] = {
    {"One-variable", "exact and numerical intervals"},
    {"Systems", "combine up to six relations"},
    {"Graph regions", "shade, trace and intersect"},
    {"Sign chart", "critical points and signs"},
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

static EXT_RAM_BSS_ATTR uint32_t s_graph_background[UI_W * UI_H];
static page_id_t s_page = PAGE_CALCULATOR;
static page_id_t s_math_return_page = PAGE_CALCULATOR;
static app_id_t s_current_app = APP_CALCULATOR;
static app_id_t s_previous_app = APP_CALCULATOR;
static int s_home_selection = APP_CALCULATOR;
static int s_script_selection = 0;
static int s_program_selection = 0;
static int s_script_count = 0;
static script_action_t s_script_action = SCRIPT_ACTION_RUN;
static int s_app_selection = 0;
static page_id_t s_reference_return_page = PAGE_CALCULATOR;
static app_id_t s_reference_return_app = APP_CALCULATOR;
static int s_reference_home_selection = 0;
static int s_periodic_atomic_number = 1;
static opencalc_reference_category_t s_reference_category = OPENCALC_REFERENCE_MATH;
static int s_reference_selection = 0;
static int s_math_tab = 0;
static int s_math_selection = 0;
static EXT_RAM_BSS_ATTR char s_scripts[SCRIPT_MAX][32];
#define SCRIPT_MODEL (opencalc_script_model())
#define s_script_editor (SCRIPT_MODEL->text)
#define s_script_edit_name (SCRIPT_MODEL->name)
#define s_script_editor_len (SCRIPT_MODEL->length)
#define s_script_editor_cursor (SCRIPT_MODEL->cursor)
#define s_script_editor_scroll_line (SCRIPT_MODEL->scroll_line)
#define s_script_editor_scroll_col (SCRIPT_MODEL->scroll_column)
static EXT_RAM_BSS_ATTR py_t s_script_py;
static EXT_RAM_BSS_ATTR char s_script_output[1024];
static char s_script_title[48] = "Python";
static char s_script_status[48] = "";
static volatile bool s_script_running = false;
static volatile bool s_script_input_active = false;
static volatile bool s_script_input_submitted = false;
static volatile bool s_script_input_cancelled = false;
static volatile bool s_script_screen_dirty = false;
static volatile bool s_script_debug_paused = false;
static volatile bool s_script_debug_step = false;
static volatile bool s_script_stop_requested = false;
static volatile int s_script_debug_resume = 0;
static bool s_script_debug_mode = false;
static script_view_t s_script_view = SCRIPT_VIEW_CONSOLE;
static size_t s_script_debug_line = 0;
static char s_script_debug_function[PY_MAX_NAME] = "<script>";
static EXT_RAM_BSS_ATTR char s_script_variables[768];
static EXT_RAM_BSS_ATTR char s_script_traceback[512];
/* Script output can arrive from core 0 while core 1 renders it. Keep the
 * render snapshots out of the UI task's limited stack and protect copies with
 * s_script_output_mutex. */
static EXT_RAM_BSS_ATTR char s_script_output_snapshot[sizeof(s_script_output)];
static EXT_RAM_BSS_ATTR char s_script_diagnostic_snapshot[sizeof(s_script_variables)];
static EXT_RAM_BSS_ATTR script_graphics_command_t s_script_graphics_snapshot[SCRIPT_GRAPHICS_MAX];
static py_profile_t s_script_profile;
static uint32_t s_script_elapsed_ms = 0;
static int64_t s_script_started_us = 0;
static bool s_script_breakpoints[SCRIPT_BREAKPOINT_MAX];
static bool s_script_key_state[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS];
static EXT_RAM_BSS_ATTR script_graphics_command_t s_script_graphics[SCRIPT_GRAPHICS_MAX];
static size_t s_script_graphics_count = 0;
static bool s_script_graphics_active = false;
static bool s_script_first_output_logged = false;
static char s_script_input[96] = "";
static size_t s_script_input_len = 0;
typedef struct {
    char path[80];
    bool debug;
} script_run_request_t;
typedef struct {
    int ok;
    char error[PY_MAX_ERROR];
    char traceback[512];
    char variables[768];
    py_profile_t profile;
    uint32_t elapsed_ms;
    UBaseType_t stack_high_water;
} script_run_result_t;
static EXT_RAM_BSS_ATTR script_run_result_t s_script_worker_result;
static QueueHandle_t s_serial_button_queue = NULL;
static QueueHandle_t s_script_request_queue = NULL;
static QueueHandle_t s_script_result_queue = NULL;
static TaskHandle_t s_script_task = NULL;
static uint32_t s_script_task_stack_size = 0;
static SemaphoreHandle_t s_script_output_mutex = NULL;
static bool s_calc_eval_pending = false;
static atomic_uint_fast32_t s_calc_eval_generation;
static bool s_solver_solve_pending = false;
static bool s_solver_symbolic_pending = false;
static bool s_graph_calc_pending = false;
static bool s_graph_symbolic_pending = false;
static char s_calc_input[CALC_EXPR_MAX] = "";
static size_t s_calc_cursor = 0;
static char s_calc_output[CALC_RESULT_MAX] = "0";
static char s_calc_ans[CALC_RESULT_MAX] = "0";
static int s_calc_result_scroll = 0;
static variable_action_t s_variable_action = VARIABLE_ACTION_INSERT;
static variable_category_t s_variable_category = VARIABLE_CATEGORY_USER;
static page_id_t s_variable_return_page = PAGE_CALCULATOR;
static int s_variable_selection = 0;
static int s_variable_scroll = 0;
static char s_variable_store_expression[CALC_EXPR_MAX] = "";
static char s_variable_name[OPENCALC_VARIABLE_NAME_MAX] = "";
static char s_variable_rename_from[OPENCALC_VARIABLE_NAME_MAX] = "";
static char s_variable_status[64] = "";
static EXT_RAM_BSS_ATTR char s_variable_persist_buffer[4096];
#define s_lists (*opencalc_worksheet_lists())
#define s_list_counts (*opencalc_worksheet_list_counts())
static int s_list_index = 0;
static int s_list_cursor = 0;
static char s_list_entry[24] = "";
static bool s_list_editing = false;
#define GRAPH_MODEL (opencalc_graph_model())
#define s_graph_exprs (GRAPH_MODEL->expressions)
#define s_graph_enabled (GRAPH_MODEL->enabled)
#define s_graph_param_x (GRAPH_MODEL->param_x)
#define s_graph_param_y (GRAPH_MODEL->param_y)
#define s_graph_param_enabled (GRAPH_MODEL->param_enabled)
#define s_graph_polar_exprs (GRAPH_MODEL->polar_expressions)
#define s_graph_polar_enabled (GRAPH_MODEL->polar_enabled)
#define s_graph_seq_exprs (GRAPH_MODEL->sequence_expressions)
#define s_graph_seq_enabled (GRAPH_MODEL->sequence_enabled)
#define s_graph_tmin (GRAPH_MODEL->t_min)
#define s_graph_tmax (GRAPH_MODEL->t_max)
#define s_graph_nmin (GRAPH_MODEL->n_min)
#define s_graph_nmax (GRAPH_MODEL->n_max)
static const uint32_t s_graph_colors[GRAPH_COLOR_COUNT] = {
    0x4aa3ff, 0xb38cff, 0xff8cc6, 0x76d7ff, 0xd0d7e2,
    0x8aa4c4, 0x5d8fd6, 0xc1a6ff, 0x9aa8ff, 0xf4f7fb,
    0x33d17a, 0xf5c542, 0xff4d4d, 0x00d2c6, 0xff9f43,
};
#define s_graph_selection (GRAPH_MODEL->selection)
#define s_graph_xmin (GRAPH_MODEL->x_min)
#define s_graph_xmax (GRAPH_MODEL->x_max)
#define s_graph_ymin (GRAPH_MODEL->y_min)
#define s_graph_ymax (GRAPH_MODEL->y_max)
#define s_graph_xtick (GRAPH_MODEL->x_tick)
#define s_graph_ytick (GRAPH_MODEL->y_tick)
#define s_graph_window_selection (GRAPH_MODEL->window_selection)
#define s_graph_calc_selection (GRAPH_MODEL->calc_selection)
#define s_graph_format_selection (GRAPH_MODEL->format_selection)
#define s_graph_style_series (GRAPH_MODEL->style_series)
#define s_graph_styles (GRAPH_MODEL->styles)
static char s_graph_status[72] = "";
#define s_graph_zoom_mode (GRAPH_MODEL->zoom_mode)
#define s_graph_split (GRAPH_MODEL->split)
#define s_graph_background_enabled (GRAPH_MODEL->background_enabled)
#define s_graph_background_loaded (GRAPH_MODEL->background_loaded)
#define s_table_x_start (GRAPH_MODEL->table_x_start)
#define s_table_step (GRAPH_MODEL->table_step)
#define s_table_func_start (GRAPH_MODEL->table_function_start)
#define s_table_rows (GRAPH_MODEL->table_rows)
#define s_table_precision (GRAPH_MODEL->table_precision)
#define s_table_setup_selection (GRAPH_MODEL->table_setup_selection)
#define s_graph_grid (GRAPH_MODEL->grid)
static inequality_t s_ineqs[INEQ_MAX];
static int s_ineq_selection = 0;
static bool s_ineq_join_or = false;
static bool s_ineq_integer_mode = false;
static bool s_ineq_sign_chart = false;
static bool s_ineq_graph_editor = false;
static opencalc_inequality_problem_t s_ineq_problem;
static opencalc_inequality_solution_t s_ineq_solution;
static char s_ineq_problem_text[160] = "x^2-4>=0";
static char s_ineq_interval_text[256] = "";
static char s_ineq_exact_text[768] = "";
static char s_ineq_status[96] = "";
static bool s_ineq_symbolic_pending = false;
static int s_ineq_result_scroll = 0;
static int s_ineq_notation = 0;
#define s_graph_trace (GRAPH_MODEL->trace)
#define s_graph_trace_x (GRAPH_MODEL->trace_x)
#define s_graph_trace_fn (GRAPH_MODEL->trace_function)
#define s_graph_symbolic_derivative (GRAPH_MODEL->symbolic_derivative)
#define s_graph_symbolic_integral (GRAPH_MODEL->symbolic_integral)
#define s_graph_symbolic_roots (GRAPH_MODEL->symbolic_roots)
#define s_graph_symbolic_asymptotes (GRAPH_MODEL->symbolic_asymptotes)
#define s_graph_tangent_enabled (GRAPH_MODEL->tangent_enabled)
#define s_graph_integral_shade_enabled (GRAPH_MODEL->integral_shade_enabled)
#define s_graph_overlay_fn (GRAPH_MODEL->overlay_function)
#define s_graph_overlay_x (GRAPH_MODEL->overlay_x)
static bool s_cursor_blink_visible = true;
static bool s_cursor_blink_last_visible = true;
static bool s_second_active = false;
static bool s_alpha_active = false;
static bool s_alpha_locked = false;
static bool s_alpha_lowercase = false;
static game_id_t s_active_game = GAME_NONE;
static int s_game_selection = 0;
static uint32_t s_doom_high_score = 0;
static uint32_t s_doom_last_saved_high_score = 0;
static bool s_usb_storage_enabled = OPENCALC_EXPORT_USB_STORAGE_TO_HOST != 0;
static bool s_sleep_enabled = true;
static bool s_power_save_enabled = false;
static int s_power_save_saved_brightness = 80;
static int s_audio_volume_percent = OPENCALC_AUDIO_VOLUME_PERCENT;
static int s_mode_selection = 0;
static int s_display_format = 0;
static int s_print_mode = 0;
static int s_angle_mode = 0;
#define s_graphing_mode (GRAPH_MODEL->mode)
static char s_stats_result_title[32] = "";
static char s_stats_result_lines[8][48];
static int s_stats_result_line_count = 0;
static int s_stats_plot_mode = 0;
static int s_stats_home_selection = 0;
static stats_category_t s_stats_category = STATS_CATEGORY_SUMMARY;
static int s_stats_category_selection = 0;
static int s_stats_one_var_selection = 0;
static int s_stats_one_var_data_list = 0;
static int s_stats_one_var_frequency_list = -1;
static int s_stats_one_var_quartile_method = 0;
static int s_stats_one_var_number_format = 0;
static char s_stats_one_var_error[64] = "";
static page_id_t s_stats_one_var_parent_page = PAGE_APP;
static page_id_t s_stats_return_page = PAGE_APP;
static EXT_RAM_BSS_ATTR stats_weighted_item_t s_stats_weighted_items[LIST_MAX_VALUES];
static EXT_RAM_BSS_ATTR double s_stats_sort_scratch[LIST_MAX_VALUES];
static int s_stats_setup_tool = STATS_TOOL_Z_TEST;
static int s_stats_setup_selection = 0;
static int s_stats_setup_field_count = 0;
static const char *s_stats_setup_labels[6];
static stats_field_kind_t s_stats_setup_kinds[6];
static double s_stats_setup_values[6];
static char s_stats_setup_entry[24] = "";
static char s_stats_setup_error[64] = "";

static void stats_setup_open(int tool);
static void stats_setup_execute(void);
static void stats_setup_commit_entry(void);
static bool stats_page_handle_key(int row, int col);
static void stats_open_one_var(void);
static void stats_calculate_one_var(void);
static double list_median(int list);
static void list_quartiles(int list, double *q1, double *q3);

static void clear_graph_expressions(void);
static int s_complex_mode = 0;
#define s_matrices (*opencalc_worksheet_matrices())
#define s_matrix_rows_by_index (*opencalc_worksheet_matrix_rows())
#define s_matrix_cols_by_index (*opencalc_worksheet_matrix_cols())
static int s_matrix_index = 0;
static int s_matrix_cursor_row = 0;
static int s_matrix_cursor_col = 0;
static int s_matrix_scroll_row = 0;
static int s_matrix_scroll_col = 0;
static char s_matrix_entry[24] = "";
static bool s_matrix_entry_active = false;
static char s_matrix_status[48] = "";
static int s_matrix_size_rows = 3;
static int s_matrix_size_cols = 3;
static int s_matrix_size_selection = 0;
static bool s_matrix_size_typing = false;
#define s_matrix_a (s_matrices[s_matrix_index])
#define s_matrix_rows (s_matrix_rows_by_index[s_matrix_index])
#define s_matrix_cols (s_matrix_cols_by_index[s_matrix_index])
static char s_solver_e1[96] = "x^2";
static char s_solver_e2[96] = "4";
static solver_workflow_t s_solver_workflow = SOLVER_WORKFLOW_EQUATION;
static int s_solver_workflow_selection = 0;
static double s_solver_guess = 1.0;
static double s_solver_lower = -10.0;
static double s_solver_upper = 10.0;
static int s_solver_precision = 10;
static double s_solver_result = 0.0;
static double s_solver_result_imag = 0.0;
static bool s_solver_has_complex_result = false;
static bool s_solver_has_result = false;
static double s_solver_poly_root_real[SOLVER_POLY_ROOT_MAX];
static double s_solver_poly_root_imag[SOLVER_POLY_ROOT_MAX];
static int s_solver_poly_root_count = 0;
static int s_solver_poly_root_selected = 0;
static char s_solver_poly_source[96] = "";
static bool s_solver_roots_are_polynomial = true;
static solver_system_status_t s_solver_system_status = SOLVER_SYSTEM_NONE;
static int s_solver_system_variables = 0;
static int s_solver_system_rank = 0;
static int s_solver_system_selected = 0;
static int s_solver_system_matrix = 0;
static bool s_solver_matrix_editing = false;
static EXT_RAM_BSS_ATTR char s_solver_symbolic_result[768];
static char s_solver_symbolic_title[32] = "Symbolic Result";
static int s_solver_symbolic_scroll = 0;
static int s_solver_saved_selection = 0;
static EXT_RAM_BSS_ATTR char s_solver_saved_e1[SOLVER_SAVED_MAX][96];
static EXT_RAM_BSS_ATTR char s_solver_saved_e2[SOLVER_SAVED_MAX][96];
static uint32_t s_solver_saved_type[SOLVER_SAVED_MAX];
static double s_fin_n = 12.0;
static double s_fin_i = 5.0;
static double s_fin_pv = 1000.0;
static double s_fin_pmt = 0.0;
static double s_fin_fv = 0.0;
static double s_fin_py = 12.0;
static double s_fin_cy = 12.0;
static bool s_fin_begin = false;
static int s_fin_selection = 3;
static char s_fin_entry[24] = "";
static bool s_fin_entry_active = false;
static char s_fin_status[64] = "";
static int s_fin_cash_cursor = 0;
static int s_fin_cash_scroll = 0;
static char s_fin_result_title[24] = "";
static double s_fin_result_value = 0.0;
static char s_fin_result_detail[64] = "";
static int s_conic_type = 0;
static double s_conic_h = 0.0;
static double s_conic_k = 0.0;
static double s_conic_a = 2.0;
static double s_conic_b = 3.0;
static double s_conic_r = 5.0;
static double s_conic_angle = 0.0;
static double s_conic_general[6] = {1.0, 0.0, 1.0, 0.0, 0.0, -25.0};
static opencalc_conic_t s_conic_model;
static opencalc_conic_t s_conic_overlays[4];
static int s_conic_overlay_count = 0;
static int s_conic_overlay_selection = 0;
static int s_conic_selection = 0;
static char s_conic_entry[24] = "";
static bool s_conic_entry_active = false;
static char s_conic_status[64] = "";
static char s_conic_equation[192] = "";
static char s_conic_result_title[32] = "Conic Analysis";
static char s_conic_result_lines[20][96];
static int s_conic_result_count = 0;
static int s_conic_result_scroll = 0;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t payload_size;
    uint32_t payload_crc32;
} worksheet_header_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t section;
    uint32_t payload_size;
    uint32_t payload_crc32;
} worksheet_journal_header_t;

typedef struct {
    uint32_t fixed_size;
    char calc_input[CALC_EXPR_MAX];
    char calc_output[CALC_RESULT_MAX];
    char calc_ans[CALC_RESULT_MAX];
    int32_t calc_history_count;
    char calc_history_expr[CALC_HISTORY_MAX][CALC_EXPR_MAX];
    char calc_history_result[CALC_HISTORY_MAX][CALC_RESULT_MAX];

    char graph_exprs[GRAPH_FUNC_COUNT][96];
    uint8_t graph_enabled[GRAPH_FUNC_COUNT];
    char graph_param_x[GRAPH_PARAM_COUNT][96];
    char graph_param_y[GRAPH_PARAM_COUNT][96];
    uint8_t graph_param_enabled[GRAPH_PARAM_COUNT];
    char graph_polar_exprs[GRAPH_POLAR_COUNT][96];
    uint8_t graph_polar_enabled[GRAPH_POLAR_COUNT];
    char graph_seq_exprs[GRAPH_SEQ_COUNT][96];
    uint8_t graph_seq_enabled[GRAPH_SEQ_COUNT];
    double graph_tmin, graph_tmax, graph_nmin, graph_nmax;
    double graph_xmin, graph_xmax, graph_ymin, graph_ymax;
    double graph_xtick, graph_ytick;
    double table_x_start, table_step;
    int32_t graph_selection, graph_window_selection, graph_calc_selection;
    int32_t graph_format_selection, graph_style_series;
    uint8_t graph_styles[GRAPH_FUNC_COUNT];
    uint8_t graph_split, graph_background_enabled, graph_grid;
    int32_t table_func_start, table_rows, table_precision, table_setup_selection;
    int32_t display_format, print_mode, angle_mode, graphing_mode, complex_mode;

    int32_t list_index;
    int32_t matrix_index;
    double fin_n, fin_i, fin_pv, fin_pmt, fin_fv, fin_py, fin_cy;
    uint8_t fin_begin;
    int32_t fin_selection, fin_cash_cursor, fin_cash_scroll;

    int32_t conic_type;
    double conic_h, conic_k, conic_a, conic_b, conic_r, conic_angle;
    double conic_general[6];
    opencalc_conic_t conic_model;
    opencalc_conic_t conic_overlays[4];
    int32_t conic_overlay_count, conic_overlay_selection, conic_selection;
} worksheet_fixed_t;

typedef struct {
    uint32_t fixed_size;
    char calc_input[192];
    char calc_output[192];
    char calc_ans[192];
    int32_t calc_history_count;
    char calc_history_expr[CALC_HISTORY_MAX][192];
    char calc_history_result[CALC_HISTORY_MAX][192];

    char graph_exprs[GRAPH_FUNC_COUNT][96];
    uint8_t graph_enabled[GRAPH_FUNC_COUNT];
    char graph_param_x[GRAPH_PARAM_COUNT][96];
    char graph_param_y[GRAPH_PARAM_COUNT][96];
    uint8_t graph_param_enabled[GRAPH_PARAM_COUNT];
    char graph_polar_exprs[GRAPH_POLAR_COUNT][96];
    uint8_t graph_polar_enabled[GRAPH_POLAR_COUNT];
    char graph_seq_exprs[GRAPH_SEQ_COUNT][96];
    uint8_t graph_seq_enabled[GRAPH_SEQ_COUNT];
    double graph_tmin, graph_tmax, graph_nmin, graph_nmax;
    double graph_xmin, graph_xmax, graph_ymin, graph_ymax;
    double graph_xtick, graph_ytick;
    double table_x_start, table_step;
    int32_t graph_selection, graph_window_selection, graph_calc_selection;
    int32_t graph_format_selection, graph_style_series;
    uint8_t graph_styles[GRAPH_FUNC_COUNT];
    uint8_t graph_split, graph_background_enabled, graph_grid;
    int32_t table_func_start, table_rows, table_precision, table_setup_selection;
    int32_t display_format, print_mode, angle_mode, graphing_mode, complex_mode;

    int32_t list_index;
    int32_t matrix_index;
    double fin_n, fin_i, fin_pv, fin_pmt, fin_fv, fin_py, fin_cy;
    uint8_t fin_begin;
    int32_t fin_selection, fin_cash_cursor, fin_cash_scroll;

    int32_t conic_type;
    double conic_h, conic_k, conic_a, conic_b, conic_r, conic_angle;
    double conic_general[6];
    opencalc_conic_t conic_model;
    opencalc_conic_t conic_overlays[4];
    int32_t conic_overlay_count, conic_overlay_selection, conic_selection;
} worksheet_fixed_v1_t;

_Static_assert(sizeof(worksheet_fixed_v1_t) - offsetof(worksheet_fixed_v1_t, graph_exprs) ==
               sizeof(worksheet_fixed_t) - offsetof(worksheet_fixed_t, graph_exprs),
               "worksheet v1 and v2 suffix layouts must match");

static void worksheet_migrate_v1(const worksheet_fixed_v1_t *old, worksheet_fixed_t *current)
{
    memset(current, 0, sizeof(*current));
    current->fixed_size = (uint32_t)sizeof(*current);
    snprintf(current->calc_input, sizeof(current->calc_input), "%.*s", 191, old->calc_input);
    snprintf(current->calc_output, sizeof(current->calc_output), "%.*s", 191, old->calc_output);
    snprintf(current->calc_ans, sizeof(current->calc_ans), "%.*s", 191, old->calc_ans);
    current->calc_history_count = old->calc_history_count;
    for (int i = 0; i < CALC_HISTORY_MAX; i++) {
        snprintf(current->calc_history_expr[i], sizeof(current->calc_history_expr[i]),
                 "%.*s", 191, old->calc_history_expr[i]);
        snprintf(current->calc_history_result[i], sizeof(current->calc_history_result[i]),
                 "%.*s", 191, old->calc_history_result[i]);
    }
    size_t suffix_size = sizeof(*current) - offsetof(worksheet_fixed_t, graph_exprs);
    memcpy(&current->graph_exprs, &old->graph_exprs, suffix_size);
}

static uint32_t s_worksheet_dirty_sections = 0;
static int64_t s_worksheet_dirty_since_us = 0;
static atomic_bool s_sensor_capture_active = false;
static atomic_bool s_script_lists_dirty = false;

static void ui_draw_current(void);
static bool expression_entry_active(void);
static void expression_append(const char *text);
static void calc_expand_ans_value(const char *input, const char *ans, char *out, size_t out_size);
static bool calc_format_fraction_value(double value, char *out, size_t out_size);
static void ui_work_execute(const void *job_data, void *result_data);
static bool submit_calc_eval_job(void);
static bool submit_solver_solve_job(void);
static bool submit_solver_scan_job(void);
static bool submit_solver_symbolic_job(const char *expression, const char *title);
static bool submit_graph_calc_job(void);
static bool submit_graph_symbolic_job(void);
static void open_graph_symbolic(void);
static bool graph_symbolic_handle_key(int row, int col);
static void ui_work_apply_result(const ui_work_result_t *result);
static void ui_work_poll_results(void);
static bool script_worker_start(void);
static int script_debug_callback(py_t *py, py_debug_event_t event,
                                 size_t line, const char *function,
                                 void *user_data);
static int script_native_callback(py_t *py, const char *module,
                                  const char *function,
                                  const py_value_t *args, size_t arg_count,
                                  py_value_t *result, void *user_data);
static void matrix_open_editor(void);
static void matrix_open_viewer(void);
static void matrix_open_size(void);
static double finance_periodic_rate(void);
static double finance_npv_from_list(int list, double rate);
static bool finance_irr_from_list(int list, double *irr);
static bool finance_solve_selected(void);
static void finance_open_tvm(void);
static void finance_open_cashflow(void);
static bool solver_page_handle_key(int row, int col);
static bool conic_page_handle_key(int row, int col);
static bool inequality_page_handle_key(int row, int col);
static bool variable_page_handle_key(int row, int col);
static void variables_open(variable_action_t action);
static void variables_load_all(void);
static void variables_save_all(void);
static void variable_format_value(double real, double imag, char *out, size_t out_size);
static bool variable_browser_item(variable_category_t category, int index, variable_browser_item_t *item);
static int variable_category_count(variable_category_t category);
static bool calc_matrix_literal(int matrix, char *out, size_t out_size);
static void conic_open_editor(int type);
static void conic_analyze_current(void);
static bool conic_graph_model(const opencalc_conic_t *model, bool clear_existing);
static int graph_first_free_slot(int needed);
static int graph_collect_pois(graph_poi_t *pois, int max_count);
static double complex poly_eval_complex_coeffs(const double *coeffs, int count, double complex x);
static int parse_csv_numbers(const char *text, double *values, int max_values);
static void format_complex_value(double real, double imag, int mode, char *out, size_t out_size);
static void worksheet_mark_dirty(void);
static void worksheet_mark_dirty_sections(uint32_t sections);
static bool worksheet_persist_flush(void);
static void worksheet_persist_poll(void);
static void worksheet_persist_load(void);
static void worksheet_persist_erase(void);

static void ui_draw_current(void);
static void status_message(const char *text);
static void app_output(const char *text);
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

static const uint8_t LOWER_FONT[26][7] = {
    /* a */ {0x00,0x00,0x0e,0x01,0x0f,0x11,0x0f},
    /* b */ {0x10,0x10,0x1e,0x11,0x11,0x11,0x1e},
    /* c */ {0x00,0x00,0x0e,0x11,0x10,0x11,0x0e},
    /* d */ {0x01,0x01,0x0f,0x11,0x11,0x11,0x0f},
    /* e */ {0x00,0x00,0x0e,0x11,0x1f,0x10,0x0e},
    /* f */ {0x06,0x08,0x1c,0x08,0x08,0x08,0x08},
    /* g */ {0x00,0x00,0x0f,0x11,0x0f,0x01,0x0e},
    /* h */ {0x10,0x10,0x1e,0x11,0x11,0x11,0x11},
    /* i */ {0x04,0x00,0x0c,0x04,0x04,0x04,0x0e},
    /* j */ {0x02,0x00,0x06,0x02,0x02,0x12,0x0c},
    /* k */ {0x10,0x12,0x14,0x18,0x14,0x12,0x11},
    /* l */ {0x0c,0x04,0x04,0x04,0x04,0x04,0x0e},
    /* m */ {0x00,0x00,0x1a,0x15,0x15,0x15,0x15},
    /* n */ {0x00,0x00,0x1e,0x11,0x11,0x11,0x11},
    /* o */ {0x00,0x00,0x0e,0x11,0x11,0x11,0x0e},
    /* p */ {0x00,0x00,0x1e,0x11,0x1e,0x10,0x10},
    /* q */ {0x00,0x00,0x0f,0x11,0x0f,0x01,0x01},
    /* r */ {0x00,0x00,0x16,0x19,0x10,0x10,0x10},
    /* s */ {0x00,0x00,0x0f,0x10,0x0e,0x01,0x1e},
    /* t */ {0x08,0x08,0x1c,0x08,0x08,0x09,0x06},
    /* u */ {0x00,0x00,0x11,0x11,0x11,0x13,0x0d},
    /* v */ {0x00,0x00,0x11,0x11,0x11,0x0a,0x04},
    /* w */ {0x00,0x00,0x11,0x15,0x15,0x15,0x0a},
    /* x */ {0x00,0x00,0x11,0x0a,0x04,0x0a,0x11},
    /* y */ {0x00,0x00,0x11,0x11,0x0f,0x01,0x0e},
    /* z */ {0x00,0x00,0x1f,0x02,0x04,0x08,0x1f},
};

enum {
    PUNCT_EXCLAMATION = 0,
    PUNCT_DOUBLE_QUOTE,
    PUNCT_SINGLE_QUOTE,
    PUNCT_COMMA,
    PUNCT_SEMICOLON,
    PUNCT_PERIOD,
    PUNCT_SLASH,
    PUNCT_BACKSLASH,
    PUNCT_BACKTICK,
    PUNCT_TILDE,
    PUNCT_LEFT_PAREN,
    PUNCT_RIGHT_PAREN,
    PUNCT_LEFT_BRACKET,
    PUNCT_RIGHT_BRACKET,
    PUNCT_LEFT_BRACE,
    PUNCT_RIGHT_BRACE,
    PUNCT_UNDERSCORE,
};

static const uint8_t PUNCT_FONT[][7] = {
    {0x04,0x04,0x04,0x04,0x04,0x00,0x04},
    {0x0a,0x0a,0x0a,0x00,0x00,0x00,0x00},
    {0x04,0x04,0x08,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x04,0x08},
    {0x00,0x04,0x00,0x00,0x04,0x04,0x08},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x04},
    {0x01,0x02,0x02,0x04,0x08,0x08,0x10},
    {0x10,0x08,0x08,0x04,0x02,0x02,0x01},
    {0x08,0x04,0x02,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x09,0x16,0x00,0x00,0x00},
    {0x02,0x04,0x08,0x08,0x08,0x04,0x02},
    {0x08,0x04,0x02,0x02,0x02,0x04,0x08},
    {0x0e,0x08,0x08,0x08,0x08,0x08,0x0e},
    {0x0e,0x02,0x02,0x02,0x02,0x02,0x0e},
    {0x03,0x04,0x04,0x08,0x04,0x04,0x03},
    {0x18,0x04,0x04,0x02,0x04,0x04,0x18},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x1f},
};

static const uint8_t *font_for(char c)
{
    if (c >= '0' && c <= '9') {
        return FONT[c - '0'];
    }
    if (c >= 'a' && c <= 'z') {
        return LOWER_FONT[c - 'a'];
    }
    if (c >= 'A' && c <= 'Z') {
        return FONT[10 + c - 'A'];
    }
    if (c == '#') {
        return FONT[36];
    }
    switch (c) {
    case '!': return PUNCT_FONT[PUNCT_EXCLAMATION];
    case '"': return PUNCT_FONT[PUNCT_DOUBLE_QUOTE];
    case '\'': return PUNCT_FONT[PUNCT_SINGLE_QUOTE];
    case ',': return PUNCT_FONT[PUNCT_COMMA];
    case ';': return PUNCT_FONT[PUNCT_SEMICOLON];
    case '.': return PUNCT_FONT[PUNCT_PERIOD];
    case '/': return PUNCT_FONT[PUNCT_SLASH];
    case '\\': return PUNCT_FONT[PUNCT_BACKSLASH];
    case '`': return PUNCT_FONT[PUNCT_BACKTICK];
    case '~': return PUNCT_FONT[PUNCT_TILDE];
    case '(': return PUNCT_FONT[PUNCT_LEFT_PAREN];
    case ')': return PUNCT_FONT[PUNCT_RIGHT_PAREN];
    case '[': return PUNCT_FONT[PUNCT_LEFT_BRACKET];
    case ']': return PUNCT_FONT[PUNCT_RIGHT_BRACKET];
    case '{': return PUNCT_FONT[PUNCT_LEFT_BRACE];
    case '}': return PUNCT_FONT[PUNCT_RIGHT_BRACE];
    case '_': return PUNCT_FONT[PUNCT_UNDERSCORE];
    default: break;
    }
    return FONT[37];
}

#define ui_pixel opencalc_ui_canvas_pixel
#define ui_clear opencalc_ui_canvas_clear
#define ui_rect opencalc_ui_canvas_rect
#define ui_border opencalc_ui_canvas_border
#define ui_line opencalc_ui_canvas_line
#define ui_circle_outline opencalc_ui_canvas_circle

static void ui_scrollbar(int x, int y, int height, int total, int visible, int first)
{
    if (height <= 0 || visible <= 0 || total <= visible) return;
    int max_first = total - visible;
    if (first < 0) first = 0;
    if (first > max_first) first = max_first;
    int thumb_height = height * visible / total;
    if (thumb_height < 8) thumb_height = 8;
    if (thumb_height > height) thumb_height = height;
    int thumb_y = y + (height - thumb_height) * first / max_first;
    ui_rect(x, y, 2, height, THEME_BORDER);
    ui_rect(x - 1, thumb_y, 4, thumb_height, THEME_ACCENT);
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
        if (c == ' ') {
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
    case '!': case '"': case '\'': case ',': case ';': case '.': case '/':
    case '\\': case '`': case '~': case '(': case ')': case '[': case ']':
    case '{': case '}': case '_': return font_for(c);
    case '^': custom[0] = 0x04; custom[1] = 0x0a; custom[2] = 0x11; break;
    case '-': custom[3] = 0x1f; break;
    case '+': custom[1] = 0x04; custom[2] = 0x04; custom[3] = 0x1f; custom[4] = 0x04; custom[5] = 0x04; break;
    case '=': custom[2] = 0x1f; custom[4] = 0x1f; break;
    case '<': custom[1] = 0x02; custom[2] = 0x04; custom[3] = 0x08; custom[4] = 0x04; custom[5] = 0x02; break;
    case '>': custom[1] = 0x08; custom[2] = 0x04; custom[3] = 0x02; custom[4] = 0x04; custom[5] = 0x08; break;
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
    return c == ' ' ? 8 : 12;
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

static void ui_draw_script_cursor(int x, int y)
{
    if (!s_cursor_blink_visible) {
        return;
    }

    ui_rect(x, y - 1, 2, 10, THEME_ACCENT);
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
        width += c == ' ' ? 4 : 6;
    }
    return width;
}

static void ui_tiny_symbol(int x, int y, char c, uint32_t color)
{
    switch (c) {
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
        cx += text[i] == ' ' ? 4 : 6;
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

    if (s_print_mode == 1) {
        if (draw) ui_calc_text(x, y, text, color);
        return ui_calc_text_width(text, strlen(text));
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
    int percent = 0;
    bool has_battery_info = board_battery_get_percent(&percent);
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }

    int bars = 0;
    if (!has_battery_info) {
        bars = 0;
    } else if (percent > 75) {
        bars = 4;
    } else if (percent > 50) {
        bars = 3;
    } else if (percent > 25) {
        bars = 2;
    } else {
        bars = 1;
    }

    uint32_t fill = THEME_BATTERY_GREEN;
    if (bars <= 1) {
        fill = THEME_BATTERY_RED;
    } else if (bars == 2) {
        fill = THEME_BATTERY_YELLOW;
    }

    bool flash_off = has_battery_info && percent <= 10 &&
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
        const char *alpha_label = s_alpha_lowercase ? "a" : "A";
        x -= ui_text_width(alpha_label, 8, 1) + 8;
        ui_mode_badge(x, y, alpha_label, THEME_ACCENT);
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
    ui_rect(5, y + 3, 24, 22, THEME_BG);
    ui_app_icon(app_id, 6, y + 3, 22, header_app->color);
    ui_text_center(y + 10, header_app->title, THEME_HEADER_TEXT, 1);
    ui_shift_indicators(y + 7);
    ui_charge_indicator(276, y + 7);
    ui_battery_indicator(288, y + 9);
}

static void ui_present(void)
{
    board_display_lock();
    board_draw_rgb888_frame_320x240(opencalc_ui_canvas_pixels());
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

static int graph_entry_count(void)
{
    switch (s_graphing_mode) {
    case 1: return GRAPH_PARAM_COUNT * 2;
    case 2: return GRAPH_POLAR_COUNT;
    case 3: return GRAPH_SEQ_COUNT;
    default: return GRAPH_FUNC_COUNT;
    }
}

static uint32_t graph_entry_color(int entry)
{
    int index = entry;
    if (s_graphing_mode == 1) {
        index = entry / 2;
    }
    return s_graph_colors[index % GRAPH_COLOR_COUNT];
}

static char *graph_entry_buffer(int entry, size_t *size)
{
    if (size != NULL) {
        *size = 96;
    }

    switch (s_graphing_mode) {
    case 1: {
        int pair = entry / 2;
        if (pair < 0 || pair >= GRAPH_PARAM_COUNT) {
            return NULL;
        }
        return (entry % 2 == 0) ? s_graph_param_x[pair] : s_graph_param_y[pair];
    }
    case 2:
        if (entry < 0 || entry >= GRAPH_POLAR_COUNT) {
            return NULL;
        }
        return s_graph_polar_exprs[entry];
    case 3:
        if (entry < 0 || entry >= GRAPH_SEQ_COUNT) {
            return NULL;
        }
        return s_graph_seq_exprs[entry];
    default:
        if (entry < 0 || entry >= GRAPH_FUNC_COUNT) {
            return NULL;
        }
        return s_graph_exprs[entry];
    }
}

static bool graph_entry_is_enabled(int entry)
{
    switch (s_graphing_mode) {
    case 1: {
        int pair = entry / 2;
        return pair >= 0 && pair < GRAPH_PARAM_COUNT && s_graph_param_enabled[pair];
    }
    case 2:
        return entry >= 0 && entry < GRAPH_POLAR_COUNT && s_graph_polar_enabled[entry];
    case 3:
        return entry >= 0 && entry < GRAPH_SEQ_COUNT && s_graph_seq_enabled[entry];
    default:
        return entry >= 0 && entry < GRAPH_FUNC_COUNT && s_graph_enabled[entry];
    }
}

static void graph_entry_toggle(int entry)
{
    switch (s_graphing_mode) {
    case 1: {
        int pair = entry / 2;
        if (pair >= 0 && pair < GRAPH_PARAM_COUNT) {
            s_graph_param_enabled[pair] = !s_graph_param_enabled[pair];
        }
        break;
    }
    case 2:
        if (entry >= 0 && entry < GRAPH_POLAR_COUNT) {
            s_graph_polar_enabled[entry] = !s_graph_polar_enabled[entry];
        }
        break;
    case 3:
        if (entry >= 0 && entry < GRAPH_SEQ_COUNT) {
            s_graph_seq_enabled[entry] = !s_graph_seq_enabled[entry];
        }
        break;
    default:
        if (entry >= 0 && entry < GRAPH_FUNC_COUNT) {
            s_graph_enabled[entry] = !s_graph_enabled[entry];
        }
        break;
    }
}

static void graph_entry_label(int entry, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    switch (s_graphing_mode) {
    case 1:
        snprintf(out, out_size, "%c%dT", entry % 2 == 0 ? 'X' : 'Y', entry / 2 + 1);
        break;
    case 2:
        snprintf(out, out_size, "r%d", entry + 1);
        break;
    case 3:
        snprintf(out, out_size, "u%d", entry + 1);
        break;
    default:
        snprintf(out, out_size, "Y%d", entry + 1);
        break;
    }
}

static int graph_series_count(void)
{
    switch (s_graphing_mode) {
    case 1: return GRAPH_PARAM_COUNT;
    case 2: return GRAPH_POLAR_COUNT;
    case 3: return GRAPH_SEQ_COUNT;
    default: return GRAPH_FUNC_COUNT;
    }
}

static bool graph_series_is_active(int series)
{
    switch (s_graphing_mode) {
    case 1:
        return series >= 0 && series < GRAPH_PARAM_COUNT &&
            s_graph_param_enabled[series] && s_graph_param_x[series][0] != '\0' &&
            s_graph_param_y[series][0] != '\0';
    case 2:
        return series >= 0 && series < GRAPH_POLAR_COUNT &&
            s_graph_polar_enabled[series] && s_graph_polar_exprs[series][0] != '\0';
    case 3:
        return series >= 0 && series < GRAPH_SEQ_COUNT &&
            s_graph_seq_enabled[series] && s_graph_seq_exprs[series][0] != '\0';
    default:
        return series >= 0 && series < GRAPH_FUNC_COUNT &&
            s_graph_enabled[series] && s_graph_exprs[series][0] != '\0';
    }
}

static uint32_t graph_symbolic_fingerprint(int series)
{
    uint32_t hash = 2166136261u;
    const char *texts[2] = {NULL, NULL};
    switch (s_graphing_mode) {
    case 1:
        if (series >= 0 && series < GRAPH_PARAM_COUNT) {
            texts[0] = s_graph_param_x[series];
            texts[1] = s_graph_param_y[series];
        }
        break;
    case 2:
        if (series >= 0 && series < GRAPH_POLAR_COUNT) texts[0] = s_graph_polar_exprs[series];
        break;
    case 3:
        if (series >= 0 && series < GRAPH_SEQ_COUNT) texts[0] = s_graph_seq_exprs[series];
        break;
    default:
        if (series >= 0 && series < GRAPH_FUNC_COUNT) texts[0] = s_graph_exprs[series];
        break;
    }
    hash = (hash ^ (uint32_t)s_graphing_mode) * 16777619u;
    hash = (hash ^ (uint32_t)series) * 16777619u;
    for (int text = 0; text < 2; text++) {
        if (texts[text] == NULL) continue;
        for (const unsigned char *p = (const unsigned char *)texts[text]; *p != '\0'; p++) {
            hash = (hash ^ *p) * 16777619u;
        }
        hash = (hash ^ 0xffu) * 16777619u;
    }
    return hash;
}

static const char *graph_style_name(graph_style_t style)
{
    static const char *const names[] = {"line", "thick", "dotted", "points"};
    return names[(unsigned)style < GRAPH_STYLE_COUNT ? style : GRAPH_STYLE_LINE];
}

static void graph_series_label(int series, char *out, size_t out_size)
{
    int entry = s_graphing_mode == 1 ? series * 2 : series;
    graph_entry_label(entry, out, out_size);
    if (s_graphing_mode == 1 && out_size > 1) {
        snprintf(out, out_size, "P%d", series + 1);
    }
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static bool graph_background_load(void)
{
    uint8_t header[54];
    uint8_t row[UI_W * 4];
    if (!usb_msc_mount_app()) {
        snprintf(s_graph_status, sizeof(s_graph_status), "storage busy - eject USB drive");
        return false;
    }
    s_usb_storage_enabled = false;
    FILE *file = fopen(GRAPH_BACKGROUND_PATH, "rb");
    if (file == NULL) {
        snprintf(s_graph_status, sizeof(s_graph_status), "missing %s", GRAPH_BACKGROUND_PATH);
        return false;
    }

    bool ok = fread(header, 1, sizeof(header), file) == sizeof(header) &&
        header[0] == 'B' && header[1] == 'M';
    uint32_t pixel_offset = ok ? read_le32(header + 10) : 0;
    int32_t width = ok ? (int32_t)read_le32(header + 18) : 0;
    int32_t height = ok ? (int32_t)read_le32(header + 22) : 0;
    uint16_t planes = ok ? read_le16(header + 26) : 0;
    uint16_t bits = ok ? read_le16(header + 28) : 0;
    uint32_t compression = ok ? read_le32(header + 30) : 1;
    int32_t abs_height = height < 0 ? -height : height;
    ok = ok && width == UI_W && abs_height == UI_H && planes == 1 &&
        (bits == 24 || bits == 32) && compression == 0;
    size_t row_bytes = ok ? (((size_t)width * bits + 31u) / 32u) * 4u : 0;
    if (!ok || row_bytes > sizeof(row) || fseek(file, (long)pixel_offset, SEEK_SET) != 0) {
        fclose(file);
        snprintf(s_graph_status, sizeof(s_graph_status), "graph.bmp must be 320x240 RGB");
        return false;
    }

    for (int source_row = 0; source_row < UI_H && ok; source_row++) {
        if (fread(row, 1, row_bytes, file) != row_bytes) {
            ok = false;
            break;
        }
        int y = height > 0 ? UI_H - 1 - source_row : source_row;
        int bytes_per_pixel = bits / 8;
        for (int x = 0; x < UI_W; x++) {
            const uint8_t *pixel = row + x * bytes_per_pixel;
            s_graph_background[y * UI_W + x] =
                ((uint32_t)pixel[2] << 16) | ((uint32_t)pixel[1] << 8) | pixel[0];
        }
    }
    fclose(file);
    if (!ok) {
        snprintf(s_graph_status, sizeof(s_graph_status), "graph.bmp read failed");
        return false;
    }
    snprintf(s_graph_status, sizeof(s_graph_status), "background loaded");
    return true;
}

static double graph_angle_to_radians_for_plot(double angle)
{
    return s_angle_mode == 0 ? angle * 3.14159265358979323846 / 180.0 : angle;
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

static int work_graph_series_count(const ui_work_job_t *job)
{
    if (job == NULL) {
        return 0;
    }
    switch (job->graph.graphing_mode) {
    case 1: return GRAPH_PARAM_COUNT;
    case 2: return GRAPH_POLAR_COUNT;
    case 3: return GRAPH_SEQ_COUNT;
    default: return GRAPH_FUNC_COUNT;
    }
}

static bool work_graph_series_enabled(const ui_work_job_t *job, int fn)
{
    if (job == NULL || fn < 0) {
        return false;
    }
    switch (job->graph.graphing_mode) {
    case 1:
        return fn < GRAPH_PARAM_COUNT && job->graph.param_enabled[fn] &&
            job->graph.param_x[fn][0] != '\0' && job->graph.param_y[fn][0] != '\0';
    case 2:
        return fn < GRAPH_POLAR_COUNT && job->graph.polar_enabled[fn] &&
            job->graph.polar_exprs[fn][0] != '\0';
    case 3:
        return fn < GRAPH_SEQ_COUNT && job->graph.seq_enabled[fn] &&
            job->graph.seq_exprs[fn][0] != '\0';
    default:
        return fn < GRAPH_FUNC_COUNT && job->graph.enabled[fn] &&
            job->graph.exprs[fn][0] != '\0';
    }
}

static bool work_graph_eval_series_at(const ui_work_job_t *job, int fn, double input,
                                      double *x, double *y, double *metric)
{
    if (!work_graph_series_enabled(job, fn) || x == NULL || y == NULL || metric == NULL) {
        return false;
    }

    switch (job->graph.graphing_mode) {
    case 1:
        if (!graph_eval_expression_var(job->graph.param_x[fn], 't', input, x) ||
            !graph_eval_expression_var(job->graph.param_y[fn], 't', input, y)) {
            return false;
        }
        *metric = *y;
        return isfinite(*x) && isfinite(*y);
    case 2: {
        double r = 0.0;
        if (!graph_eval_expression_var(job->graph.polar_exprs[fn], 't', input, &r)) {
            return false;
        }
        double rad = job->degrees ? input * 3.14159265358979323846 / 180.0 : input;
        *x = r * cos(rad);
        *y = r * sin(rad);
        *metric = r;
        return isfinite(*x) && isfinite(*y) && isfinite(r);
    }
    case 3:
        if (!graph_eval_expression_var(job->graph.seq_exprs[fn], 'n', input, y)) {
            return false;
        }
        *x = input;
        *metric = *y;
        return isfinite(*y);
    default:
        if (!graph_eval_expression(job->graph.exprs[fn], input, y)) {
            return false;
        }
        *x = input;
        *metric = *y;
        return isfinite(*y);
    }
}

static void work_graph_input_range(const ui_work_job_t *job, double *min_input, double *max_input)
{
    if (job == NULL || min_input == NULL || max_input == NULL) {
        return;
    }
    switch (job->graph.graphing_mode) {
    case 1:
    case 2:
        *min_input = job->graph.tmin;
        *max_input = job->graph.tmax;
        break;
    case 3:
        *min_input = job->graph.nmin;
        *max_input = job->graph.nmax;
        break;
    default:
        *min_input = job->graph.xmin;
        *max_input = job->graph.xmax;
        break;
    }
}

static const char *work_graph_series_prefix(const ui_work_job_t *job)
{
    switch (job != NULL ? job->graph.graphing_mode : 0) {
    case 1: return "P";
    case 2: return "r";
    case 3: return "u";
    default: return "Y";
    }
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

static void work_graph_add_poi_at(graph_poi_t *pois, int *count, graph_poi_type_t type, int fn, int other_fn,
                                  double input, double x, double y);

static void work_graph_add_poi(graph_poi_t *pois, int *count, graph_poi_type_t type, int fn, int other_fn, double x, double y)
{
    work_graph_add_poi_at(pois, count, type, fn, other_fn, x, x, y);
}

static void work_graph_add_poi_at(graph_poi_t *pois, int *count, graph_poi_type_t type, int fn, int other_fn,
                                  double input, double x, double y)
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
    pois[*count] = (graph_poi_t){.input = input, .x = x, .y = y, .fn = fn, .other_fn = other_fn, .type = type};
    (*count)++;
}

static int work_graph_collect_pois(const ui_work_job_t *job, graph_poi_t *pois, int max_count)
{
    int count = 0;
    int enabled[10];
    int enabled_count = 0;
    double min_input = 0.0;
    double max_input = 0.0;
    work_graph_input_range(job, &min_input, &max_input);
    double step = (max_input - min_input) / 96.0;
    if (step <= 0.0) {
        return 0;
    }

    for (int fn = 0; fn < work_graph_series_count(job); fn++) {
        if (!work_graph_series_enabled(job, fn)) {
            continue;
        }
        enabled[enabled_count++] = fn;

        double px0 = 0.0;
        double py0 = 0.0;
        double m0 = 0.0;
        if (job->graph.graphing_mode == 0 && min_input <= 0.0 && max_input >= 0.0 &&
            work_graph_eval_series_at(job, fn, 0.0, &px0, &py0, &m0)) {
            work_graph_add_poi_at(pois, &count, GRAPH_POI_Y_INTERCEPT, fn, -1, 0.0, px0, py0);
        }

        double input_prev2 = min_input;
        double x_prev2 = 0.0;
        double y_prev2 = 0.0;
        double metric_prev2 = 0.0;
        double input_prev = min_input + step;
        double x_prev = 0.0;
        double y_prev = 0.0;
        double metric_prev = 0.0;
        bool have_prev2 = work_graph_eval_series_at(job, fn, input_prev2, &x_prev2, &y_prev2, &metric_prev2);
        bool have_prev = work_graph_eval_series_at(job, fn, input_prev, &x_prev, &y_prev, &metric_prev);
        for (int i = 2; i <= 96 && count < max_count; i++) {
            double input = min_input + step * (double)i;
            double x = 0.0;
            double y = 0.0;
            double metric = 0.0;
            bool have = work_graph_eval_series_at(job, fn, input, &x, &y, &metric);

            bool crosses_x_axis = have_prev && have &&
                ((y_prev <= 0.0 && y >= 0.0) || (y_prev >= 0.0 && y <= 0.0));
            bool exact_sequence_zero = job->graph.graphing_mode == 3 && have && fabs(y) <= 1e-10;
            if ((job->graph.graphing_mode != 3 && crosses_x_axis) || exact_sequence_zero) {
                if (job->graph.graphing_mode == 0) {
                    double zx = 0.0;
                    double zy = 0.0;
                    if (work_graph_refine_zero(job, fn, input_prev, input, &zx, &zy)) {
                        work_graph_add_poi(pois, &count, GRAPH_POI_ZERO, fn, -1, zx, zy);
                    }
                } else {
                    work_graph_add_poi_at(pois, &count, GRAPH_POI_ZERO, fn, -1, input, x, y);
                }
            }

            if (job->graph.graphing_mode != 0 && have_prev && have &&
                ((x_prev <= 0.0 && x >= 0.0) || (x_prev >= 0.0 && x <= 0.0))) {
                double use_input = fabs(x_prev) <= fabs(x) ? input_prev : input;
                double use_x = fabs(x_prev) <= fabs(x) ? x_prev : x;
                double use_y = fabs(x_prev) <= fabs(x) ? y_prev : y;
                work_graph_add_poi_at(pois, &count, GRAPH_POI_Y_INTERCEPT, fn, -1,
                                      use_input, use_x, use_y);
            }

            if (have_prev2 && have_prev && have) {
                if (y_prev < y_prev2 && y_prev < y) {
                    work_graph_add_poi_at(pois, &count, GRAPH_POI_MIN, fn, -1, input_prev, x_prev, y_prev);
                } else if (y_prev > y_prev2 && y_prev > y) {
                    work_graph_add_poi_at(pois, &count, GRAPH_POI_LOCAL_MAX, fn, -1, input_prev, x_prev, y_prev);
                }
            }

            input_prev2 = input_prev;
            x_prev2 = x_prev;
            y_prev2 = y_prev;
            metric_prev2 = metric_prev;
            have_prev2 = have_prev;
            input_prev = input;
            x_prev = x;
            y_prev = y;
            metric_prev = metric;
            have_prev = have;
        }
    }

    if (job->graph.graphing_mode != 0) {
        for (int a = 0; a < enabled_count; a++) {
            for (int b = a + 1; b < enabled_count; b++) {
                int fn_a = enabled[a];
                int fn_b = enabled[b];
                double ax = 0.0, ay = 0.0, am = 0.0;
                bool have_a = false;
                for (int i = 0; i <= 96 && count < max_count; i++) {
                    double input_a = min_input + step * (double)i;
                    have_a = work_graph_eval_series_at(job, fn_a, input_a, &ax, &ay, &am);
                    if (!have_a) {
                        continue;
                    }
                    for (int j = 0; j <= 96; j++) {
                        double input_b = min_input + step * (double)j;
                        double bx = 0.0, by = 0.0, bm = 0.0;
                        if (!work_graph_eval_series_at(job, fn_b, input_b, &bx, &by, &bm)) {
                            continue;
                        }
                        double tol_x = fabs(job->graph.xmax - job->graph.xmin) / 80.0;
                        double tol_y = fabs(job->graph.ymax - job->graph.ymin) / 80.0;
                        if (tol_x <= 0.0) tol_x = 0.1;
                        if (tol_y <= 0.0) tol_y = 0.1;
                        if (fabs(ax - bx) <= tol_x && fabs(ay - by) <= tol_y) {
                            work_graph_add_poi(pois, &count, GRAPH_POI_INTERSECTION, fn_a, fn_b,
                                               (ax + bx) * 0.5, (ay + by) * 0.5);
                            pois[count - 1].input = input_a;
                            break;
                        }
                    }
                }
            }
        }
        return count;
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
    for (int i = 0; i < work_graph_series_count(job); i++) {
        if (work_graph_series_enabled(job, i)) {
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
    double min_input = 0.0;
    double max_input = 0.0;
    work_graph_input_range(job, &min_input, &max_input);
    *trace_x = (min_input + max_input) * 0.5;
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
        double dist = fabs(pois[i].input - target);
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
    result->graph.trace_x = pois[best].input;
    result->graph.trace_fn = pois[best].fn;
    snprintf(result->graph.status, sizeof(result->graph.status), "%s %s%d x %.4g y %.4g",
             graph_poi_label(type), work_graph_series_prefix(job), pois[best].fn + 1, display_x, display_y);
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

    double x = 0.0;
    double y = 0.0;
    double metric = 0.0;
    if (!work_graph_eval_series_at(job, trace_fn, trace_x, &x, &y, &metric)) {
        return false;
    }
    result->graph.trace = trace;
    result->graph.trace_x = trace_x;
    result->graph.trace_fn = trace_fn;
    if (job->graph.graphing_mode == 1) {
        snprintf(result->graph.status, sizeof(result->graph.status), "value P%d t %.4g x %.4g y %.4g",
                 trace_fn + 1, trace_x, x, y);
    } else if (job->graph.graphing_mode == 2) {
        snprintf(result->graph.status, sizeof(result->graph.status), "value r%d t %.4g r %.4g",
                 trace_fn + 1, trace_x, metric);
    } else if (job->graph.graphing_mode == 3) {
        snprintf(result->graph.status, sizeof(result->graph.status), "value u%d n %.0f = %.6g",
                 trace_fn + 1, trace_x, y);
    } else {
        snprintf(result->graph.status, sizeof(result->graph.status), "value Y%d x %.4g y %.6g",
                 trace_fn + 1, trace_x, y);
    }
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

    double min_input = 0.0;
    double max_input = 0.0;
    work_graph_input_range(job, &min_input, &max_input);
    double span = max_input - min_input;
    double h = span > 0.0 ? span / 2000.0 : 0.001;
    if (h < 1e-6) h = 1e-6;

    double x0 = 0.0, y0 = 0.0, m0 = 0.0;
    double x1 = 0.0, y1 = 0.0, m1 = 0.0;
    if (!work_graph_eval_series_at(job, trace_fn, trace_x - h, &x0, &y0, &m0) ||
        !work_graph_eval_series_at(job, trace_fn, trace_x + h, &x1, &y1, &m1)) {
        return false;
    }

    result->graph.trace = trace;
    result->graph.trace_x = trace_x;
    result->graph.trace_fn = trace_fn;
    if (job->graph.graphing_mode == 1 || job->graph.graphing_mode == 2) {
        double dx = x1 - x0;
        if (fabs(dx) <= 1e-12) {
            return false;
        }
        snprintf(result->graph.status, sizeof(result->graph.status), "dy/dx %s%d t %.4g = %.6g",
                 work_graph_series_prefix(job), trace_fn + 1, trace_x, (y1 - y0) / dx);
    } else if (job->graph.graphing_mode == 3) {
        double xm = 0.0, ym = 0.0, mm = 0.0;
        double xp = 0.0, yp = 0.0, mp = 0.0;
        double n = floor(trace_x + 0.5);
        if (!work_graph_eval_series_at(job, trace_fn, n, &xm, &ym, &mm) ||
            !work_graph_eval_series_at(job, trace_fn, n + 1.0, &xp, &yp, &mp)) {
            return false;
        }
        snprintf(result->graph.status, sizeof(result->graph.status), "delta u%d n %.0f = %.6g",
                 trace_fn + 1, n, yp - ym);
    } else {
        snprintf(result->graph.status, sizeof(result->graph.status), "dy/dx Y%d x %.4g = %.6g",
                 trace_fn + 1, trace_x, (y1 - y0) / (2.0 * h));
    }
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

    if (job->graph.graphing_mode == 3) {
        int first = (int)ceil(a);
        int last = (int)floor(b);
        double sum = 0.0;
        for (int n = first; n <= last; n++) {
            double x = 0.0, y = 0.0, metric = 0.0;
            if (!work_graph_eval_series_at(job, trace_fn, (double)n, &x, &y, &metric)) {
                return false;
            }
            sum += y;
        }
        result->graph.trace = trace;
        result->graph.trace_x = trace_x;
        result->graph.trace_fn = trace_fn;
        snprintf(result->graph.status, sizeof(result->graph.status), "sum u%d %d..%d = %.6g",
                 trace_fn + 1, first, last, sum * sign);
        return true;
    }

    const int steps = 96;
    double dx = (b - a) / (double)steps;
    double sum = 0.0;
    double previous_x = 0.0;
    double previous_y = 0.0;
    double previous_metric = 0.0;
    bool have_previous = false;
    for (int i = 0; i <= steps; i++) {
        double y = 0.0;
        double x = a + dx * (double)i;
        double px = 0.0;
        double metric = 0.0;
        if (!work_graph_eval_series_at(job, trace_fn, x, &px, &y, &metric)) {
            return false;
        }
        if (job->graph.graphing_mode == 1) {
            if (have_previous) sum += (previous_y + y) * 0.5 * (px - previous_x);
        } else if (job->graph.graphing_mode == 2) {
            double radians_per_input = job->degrees ? 3.14159265358979323846 / 180.0 : 1.0;
            if (have_previous) {
                sum += 0.25 * (previous_metric * previous_metric + metric * metric) *
                    dx * radians_per_input;
            }
        } else {
            sum += metric * (i == 0 || i == steps ? 0.5 : 1.0) * dx;
        }
        previous_x = px;
        previous_y = y;
        previous_metric = metric;
        have_previous = true;
    }

    result->graph.trace = trace;
    result->graph.trace_x = trace_x;
    result->graph.trace_fn = trace_fn;
    const char *operation = job->graph.graphing_mode == 2 ? "area" : "int";
    snprintf(result->graph.status, sizeof(result->graph.status), "%s %s%d 0..%.4g = %.6g",
             operation, work_graph_series_prefix(job), trace_fn + 1, trace_x, sum * sign);
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

static void inequality_slot_key(int slot, char *key, size_t key_size)
{
    snprintf(key, key_size, "ineq_%d", slot);
}

static bool inequality_set_text(int slot, const char *text, bool persist)
{
    if (slot < 0 || slot >= INEQ_MAX || text == NULL || text[0] == '\0') return false;
    opencalc_inequality_problem_t problem;
    if (!opencalc_inequality_parse(text, &problem)) return false;
    s_ineqs[slot].enabled = true;
    s_ineqs[slot].problem = problem;
    snprintf(s_ineqs[slot].text, sizeof(s_ineqs[slot].text), "%s", text);
    if (persist) {
        char key[16];
        inequality_slot_key(slot, key, sizeof(key));
        opencalc_persist_set_string(key, text);
        snprintf(key, sizeof(key), "ineq_on_%d", slot);
        opencalc_persist_set_u32(key, 1);
    }
    return true;
}

static void inequality_delete_slot(int slot)
{
    if (slot < 0 || slot >= INEQ_MAX) return;
    memset(&s_ineqs[slot], 0, sizeof(s_ineqs[slot]));
    char key[16];
    inequality_slot_key(slot, key, sizeof(key));
    opencalc_persist_erase(key);
    snprintf(key, sizeof(key), "ineq_on_%d", slot);
    opencalc_persist_erase(key);
}

static void inequality_load_all(void)
{
    inequality_clear_all();
    char key[16];
    char text[160];
    for (int slot = 0; slot < INEQ_MAX; slot++) {
        inequality_slot_key(slot, key, sizeof(key));
        if (opencalc_persist_get_string(key, text, sizeof(text)) &&
            inequality_set_text(slot, text, false)) {
            snprintf(key, sizeof(key), "ineq_on_%d", slot);
            s_ineqs[slot].enabled = opencalc_persist_get_u32(key, 1) != 0;
        }
    }
    s_ineq_join_or = opencalc_persist_get_u32("ineq_or", 0) != 0;
    s_ineq_integer_mode = opencalc_persist_get_u32("ineq_int", 0) != 0;
    s_ineq_notation = (int)(opencalc_persist_get_u32("ineq_note", 0) % 3);
    opencalc_persist_get_string("ineq_last", s_ineq_problem_text, sizeof(s_ineq_problem_text));
    if (s_ineq_problem_text[0] == '\0') snprintf(s_ineq_problem_text, sizeof(s_ineq_problem_text), "x^2-4>=0");
}

static bool inequality_load_from_calc(void)
{
    if (!inequality_set_text(s_ineq_selection, s_calc_input, true)) return false;
    snprintf(s_ineq_status, sizeof(s_ineq_status), "I%d saved from Calculator", s_ineq_selection + 1);
    return true;
}

static bool inequality_any_enabled(void)
{
    for (int i = 0; i < INEQ_MAX; i++) {
        if (s_ineqs[i].enabled && s_ineqs[i].problem.count > 0) return true;
    }
    return false;
}

static bool inequality_clause_difference(const opencalc_inequality_clause_t *clause,
                                         double x, double y, double *difference)
{
    double lhs = 0.0, rhs = 0.0;
    if (clause == NULL || difference == NULL ||
        !graph_eval_expression_xy(clause->lhs, x, y, &lhs) ||
        !graph_eval_expression_xy(clause->rhs, x, y, &rhs)) return false;
    *difference = lhs - rhs;
    return isfinite(*difference);
}

static bool inequality_expr_is_y(const char *expr)
{
    return expr != NULL && (strcmp(expr, "y") == 0 || strcmp(expr, "Y") == 0);
}

static bool inequality_expr_has_y(const char *expr)
{
    if (expr == NULL) return false;
    for (const char *p = expr; *p; p++) {
        if (*p == 'y' || *p == 'Y') return true;
    }
    return false;
}

static bool inequality_expr_is_x(const char *expr)
{
    return expr != NULL && (strcmp(expr, "x") == 0 || strcmp(expr, "X") == 0);
}

static bool inequality_expr_has_x(const char *expr)
{
    if (expr == NULL) return false;
    for (const char *p = expr; *p; p++) {
        if (*p == 'x' || *p == 'X') return true;
    }
    return false;
}

static opencalc_inequality_relation_t inequality_reverse_relation(opencalc_inequality_relation_t relation)
{
    switch (relation) {
    case OPENCALC_INEQ_LT: return OPENCALC_INEQ_GT;
    case OPENCALC_INEQ_LE: return OPENCALC_INEQ_GE;
    case OPENCALC_INEQ_GT: return OPENCALC_INEQ_LT;
    case OPENCALC_INEQ_GE: return OPENCALC_INEQ_LE;
    default: return relation;
    }
}

static bool inequality_explicit_y_clause(const opencalc_inequality_clause_t *source,
                                         char *boundary, size_t boundary_size,
                                         opencalc_inequality_relation_t *relation)
{
    if (inequality_expr_is_y(source->lhs) && !inequality_expr_has_y(source->rhs)) {
        snprintf(boundary, boundary_size, "%s", source->rhs);
        *relation = source->relation;
        return true;
    }
    if (inequality_expr_is_y(source->rhs) && !inequality_expr_has_y(source->lhs)) {
        snprintf(boundary, boundary_size, "%s", source->lhs);
        *relation = inequality_reverse_relation(source->relation);
        return true;
    }
    return false;
}

static void inequality_sync_graph_boundaries(void)
{
    int graph_slot = 0;
    clear_graph_expressions();
    for (int i = 0; i < INEQ_MAX && graph_slot < GRAPH_FUNC_COUNT; i++) {
        if (!s_ineqs[i].enabled) continue;
        for (int c = 0; c < s_ineqs[i].problem.count && graph_slot < GRAPH_FUNC_COUNT; c++) {
            char boundary[96];
            opencalc_inequality_relation_t relation;
            if (!inequality_explicit_y_clause(&s_ineqs[i].problem.clauses[c], boundary,
                                              sizeof(boundary), &relation)) continue;
            snprintf(s_graph_exprs[graph_slot], sizeof(s_graph_exprs[graph_slot]), "%s", boundary);
            s_graph_enabled[graph_slot] = true;
            graph_slot++;
        }
    }
}

static bool inequality_problem_has_y(const opencalc_inequality_problem_t *problem)
{
    if (problem == NULL) return false;
    for (int i = 0; i < problem->count; i++) {
        if (inequality_expr_has_y(problem->clauses[i].lhs) ||
            inequality_expr_has_y(problem->clauses[i].rhs)) return true;
    }
    return false;
}

static bool inequality_vertical_clause(const opencalc_inequality_clause_t *clause, double *x)
{
    const char *constant = NULL;
    if (inequality_expr_is_x(clause->lhs) && !inequality_expr_has_x(clause->rhs) &&
        !inequality_expr_has_y(clause->rhs)) constant = clause->rhs;
    else if (inequality_expr_is_x(clause->rhs) && !inequality_expr_has_x(clause->lhs) &&
             !inequality_expr_has_y(clause->lhs)) constant = clause->lhs;
    return constant != NULL && graph_eval_expression_xy(constant, 0.0, 0.0, x);
}

static void draw_inequality_layer(const graph_view_t *view)
{
    if (view == NULL || !inequality_any_enabled()) {
        return;
    }

    const int fill_step = 4;
    for (int px = 0; px < UI_W; px += fill_step) {
        double x = graph_world_x(view, px);
        for (int py = view->screen_top; py <= view->screen_bottom; py += fill_step) {
            double y = s_graph_ymax -
                ((double)(py - view->screen_top) /
                 (double)(view->screen_bottom - view->screen_top)) *
                (s_graph_ymax - s_graph_ymin);
            int active = 0;
            int matches = 0;
            int first_match = 0;
            for (int i = 0; i < INEQ_MAX; i++) {
                if (!s_ineqs[i].enabled) {
                    continue;
                }
                active++;
                bool ok = false;
                if (!opencalc_inequality_evaluate(&s_ineqs[i].problem, x, y, &ok)) continue;
                if (ok && matches++ == 0) first_match = i;
            }

            bool shade = s_ineq_join_or ? matches > 0 : active > 0 && matches == active;
            if (shade) {
                uint32_t color = s_light_mode ? 0xc7def5 :
                    (matches > 1 ? 0x203a58 : (s_graph_colors[first_match % GRAPH_COLOR_COUNT] & 0x303030));
                ui_rect(px, py, fill_step, fill_step, color);
            }
        }
    }

    for (int i = 0; i < INEQ_MAX; i++) {
        if (!s_ineqs[i].enabled) {
            continue;
        }

        uint32_t color = s_graph_colors[i % GRAPH_COLOR_COUNT];
        for (int clause_index = 0; clause_index < s_ineqs[i].problem.count; clause_index++) {
            const opencalc_inequality_clause_t *clause = &s_ineqs[i].problem.clauses[clause_index];
            bool inclusive = clause->relation == OPENCALC_INEQ_LE || clause->relation == OPENCALC_INEQ_GE;
            double vertical_x = 0.0;
            if (inequality_vertical_clause(clause, &vertical_x)) {
                int sx = graph_screen_x(view, vertical_x);
                if (inclusive) ui_line(sx, view->screen_top, sx, view->screen_bottom, color);
                else for (int py = view->screen_top; py < view->screen_bottom; py += 8) {
                    ui_line(sx, py, sx, py + 3, color);
                }
                continue;
            }
            char explicit_y[96];
            opencalc_inequality_relation_t relation;
            if (inequality_explicit_y_clause(clause, explicit_y, sizeof(explicit_y), &relation)) {
                inclusive = relation == OPENCALC_INEQ_LE || relation == OPENCALC_INEQ_GE;
                bool have_previous = false;
                int previous_x = 0, previous_y = 0;
                for (int px = 0; px < UI_W; px += 2) {
                    double y = 0.0;
                    if (!graph_eval_expression(explicit_y, graph_world_x(view, px), &y)) {
                        have_previous = false;
                        continue;
                    }
                    int py = graph_screen_y(view, y);
                    if (py < view->screen_top - 20 || py > view->screen_bottom + 20) {
                        have_previous = false;
                        continue;
                    }
                    if (have_previous && (inclusive || ((px / 4) % 2 == 0))) {
                        ui_line(previous_x, previous_y, px, py, color);
                    }
                    previous_x = px;
                    previous_y = py;
                    have_previous = true;
                }
                continue;
            }
            const int contour_step = 4;
            for (int px = 0; px < UI_W - contour_step; px += contour_step) {
                double x = graph_world_x(view, px);
                double x_next = graph_world_x(view, px + contour_step);
                for (int py = view->screen_top; py < view->screen_bottom - contour_step; py += contour_step) {
                    double y = s_graph_ymax - ((double)(py - view->screen_top) /
                        (double)(view->screen_bottom - view->screen_top)) * (s_graph_ymax - s_graph_ymin);
                    double y_next = s_graph_ymax - ((double)(py + contour_step - view->screen_top) /
                        (double)(view->screen_bottom - view->screen_top)) * (s_graph_ymax - s_graph_ymin);
                    double d = 0.0, dx = 0.0, dy = 0.0;
                    if (!inequality_clause_difference(clause, x, y, &d) ||
                        !inequality_clause_difference(clause, x_next, y, &dx) ||
                        !inequality_clause_difference(clause, x, y_next, &dy)) continue;
                    bool crossing = fabs(d) < 1e-7 || d * dx <= 0.0 || d * dy <= 0.0;
                    if (crossing && (inclusive || ((px + py) % 8) < 4)) {
                        ui_rect(px, py, contour_step, contour_step, color);
                    }
                }
            }
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

static int inequality_collect_intersections(graph_poi_t *points, int max_count)
{
    inequality_sync_graph_boundaries();
    graph_poi_t graph_points[GRAPH_POI_LIMIT];
    int graph_count = graph_collect_pois(graph_points, GRAPH_POI_LIMIT);
    int count = 0;
    for (int i = 0; i < graph_count && count < max_count; i++) {
        if (graph_points[i].type == GRAPH_POI_INTERSECTION) points[count++] = graph_points[i];
    }

    for (int vertical_slot = 0; vertical_slot < INEQ_MAX && count < max_count; vertical_slot++) {
        if (!s_ineqs[vertical_slot].enabled) continue;
        for (int vc = 0; vc < s_ineqs[vertical_slot].problem.count && count < max_count; vc++) {
            double x = 0.0;
            if (!inequality_vertical_clause(&s_ineqs[vertical_slot].problem.clauses[vc], &x)) continue;
            for (int curve_slot = 0; curve_slot < INEQ_MAX && count < max_count; curve_slot++) {
                if (!s_ineqs[curve_slot].enabled) continue;
                for (int cc = 0; cc < s_ineqs[curve_slot].problem.count && count < max_count; cc++) {
                    char boundary[96];
                    opencalc_inequality_relation_t relation;
                    if (!inequality_explicit_y_clause(&s_ineqs[curve_slot].problem.clauses[cc],
                                                      boundary, sizeof(boundary), &relation)) continue;
                    double y = 0.0;
                    if (!graph_eval_expression(boundary, x, &y)) continue;
                    bool duplicate = false;
                    for (int p = 0; p < count; p++) {
                        if (fabs(points[p].x - x) < 1e-5 && fabs(points[p].y - y) < 1e-5) {
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate) {
                        points[count++] = (graph_poi_t){.x = x, .y = y, .fn = curve_slot,
                            .other_fn = vertical_slot, .type = GRAPH_POI_INTERSECTION};
                    }
                }
            }
        }
    }
    return count;
}

static void inequality_open_editor(bool graph_editor)
{
    s_ineq_graph_editor = graph_editor;
    s_ineq_status[0] = '\0';
    s_page = PAGE_INEQ_EDITOR;
    s_current_app = APP_INEQUALITY;
    ui_draw_current();
}

static bool inequality_solve_text(const char *text, bool sign_chart)
{
    if (text == NULL || text[0] == '\0' ||
        !opencalc_inequality_parse(text, &s_ineq_problem)) {
        snprintf(s_ineq_status, sizeof(s_ineq_status), "Enter a valid inequality in Calculator");
        return false;
    }
    if (inequality_problem_has_y(&s_ineq_problem)) {
        snprintf(s_ineq_status, sizeof(s_ineq_status), "Use Graph regions for x/y inequalities");
        return false;
    }
    if (!opencalc_inequality_solve_1d(&s_ineq_problem, 100.0, &s_ineq_solution) ||
        !opencalc_inequality_format_intervals(&s_ineq_solution, s_ineq_interval_text,
                                              sizeof(s_ineq_interval_text))) {
        snprintf(s_ineq_status, sizeof(s_ineq_status), "Numerical solve failed");
        return false;
    }
    if (s_ineq_integer_mode) {
        char real_intervals[sizeof(s_ineq_interval_text)];
        snprintf(real_intervals, sizeof(real_intervals), "%s", s_ineq_interval_text);
        snprintf(s_ineq_interval_text, sizeof(s_ineq_interval_text), "Z intersect %.230s", real_intervals);
    }
    snprintf(s_ineq_problem_text, sizeof(s_ineq_problem_text), "%s", text);
    opencalc_persist_set_string("ineq_last", text);
    s_ineq_sign_chart = sign_chart;
    s_ineq_exact_text[0] = '\0';
    s_ineq_result_scroll = 0;
    s_ineq_notation = 0;
    snprintf(s_ineq_status, sizeof(s_ineq_status), "%s domain; numerical interval fallback",
             s_ineq_integer_mode ? "Integer" : "Real");
    s_page = PAGE_INEQ_RESULT;
    s_current_app = APP_INEQUALITY;
    ui_draw_current();
    return true;
}

static bool inequality_solve_from_calc(bool sign_chart)
{
    const char *text = s_calc_input[0] != '\0' ? s_calc_input : s_ineq_problem_text;
    return inequality_solve_text(text, sign_chart);
}

static void inequality_format_standard(char *out, size_t out_size)
{
    out[0] = '\0';
    if (s_ineq_solution.empty) {
        snprintf(out, out_size, "no solution");
        return;
    }
    if (s_ineq_solution.all_real) {
        snprintf(out, out_size, "all %s numbers", s_ineq_integer_mode ? "integer" : "real");
        return;
    }
    size_t used = 0;
    for (int i = 0; i < s_ineq_solution.interval_count && used + 2 < out_size; i++) {
        const opencalc_inequality_interval_t *interval = &s_ineq_solution.intervals[i];
        char part[72];
        if (!interval->low_infinite && !interval->high_infinite &&
            fabs(interval->low - interval->high) < 1e-7) {
            snprintf(part, sizeof(part), "x=%.6g", interval->low);
        } else if (interval->low_infinite) {
            snprintf(part, sizeof(part), "x%s%.6g", interval->high_closed ? "<=" : "<", interval->high);
        } else if (interval->high_infinite) {
            snprintf(part, sizeof(part), "x%s%.6g", interval->low_closed ? ">=" : ">", interval->low);
        } else {
            snprintf(part, sizeof(part), "%.6g%sx%s%.6g", interval->low,
                     interval->low_closed ? "<=" : "<", interval->high_closed ? "<=" : "<",
                     interval->high);
        }
        int written = snprintf(out + used, out_size - used, "%s%s", i ? " or " : "", part);
        if (written < 0 || (size_t)written >= out_size - used) break;
        used += (size_t)written;
    }
}

static bool inequality_request_exact(void)
{
    char request[224];
    snprintf(request, sizeof(request), "solve(%s,x)", s_ineq_problem_text);
    if (!submit_solver_symbolic_job(request, "Exact Inequality")) return false;
    s_ineq_symbolic_pending = true;
    return true;
}

static void inequality_copy_result_to_calculator(void)
{
    const char *result = s_ineq_exact_text[0] != '\0' ? s_ineq_exact_text : s_ineq_interval_text;
    snprintf(s_calc_input, sizeof(s_calc_input), "%s", result);
    s_calc_cursor = strlen(s_calc_input);
    s_page = PAGE_CALCULATOR;
    s_current_app = APP_CALCULATOR;
    ui_draw_current();
}

static void inequality_save_intervals_to_list(void)
{
    int count = 0;
    for (int i = 0; i < s_ineq_solution.interval_count && count + 1 < LIST_MAX_VALUES; i++) {
        const opencalc_inequality_interval_t *interval = &s_ineq_solution.intervals[i];
        if (!interval->low_infinite) s_lists[s_list_index][count++] = interval->low;
        if (!interval->high_infinite &&
            (interval->low_infinite || fabs(interval->high - interval->low) > 1e-9)) {
            s_lists[s_list_index][count++] = interval->high;
        }
    }
    s_list_counts[s_list_index] = count;
    snprintf(s_ineq_status, sizeof(s_ineq_status), "%d finite endpoints saved to L%d", count,
             s_list_index + 1);
    ui_draw_current();
}

static void inequality_store_intersections(void)
{
    graph_poi_t pois[GRAPH_POI_LIMIT];
    int poi_count = inequality_collect_intersections(pois, GRAPH_POI_LIMIT);
    int count = 0;
    for (int i = 0; i < poi_count && count < LIST_MAX_VALUES; i++) {
        s_lists[0][count] = pois[i].x;
        s_lists[1][count] = pois[i].y;
        count++;
    }
    s_list_counts[0] = count;
    s_list_counts[1] = count;
    snprintf(s_ineq_status, sizeof(s_ineq_status), "%d intersections saved to L1/L2", count);
    ui_draw_current();
}

static void inequality_optimize_from_calc(void)
{
    if (s_calc_input[0] == '\0') {
        snprintf(s_ineq_status, sizeof(s_ineq_status), "Put a linear objective in Calculator first");
        ui_draw_current();
        return;
    }
    graph_poi_t pois[GRAPH_POI_LIMIT];
    int poi_count = inequality_collect_intersections(pois, GRAPH_POI_LIMIT);
    bool found = false;
    double minimum = 0.0, maximum = 0.0;
    double min_x = 0.0, min_y = 0.0, max_x = 0.0, max_y = 0.0;
    for (int i = 0; i < poi_count; i++) {
        bool feasible = false;
        int active = 0, matches = 0;
        for (int slot = 0; slot < INEQ_MAX; slot++) {
            if (!s_ineqs[slot].enabled) continue;
            active++;
            bool match = false;
            if (opencalc_inequality_evaluate(&s_ineqs[slot].problem, pois[i].x, pois[i].y, &match) && match) matches++;
        }
        feasible = s_ineq_join_or ? matches > 0 : active > 0 && matches == active;
        double value = 0.0;
        if (!feasible || !graph_eval_expression_xy(s_calc_input, pois[i].x, pois[i].y, &value)) continue;
        if (!found || value < minimum) { minimum = value; min_x = pois[i].x; min_y = pois[i].y; }
        if (!found || value > maximum) { maximum = value; max_x = pois[i].x; max_y = pois[i].y; }
        found = true;
    }
    if (found) {
        snprintf(s_ineq_status, sizeof(s_ineq_status), "min %.4g@(%.3g,%.3g) max %.4g@(%.3g,%.3g)",
                 minimum, min_x, min_y, maximum, max_x, max_y);
    } else {
        snprintf(s_ineq_status, sizeof(s_ineq_status), "No bounded feasible vertices found");
    }
    ui_draw_current();
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
    int count = GRAPH_FUNC_COUNT;
    if (s_graphing_mode == 1) {
        count = GRAPH_PARAM_COUNT;
    } else if (s_graphing_mode == 2) {
        count = GRAPH_POLAR_COUNT;
    } else if (s_graphing_mode == 3) {
        count = GRAPH_SEQ_COUNT;
    }

    for (int fn = 0; fn < count; fn++) {
        bool enabled = false;
        switch (s_graphing_mode) {
        case 1:
            enabled = s_graph_param_enabled[fn] &&
                s_graph_param_x[fn][0] != '\0' && s_graph_param_y[fn][0] != '\0';
            break;
        case 2:
            enabled = s_graph_polar_enabled[fn] && s_graph_polar_exprs[fn][0] != '\0';
            break;
        case 3:
            enabled = s_graph_seq_enabled[fn] && s_graph_seq_exprs[fn][0] != '\0';
            break;
        default:
            enabled = s_graph_enabled[fn] && s_graph_exprs[fn][0] != '\0';
            break;
        }
        if (!enabled) {
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
        if (s_graphing_mode == 1 || s_graphing_mode == 2) {
            s_graph_trace_x = (s_graph_tmin + s_graph_tmax) / 2.0;
        } else if (s_graphing_mode == 3) {
            s_graph_trace_x = (s_graph_nmin + s_graph_nmax) / 2.0;
        } else {
            s_graph_trace_x = (s_graph_xmin + s_graph_xmax) / 2.0;
        }
        s_graph_trace_fn = first_enabled;
    } else {
        s_graph_trace_fn = next_enabled >= 0 ? next_enabled : first_enabled;
    }
    s_graph_trace = true;
    s_page = PAGE_GRAPH;
    s_current_app = APP_GRAPH;
    return true;
}

static bool graph_select_relative(int direction)
{
    int count = graph_series_count();
    if (count <= 0) return false;
    int start = s_graph_trace_fn;
    for (int offset = 1; offset <= count; offset++) {
        int candidate = (start + direction * offset) % count;
        if (candidate < 0) candidate += count;
        if (graph_series_is_active(candidate)) {
            s_graph_trace_fn = candidate;
            s_graph_trace = true;
            return true;
        }
    }
    return false;
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
        ui_rect(x + 23, y + 5, 24, 22, THEME_BG);
        ui_app_icon((app_id_t)i, x + 24, y + 6, 22, APPS[i].color);
        ui_text_centered_in_box(x + 2, y + 30, 66, HOME_LABELS[i], THEME_TEXT, 1);
    }

    ui_text_center(216, "enter - open", THEME_MUTED, 1);
    ui_present();
}

static const app_tool_t *app_tools_for(app_id_t app_id, int *count)
{
    switch (app_id) {
    case APP_STATS:
        *count = (int)(sizeof(STATS_HOME_TOOLS) / sizeof(STATS_HOME_TOOLS[0]));
        return STATS_HOME_TOOLS;
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

static void matrix_format_cell(double value, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) return;
    if (fabs(value) < 1e-12) value = 0.0;
    snprintf(out, out_size, "%.6g", value);
    if (strlen(out) > 9) snprintf(out, out_size, "%.3e", value);
}

static void ui_draw_list_slots(void)
{
    for (int index = 0; index < LIST_COUNT; index++) {
        int x = 10 + index * 50;
        bool selected = index == s_list_index;
        ui_rect(x, 31, 47, 17, selected ? THEME_ACCENT : THEME_SURFACE);
        char label[8];
        snprintf(label, sizeof(label), "L%d", index + 1);
        ui_text_centered_in_box(x, 36, 47, label,
                                selected ? THEME_HEADER_TEXT : THEME_MUTED, 1);
        if (s_list_counts[index] > 0) {
            ui_rect(x + 20, 46, 7, 2, selected ? THEME_HEADER_TEXT : THEME_ACCENT);
        }
    }
}

static void ui_draw_list_workspace(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_LISTS]);
    ui_draw_list_slots();

    char info[64];
    snprintf(info, sizeof(info), "L%d   %d value%s", s_list_index + 1,
             s_list_counts[s_list_index], s_list_counts[s_list_index] == 1 ? "" : "s");
    ui_text(12, 53, info, THEME_TEXT, 1);
    ui_text(220, 53, "LEFT/RIGHT", THEME_MUTED, 1);

    ui_text(12, 69, "INDEX", THEME_MUTED, 1);
    ui_text(67, 69, "VALUE", THEME_MUTED, 1);
    int shown = s_list_counts[s_list_index] < 6 ? s_list_counts[s_list_index] : 6;
    for (int row = 0; row < 6; row++) {
        int y = 79 + row * 18;
        ui_rect(10, y, 190, 16, row % 2 == 0 ? THEME_SURFACE : THEME_BG);
        if (row < shown) {
            char index[8];
            char value[24];
            snprintf(index, sizeof(index), "%d", row + 1);
            matrix_format_cell(s_lists[s_list_index][row], value, sizeof(value));
            ui_text(18, y + 5, index, THEME_MUTED, 1);
            ui_text(67, y + 5, value, THEME_TEXT, 1);
        } else if (row == 0) {
            ui_text(50, y + 5, "No values", THEME_MUTED, 1);
        }
    }
    if (s_list_counts[s_list_index] > shown) {
        snprintf(info, sizeof(info), "+%d more", s_list_counts[s_list_index] - shown);
        ui_text(135, 188, info, THEME_MUTED, 1);
    }

    int tool_count = (int)(sizeof(LIST_TOOLS) / sizeof(LIST_TOOLS[0]));
    if (s_app_selection >= tool_count) s_app_selection = tool_count - 1;
    for (int index = 0; index < tool_count; index++) {
        int y = 67 + index * 22;
        ui_rect(214, y, 96, 19, index == s_app_selection ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_text(220, y + 6, LIST_TOOLS[index].label,
                index == s_app_selection ? THEME_TEXT : THEME_MUTED, 1);
    }

    ui_rect(10, 197, 300, 18, THEME_SURFACE_2);
    ui_text(16, 203, s_calc_output, THEME_ACCENT, 1);
    ui_text(12, 224, LIST_TOOLS[s_app_selection].detail, THEME_MUTED, 1);
    ui_present();
}

static void matrix_keep_cursor_visible(int visible_rows, int visible_cols)
{
    if (s_matrix_rows <= 0 || s_matrix_cols <= 0) {
        s_matrix_cursor_row = 0;
        s_matrix_cursor_col = 0;
        s_matrix_scroll_row = 0;
        s_matrix_scroll_col = 0;
        return;
    }
    if (s_matrix_cursor_row < 0) s_matrix_cursor_row = 0;
    if (s_matrix_cursor_col < 0) s_matrix_cursor_col = 0;
    if (s_matrix_cursor_row >= s_matrix_rows) s_matrix_cursor_row = s_matrix_rows - 1;
    if (s_matrix_cursor_col >= s_matrix_cols) s_matrix_cursor_col = s_matrix_cols - 1;
    if (s_matrix_cursor_row < s_matrix_scroll_row) s_matrix_scroll_row = s_matrix_cursor_row;
    if (s_matrix_cursor_col < s_matrix_scroll_col) s_matrix_scroll_col = s_matrix_cursor_col;
    if (s_matrix_cursor_row >= s_matrix_scroll_row + visible_rows) {
        s_matrix_scroll_row = s_matrix_cursor_row - visible_rows + 1;
    }
    if (s_matrix_cursor_col >= s_matrix_scroll_col + visible_cols) {
        s_matrix_scroll_col = s_matrix_cursor_col - visible_cols + 1;
    }
}

static void ui_draw_matrix_slots(void)
{
    for (int i = 0; i < MATRIX_COUNT; i++) {
        int x = 10 + i * 30;
        bool selected = i == s_matrix_index;
        ui_rect(x, 31, 28, 17, selected ? THEME_ACCENT : THEME_SURFACE);
        char label[4];
        snprintf(label, sizeof(label), "%c", 'A' + i);
        ui_text_centered_in_box(x, 36, 28, label,
                                selected ? THEME_HEADER_TEXT : THEME_MUTED, 1);
        if (s_matrix_rows_by_index[i] > 0 && s_matrix_cols_by_index[i] > 0) {
            ui_rect(x + 11, 46, 6, 2, selected ? THEME_HEADER_TEXT : THEME_ACCENT);
        }
    }
}

static void ui_draw_matrix_grid(int top, int visible_rows, int visible_cols, bool highlight)
{
    const int left = 28;
    const int cell_w = 70;
    const int cell_h = 20;

    matrix_keep_cursor_visible(visible_rows, visible_cols);
    for (int col = 0; col < visible_cols; col++) {
        int matrix_col = s_matrix_scroll_col + col;
        if (matrix_col >= s_matrix_cols) break;
        char label[8];
        snprintf(label, sizeof(label), "C%d", matrix_col + 1);
        ui_text_centered_in_box(left + col * cell_w, top - 13, cell_w - 2,
                                label, THEME_MUTED, 1);
    }

    for (int row = 0; row < visible_rows; row++) {
        int matrix_row = s_matrix_scroll_row + row;
        if (matrix_row >= s_matrix_rows) break;
        char row_label[8];
        snprintf(row_label, sizeof(row_label), "%d", matrix_row + 1);
        ui_text_centered_in_box(1, top + row * cell_h + 6, 24, row_label, THEME_MUTED, 1);
        for (int col = 0; col < visible_cols; col++) {
            int matrix_col = s_matrix_scroll_col + col;
            if (matrix_col >= s_matrix_cols) break;
            int x = left + col * cell_w;
            int y = top + row * cell_h;
            bool selected = highlight && matrix_row == s_matrix_cursor_row &&
                            matrix_col == s_matrix_cursor_col;
            ui_rect(x, y, cell_w - 2, cell_h - 2,
                    selected ? THEME_ACCENT : THEME_BORDER);
            ui_rect(x + 1, y + 1, cell_w - 4, cell_h - 4,
                    selected ? THEME_ACCENT_2 : THEME_SURFACE);
            char value[16];
            matrix_format_cell(s_matrix_a[matrix_row][matrix_col], value, sizeof(value));
            ui_text_centered_in_box(x + 2, y + 6, cell_w - 6, value, THEME_TEXT, 1);
        }
    }
}

static void ui_draw_matrix_workspace(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_MATRICES]);
    ui_draw_matrix_slots();

    char info[48];
    if (s_matrix_rows > 0 && s_matrix_cols > 0) {
        snprintf(info, sizeof(info), "Matrix %c   %d x %d   %d cells",
                 'A' + s_matrix_index, s_matrix_rows, s_matrix_cols,
                 s_matrix_rows * s_matrix_cols);
    } else {
        snprintf(info, sizeof(info), "Matrix %c   empty", 'A' + s_matrix_index);
    }
    ui_text(12, 53, info, THEME_TEXT, 1);
    ui_text(220, 53, "LEFT/RIGHT", THEME_MUTED, 1);

    const int preview_rows = 6;
    const int preview_cols = 3;
    for (int row = 0; row < preview_rows && row < s_matrix_rows; row++) {
        for (int col = 0; col < preview_cols && col < s_matrix_cols; col++) {
            int x = 12 + col * 65;
            int y = 68 + row * 20;
            ui_rect(x, y, 61, 18, THEME_BORDER);
            ui_rect(x + 1, y + 1, 59, 16, THEME_SURFACE);
            char value[16];
            matrix_format_cell(s_matrix_a[row][col], value, sizeof(value));
            ui_text_centered_in_box(x + 1, y + 6, 59, value, THEME_TEXT, 1);
        }
    }
    if (s_matrix_rows == 0 || s_matrix_cols == 0) {
        ui_text(43, 111, "No cells", THEME_MUTED, 2);
    } else if (s_matrix_rows > preview_rows || s_matrix_cols > preview_cols) {
        ui_text(137, 190, "...", THEME_MUTED, 1);
    }

    int tool_count = (int)(sizeof(MATRIX_TOOLS) / sizeof(MATRIX_TOOLS[0]));
    if (s_app_selection >= tool_count) s_app_selection = tool_count - 1;
    const int visible_count = 7;
    int first = s_app_selection >= visible_count ? s_app_selection - visible_count + 1 : 0;
    if (first + visible_count > tool_count) first = tool_count - visible_count;
    for (int visible = 0; visible < visible_count; visible++) {
        int index = first + visible;
        int y = 67 + visible * 17;
        ui_rect(214, y, 96, 15, index == s_app_selection ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_text(220, y + 4, MATRIX_TOOLS[index].label,
                index == s_app_selection ? THEME_TEXT : THEME_MUTED, 1);
    }

    ui_rect(10, 197, 300, 18, THEME_SURFACE_2);
    ui_text(16, 203, s_calc_output, THEME_ACCENT, 1);
    ui_text(12, 224, MATRIX_TOOLS[s_app_selection].detail, THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_matrix_editor(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_MATRICES]);
    char title[48];
    snprintf(title, sizeof(title), "Edit %c   %d x %d   R%d C%d",
             'A' + s_matrix_index, s_matrix_rows, s_matrix_cols,
             s_matrix_cursor_row + 1, s_matrix_cursor_col + 1);
    ui_text(10, 34, title, THEME_TEXT, 1);
    ui_draw_matrix_grid(62, 6, 4, true);

    ui_rect(8, 185, 304, 28, THEME_SURFACE_2);
    char current[24];
    if (s_matrix_entry_active) snprintf(current, sizeof(current), "%s", s_matrix_entry);
    else matrix_format_cell(s_matrix_a[s_matrix_cursor_row][s_matrix_cursor_col], current, sizeof(current));
    ui_text(16, 193, "VALUE", THEME_MUTED, 1);
    ui_text(70, 192, current, THEME_TEXT, 2);
    if (s_matrix_entry_active && s_cursor_blink_visible) {
        ui_rect(70 + ui_text_width(current, 20, 2), 191, 2, 15, THEME_ACCENT);
    }
    ui_text(10, 224, s_matrix_status[0] ? s_matrix_status : "Enter next   2nd+Enter done",
            s_matrix_status[0] ? THEME_BATTERY_YELLOW : THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_matrix_viewer(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_MATRICES]);
    char title[48];
    snprintf(title, sizeof(title), "Matrix %c   %d x %d   R%d C%d",
             'A' + s_matrix_index, s_matrix_rows, s_matrix_cols,
             s_matrix_cursor_row + 1, s_matrix_cursor_col + 1);
    ui_text(10, 34, title, THEME_TEXT, 1);
    ui_draw_matrix_grid(62, 7, 4, true);
    char value[48];
    matrix_format_cell(s_matrix_a[s_matrix_cursor_row][s_matrix_cursor_col], value, sizeof(value));
    char status[80];
    snprintf(status, sizeof(status), "[%d,%d] = %s", s_matrix_cursor_row + 1,
             s_matrix_cursor_col + 1, value);
    ui_text(10, 207, status, THEME_ACCENT, 1);
    ui_text(10, 224, "Enter edit   Back workspace", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_matrix_size(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_MATRICES]);
    char title[48];
    snprintf(title, sizeof(title), "Resize Matrix %c", 'A' + s_matrix_index);
    ui_text(18, 38, title, THEME_TEXT, 2);

    const char *labels[2] = {"ROWS", "COLUMNS"};
    int values[2] = {s_matrix_size_rows, s_matrix_size_cols};
    for (int i = 0; i < 2; i++) {
        int y = 83 + i * 47;
        ui_rect(28, y, 264, 35, i == s_matrix_size_selection ? THEME_ACCENT : THEME_BORDER);
        ui_rect(30, y + 2, 260, 31, i == s_matrix_size_selection ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_text(44, y + 12, labels[i], THEME_MUTED, 1);
        char value[8];
        snprintf(value, sizeof(value), "%d", values[i]);
        ui_text(224, y + 8, value, THEME_TEXT, 2);
    }
    char summary[64];
    snprintf(summary, sizeof(summary), "%d x %d = %d cells", s_matrix_size_rows,
             s_matrix_size_cols, s_matrix_size_rows * s_matrix_size_cols);
    ui_text_center(183, summary, THEME_ACCENT, 1);
    ui_text_center(222, "1..99   Enter apply", THEME_MUTED, 1);
    ui_present();
}

static void finance_format_value(double value, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) return;
    if (fabs(value) < 1e-12) value = 0.0;
    snprintf(out, out_size, "%.8g", value);
    if (strlen(out) > 12) snprintf(out, out_size, "%.5e", value);
}

static void ui_draw_finance_workspace(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_FINANCE]);

    const char *labels[5] = {"N", "I%", "PV", "PMT", "FV"};
    const double values[5] = {s_fin_n, s_fin_i, s_fin_pv, s_fin_pmt, s_fin_fv};
    for (int i = 0; i < 5; i++) {
        int x = 10 + i * 60;
        ui_rect(x, 33, 56, 32, THEME_SURFACE);
        ui_text(x + 5, 38, labels[i], THEME_MUTED, 1);
        char value[16];
        finance_format_value(values[i], value, sizeof(value));
        ui_text_centered_in_box(x + 2, 52, 52, value, THEME_TEXT, 1);
    }

    char settings[64];
    snprintf(settings, sizeof(settings), "%s payments   P/Y %.0f   C/Y %.0f   cash L%d",
             s_fin_begin ? "BEGIN" : "END", s_fin_py, s_fin_cy, s_list_index + 1);
    ui_text(12, 72, settings, THEME_ACCENT, 1);

    ui_text(12, 91, "CASH-FLOW PROFILE", THEME_MUTED, 1);
    ui_rect(10, 103, 178, 79, THEME_SURFACE);
    const int flow_count = s_list_counts[s_list_index];
    if (flow_count == 0) {
        ui_text_centered_in_box(10, 137, 178, "No cash flows", THEME_MUTED, 1);
    } else {
        int shown = flow_count < 8 ? flow_count : 8;
        double max_abs = 1.0;
        for (int i = 0; i < shown; i++) {
            double amount = fabs(s_lists[s_list_index][i]);
            if (amount > max_abs) max_abs = amount;
        }
        const int axis_y = 143;
        ui_line(18, axis_y, 180, axis_y, THEME_BORDER);
        for (int i = 0; i < shown; i++) {
            int x = 23 + i * 20;
            int height = (int)(fabs(s_lists[s_list_index][i]) * 31.0 / max_abs);
            if (height < 2) height = 2;
            if (s_lists[s_list_index][i] >= 0.0) {
                ui_rect(x, axis_y - height, 11, height, THEME_BATTERY_GREEN);
            } else {
                ui_rect(x, axis_y + 1, 11, height, THEME_BATTERY_RED);
            }
        }
        char count[24];
        snprintf(count, sizeof(count), "%d periods%s", flow_count, flow_count > shown ? "  ..." : "");
        ui_text(18, 169, count, THEME_MUTED, 1);
    }

    int tool_count = (int)(sizeof(FINANCE_TOOLS) / sizeof(FINANCE_TOOLS[0]));
    if (s_app_selection >= tool_count) s_app_selection = tool_count - 1;
    const int visible_count = 6;
    int first = s_app_selection >= visible_count ? s_app_selection - visible_count + 1 : 0;
    if (first + visible_count > tool_count) first = tool_count - visible_count;
    for (int visible = 0; visible < visible_count; visible++) {
        int index = first + visible;
        int y = 91 + visible * 16;
        ui_rect(196, y, 114, 15, index == s_app_selection ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_text(202, y + 4, FINANCE_TOOLS[index].label,
                index == s_app_selection ? THEME_TEXT : THEME_MUTED, 1);
    }

    ui_rect(10, 192, 300, 22, THEME_SURFACE_2);
    ui_text(16, 200, s_calc_output, THEME_ACCENT, 1);
    ui_text(12, 224, FINANCE_TOOLS[s_app_selection].detail, THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_finance_tvm(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_FINANCE]);
    ui_text(12, 35, "TVM WORKSHEET", THEME_TEXT, 2);
    ui_text(218, 39, s_fin_begin ? "BEGIN" : "END", THEME_ACCENT, 1);

    const char *labels[7] = {"N", "I%", "PV", "PMT", "FV", "P/Y", "C/Y"};
    const double values[7] = {s_fin_n, s_fin_i, s_fin_pv, s_fin_pmt,
                              s_fin_fv, s_fin_py, s_fin_cy};
    for (int i = 0; i < 7; i++) {
        int column = i < 4 ? 0 : 1;
        int row = i < 4 ? i : i - 4;
        int x = 10 + column * 155;
        int y = 59 + row * 32;
        bool selected = i == s_fin_selection;
        ui_rect(x, y, 145, 27, selected ? THEME_ACCENT : THEME_BORDER);
        ui_rect(x + 1, y + 1, 143, 25, selected ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_text(x + 8, y + 10, labels[i], THEME_MUTED, 1);
        char value[24];
        if (selected && s_fin_entry_active) snprintf(value, sizeof(value), "%s", s_fin_entry);
        else finance_format_value(values[i], value, sizeof(value));
        ui_text(x + 45, y + 9, value, THEME_TEXT, 1);
        if (selected && s_fin_entry_active && s_cursor_blink_visible) {
            ui_rect(x + 45 + ui_text_width(value, 20, 1), y + 7, 2, 13, THEME_ACCENT);
        }
    }

    ui_rect(165, 155, 145, 27, THEME_SURFACE_2);
    ui_text(173, 165, "TIMING", THEME_MUTED, 1);
    ui_text(242, 165, s_fin_begin ? "BEGIN" : "END", THEME_TEXT, 1);
    ui_text(10, 202, s_fin_status[0] ? s_fin_status : "Edit a field or solve the selected unknown",
            s_fin_status[0] ? THEME_BATTERY_YELLOW : THEME_MUTED, 1);
    ui_text(10, 224, "Enter save/next   2nd+Enter solve", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_finance_cashflow(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_FINANCE]);
    char title[48];
    snprintf(title, sizeof(title), "CASH FLOWS  L%d   %d entries",
             s_list_index + 1, s_list_counts[s_list_index]);
    ui_text(10, 34, title, THEME_TEXT, 1);

    double npv = finance_npv_from_list(s_list_index, finance_periodic_rate());
    double irr = 0.0;
    bool has_irr = s_list_counts[s_list_index] >= 2 && finance_irr_from_list(s_list_index, &irr);
    char summary[72];
    if (has_irr) {
        snprintf(summary, sizeof(summary), "NPV %.7g   IRR %.6g%%", npv, irr * 100.0);
    } else {
        snprintf(summary, sizeof(summary), "NPV %.7g   IRR --", npv);
    }
    ui_text(10, 48, summary, THEME_ACCENT, 1);
    ui_text(12, 66, "PERIOD", THEME_MUTED, 1);
    ui_text(74, 66, "CASH FLOW", THEME_MUTED, 1);
    ui_text(210, 66, "PRESENT VALUE", THEME_MUTED, 1);

    const int visible_rows = 6;
    if (s_fin_cash_cursor < s_fin_cash_scroll) s_fin_cash_scroll = s_fin_cash_cursor;
    if (s_fin_cash_cursor >= s_fin_cash_scroll + visible_rows) {
        s_fin_cash_scroll = s_fin_cash_cursor - visible_rows + 1;
    }
    for (int visible = 0; visible < visible_rows; visible++) {
        int index = s_fin_cash_scroll + visible;
        if (index > s_list_counts[s_list_index]) break;
        int y = 77 + visible * 18;
        bool selected = index == s_fin_cash_cursor;
        ui_rect(8, y, 304, 17, selected ? THEME_ACCENT_2 : THEME_SURFACE);
        char period[12];
        snprintf(period, sizeof(period), "CF%d", index);
        ui_text(14, y + 5, period, selected ? THEME_TEXT : THEME_MUTED, 1);
        if (index == s_list_counts[s_list_index]) {
            ui_text(74, y + 5, "+ new flow", THEME_MUTED, 1);
            continue;
        }
        char amount[24];
        if (selected && s_fin_entry_active) snprintf(amount, sizeof(amount), "%s", s_fin_entry);
        else finance_format_value(s_lists[s_list_index][index], amount, sizeof(amount));
        ui_text(74, y + 5, amount, THEME_TEXT, 1);
        double discounted = s_lists[s_list_index][index] /
                            pow(1.0 + finance_periodic_rate(), (double)index);
        char present[24];
        finance_format_value(discounted, present, sizeof(present));
        ui_text(210, y + 5, present, THEME_MUTED, 1);
        if (selected && s_fin_entry_active && s_cursor_blink_visible) {
            ui_rect(74 + ui_text_width(amount, 20, 1), y + 3, 2, 13, THEME_ACCENT);
        }
    }

    ui_rect(8, 190, 304, 24, THEME_SURFACE_2);
    ui_text(14, 198, s_fin_status[0] ? s_fin_status : "Arrows move   Left/right change list",
            s_fin_status[0] ? THEME_BATTERY_YELLOW : THEME_MUTED, 1);
    ui_text(10, 224, "Enter save/next   Del remove   2nd+Enter done", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_finance_result(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_FINANCE]);
    ui_text_center(45, s_fin_result_title, THEME_MUTED, 1);
    char value[32];
    finance_format_value(s_fin_result_value, value, sizeof(value));
    if (strcmp(s_fin_result_title, "INTERNAL RATE") == 0 && strlen(value) + 1 < sizeof(value)) {
        strcat(value, "%");
    }
    ui_text_center(75, value, THEME_TEXT, 3);
    ui_line(42, 106, 278, 106, THEME_ACCENT);
    ui_text_center(124, s_fin_result_detail, THEME_MUTED, 1);
    char settings[64];
    if (strcmp(s_fin_result_title, "INTERNAL RATE") == 0) {
        snprintf(settings, sizeof(settings), "L%d   %d cash-flow entries",
                 s_list_index + 1, s_list_counts[s_list_index]);
    } else {
        snprintf(settings, sizeof(settings), "L%d  %d flows   discount %.6g%%",
                 s_list_index + 1, s_list_counts[s_list_index],
                 finance_periodic_rate() * 100.0);
    }
    ui_text_center(146, settings, THEME_TEXT, 1);
    ui_text_center(224, "Enter or Back - dashboard", THEME_MUTED, 1);
    ui_present();
}

static const app_tool_t *solver_workflow_actions(solver_workflow_t workflow,
                                                  int *count, const char **title)
{
    switch (workflow) {
    case SOLVER_WORKFLOW_EQUATION:
        *count = (int)(sizeof(SOLVER_EQUATION_ACTIONS) / sizeof(SOLVER_EQUATION_ACTIONS[0]));
        *title = "EQUATION SOLVER";
        return SOLVER_EQUATION_ACTIONS;
    case SOLVER_WORKFLOW_SYSTEM:
        *count = (int)(sizeof(SOLVER_SYSTEM_ACTIONS) / sizeof(SOLVER_SYSTEM_ACTIONS[0]));
        *title = "SYSTEM SOLVER";
        return SOLVER_SYSTEM_ACTIONS;
    case SOLVER_WORKFLOW_POLYNOMIAL:
        *count = (int)(sizeof(SOLVER_POLYNOMIAL_ACTIONS) / sizeof(SOLVER_POLYNOMIAL_ACTIONS[0]));
        *title = "POLYNOMIAL SOLVER";
        return SOLVER_POLYNOMIAL_ACTIONS;
    case SOLVER_WORKFLOW_NUMERIC:
    default:
        *count = (int)(sizeof(SOLVER_NUMERIC_ACTIONS) / sizeof(SOLVER_NUMERIC_ACTIONS[0]));
        *title = "NUMERIC SOLVER";
        return SOLVER_NUMERIC_ACTIONS;
    }
}

static void ui_draw_solver_workspace(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_SOLVER]);
    ui_text(16, 34, "SOLVER", THEME_TEXT, 1);
    ui_text(70, 34, "choose a method", THEME_MUTED, 1);

    for (int i = 0; i < (int)(sizeof(SOLVER_TOOLS) / sizeof(SOLVER_TOOLS[0])); i++) {
        int y = 51 + i * 31;
        bool selected = i == s_app_selection;
        ui_rect(14, y, 292, 27, selected ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_rect(14, y, 4, 27, selected ? THEME_ACCENT : THEME_BORDER);
        char number[4];
        snprintf(number, sizeof(number), "%d", i + 1);
        ui_text(24, y + 8, number, selected ? THEME_ACCENT : THEME_MUTED, 1);
        ui_text(43, y + 5, SOLVER_TOOLS[i].label, THEME_TEXT, 1);
        ui_text(164, y + 15, SOLVER_TOOLS[i].detail, THEME_MUTED, 1);
    }
    ui_text(16, 220, "1-5 open   arrows select   Back home", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_solver_workflow(void)
{
    int count = 0;
    const char *title = NULL;
    const app_tool_t *actions = solver_workflow_actions(s_solver_workflow, &count, &title);
    if (s_solver_workflow_selection >= count) s_solver_workflow_selection = count - 1;

    ui_clear(THEME_BG);
    ui_header(&APPS[APP_SOLVER]);
    ui_text(14, 34, title, THEME_TEXT, 1);

    char context[112];
    if (s_solver_workflow == SOLVER_WORKFLOW_SYSTEM) {
        snprintf(context, sizeof(context), "Matrix %c  %dx%d  %s",
                 'A' + s_matrix_index, s_matrix_rows, s_matrix_cols,
                 s_matrix_cols == s_matrix_rows + 1 ? "augmented" : "set n x (n+1)");
    } else if (s_solver_workflow == SOLVER_WORKFLOW_POLYNOMIAL) {
        snprintf(context, sizeof(context), "P(x): %.45s", s_solver_e1);
    } else {
        snprintf(context, sizeof(context), "%.35s = %.35s", s_solver_e1, s_solver_e2);
    }
    ui_rect(14, 48, 292, 22, THEME_SURFACE_2);
    ui_text(21, 55, context, THEME_MUTED, 1);

    const int visible_count = 6;
    int first = s_solver_workflow_selection >= visible_count ?
        s_solver_workflow_selection - visible_count + 1 : 0;
    if (first + visible_count > count) first = count > visible_count ? count - visible_count : 0;
    for (int row = 0; row < visible_count && first + row < count; row++) {
        int i = first + row;
        int y = 76 + row * 22;
        bool selected = i == s_solver_workflow_selection;
        ui_rect(14, y, 292, 20, selected ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_rect(14, y, 4, 20, selected ? THEME_ACCENT : THEME_BORDER);
        ui_text(24, y + 5, actions[i].label, THEME_TEXT, 1);
        ui_text(161, y + 5, actions[i].detail, THEME_MUTED, 1);
    }
    if (count > visible_count) {
        char position[12];
        snprintf(position, sizeof(position), "%d/%d", s_solver_workflow_selection + 1, count);
        ui_text(275, 34, position, THEME_MUTED, 1);
    }

    char status[64];
    if (s_solver_symbolic_pending || s_solver_solve_pending) {
        snprintf(status, sizeof(status), "Solver working...");
    } else if (s_solver_workflow == SOLVER_WORKFLOW_NUMERIC) {
        snprintf(status, sizeof(status), "guess %.4g  [%.4g,%.4g]  %d digits",
                 s_solver_guess, s_solver_lower, s_solver_upper, s_solver_precision);
    } else {
        snprintf(status, sizeof(status), "Exact CAS enabled  angle mode %s", s_angle_mode == 0 ? "degrees" : "radians");
    }
    ui_text(16, 211, status, THEME_MUTED, 1);
    ui_text(16, 224, "arrows select   Enter run   Back solver", THEME_MUTED, 1);
    ui_present();
}

static int solver_symbolic_wrap(int *offsets, int max_lines)
{
    int count = 0;
    int length = (int)strlen(s_solver_symbolic_result);
    int offset = 0;
    while (offset < length && count < max_lines) {
        while (offset < length && (s_solver_symbolic_result[offset] == '\n' ||
                                   s_solver_symbolic_result[offset] == '\r' ||
                                   s_solver_symbolic_result[offset] == ' ')) offset++;
        if (offset >= length) break;
        offsets[count++] = offset;
        int end = offset;
        int last_space = -1;
        while (end < length && end - offset < 45 && s_solver_symbolic_result[end] != '\n') {
            if (s_solver_symbolic_result[end] == ' ') last_space = end;
            end++;
        }
        if (end < length && s_solver_symbolic_result[end] != '\n' && last_space > offset) end = last_space;
        offset = end;
    }
    return count;
}

static void ui_draw_solver_symbolic_result(void)
{
    int offsets[64];
    int line_count = solver_symbolic_wrap(offsets, 64);
    if (s_solver_symbolic_scroll < 0) s_solver_symbolic_scroll = 0;
    if (s_solver_symbolic_scroll > line_count - 1) s_solver_symbolic_scroll = line_count > 0 ? line_count - 1 : 0;

    ui_clear(THEME_BG);
    ui_header(&APPS[APP_SOLVER]);
    ui_text(14, 34, s_solver_symbolic_title, THEME_TEXT, 1);
    ui_rect(14, 49, 292, 20, THEME_SURFACE_2);
    ui_text(21, 55, "Symbolic CAS result", THEME_ACCENT, 1);

    for (int row = 0; row < 8 && s_solver_symbolic_scroll + row < line_count; row++) {
        int index = s_solver_symbolic_scroll + row;
        int start = offsets[index];
        int end = index + 1 < line_count ? offsets[index + 1] : (int)strlen(s_solver_symbolic_result);
        while (end > start && (s_solver_symbolic_result[end - 1] == ' ' ||
                               s_solver_symbolic_result[end - 1] == '\n' ||
                               s_solver_symbolic_result[end - 1] == '\r')) end--;
        int copy = end - start;
        if (copy > 45) copy = 45;
        char line[48];
        memcpy(line, s_solver_symbolic_result + start, (size_t)copy);
        line[copy] = '\0';
        ui_rect(14, 75 + row * 17, 292, 15, row == 0 ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_text(21, 79 + row * 17, line, row == 0 ? THEME_TEXT : THEME_MUTED, 1);
    }

    char position[20];
    snprintf(position, sizeof(position), "%d/%d", line_count == 0 ? 0 : s_solver_symbolic_scroll + 1, line_count);
    ui_text(270, 34, position, THEME_MUTED, 1);
    ui_text(12, 220, "up/down scroll  Enter copy to Calc  Back workflow", THEME_MUTED, 1);
    ui_present();
}

static int solver_system_pivot_row(int variable)
{
    for (int row = 0; row < s_matrix_rows; row++) {
        int pivot = -1;
        for (int col = 0; col < s_solver_system_variables; col++) {
            if (fabs(s_matrix_a[row][col]) > 1e-9) { pivot = col; break; }
        }
        if (pivot == variable) return row;
    }
    return -1;
}

static void solver_system_variable_text(int variable, char *out, size_t out_size)
{
    int row = solver_system_pivot_row(variable);
    if (row < 0) {
        snprintf(out, out_size, "x%d is free", variable + 1);
        return;
    }
    int rhs = s_solver_system_variables;
    int written = snprintf(out, out_size, "x%d = %.8g", variable + 1, s_matrix_a[row][rhs]);
    size_t used = written > 0 ? (size_t)written : 0;
    for (int col = 0; col < s_solver_system_variables && used + 12 < out_size; col++) {
        if (col == variable || fabs(s_matrix_a[row][col]) <= 1e-9) continue;
        written = snprintf(out + used, out_size - used, "%+.5g*x%d", -s_matrix_a[row][col], col + 1);
        if (written < 0) break;
        used += (size_t)written;
    }
}

static void ui_draw_solver_system_result(void)
{
    static const char *status_names[] = {"NO RESULT", "UNIQUE", "DEPENDENT", "INCONSISTENT"};
    uint32_t status_color = s_solver_system_status == SOLVER_SYSTEM_UNIQUE ? THEME_BATTERY_GREEN :
        (s_solver_system_status == SOLVER_SYSTEM_DEPENDENT ? THEME_BATTERY_YELLOW : THEME_BATTERY_RED);
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_SOLVER]);
    ui_text(14, 34, "LINEAR SYSTEM RESULT", THEME_TEXT, 1);
    ui_rect(14, 49, 292, 25, THEME_SURFACE_2);
    ui_text(21, 57, status_names[s_solver_system_status], status_color, 1);
    char summary[64];
    snprintf(summary, sizeof(summary), "Matrix %c  rank %d  variables %d",
             'A' + s_solver_system_matrix, s_solver_system_rank, s_solver_system_variables);
    ui_text(112, 57, summary, THEME_MUTED, 1);

    if (s_solver_system_status == SOLVER_SYSTEM_INCONSISTENT) {
        ui_text_center(112, "No solution: contradictory RREF row", THEME_TEXT, 1);
        ui_text_center(132, "The original equations are inconsistent.", THEME_MUTED, 1);
    } else if (s_solver_system_variables > 0) {
        const int visible = 7;
        int first = s_solver_system_selected >= visible ? s_solver_system_selected - visible + 1 : 0;
        if (first + visible > s_solver_system_variables) first = s_solver_system_variables > visible ? s_solver_system_variables - visible : 0;
        for (int i = 0; i < visible && first + i < s_solver_system_variables; i++) {
            int variable = first + i;
            char line[80];
            solver_system_variable_text(variable, line, sizeof(line));
            int y = 82 + i * 18;
            bool selected = variable == s_solver_system_selected;
            ui_rect(14, y, 292, 16, selected ? THEME_ACCENT_2 : THEME_SURFACE);
            ui_text(22, y + 4, line, selected ? THEME_TEXT : THEME_MUTED, 1);
        }
    }
    ui_text(12, 214, s_solver_system_status == SOLVER_SYSTEM_DEPENDENT ?
            "Free variables produce a parametric solution." : "RREF solution; substitute to validate if needed.", THEME_MUTED, 1);
    ui_text(12, 226, "up/down browse  Enter copy value  Back system", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_solver_saved(void)
{
    static const char *type_names[] = {"EQ", "SYS", "POLY", "NUM"};
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_SOLVER]);
    ui_text(14, 34, "SAVED PROBLEMS", THEME_TEXT, 1);
    ui_text(116, 34, "stored on chip", THEME_MUTED, 1);
    for (int i = 0; i < SOLVER_SAVED_MAX; i++) {
        int y = 53 + i * 30;
        bool selected = i == s_solver_saved_selection;
        ui_rect(14, y, 292, 26, selected ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_rect(14, y, 4, 26, selected ? THEME_ACCENT : THEME_BORDER);
        char label[112];
        if (s_solver_saved_e1[i][0] == '\0') snprintf(label, sizeof(label), "%d  Empty slot", i + 1);
        else if (s_solver_saved_e2[i][0] == '\0') {
            snprintf(label, sizeof(label), "%d  %s  %.43s", i + 1,
                     type_names[s_solver_saved_type[i]], s_solver_saved_e1[i]);
        } else {
            snprintf(label, sizeof(label), "%d  %s  %.29s = %.29s", i + 1,
                     type_names[s_solver_saved_type[i]], s_solver_saved_e1[i], s_solver_saved_e2[i]);
        }
        ui_text(24, y + 8, label, s_solver_saved_e1[i][0] ? THEME_TEXT : THEME_MUTED, 1);
    }
    ui_text(12, 214, "Enter loads the selected equation into Solver.", THEME_MUTED, 1);
    ui_text(12, 226, "up/down select   Del delete   Back solver", THEME_MUTED, 1);
    ui_present();
}

static const int *stats_category_tools(stats_category_t category, int *count, const char **title)
{
    switch (category) {
    case STATS_CATEGORY_SUMMARY:
        *count = (int)(sizeof(STATS_SUMMARY_TOOLS) / sizeof(STATS_SUMMARY_TOOLS[0]));
        *title = "SUMMARY STATISTICS";
        return STATS_SUMMARY_TOOLS;
    case STATS_CATEGORY_REGRESSION:
        *count = (int)(sizeof(STATS_REGRESSION_TOOLS) / sizeof(STATS_REGRESSION_TOOLS[0]));
        *title = "REGRESSION";
        return STATS_REGRESSION_TOOLS;
    case STATS_CATEGORY_DISTRIBUTIONS:
        *count = (int)(sizeof(STATS_DISTRIBUTION_TOOLS) / sizeof(STATS_DISTRIBUTION_TOOLS[0]));
        *title = "DISTRIBUTIONS";
        return STATS_DISTRIBUTION_TOOLS;
    case STATS_CATEGORY_INTERVALS:
        *count = (int)(sizeof(STATS_INTERVAL_TOOLS) / sizeof(STATS_INTERVAL_TOOLS[0]));
        *title = "CONFIDENCE INTERVALS";
        return STATS_INTERVAL_TOOLS;
    case STATS_CATEGORY_TESTS:
    default:
        *count = (int)(sizeof(STATS_TEST_TOOLS) / sizeof(STATS_TEST_TOOLS[0]));
        *title = "HYPOTHESIS TESTS";
        return STATS_TEST_TOOLS;
    }
}

static void ui_draw_stats_workspace(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_STATS]);
    ui_text(16, 34, "STATS", THEME_TEXT, 1);
    ui_text(62, 34, "choose a workflow", THEME_MUTED, 1);

    for (int i = 0; i < (int)(sizeof(STATS_HOME_TOOLS) / sizeof(STATS_HOME_TOOLS[0])); i++) {
        int y = 48 + i * 22;
        bool selected = i == s_stats_home_selection;
        ui_rect(14, y, 292, 20, selected ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_rect(14, y, 4, 20, selected ? THEME_ACCENT : THEME_BORDER);
        char number[4];
        snprintf(number, sizeof(number), "%d", i + 1);
        ui_text(24, y + 5, number, selected ? THEME_ACCENT : THEME_MUTED, 1);
        ui_text(42, y + 5, STATS_HOME_TOOLS[i].label, THEME_TEXT, 1);
    }

    ui_text(16, 220, "1-7 open   arrows select   Back home", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_stats_category(void)
{
    int count = 0;
    const char *title = NULL;
    const int *tools = stats_category_tools(s_stats_category, &count, &title);
    if (s_stats_category_selection >= count) s_stats_category_selection = count - 1;

    ui_clear(THEME_BG);
    ui_header(&APPS[APP_STATS]);
    ui_text(16, 34, title, THEME_TEXT, 1);

    const int visible_count = 7;
    int first = s_stats_category_selection >= visible_count ?
        s_stats_category_selection - visible_count + 1 : 0;
    if (first + visible_count > count) first = count > visible_count ? count - visible_count : 0;
    for (int row = 0; row < visible_count && first + row < count; row++) {
        int index = first + row;
        int tool = tools[index];
        int y = 50 + row * 22;
        bool selected = index == s_stats_category_selection;
        ui_rect(14, y, 292, 20, selected ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_rect(14, y, 4, 20, selected ? THEME_ACCENT : THEME_BORDER);
        ui_text(24, y + 5, STATS_TOOLS[tool].label, THEME_TEXT, 1);
        ui_text(154, y + 5, STATS_TOOLS[tool].detail, THEME_MUTED, 1);
    }
    if (count > visible_count) {
        char position[16];
        snprintf(position, sizeof(position), "%d/%d", s_stats_category_selection + 1, count);
        ui_text(272, 34, position, THEME_MUTED, 1);
    }
    ui_scrollbar(312, 50, 152, count, visible_count, first);
    ui_text(16, 220, "arrows select   Enter open   Back stats", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_stats_one_var(void)
{
    static const char *labels[] = {
        "Data list", "Frequency list", "First quartile", "Number format"
    };
    char values[4][24];
    snprintf(values[0], sizeof(values[0]), "L%d", s_stats_one_var_data_list + 1);
    if (s_stats_one_var_frequency_list < 0) snprintf(values[1], sizeof(values[1]), "None");
    else snprintf(values[1], sizeof(values[1]), "L%d", s_stats_one_var_frequency_list + 1);
    snprintf(values[2], sizeof(values[2]), "%s",
             s_stats_one_var_quartile_method == 0 ? "Median method" : "Inclusive method");
    snprintf(values[3], sizeof(values[3]), "%s",
             s_stats_one_var_number_format == 0 ? "Exact" : "Decimal");

    ui_clear(THEME_BG);
    ui_header(&APPS[APP_STATS]);
    ui_text(16, 34, "ONE-VARIABLE STATS", THEME_TEXT, 1);
    ui_text(16, 47, "Configure the sample", THEME_MUTED, 1);

    for (int i = 0; i < 4; i++) {
        int y = 64 + i * 28;
        bool selected = i == s_stats_one_var_selection;
        ui_rect(14, y, 292, 24, selected ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_rect(14, y, 4, 24, selected ? THEME_ACCENT : THEME_BORDER);
        ui_text(24, y + 7, labels[i], selected ? THEME_TEXT : THEME_MUTED, 1);
        int value_x = 298 - ui_text_width(values[i], 24, 1);
        ui_text(value_x, y + 7, values[i], selected ? THEME_ACCENT : THEME_TEXT, 1);
    }

    bool calculate_selected = s_stats_one_var_selection == 4;
    ui_rect(86, 180, 148, 25, calculate_selected ? THEME_ACCENT : THEME_SURFACE_2);
    ui_text_centered_in_box(86, 188, 148, "Calculate", calculate_selected ? THEME_BG : THEME_TEXT, 1);
    if (s_stats_one_var_error[0] != '\0') {
        ui_text(16, 210, s_stats_one_var_error, THEME_BATTERY_RED, 1);
    }
    ui_text(16, 224, "left/right change   Enter next/calculate", THEME_MUTED, 1);
    ui_present();
}

static int conic_field_count(void)
{
    if (s_conic_type == OPENCALC_CONIC_CIRCLE) return 3;
    if (s_conic_type == OPENCALC_CONIC_PARABOLA) return 4;
    if (s_conic_type == OPENCALC_CONIC_ELLIPSE || s_conic_type == OPENCALC_CONIC_HYPERBOLA) return 5;
    return 6;
}

static const char *conic_field_label(int field)
{
    static const char *const general[] = {"A x^2", "B xy", "C y^2", "D x", "E y", "F"};
    static const char *const canonical[] = {"center h", "center k", "axis a / focal p", "axis b", "rotation deg"};
    if (s_conic_type == OPENCALC_CONIC_GENERAL) return general[field];
    if (s_conic_type == OPENCALC_CONIC_CIRCLE && field == 2) return "radius";
    if (s_conic_type == OPENCALC_CONIC_PARABOLA && field == 2) return "focal p";
    return canonical[field];
}

static double *conic_field_value(int field)
{
    if (s_conic_type == OPENCALC_CONIC_GENERAL) return &s_conic_general[field];
    if (field == 0) return &s_conic_h;
    if (field == 1) return &s_conic_k;
    if (s_conic_type == OPENCALC_CONIC_CIRCLE) return &s_conic_r;
    if (field == 2) return &s_conic_a;
    if (field == 3 && s_conic_type != OPENCALC_CONIC_PARABOLA) return &s_conic_b;
    return &s_conic_angle;
}

static int conic_action_count(void)
{
    return 6;
}

static const char *conic_action_label(int action)
{
    static const char *const labels[] = {
        "Analyze and convert", "Graph now", "Add to overlays",
        "Point / tangent", "Copy equation", "Construct from Calc"
    };
    return labels[action];
}

static void ui_draw_conic_workspace(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_CONICS]);
    ui_text(16, 34, "CONICS", THEME_TEXT, 1);
    ui_text(75, 34, "construct, analyze, graph", THEME_MUTED, 1);
    for (int i = 0; i < 6; i++) {
        int y = 48 + i * 27;
        bool selected = i == s_app_selection;
        ui_rect(14, y, 292, 24, selected ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_rect(14, y, 4, 24, selected ? THEME_ACCENT : THEME_BORDER);
        char number[4];
        snprintf(number, sizeof(number), "%d", i + 1);
        ui_text(24, y + 7, number, selected ? THEME_ACCENT : THEME_MUTED, 1);
        ui_text(43, y + 7, CONICS_TOOLS[i].label, THEME_TEXT, 1);
        ui_text(160, y + 7, CONICS_TOOLS[i].detail, THEME_MUTED, 1);
    }
    ui_text(14, 220, "1-6 open   arrows select   Back home", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_conic_editor(void)
{
    int fields = conic_field_count();
    int total = fields + conic_action_count();
    if (s_conic_selection >= total) s_conic_selection = total - 1;
    int first = s_conic_selection >= 7 ? s_conic_selection - 6 : 0;
    if (first + 7 > total) first = total > 7 ? total - 7 : 0;

    ui_clear(THEME_BG);
    ui_header(&APPS[APP_CONICS]);
    ui_text(14, 34, opencalc_conic_kind_name((opencalc_conic_kind_t)s_conic_type), THEME_TEXT, 1);
    char position[16];
    snprintf(position, sizeof(position), "%d/%d", s_conic_selection + 1, total);
    ui_text(276, 34, position, THEME_MUTED, 1);
    ui_rect(14, 48, 292, 18, THEME_SURFACE_2);
    ui_text(20, 53, s_conic_type == OPENCALC_CONIC_GENERAL ?
            "Ax^2 + Bxy + Cy^2 + Dx + Ey + F = 0" :
            "Canonical parameters; rotation is counterclockwise", THEME_MUTED, 1);

    for (int row = 0; row < 7 && first + row < total; row++) {
        int item = first + row;
        int y = 71 + row * 20;
        bool selected = item == s_conic_selection;
        ui_rect(14, y, 292, 18, selected ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_rect(14, y, 4, 18, selected ? THEME_ACCENT : THEME_BORDER);
        if (item < fields) {
            ui_text(24, y + 5, conic_field_label(item), selected ? THEME_TEXT : THEME_MUTED, 1);
            char value[24];
            if (selected && s_conic_entry_active) snprintf(value, sizeof(value), "%s", s_conic_entry);
            else snprintf(value, sizeof(value), "%.9g", *conic_field_value(item));
            int value_x = 296 - ui_text_width(value, 22, 1);
            ui_text(value_x, y + 5, value, THEME_TEXT, 1);
            if (selected && s_conic_entry_active && s_cursor_blink_visible) {
                ui_rect(value_x + ui_text_width(value, 22, 1), y + 3, 2, 13, THEME_ACCENT);
            }
        } else {
            int action = item - fields;
            ui_text(24, y + 5, conic_action_label(action), selected ? THEME_TEXT : THEME_MUTED, 1);
        }
    }
    ui_text(14, 214, s_conic_status[0] ? s_conic_status : "Type values directly; Enter commits/opens",
            s_conic_status[0] ? THEME_BATTERY_YELLOW : THEME_MUTED, 1);
    ui_text(14, 226, "up/down move   Del erase   Back conics", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_conic_result(void)
{
    if (s_conic_result_scroll < 0) s_conic_result_scroll = 0;
    if (s_conic_result_scroll >= s_conic_result_count) {
        s_conic_result_scroll = s_conic_result_count > 0 ? s_conic_result_count - 1 : 0;
    }
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_CONICS]);
    ui_text(14, 34, s_conic_result_title, THEME_TEXT, 1);
    char position[16];
    snprintf(position, sizeof(position), "%d/%d", s_conic_result_count ? s_conic_result_scroll + 1 : 0,
             s_conic_result_count);
    ui_text(276, 34, position, THEME_MUTED, 1);
    for (int row = 0; row < 9 && s_conic_result_scroll + row < s_conic_result_count; row++) {
        int index = s_conic_result_scroll + row;
        int y = 51 + row * 18;
        ui_rect(14, y, 292, 16, row == 0 ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_text(21, y + 4, s_conic_result_lines[index], row == 0 ? THEME_TEXT : THEME_MUTED, 1);
    }
    ui_text(12, 218, "up/down select   Enter copy row", THEME_MUTED, 1);
    ui_text(12, 229, "Graph key exports   Back editor", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_conic_graphs(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_CONICS]);
    ui_text(14, 34, "CONIC GRAPHS", THEME_TEXT, 1);
    ui_text(116, 34, "up to four overlays", THEME_MUTED, 1);
    for (int i = 0; i < 4; i++) {
        int y = 55 + i * 32;
        bool selected = i == s_conic_overlay_selection;
        ui_rect(14, y, 292, 27, selected ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_rect(14, y, 4, 27, selected ? THEME_ACCENT : THEME_BORDER);
        char line[80];
        if (i < s_conic_overlay_count && s_conic_overlays[i].valid) {
            opencalc_conic_analysis_t analysis;
            opencalc_conic_analyze(&s_conic_overlays[i], &analysis);
            snprintf(line, sizeof(line), "%d  %s%s", i + 1,
                     opencalc_conic_kind_name(analysis.kind), analysis.rotated ? "  rotated" : "");
        } else snprintf(line, sizeof(line), "%d  Empty overlay", i + 1);
        ui_text(24, y + 9, line, i < s_conic_overlay_count ? THEME_TEXT : THEME_MUTED, 1);
    }
    ui_rect(14, 190, 142, 22, THEME_SURFACE_2);
    ui_text_centered_in_box(14, 197, 142, "Enter: graph all", THEME_TEXT, 1);
    ui_rect(164, 190, 142, 22, THEME_SURFACE_2);
    ui_text_centered_in_box(164, 197, 142, "Del: remove", THEME_TEXT, 1);
    ui_text(14, 226, "Graph supports pan, zoom, trace and intersections", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_inequality_workspace(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_INEQUALITY]);
    ui_text(14, 35, "INEQUALITIES", THEME_TEXT, 1);
    ui_text(211, 35, s_ineq_integer_mode ? "INTEGER" : "REAL", THEME_ACCENT, 1);
    for (int i = 0; i < 4; i++) {
        int y = 55 + i * 35;
        bool selected = i == s_app_selection;
        ui_rect(14, y, 292, 29, selected ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_rect(14, y, 4, 29, selected ? THEME_ACCENT : THEME_BORDER);
        char number[4];
        char detail[24];
        snprintf(number, sizeof(number), "%d", i + 1);
        snprintf(detail, sizeof(detail), "%.23s", INEQUALITY_TOOLS[i].detail);
        ui_text(25, y + 7, number, selected ? THEME_TEXT : THEME_MUTED, 1);
        ui_text(45, y + 7, INEQUALITY_TOOLS[i].label, selected ? THEME_TEXT : THEME_MUTED, 1);
        ui_text(166, y + 7, detail, THEME_MUTED, 1);
    }
    ui_rect(14, 201, 292, 18, THEME_SURFACE_2);
    ui_text(20, 207, "Use Calculator input for equations and objectives", THEME_MUTED, 1);
    ui_text(14, 227, "Enter open   2nd+Window real/integer", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_inequality_editor(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_INEQUALITY]);
    ui_text(14, 34, s_ineq_graph_editor ? "GRAPH REGIONS" : "SYSTEM EDITOR", THEME_TEXT, 1);
    ui_text(219, 34, s_ineq_join_or ? "OR" : "AND", THEME_ACCENT, 1);
    for (int i = 0; i < INEQ_MAX; i++) {
        int y = 52 + i * 24;
        bool selected = i == s_ineq_selection;
        ui_rect(14, y, 292, 20, selected ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_rect(14, y, 4, 20, s_graph_colors[i % GRAPH_COLOR_COUNT]);
        char label[8];
        char relation[37];
        snprintf(label, sizeof(label), "I%d", i + 1);
        snprintf(relation, sizeof(relation), "%.36s", s_ineqs[i].text[0] ? s_ineqs[i].text :
                 "Empty - Enter imports Calculator");
        ui_text(24, y + 6, label, selected ? THEME_TEXT : THEME_MUTED, 1);
        ui_text(45, y + 6, relation,
                s_ineqs[i].enabled ? THEME_TEXT : THEME_MUTED, 1);
        if (s_ineqs[i].text[0]) ui_text(278, y + 6, s_ineqs[i].enabled ? "ON" : "OFF",
                                       s_ineqs[i].enabled ? THEME_BATTERY_GREEN : THEME_MUTED, 1);
    }
    char status[49];
    snprintf(status, sizeof(status), "%.48s", s_ineq_status[0] ? s_ineq_status :
             "Enter import   2nd+Enter enable   Del remove");
    ui_text(14, 200, status,
            s_ineq_status[0] ? THEME_BATTERY_YELLOW : THEME_MUTED, 1);
    ui_text(14, 214, "Window AND/OR   Graph shade   Trace -> L1/L2", THEME_MUTED, 1);
    ui_text(14, 228, "Zoom optimize Calculator objective   Back menu", THEME_MUTED, 1);
    ui_present();
}

static bool inequality_endpoint_closed(double value)
{
    for (int i = 0; i < s_ineq_solution.interval_count; i++) {
        const opencalc_inequality_interval_t *interval = &s_ineq_solution.intervals[i];
        if ((!interval->low_infinite && fabs(interval->low - value) < 1e-5 && interval->low_closed) ||
            (!interval->high_infinite && fabs(interval->high - value) < 1e-5 && interval->high_closed)) return true;
    }
    return false;
}

static void ui_draw_inequality_number_line(int y)
{
    double extent = 10.0;
    for (int i = 0; i < s_ineq_solution.critical_count; i++) {
        double magnitude = fabs(s_ineq_solution.critical[i]);
        if (magnitude * 1.35 > extent) extent = magnitude * 1.35;
    }
    int left = 24, right = 296;
    ui_line(left, y, right, y, THEME_BORDER);
    ui_line(left, y, left + 5, y - 3, THEME_BORDER);
    ui_line(left, y, left + 5, y + 3, THEME_BORDER);
    ui_line(right, y, right - 5, y - 3, THEME_BORDER);
    ui_line(right, y, right - 5, y + 3, THEME_BORDER);
    for (int i = 0; i < s_ineq_solution.interval_count; i++) {
        const opencalc_inequality_interval_t *interval = &s_ineq_solution.intervals[i];
        double low = interval->low_infinite ? -extent : interval->low;
        double high = interval->high_infinite ? extent : interval->high;
        int x0 = left + (int)((low + extent) * (right - left) / (2.0 * extent));
        int x1 = left + (int)((high + extent) * (right - left) / (2.0 * extent));
        if (x0 < left) x0 = left;
        if (x1 > right) x1 = right;
        ui_rect(x0, y - 2, x1 - x0 + 1, 5, THEME_ACCENT);
    }
    for (int i = 0; i < s_ineq_solution.critical_count; i++) {
        double value = s_ineq_solution.critical[i];
        if (value < -extent || value > extent) continue;
        int x = left + (int)((value + extent) * (right - left) / (2.0 * extent));
        if (inequality_endpoint_closed(value)) ui_rect(x - 3, y - 3, 7, 7, THEME_ACCENT);
        else {
            ui_rect(x - 3, y - 3, 7, 7, THEME_ACCENT);
            ui_rect(x - 1, y - 1, 3, 3, THEME_BG);
        }
        char label[20];
        snprintf(label, sizeof(label), "%.4g", value);
        ui_text(x - ui_text_width(label, 18, 1) / 2, y + 8, label, THEME_MUTED, 1);
    }
}

static void ui_draw_inequality_result(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_INEQUALITY]);
    ui_text(14, 34, s_ineq_sign_chart ? "SIGN CHART" : "ONE-VARIABLE SOLUTION", THEME_TEXT, 1);
    ui_text(250, 34, s_ineq_integer_mode ? "Z" : "R", THEME_ACCENT, 1);
    ui_rect(14, 49, 292, 24, THEME_SURFACE);
    char problem[47];
    char notation[192];
    char shown_notation[39];
    snprintf(problem, sizeof(problem), "%.46s", s_ineq_problem_text);
    const char *notation_label = "Interval";
    if (s_ineq_notation == 1) {
        notation_label = "Set";
        snprintf(notation, sizeof(notation), "{x in %s | %.150s}",
                 s_ineq_integer_mode ? "Z" : "R", s_ineq_problem_text);
    } else if (s_ineq_notation == 2) {
        notation_label = "Standard";
        inequality_format_standard(notation, sizeof(notation));
    } else {
        snprintf(notation, sizeof(notation), "%s", s_ineq_interval_text);
    }
    snprintf(shown_notation, sizeof(shown_notation), "%.38s", notation);
    ui_text(20, 57, problem, THEME_TEXT, 1);
    ui_text(14, 82, notation_label, THEME_MUTED, 1);
    ui_text(72, 82, shown_notation, THEME_ACCENT, 1);
    ui_draw_inequality_number_line(118);

    if (s_ineq_sign_chart) {
        ui_text(14, 144, "Signs", THEME_MUTED, 1);
        int x = 58;
        for (int i = 0; i <= s_ineq_solution.critical_count && x < 300; i++) {
            double sample;
            if (s_ineq_solution.critical_count == 0) sample = 0.0;
            else if (i == 0) sample = s_ineq_solution.critical[0] - 1.0;
            else if (i == s_ineq_solution.critical_count) sample = s_ineq_solution.critical[i - 1] + 1.0;
            else sample = (s_ineq_solution.critical[i - 1] + s_ineq_solution.critical[i]) * 0.5;
            bool match = false;
            opencalc_inequality_evaluate(&s_ineq_problem, sample, 0.0, &match);
            ui_text(x, 144, match ? "+" : "-", match ? THEME_BATTERY_GREEN : THEME_BATTERY_RED, 1);
            x += 35;
        }
    }
    ui_rect(14, 163, 292, 35, THEME_SURFACE_2);
    if (s_ineq_exact_text[0]) {
        int offset = s_ineq_result_scroll;
        char excerpt[37];
        snprintf(excerpt, sizeof(excerpt), "%.36s", s_ineq_exact_text + offset);
        ui_text(20, 171, "Exact CAS", THEME_MUTED, 1);
        ui_text(84, 171, excerpt, THEME_TEXT, 1);
    } else {
        ui_text(20, 171, "Enter requests exact symbolic solution", THEME_MUTED, 1);
        ui_text(20, 184, "Numerical fallback searches -100 to 100", THEME_MUTED, 1);
    }
    char status[49];
    snprintf(status, sizeof(status), "%.48s", s_ineq_status);
    ui_text(14, 205, status, THEME_BATTERY_YELLOW, 1);
    ui_text(14, 218, "Left/right notation   Y= copy   Trace save", THEME_MUTED, 1);
    ui_text(14, 230, "Window R/Z   Back inequalities", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_app_page(app_id_t app_id)
{
    if (app_id == APP_SOLVER) {
        ui_draw_solver_workspace();
        return;
    }
    if (app_id == APP_STATS) {
        ui_draw_stats_workspace();
        return;
    }
    if (app_id == APP_LISTS) {
        ui_draw_list_workspace();
        return;
    }
    if (app_id == APP_MATRICES) {
        ui_draw_matrix_workspace();
        return;
    }
    if (app_id == APP_FINANCE) {
        ui_draw_finance_workspace();
        return;
    }
    if (app_id == APP_CONICS) {
        ui_draw_conic_workspace();
        return;
    }
    if (app_id == APP_INEQUALITY) {
        ui_draw_inequality_workspace();
        return;
    }
    const app_info_t *app = &APPS[app_id];
    int tool_count = 0;
    const app_tool_t *tools = app_tools_for(app_id, &tool_count);

    ui_clear(THEME_BG);
    ui_header(app);

    if (tool_count > 0 && tools != NULL) {
        if (s_app_selection >= tool_count) {
            s_app_selection = tool_count - 1;
        }
        int list_y = app_id == APP_SOLVER ? 78 : 36;
        if (app_id == APP_SOLVER) {
            char line[112];
            snprintf(line, sizeof(line), "E1: %.28s", s_solver_e1);
            ui_text(18, 34, line, THEME_TEXT, 1);
            snprintf(line, sizeof(line), "E2: %.28s", s_solver_e2);
            ui_text(18, 46, line, THEME_TEXT, 1);
            snprintf(line, sizeof(line), "guess %.6g", s_solver_guess);
            ui_text(18, 58, line, THEME_MUTED, 1);
            if (s_solver_has_result) {
                if (s_solver_has_complex_result) {
                    char root[48];
                    format_complex_value(s_solver_result, s_solver_result_imag, s_complex_mode, root, sizeof(root));
                    snprintf(line, sizeof(line), "x=%.42s", root);
                } else {
                    snprintf(line, sizeof(line), "x=%.10g", s_solver_result);
                }
                ui_text(148, 58, line, THEME_ACCENT, 1);
            }
        }
        int visible_count = 7;
        int first_tool = s_app_selection >= visible_count ? s_app_selection - visible_count + 1 : 0;
        if (first_tool + visible_count > tool_count) {
            first_tool = tool_count > visible_count ? tool_count - visible_count : 0;
        }
        for (int visible = 0; visible < visible_count && first_tool + visible < tool_count; visible++) {
            int i = first_tool + visible;
            uint32_t bg = (i == s_app_selection) ? THEME_ACCENT_2 : THEME_SURFACE;
            ui_rect(18, list_y + visible * 16, 284, 15, bg);
            ui_text(28, list_y + 4 + visible * 16, tools[i].label, THEME_TEXT, 1);
            ui_text(134, list_y + 4 + visible * 16, tools[i].detail, THEME_MUTED, 1);
        }
        ui_scrollbar(310, list_y, visible_count * 16 - 1,
                     tool_count, visible_count, first_tool);
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

static void stats_result_begin(const char *title)
{
    snprintf(s_stats_result_title, sizeof(s_stats_result_title), "%s", title != NULL ? title : "Stats");
    memset(s_stats_result_lines, 0, sizeof(s_stats_result_lines));
    s_stats_result_line_count = 0;
}

static void stats_result_line(const char *fmt, ...)
{
    if (s_stats_result_line_count < 0 ||
        s_stats_result_line_count >= (int)(sizeof(s_stats_result_lines) / sizeof(s_stats_result_lines[0]))) {
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_stats_result_lines[s_stats_result_line_count],
              sizeof(s_stats_result_lines[s_stats_result_line_count]), fmt, ap);
    va_end(ap);
    s_stats_result_line_count++;
}

static void stats_result_show(void)
{
    if (s_stats_result_line_count > 0) {
        snprintf(s_calc_output, sizeof(s_calc_output), "%s", s_stats_result_lines[0]);
    }
    s_page = PAGE_STATS_RESULT;
    s_current_app = APP_STATS;
    ui_draw_current();
}

static void ui_draw_stats_result(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_STATS]);

    ui_text(16, 36, s_stats_result_title[0] ? s_stats_result_title : "Stats Result", THEME_TEXT, 1);
    for (int i = 0; i < s_stats_result_line_count && i < 8; i++) {
        int y = 56 + i * 18;
        ui_rect(14, y - 3, 292, 15, i == 0 ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_text(22, y + 1, s_stats_result_lines[i], i == 0 ? THEME_TEXT : THEME_MUTED, 1);
    }

    ui_text(16, 220, "back - stats  enter - stats", THEME_MUTED, 1);
    ui_present();
}

static const char *stats_tail_text(double value)
{
    int tail = (int)llround(value);
    return tail < 0 ? "<" : (tail > 0 ? ">" : "not equal");
}

static void stats_setup_value_text(int field, char *out, size_t out_size)
{
    if (field < 0 || field >= s_stats_setup_field_count || out == NULL || out_size == 0) return;
    if (field == s_stats_setup_selection && s_stats_setup_entry[0] != '\0') {
        snprintf(out, out_size, "%s", s_stats_setup_entry);
    } else if (s_stats_setup_kinds[field] == STATS_FIELD_LIST) {
        snprintf(out, out_size, "L%d", (int)llround(s_stats_setup_values[field]));
    } else if (s_stats_setup_kinds[field] == STATS_FIELD_TAIL) {
        snprintf(out, out_size, "%s", stats_tail_text(s_stats_setup_values[field]));
    } else if (s_stats_setup_kinds[field] == STATS_FIELD_INTEGER) {
        snprintf(out, out_size, "%d", (int)llround(s_stats_setup_values[field]));
    } else {
        snprintf(out, out_size, "%.10g", s_stats_setup_values[field]);
    }
}

static void ui_draw_stats_setup(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_STATS]);
    ui_text(16, 34, STATS_TOOLS[s_stats_setup_tool].label, THEME_TEXT, 1);

    int first = s_stats_setup_selection >= 7 ? s_stats_setup_selection - 6 : 0;
    for (int row = 0; row < 7 && first + row < s_stats_setup_field_count; ++row) {
        int field = first + row;
        int y = 52 + row * 21;
        bool selected = field == s_stats_setup_selection;
        ui_rect(14, y - 3, 292, 18, selected ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_text(22, y + 2, s_stats_setup_labels[field], selected ? THEME_TEXT : THEME_MUTED, 1);
        char value[32] = "";
        stats_setup_value_text(field, value, sizeof(value));
        ui_text(174, y + 2, value, selected ? THEME_TEXT : THEME_ACCENT, 1);
    }

    if (s_stats_setup_error[0] != '\0') {
        ui_text(16, 202, s_stats_setup_error, THEME_BATTERY_RED, 1);
    }
    ui_text(16, 220, "up/down field  left/right option  enter next/run", THEME_MUTED, 1);
    ui_present();
}

static bool list_minmax(int list, double *min, double *max)
{
    if (list < 0 || list >= LIST_COUNT || s_list_counts[list] <= 0 || min == NULL || max == NULL) {
        return false;
    }
    *min = s_lists[list][0];
    *max = s_lists[list][0];
    for (int i = 1; i < s_list_counts[list]; i++) {
        if (s_lists[list][i] < *min) *min = s_lists[list][i];
        if (s_lists[list][i] > *max) *max = s_lists[list][i];
    }
    if (fabs(*max - *min) < 1e-9) {
        *min -= 1.0;
        *max += 1.0;
    }
    return true;
}

static int stats_plot_x(double x, double xmin, double xmax)
{
    return 24 + (int)((x - xmin) * 271.0 / (xmax - xmin));
}

static int stats_plot_y(double y, double ymin, double ymax)
{
    return 202 - (int)((y - ymin) * 151.0 / (ymax - ymin));
}

static void ui_draw_stats_plot(void)
{
    static const char *plot_names[] = {"Scatter", "XY-Line", "Histogram", "Box Plot", "Normal Prob"};
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_STATS]);

    char title[48];
    snprintf(title, sizeof(title), "%s Plot", plot_names[s_stats_plot_mode]);
    ui_text(16, 34, title, THEME_TEXT, 1);
    ui_border(22, 50, 276, 154, THEME_BORDER);

    if (s_stats_plot_mode == 3) {
        if (s_list_counts[0] < 2) {
            ui_text_center(118, "Box plot needs L1", THEME_MUTED, 1);
        } else {
            double min = 0.0, max = 0.0, q1 = 0.0, q3 = 0.0;
            list_minmax(0, &min, &max);
            list_quartiles(0, &q1, &q3);
            double median = list_median(0);
            int x_min = stats_plot_x(min, min, max);
            int x_q1 = stats_plot_x(q1, min, max);
            int x_med = stats_plot_x(median, min, max);
            int x_q3 = stats_plot_x(q3, min, max);
            int x_max = stats_plot_x(max, min, max);
            ui_line(x_min, 126, x_max, 126, THEME_MUTED);
            ui_line(x_min, 116, x_min, 136, THEME_TEXT);
            ui_line(x_max, 116, x_max, 136, THEME_TEXT);
            ui_border(x_q1, 96, x_q3 - x_q1 + 1, 61, THEME_ACCENT);
            ui_line(x_med, 97, x_med, 155, THEME_ACCENT);
            char range[48];
            snprintf(range, sizeof(range), "%.5g   %.5g", min, max);
            ui_text_center(178, range, THEME_MUTED, 1);
        }
    } else if (s_stats_plot_mode == 4) {
        int count = s_list_counts[0];
        if (count < 3) {
            ui_text_center(118, "Normal plot needs 3+ in L1", THEME_MUTED, 1);
        } else {
            memcpy(s_stats_sort_scratch, s_lists[0], sizeof(double) * (size_t)count);
            for (int i = 1; i < count; ++i) {
                double value = s_stats_sort_scratch[i];
                int j = i - 1;
                while (j >= 0 && s_stats_sort_scratch[j] > value) {
                    s_stats_sort_scratch[j + 1] = s_stats_sort_scratch[j];
                    --j;
                }
                s_stats_sort_scratch[j + 1] = value;
            }
            double ymin = s_stats_sort_scratch[0], ymax = s_stats_sort_scratch[count - 1];
            if (fabs(ymax - ymin) < 1e-12) { ymin -= 1.0; ymax += 1.0; }
            double xmin = opencalc_stats_inverse_normal(0.625 / count, 0.0, 1.0);
            double xmax = opencalc_stats_inverse_normal((count - 0.375) / count, 0.0, 1.0);
            for (int i = 0; i < count; ++i) {
                double rank_p = (i + 0.625) / count;
                int px = stats_plot_x(opencalc_stats_inverse_normal(rank_p, 0.0, 1.0), xmin, xmax);
                int py = stats_plot_y(s_stats_sort_scratch[i], ymin, ymax);
                ui_rect(px - 1, py - 1, 3, 3, THEME_ACCENT);
            }
        }
    } else if (s_stats_plot_mode == 2) {
        double min = 0.0;
        double max = 0.0;
        if (!list_minmax(0, &min, &max)) {
            ui_text_center(118, "Histogram needs L1", THEME_MUTED, 1);
        } else {
            const int bins = 8;
            int counts[8] = {0};
            for (int i = 0; i < s_list_counts[0]; i++) {
                int bin = (int)((s_lists[0][i] - min) * (double)bins / (max - min));
                if (bin < 0) bin = 0;
                if (bin >= bins) bin = bins - 1;
                counts[bin]++;
            }
            int max_count = 1;
            for (int i = 0; i < bins; i++) {
                if (counts[i] > max_count) max_count = counts[i];
            }
            for (int i = 0; i < bins; i++) {
                int x = 28 + i * 33;
                int h = counts[i] * 142 / max_count;
                ui_rect(x, 202 - h, 27, h, THEME_ACCENT);
            }
        }
    } else {
        int count = s_list_counts[0] < s_list_counts[1] ? s_list_counts[0] : s_list_counts[1];
        double xmin = 0.0;
        double xmax = 0.0;
        double ymin = 0.0;
        double ymax = 0.0;
        if (count <= 0 || !list_minmax(0, &xmin, &xmax) || !list_minmax(1, &ymin, &ymax)) {
            ui_text_center(118, "Plot needs L1,L2", THEME_MUTED, 1);
        } else {
            int prev_x = 0;
            int prev_y = 0;
            for (int i = 0; i < count; i++) {
                int px = stats_plot_x(s_lists[0][i], xmin, xmax);
                int py = stats_plot_y(s_lists[1][i], ymin, ymax);
                ui_rect(px - 1, py - 1, 3, 3, THEME_ACCENT);
                if (s_stats_plot_mode == 1 && i > 0) {
                    ui_line(prev_x, prev_y, px, py, THEME_ACCENT);
                }
                prev_x = px;
                prev_y = py;
            }
        }
    }

    ui_text(16, 220, "left, right - plot  back - stats", THEME_MUTED, 1);
    ui_present();
}

static void solver_format_root_value(int index, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0 || index < 0 || index >= s_solver_poly_root_count) {
        return;
    }

    double real = s_solver_poly_root_real[index];
    double imag = s_solver_poly_root_imag[index];
    if (fabs(real) < 5e-11) {
        real = 0.0;
    }
    if (fabs(imag) < 5e-11) {
        imag = 0.0;
    }

    if (imag == 0.0) {
        snprintf(out, out_size, "%.*g", s_solver_precision, real);
    } else {
        snprintf(out, out_size, "%.*g%+.*gi",
                 s_solver_precision, real, s_solver_precision, imag);
    }
}

static double solver_selected_root_residual(void)
{
    if (s_solver_poly_root_selected < 0 || s_solver_poly_root_selected >= s_solver_poly_root_count) {
        return NAN;
    }

    if (!s_solver_roots_are_polynomial) {
        double left = 0.0;
        double right = 0.0;
        double root = s_solver_poly_root_real[s_solver_poly_root_selected];
        if (!graph_eval_expression(s_solver_e1, root, &left) ||
            !graph_eval_expression(s_solver_e2, root, &right)) {
            return NAN;
        }
        return fabs(left - right);
    }

    double coeffs[11] = {0.0};
    int coeff_count = parse_csv_numbers(s_solver_poly_source, coeffs, 11);
    if (coeff_count < 2) {
        return NAN;
    }

    double complex root = s_solver_poly_root_real[s_solver_poly_root_selected] +
        s_solver_poly_root_imag[s_solver_poly_root_selected] * I;
    return cabs(poly_eval_complex_coeffs(coeffs, coeff_count, root));
}

static void solver_copy_selected_root_to_calc(void)
{
    char root[48];
    solver_format_root_value(s_solver_poly_root_selected, root, sizeof(root));
    snprintf(s_calc_input, sizeof(s_calc_input), "%s", root);
    s_calc_cursor = strlen(s_calc_input);
    opencalc_calc_history_clear_selection();
    s_current_app = APP_CALCULATOR;
    s_page = PAGE_CALCULATOR;
}

static void ui_draw_solver_roots(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_SOLVER]);

    char line[112];
    snprintf(line, sizeof(line), s_solver_roots_are_polynomial ?
             "coeffs high..constant: %.44s" : "equation: %.60s",
             s_solver_poly_source[0] ? s_solver_poly_source : "(none)");
    ui_text(14, 34, line, THEME_MUTED, 1);

    if (s_solver_poly_root_count <= 0) {
        ui_text_center(104, "No roots loaded", THEME_TEXT, 1);
        ui_text(20, 220, "back - solver", THEME_MUTED, 1);
        ui_present();
        return;
    }

    if (s_solver_roots_are_polynomial) {
        snprintf(line, sizeof(line), "degree %d  roots %d",
                 s_solver_poly_root_count, s_solver_poly_root_count);
    } else if (s_solver_poly_root_count == SOLVER_POLY_ROOT_MAX) {
        snprintf(line, sizeof(line), "%d roots shown (scan limit)", s_solver_poly_root_count);
    } else {
        snprintf(line, sizeof(line), "%d real roots in [%.4g, %.4g]",
                 s_solver_poly_root_count, s_solver_lower, s_solver_upper);
    }
    ui_text(14, 48, line, THEME_TEXT, 1);

    int visible_count = 7;
    int first = s_solver_poly_root_selected >= visible_count ?
        s_solver_poly_root_selected - visible_count + 1 : 0;
    if (first + visible_count > s_solver_poly_root_count) {
        first = s_solver_poly_root_count > visible_count ? s_solver_poly_root_count - visible_count : 0;
    }

    for (int visible = 0; visible < visible_count && first + visible < s_solver_poly_root_count; visible++) {
        int i = first + visible;
        int y = 66 + visible * 15;
        bool selected = i == s_solver_poly_root_selected;
        char root[48];
        solver_format_root_value(i, root, sizeof(root));
        snprintf(line, sizeof(line), "x%d = %s", i + 1, root);
        ui_rect(14, y - 2, 292, 14, selected ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_text(22, y + 2, line, selected ? THEME_TEXT : THEME_MUTED, 1);
    }
    ui_scrollbar(312, 64, 105, s_solver_poly_root_count, visible_count, first);

    int selected = s_solver_poly_root_selected;
    double real = s_solver_poly_root_real[selected];
    double imag = s_solver_poly_root_imag[selected];
    double residual = solver_selected_root_residual();
    if (fabs(real) < 5e-11) real = 0.0;
    if (fabs(imag) < 5e-11) imag = 0.0;
    if (isfinite(residual) && residual < 5e-11) residual = 0.0;

    ui_rect(14, 178, 292, 34, THEME_SURFACE);
    snprintf(line, sizeof(line), "selected x%d", selected + 1);
    ui_text(22, 184, line, THEME_TEXT, 1);
    snprintf(line, sizeof(line), "real %.10g   imag %.10g", real, imag);
    ui_text(22, 196, line, THEME_MUTED, 1);
    if (isfinite(residual)) {
        snprintf(line, sizeof(line), "%s %.3g",
                 s_solver_roots_are_polynomial ? "|P(x)|" : "residual", residual);
        ui_text(198, 184, line, residual < 1e-7 ? THEME_BATTERY_GREEN : THEME_BATTERY_YELLOW, 1);
    }

    ui_text(10, 220, "up, down - select  enter - copy  back - solver", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_list_editor(void)
{
    enum { VISIBLE_COLUMNS = 3, VISIBLE_ROWS = 8 };
    const int index_x = 8;
    const int grid_x = 28;
    const int column_w = 94;
    const int header_y = 50;
    const int rows_y = 72;
    const int row_h = 17;
    int first_list = s_list_index - 1;
    int first_row = s_list_cursor - 3;
    char line[48];

    if (first_list < 0) first_list = 0;
    if (first_list > LIST_COUNT - VISIBLE_COLUMNS) first_list = LIST_COUNT - VISIBLE_COLUMNS;
    if (first_row < 0) first_row = 0;
    if (first_row > LIST_MAX_VALUES - VISIBLE_ROWS) first_row = LIST_MAX_VALUES - VISIBLE_ROWS;

    ui_clear(THEME_BG);
    ui_header(&APPS[APP_LISTS]);

    snprintf(line, sizeof(line), "L%d[%d]   %d values%s", s_list_index + 1,
             s_list_cursor + 1, s_list_counts[s_list_index], s_list_editing ? "   EDIT" : "");
    ui_text(12, 33, line, s_list_editing ? THEME_ACCENT : THEME_MUTED, 1);

    ui_rect(index_x, header_y, 18, 20, THEME_SURFACE);
    ui_text(index_x + 5, header_y + 7, "#", THEME_MUTED, 1);
    for (int column = 0; column < VISIBLE_COLUMNS; ++column) {
        int list = first_list + column;
        int x = grid_x + column * column_w;
        bool active = list == s_list_index;
        snprintf(line, sizeof(line), "L%d  n=%d", list + 1, s_list_counts[list]);
        ui_rect(x, header_y, column_w - 2, 20, active ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_border(x, header_y, column_w - 2, 20, active ? THEME_ACCENT : THEME_BORDER);
        ui_text(x + 7, header_y + 7, line, active ? THEME_TEXT : THEME_MUTED, 1);
    }

    for (int visible_row = 0; visible_row < VISIBLE_ROWS; ++visible_row) {
        int value_index = first_row + visible_row;
        int y = rows_y + visible_row * row_h;
        snprintf(line, sizeof(line), "%d", value_index + 1);
        ui_text(index_x + 1, y + 5, line, THEME_MUTED, 1);

        for (int column = 0; column < VISIBLE_COLUMNS; ++column) {
            int list = first_list + column;
            int x = grid_x + column * column_w;
            bool selected = list == s_list_index && value_index == s_list_cursor;
            bool populated = value_index < s_list_counts[list];
            const char *display = "";

            if (selected && s_list_editing) {
                display = s_list_entry;
                size_t display_len = strlen(display);
                if (display_len > 13) display += display_len - 13;
            } else if (populated) {
                snprintf(line, sizeof(line), "%.6g", s_lists[list][value_index]);
                display = line;
            }

            ui_rect(x, y, column_w - 2, row_h - 1,
                    selected ? THEME_ACCENT_2 : (visible_row % 2 == 0 ? THEME_SURFACE : THEME_BG));
            ui_border(x, y, column_w - 2, row_h - 1, selected ? THEME_ACCENT : THEME_BORDER);
            if (display[0] != '\0') {
                ui_text(x + 5, y + 5, display, selected ? THEME_TEXT : THEME_MUTED, 1);
            }
            if (selected && s_list_editing) {
                int cursor_x = x + 5 + ui_text_width(display, strlen(display), 1);
                if (cursor_x > x + column_w - 8) cursor_x = x + column_w - 8;
                ui_rect(cursor_x, y + 3, 2, row_h - 7, THEME_ACCENT);
            }
        }
    }

    int scroll_total = s_list_counts[s_list_index] + 1;
    if (scroll_total > LIST_MAX_VALUES) scroll_total = LIST_MAX_VALUES;
    ui_scrollbar(314, rows_y, VISIBLE_ROWS * row_h - 1,
                 scroll_total, VISIBLE_ROWS, first_row);

    ui_text(10, 213, "arrows move   type edits   enter saves", THEME_MUTED, 1);
    ui_text(10, 226, "del removes   2nd+del clears   2nd+mode quits", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_settings(void)
{
    char brightness[40];
    char sleep[40];
    char power_save[40];
    char theme[40];
    char audio[40];
    char reset[40];

    snprintf(brightness, sizeof(brightness), "Brightness %d%%", board_get_backlight_brightness());
    snprintf(sleep, sizeof(sleep), "Auto sleep %s", s_sleep_enabled ? "on" : "off");
    snprintf(power_save, sizeof(power_save), "Power save %s", s_power_save_enabled ? "on" : "off");
    snprintf(theme, sizeof(theme), "Theme %s", s_light_mode ? "light" : "dark");
    snprintf(audio, sizeof(audio), "Audio %d%%", s_audio_volume_percent);
    snprintf(reset, sizeof(reset), "Reset to factory");

    const char *items[SETTINGS_COUNT] = {
        brightness,
        sleep,
        power_save,
        theme,
        audio,
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
    } else if (s_script_selection == 4) {
        footer = "left, right - audio";
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
    static const char *complex_modes[] = {"REAL", "a+bi", "re^ti"};

    char rows[5][40];
    snprintf(rows[0], sizeof(rows[0]), "Display %s", display[s_display_format]);
    snprintf(rows[1], sizeof(rows[1]), "Print %s", print[s_print_mode]);
    snprintf(rows[2], sizeof(rows[2]), "Angle %s", angle[s_angle_mode]);
    snprintf(rows[3], sizeof(rows[3]), "Graph %s", graph[s_graphing_mode]);
    snprintf(rows[4], sizeof(rows[4]), "Complex %s", complex_modes[s_complex_mode]);

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

    bool truncated = false;
    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 3 && strcmp(entry->d_name + len - 3, ".py") == 0) {
            if (s_script_count >= SCRIPT_MAX) {
                truncated = true;
                continue;
            }
            int insert_at = s_script_count++;
            while (insert_at > 0 && strcmp(entry->d_name, s_scripts[insert_at - 1]) < 0) {
                snprintf(s_scripts[insert_at], sizeof(s_scripts[insert_at]), "%s", s_scripts[insert_at - 1]);
                insert_at--;
            }
            snprintf(s_scripts[insert_at], sizeof(s_scripts[insert_at]), "%s", entry->d_name);
        }
    }
    closedir(dir);
    if (s_script_selection >= s_script_count) {
        s_script_selection = 0;
    }
    if (truncated) {
        snprintf(s_script_status, sizeof(s_script_status), "showing first %d scripts", SCRIPT_MAX);
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
    s_script_editor_scroll_col = 0;
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
        s_script_editor[0] = '\0';
        s_script_editor_len = strlen(s_script_editor);
        s_script_editor_cursor = s_script_editor_len;
        s_script_editor_scroll_line = 0;
        s_script_editor_scroll_col = 0;
        memset(s_script_breakpoints, 0, sizeof(s_script_breakpoints));
        snprintf(s_script_status, sizeof(s_script_status), "new blank - 2nd enter saves");
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
    s_script_editor_scroll_col = 0;
    memset(s_script_breakpoints, 0, sizeof(s_script_breakpoints));
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
                        s_script_action == SCRIPT_ACTION_DEBUG ? "Debug script" :
                        "Run script";
    const char *footer = s_script_action == SCRIPT_ACTION_EDIT ? "enter - edit  back - menu" :
                         s_script_action == SCRIPT_ACTION_DELETE ? "enter - delete  back - menu" :
                         s_script_action == SCRIPT_ACTION_DEBUG ? "enter - debug  back - menu" :
                         "enter - run  back - menu";
    ui_text(18, 32, title, THEME_ACCENT, 1);

    if (s_script_count == 0) {
        ui_text(22, 54, "No scripts in /scripts", THEME_TEXT, 1);
        ui_text(22, 76, (s_script_action == SCRIPT_ACTION_RUN ||
                         s_script_action == SCRIPT_ACTION_DEBUG) ?
                        "Add .py over USB" : "Use New program first", THEME_TEXT, 1);
    }

    const int visible_count = 6;
    int first = s_script_selection >= visible_count ? s_script_selection - visible_count + 1 : 0;
    if (first + visible_count > s_script_count) {
        first = s_script_count > visible_count ? s_script_count - visible_count : 0;
    }
    for (int row = 0; row < visible_count && first + row < s_script_count; row++) {
        int i = first + row;
        uint32_t bg = (i == s_script_selection) ? THEME_ACCENT_2 : THEME_SURFACE;
        ui_rect(18, 50 + row * 20, 284, 17, bg);
        ui_text(28, 55 + row * 20, s_scripts[i], THEME_TEXT, 1);
    }
    ui_scrollbar(310, 50, 117, s_script_count, visible_count, first);
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
        "Debug script",
        "Edit script",
        "New script",
        "Delete script",
    };
    static const char *const detail[PROGRAM_MENU_COUNT] = {
        "Open /scripts and run",
        "Breakpoints, step and inspect",
        "Choose a file and edit text",
        "Create and edit a new .py",
        "Choose a file and delete it",
    };

    ui_clear(THEME_BG);
    ui_header(&APPS[APP_PYTHON]);
    for (int i = 0; i < PROGRAM_MENU_COUNT; i++) {
        int y = 34 + i * 34;
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

    if (s_script_output_mutex != NULL) {
        xSemaphoreTake(s_script_output_mutex, portMAX_DELAY);
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
    if (s_script_output_mutex != NULL) {
        xSemaphoreGive(s_script_output_mutex);
    }
    s_script_screen_dirty = true;
}

static void script_output_callback(const char *text, void *user_data)
{
    (void)user_data;
    if (!s_script_first_output_logged) {
        s_script_first_output_logged = true;
        printf("Python worker: first output (%u bytes), stack free=%u bytes\n",
               (unsigned)(text != NULL ? strlen(text) : 0),
               (unsigned)uxTaskGetStackHighWaterMark(NULL));
    }
    script_output_append(text);
    vTaskDelay(pdMS_TO_TICKS(1));
}

static void script_value_none(py_value_t *value)
{
    memset(value, 0, sizeof(*value));
    value->type = PY_VALUE_NONE;
}

static void script_value_bool(py_value_t *value, bool state)
{
    script_value_none(value);
    value->type = PY_VALUE_BOOL;
    value->int_value = state ? 1 : 0;
}

static void script_value_float(py_value_t *value, double number)
{
    script_value_none(value);
    value->type = PY_VALUE_FLOAT;
    value->float_value = number;
}

static void script_value_int(py_value_t *value, int number)
{
    script_value_none(value);
    value->type = PY_VALUE_INT;
    value->int_value = number;
}

static void script_value_string(py_value_t *value, const char *text)
{
    script_value_none(value);
    value->type = PY_VALUE_STRING;
    snprintf(value->string_value, sizeof(value->string_value), "%s", text != NULL ? text : "");
}

static bool script_arg_number(const py_value_t *value, double *number)
{
    if (value->type == PY_VALUE_FLOAT) {
        *number = value->float_value;
        return true;
    }
    if (value->type == PY_VALUE_INT || value->type == PY_VALUE_BOOL) {
        *number = value->int_value;
        return true;
    }
    return false;
}

static bool script_arg_int(const py_value_t *value, int *number)
{
    double converted = 0.0;
    if (!script_arg_number(value, &converted) || !isfinite(converted) ||
        converted < INT_MIN || converted > INT_MAX) return false;
    *number = (int)converted;
    return true;
}

static bool script_storage_path(const py_value_t *value, char *path, size_t path_size)
{
    if (value->type != PY_VALUE_STRING || value->string_value[0] == '\0' ||
        strchr(value->string_value, '/') != NULL || strstr(value->string_value, "..") != NULL) {
        return false;
    }
    return snprintf(path, path_size, "/data/user/%s", value->string_value) > 0;
}

static bool script_graphics_append(const script_graphics_command_t *command)
{
    bool stored = false;
    if (s_script_output_mutex != NULL) xSemaphoreTake(s_script_output_mutex, portMAX_DELAY);
    if (command->kind == SCRIPT_GFX_CLEAR) s_script_graphics_count = 0;
    if (s_script_graphics_count < SCRIPT_GRAPHICS_MAX) {
        s_script_graphics[s_script_graphics_count++] = *command;
        s_script_graphics_active = true;
        stored = true;
    }
    if (s_script_output_mutex != NULL) xSemaphoreGive(s_script_output_mutex);
    if (stored) s_script_screen_dirty = true;
    return stored;
}

static bool script_graphics_command_valid(script_graphics_command_t *command)
{
    command->color &= 0xffffffU;
    if (command->kind == SCRIPT_GFX_CLEAR) return true;
    if (command->x0 < -UI_W || command->x0 >= UI_W * 2 ||
        command->y0 < -UI_H || command->y0 >= UI_H * 2) return false;
    if (command->kind == SCRIPT_GFX_LINE &&
        (command->x1 < -UI_W || command->x1 >= UI_W * 2 ||
         command->y1 < -UI_H || command->y1 >= UI_H * 2)) return false;
    if (command->kind == SCRIPT_GFX_RECT &&
        (command->x1 < 0 || command->x1 > UI_W ||
         command->y1 < 0 || command->y1 > UI_H)) return false;
    return true;
}

static int script_native_math(py_t *py, const char *function,
                              const py_value_t *args, size_t arg_count,
                              py_value_t *result)
{
    double value = 0.0;
    if ((strcmp(function, "eval") == 0 || strcmp(function, "cas") == 0) &&
        arg_count == 1 && args[0].type == PY_VALUE_STRING) {
        if (strcmp(function, "cas") == 0) {
            char output[PY_MAX_STRING];
            if (!opencalc_giac_eval(args[0].string_value, s_angle_mode == 0,
                                    output, sizeof(output))) {
                py_runtime_error(py, output[0] ? output : "CAS evaluation failed");
                return 0;
            }
            script_value_string(result, output);
            return 1;
        }
        if (!opencalc_math_eval_expression(args[0].string_value, &value)) {
            py_runtime_error(py, "math.eval() could not evaluate expression");
            return 0;
        }
        script_value_float(result, value);
        return 1;
    }
    if (arg_count != 1 || !script_arg_number(&args[0], &value)) {
        py_runtime_error(py, "math function expects one number");
        return 0;
    }
    if (strcmp(function, "sin") == 0) value = sin(value);
    else if (strcmp(function, "cos") == 0) value = cos(value);
    else if (strcmp(function, "tan") == 0) value = tan(value);
    else if (strcmp(function, "sqrt") == 0 && value >= 0.0) value = sqrt(value);
    else if (strcmp(function, "log") == 0 && value > 0.0) value = log(value);
    else if (strcmp(function, "log10") == 0 && value > 0.0) value = log10(value);
    else if (strcmp(function, "exp") == 0) value = exp(value);
    else if (strcmp(function, "floor") == 0) value = floor(value);
    else if (strcmp(function, "ceil") == 0) value = ceil(value);
    else {
        py_runtime_error(py, "unknown math function or domain error");
        return 0;
    }
    if (!isfinite(value)) {
        py_runtime_error(py, "math function result is not finite");
        return 0;
    }
    script_value_float(result, value);
    return 1;
}

static int script_sensor_gpio_mode(int pin, int mode, void *user_data)
{
    (void)user_data;
    return opencalc_sensor_digital_mode(pin, (opencalc_sensor_pin_mode_t)mode);
}

static int script_sensor_gpio_write(int pin, int value, void *user_data)
{
    (void)user_data;
    return opencalc_sensor_digital_write(pin, value != 0);
}

static int script_sensor_gpio_read(int pin, int *value, void *user_data)
{
    (void)user_data;
    bool high = false;
    if (value == NULL || !opencalc_sensor_digital_read(pin, &high)) return 0;
    *value = high ? 1 : 0;
    return 1;
}

static bool script_list_number(const py_value_t *arg, int *index)
{
    int number = 0;
    if (!script_arg_int(arg, &number) || number < 1 || number > LIST_COUNT) return false;
    *index = number - 1;
    return true;
}

static void script_lists_mark_dirty(void)
{
    atomic_store(&s_script_lists_dirty, true);
}

static bool script_lists_lock(TickType_t wait)
{
    return opencalc_worksheet_model_lock(wait);
}

static void script_lists_unlock(void)
{
    opencalc_worksheet_model_unlock();
}

static int script_native_sensors(py_t *py, const char *function,
                                 const py_value_t *args, size_t arg_count,
                                 py_value_t *result)
{
    if (strcmp(function, "available") == 0 && arg_count == 0) {
        script_value_bool(result, opencalc_sensor_hub_available());
        return 1;
    }
    if (strcmp(function, "mode") == 0 && arg_count == 2) {
        int pin = 0, mode = 0;
        if (!script_arg_int(&args[0], &pin) || !script_arg_int(&args[1], &mode) ||
            !opencalc_sensor_digital_mode(pin, (opencalc_sensor_pin_mode_t)mode)) {
            py_runtime_error(py, "sensors.mode(pin, mode) failed; use D0..D11 and mode 0..2");
            return 0;
        }
        return 1;
    }
    if (strcmp(function, "digital_write") == 0 && arg_count == 2) {
        int pin = 0, value = 0;
        if (!script_arg_int(&args[0], &pin) || !script_arg_int(&args[1], &value) ||
            !opencalc_sensor_digital_write(pin, value != 0)) {
            py_runtime_error(py, "sensors.digital_write(D0..D11, value) failed");
            return 0;
        }
        return 1;
    }
    if (strcmp(function, "digital_read") == 0 && arg_count == 1) {
        int pin = 0;
        bool high = false;
        if (!script_arg_int(&args[0], &pin) || !opencalc_sensor_digital_read(pin, &high)) {
            py_runtime_error(py, "sensors.digital_read() failed");
            return 0;
        }
        script_value_int(result, high ? 1 : 0);
        return 1;
    }
    if ((strcmp(function, "analog_read") == 0 || strcmp(function, "analog_raw") == 0) &&
        arg_count == 1) {
        int channel = 0;
        if (!script_arg_int(&args[0], &channel)) {
            py_runtime_error(py, "analog channel must be A0..A3 (0..3)");
            return 0;
        }
        if (strcmp(function, "analog_raw") == 0) {
            int16_t raw = 0;
            if (!opencalc_sensor_analog_read_raw(channel, &raw)) {
                py_runtime_error(py, "ADS1115 raw read failed");
                return 0;
            }
            script_value_int(result, raw);
        } else {
            double volts = 0.0;
            if (!opencalc_sensor_analog_read_volts(channel, &volts)) {
                py_runtime_error(py, "ADS1115 voltage read failed");
                return 0;
            }
            script_value_float(result, volts);
        }
        return 1;
    }
    if (strcmp(function, "analog_diff") == 0 && arg_count == 2) {
        int positive = 0, negative = 0;
        double volts = 0.0;
        if (!script_arg_int(&args[0], &positive) || !script_arg_int(&args[1], &negative) ||
            !opencalc_sensor_analog_read_differential(positive, negative, &volts)) {
            py_runtime_error(py, "analog_diff supports A0-A1, A0-A3, A1-A3, or A2-A3");
            return 0;
        }
        script_value_float(result, volts);
        return 1;
    }
    if (strcmp(function, "rate") == 0 && arg_count <= 1) {
        if (arg_count == 1) {
            int rate = 0;
            if (!script_arg_int(&args[0], &rate) || !opencalc_sensor_analog_set_rate(rate)) {
                py_runtime_error(py, "sensors.rate() expects 8..860 samples/s");
                return 0;
            }
        }
        script_value_int(result, opencalc_sensor_analog_get_rate());
        return 1;
    }
    if (strcmp(function, "delay") == 0 && arg_count == 1) {
        int delay_ms = 0;
        if (!script_arg_int(&args[0], &delay_ms) || delay_ms < 0 || delay_ms > 60000) {
            py_runtime_error(py, "sensors.delay(ms) expects 0..60000");
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        return 1;
    }
    if (strcmp(function, "list_clear") == 0 && arg_count == 1) {
        int list = 0;
        if (!script_list_number(&args[0], &list) || !script_lists_lock(portMAX_DELAY)) {
            py_runtime_error(py, "list number must be 1..6");
            return 0;
        }
        memset(s_lists[list], 0, sizeof(s_lists[list]));
        s_list_counts[list] = 0;
        script_lists_unlock();
        script_lists_mark_dirty();
        return 1;
    }
    if (strcmp(function, "list_append") == 0 && arg_count == 2) {
        int list = 0;
        double value = 0.0;
        if (!script_list_number(&args[0], &list) || !script_arg_number(&args[1], &value) ||
            !isfinite(value) || !script_lists_lock(portMAX_DELAY)) {
            py_runtime_error(py, "list_append(list, value) failed or list is full");
            return 0;
        }
        if (s_list_counts[list] >= LIST_MAX_VALUES) {
            script_lists_unlock();
            py_runtime_error(py, "list_append(list, value) failed or list is full");
            return 0;
        }
        s_lists[list][s_list_counts[list]++] = value;
        int new_count = s_list_counts[list];
        script_lists_unlock();
        script_lists_mark_dirty();
        script_value_int(result, new_count);
        return 1;
    }
    if (strcmp(function, "list_count") == 0 && arg_count == 1) {
        int list = 0;
        if (!script_list_number(&args[0], &list)) {
            py_runtime_error(py, "list number must be 1..6");
            return 0;
        }
        if (!script_lists_lock(portMAX_DELAY)) {
            py_runtime_error(py, "list storage unavailable");
            return 0;
        }
        int count = s_list_counts[list];
        script_lists_unlock();
        script_value_int(result, count);
        return 1;
    }
    if (strcmp(function, "capture") == 0 && arg_count == 4) {
        int channel = 0, count = 0, rate = 0, list = 0;
        if (!script_arg_int(&args[0], &channel) || !script_arg_int(&args[1], &count) ||
            !script_arg_int(&args[2], &rate) || !script_list_number(&args[3], &list) ||
            channel < 0 || channel >= OPENCALC_SENSOR_ANALOG_COUNT ||
            count < 1 || count > LIST_MAX_VALUES ||
            !opencalc_sensor_analog_set_rate(rate)) {
            py_runtime_error(py, "capture(channel,count,rate,list) arguments are invalid");
            return 0;
        }
        if (!script_lists_lock(portMAX_DELAY)) {
            py_runtime_error(py, "list storage unavailable");
            return 0;
        }
        atomic_store(&s_sensor_capture_active, true);
        s_list_counts[list] = 0;
        for (int i = 0; i < count; i++) {
            double volts = 0.0;
            if (s_script_stop_requested || !opencalc_sensor_analog_read_volts(channel, &volts)) {
                atomic_store(&s_sensor_capture_active, false);
                script_lists_unlock();
                script_lists_mark_dirty();
                py_runtime_error(py, s_script_stop_requested ? "capture stopped" : "capture read failed");
                return 0;
            }
            s_lists[list][s_list_counts[list]++] = volts;
            taskYIELD();
        }
        atomic_store(&s_sensor_capture_active, false);
        int captured = s_list_counts[list];
        script_lists_unlock();
        script_lists_mark_dirty();
        script_value_int(result, captured);
        return 1;
    }
    if ((strcmp(function, "wait_analog") == 0 && arg_count == 4) ||
        (strcmp(function, "wait_digital") == 0 && arg_count == 3)) {
        int channel = 0, direction_or_state = 0, timeout_ms = 0;
        double threshold = 0.0;
        bool analog = strcmp(function, "wait_analog") == 0;
        bool valid = script_arg_int(&args[0], &channel);
        if (analog) {
            valid = valid && script_arg_number(&args[1], &threshold) &&
                    script_arg_int(&args[2], &direction_or_state) &&
                    script_arg_int(&args[3], &timeout_ms) &&
                    (direction_or_state == -1 || direction_or_state == 1);
        } else {
            valid = valid && script_arg_int(&args[1], &direction_or_state) &&
                    script_arg_int(&args[2], &timeout_ms);
        }
        if (!valid || timeout_ms < 0 || timeout_ms > 60000) {
            py_runtime_error(py, "invalid trigger arguments");
            return 0;
        }
        int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
        bool triggered = false;
        do {
            if (analog) {
                double volts = 0.0;
                if (!opencalc_sensor_analog_read_volts(channel, &volts)) break;
                triggered = direction_or_state > 0 ? volts >= threshold : volts <= threshold;
            } else {
                bool high = false;
                if (!opencalc_sensor_digital_read(channel, &high)) break;
                triggered = high == (direction_or_state != 0);
            }
            if (!triggered) vTaskDelay(pdMS_TO_TICKS(5));
        } while (!s_script_stop_requested && esp_timer_get_time() < deadline);
        script_value_bool(result, triggered);
        return 1;
    }
    if (strcmp(function, "i2c_present") == 0 && arg_count == 1) {
        int address = 0;
        if (!script_arg_int(&args[0], &address) || address < 0x08 || address > 0x77) {
            py_runtime_error(py, "I2C address must be 0x08..0x77");
            return 0;
        }
        script_value_bool(result, opencalc_sensor_i2c_present((uint8_t)address));
        return 1;
    }
    if ((strcmp(function, "i2c_read8") == 0 || strcmp(function, "i2c_read16") == 0) &&
        arg_count == 2) {
        int address = 0, reg = 0;
        uint8_t data[2] = {0};
        size_t size = strcmp(function, "i2c_read16") == 0 ? 2 : 1;
        if (!script_arg_int(&args[0], &address) || !script_arg_int(&args[1], &reg) ||
            address < 0x08 || address > 0x77 || reg < 0 || reg > 255 ||
            !opencalc_sensor_i2c_read((uint8_t)address, (uint8_t)reg, data, size)) {
            py_runtime_error(py, "external I2C read failed");
            return 0;
        }
        script_value_int(result, size == 1 ? data[0] : ((int)data[0] << 8) | data[1]);
        return 1;
    }
    if (strcmp(function, "i2c_write8") == 0 && arg_count == 3) {
        int address = 0, reg = 0, value = 0;
        if (!script_arg_int(&args[0], &address) || !script_arg_int(&args[1], &reg) ||
            !script_arg_int(&args[2], &value) || address < 0x08 || address > 0x77 ||
            reg < 0 || reg > 255 || value < 0 || value > 255) {
            py_runtime_error(py, "external I2C write arguments are invalid");
            return 0;
        }
        uint8_t byte = (uint8_t)value;
        if (!opencalc_sensor_i2c_write((uint8_t)address, (uint8_t)reg, &byte, 1)) {
            py_runtime_error(py, "external I2C write failed");
            return 0;
        }
        return 1;
    }
    py_runtime_error(py, "unknown sensors function or wrong arguments");
    return 0;
}

static int script_native_callback(py_t *py, const char *module,
                                  const char *function,
                                  const py_value_t *args, size_t arg_count,
                                  py_value_t *result, void *user_data)
{
    (void)user_data;
    script_value_none(result);
    if (strcmp(module, "math") == 0) {
        return script_native_math(py, function, args, arg_count, result);
    }
    if (strcmp(module, "sensors") == 0) {
        return script_native_sensors(py, function, args, arg_count, result);
    }
    if (strcmp(module, "keys") == 0 && strcmp(function, "down") == 0 && arg_count == 1) {
        int button = 0;
        if (!script_arg_int(&args[0], &button) || button < 1 || button > 50) {
            py_runtime_error(py, "keys.down() expects button 1..50");
            return 0;
        }
        int index = button - 1;
        script_value_bool(result, s_script_key_state[index / BOARD_KEYPAD_COLS]
                                                   [index % BOARD_KEYPAD_COLS]);
        return 1;
    }
    if (strcmp(module, "audio") == 0) {
        if (strcmp(function, "available") == 0 && arg_count == 0) {
            script_value_bool(result, opencalc_audio_available());
            return 1;
        }
        if (strcmp(function, "volume") == 0 && arg_count <= 1) {
            if (arg_count == 1) {
                int volume = 0;
                if (!script_arg_int(&args[0], &volume) || volume < 0 || volume > 100) {
                    py_runtime_error(py, "audio.volume() expects 0..100");
                    return 0;
                }
                opencalc_audio_set_volume_percent(volume);
            }
            result->type = PY_VALUE_INT;
            result->int_value = opencalc_audio_get_volume_percent();
            return 1;
        }
        if (strcmp(function, "tone") == 0 && (arg_count == 2 || arg_count == 3)) {
            int frequency = 0, duration = 0, volume = s_audio_volume_percent;
            if (!script_arg_int(&args[0], &frequency) || !script_arg_int(&args[1], &duration) ||
                (arg_count == 3 && !script_arg_int(&args[2], &volume)) ||
                frequency < 1 || frequency > 20000 || duration < 1 || duration > 5000 ||
                volume < 0 || volume > 100) {
                py_runtime_error(py, "audio.tone() arguments are out of range");
                return 0;
            }
            opencalc_audio_play_tone((uint16_t)frequency, (uint16_t)duration, (uint8_t)volume);
            return 1;
        }
    }
    if (strcmp(module, "storage") == 0 && arg_count >= 1) {
        char path[192];
        if (!script_storage_path(&args[0], path, sizeof(path))) {
            py_runtime_error(py, "storage filename must be a simple relative name");
            return 0;
        }
        (void)mkdir("/data/user", 0775);
        if (strcmp(function, "exists") == 0 && arg_count == 1) {
            FILE *file = fopen(path, "r");
            script_value_bool(result, file != NULL);
            if (file != NULL) fclose(file);
            return 1;
        }
        if (strcmp(function, "read") == 0 && arg_count == 1) {
            FILE *file = fopen(path, "r");
            if (file == NULL) {
                py_runtime_error(py, "storage.read() could not open file");
                return 0;
            }
            size_t count = fread(result->string_value, 1, sizeof(result->string_value) - 1, file);
            fclose(file);
            result->string_value[count] = '\0';
            result->type = PY_VALUE_STRING;
            return 1;
        }
        if (strcmp(function, "write") == 0 && arg_count == 2 && args[1].type == PY_VALUE_STRING) {
            FILE *file = fopen(path, "w");
            if (file == NULL) {
                py_runtime_error(py, "storage.write() could not open file");
                return 0;
            }
            size_t length = strlen(args[1].string_value);
            bool ok = fwrite(args[1].string_value, 1, length, file) == length;
            fclose(file);
            if (!ok) {
                py_runtime_error(py, "storage.write() failed");
                return 0;
            }
            script_value_bool(result, true);
            return 1;
        }
        if (strcmp(function, "remove") == 0 && arg_count == 1) {
            script_value_bool(result, remove(path) == 0);
            return 1;
        }
    }
    if (strcmp(module, "graphics") == 0) {
        script_graphics_command_t command = {0};
        int values[5] = {0};
        size_t numeric_count = arg_count;
        command.color = 0xffffff;
        if (strcmp(function, "text") == 0) {
            int color = 0;
            if (arg_count != 4 || args[2].type != PY_VALUE_STRING ||
                !script_arg_int(&args[0], &command.x0) ||
                !script_arg_int(&args[1], &command.y0) ||
                !script_arg_int(&args[3], &color)) {
                py_runtime_error(py, "graphics.text(x,y,text,color) expected");
                return 0;
            }
            command.kind = SCRIPT_GFX_TEXT;
            command.color = (uint32_t)color;
            snprintf(command.text, sizeof(command.text), "%s", args[2].string_value);
        } else {
            for (size_t i = 0; i < numeric_count && i < 5; i++) {
                if (!script_arg_int(&args[i], &values[i])) {
                    py_runtime_error(py, "graphics arguments must be integers");
                    return 0;
                }
            }
            if (strcmp(function, "clear") == 0 && arg_count <= 1) {
                command.kind = SCRIPT_GFX_CLEAR;
                command.color = arg_count ? (uint32_t)values[0] : 0x000000;
            } else if (strcmp(function, "pixel") == 0 && arg_count == 3) {
                command.kind = SCRIPT_GFX_PIXEL;
                command.x0 = values[0]; command.y0 = values[1]; command.color = (uint32_t)values[2];
            } else if (strcmp(function, "line") == 0 && arg_count == 5) {
                command.kind = SCRIPT_GFX_LINE;
                command.x0 = values[0]; command.y0 = values[1];
                command.x1 = values[2]; command.y1 = values[3]; command.color = (uint32_t)values[4];
            } else if (strcmp(function, "rect") == 0 && arg_count == 5) {
                command.kind = SCRIPT_GFX_RECT;
                command.x0 = values[0]; command.y0 = values[1];
                command.x1 = values[2]; command.y1 = values[3]; command.color = (uint32_t)values[4];
            } else {
                py_runtime_error(py, "unknown graphics call or wrong arguments");
                return 0;
            }
        }
        if (!script_graphics_command_valid(&command)) {
            py_runtime_error(py, "graphics coordinates are out of range");
            return 0;
        }
        if (!script_graphics_append(&command)) {
            py_runtime_error(py, "graphics command buffer full");
            return 0;
        }
        return 1;
    }
    py_runtime_error(py, "unknown OpenCalc module function");
    return 0;
}

static int script_debug_callback(py_t *py, py_debug_event_t event,
                                 size_t line, const char *function,
                                 void *user_data)
{
    (void)user_data;
    if (event == PY_DEBUG_ERROR) {
        if (s_script_output_mutex != NULL) xSemaphoreTake(s_script_output_mutex, portMAX_DELAY);
        snprintf(s_script_traceback, sizeof(s_script_traceback), "%s", py_get_traceback(py));
        if (s_script_output_mutex != NULL) xSemaphoreGive(s_script_output_mutex);
        s_script_screen_dirty = true;
        return 1;
    }
    if (event != PY_DEBUG_STATEMENT) return !s_script_stop_requested;
    s_script_profile = *py_get_profile(py);
    s_script_elapsed_ms = (uint32_t)((esp_timer_get_time() - s_script_started_us) / 1000);
    if ((py_get_profile(py)->statements & 31UL) == 0) vTaskDelay(pdMS_TO_TICKS(1));
    if (s_script_stop_requested) {
        py_runtime_error(py, "script stopped by user");
        return 0;
    }
    int64_t now = esp_timer_get_time();
    if ((uint64_t)(now - s_script_started_us) > OPENCALC_SCRIPT_TIMEOUT_MS * 1000ULL) {
        py_runtime_error(py, "script execution time limit exceeded");
        return 0;
    }
    if (!s_script_debug_mode ||
        (!s_script_debug_step && (line >= SCRIPT_BREAKPOINT_MAX || !s_script_breakpoints[line]))) {
        return 1;
    }

    int64_t pause_started = now;
    if (s_script_output_mutex != NULL) xSemaphoreTake(s_script_output_mutex, portMAX_DELAY);
    py_format_variables(py, s_script_variables, sizeof(s_script_variables));
    s_script_debug_line = line;
    snprintf(s_script_debug_function, sizeof(s_script_debug_function), "%s",
             function != NULL ? function : "<script>");
    if (s_script_output_mutex != NULL) xSemaphoreGive(s_script_output_mutex);
    s_script_debug_step = false;
    s_script_debug_resume = 0;
    s_script_debug_paused = true;
    s_script_view = SCRIPT_VIEW_VARIABLES;
    snprintf(s_script_status, sizeof(s_script_status), "paused L%u  Enter step  Trace run", (unsigned)line);
    s_script_screen_dirty = true;

    while (s_script_debug_resume == 0 && !s_script_stop_requested) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
    }
    s_script_started_us += esp_timer_get_time() - pause_started;
    s_script_debug_paused = false;
    if (s_script_stop_requested) {
        py_runtime_error(py, "script stopped by user");
        return 0;
    }
    s_script_debug_step = s_script_debug_resume == 1;
    snprintf(s_script_status, sizeof(s_script_status), "running...");
    s_script_screen_dirty = true;
    return 1;
}

static bool alpha_case_key(int row, int col)
{
    if ((!s_alpha_active && !s_alpha_locked) || row != 0 || (col != 0 && col != 1)) {
        return false;
    }
    s_alpha_lowercase = col == 1;
    s_second_active = false;
    return true;
}

static const char *alpha_key_text(const board_key_t *key, char converted[2])
{
    if (key == NULL || key->alpha == NULL || key->alpha[0] == '\0') {
        return NULL;
    }
    if (key->alpha[1] == '\0' && isalpha((unsigned char)key->alpha[0])) {
        converted[0] = s_alpha_lowercase
            ? (char)tolower((unsigned char)key->alpha[0])
            : (char)toupper((unsigned char)key->alpha[0]);
        converted[1] = '\0';
        return converted;
    }
    return key->alpha;
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
    s_cursor_blink_visible = true;
    s_cursor_blink_last_visible = true;

    if (row == 1 && col == 0) {
        s_second_active = !s_second_active;
        return true;
    }
    if (row == 2 && col == 0) {
        if (s_second_active) {
            s_alpha_locked = !s_alpha_locked;
            s_alpha_active = s_alpha_locked;
            s_second_active = false;
        } else if (s_alpha_locked) {
            s_alpha_locked = false;
            s_alpha_active = false;
        } else {
            s_alpha_active = !s_alpha_active;
            s_second_active = false;
        }
        return true;
    }
    if (alpha_case_key(row, col)) {
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
    char converted[2];
    const char *alpha_text = alpha_key_text(key, converted);
    if ((s_alpha_active || s_alpha_locked) && alpha_text != NULL) {
        script_input_append_text(alpha_text);
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

static void variables_save_all(void)
{
    size_t used = 0;
    s_variable_persist_buffer[0] = '\0';
    opencalc_variable_t variable;
    for (size_t i = 0; opencalc_math_variable_at(i, &variable); i++) {
        int written = snprintf(s_variable_persist_buffer + used,
                               sizeof(s_variable_persist_buffer) - used,
                               "%s|%.17g|%.17g\n", variable.name, variable.real, variable.imag);
        if (written < 0 || (size_t)written >= sizeof(s_variable_persist_buffer) - used) break;
        used += (size_t)written;
    }
    opencalc_persist_set_string("vars_v1", s_variable_persist_buffer);
}

static void variables_load_all(void)
{
    opencalc_math_variables_reset();
    if (!opencalc_persist_get_string("vars_v1", s_variable_persist_buffer,
                                     sizeof(s_variable_persist_buffer))) return;
    char *save = NULL;
    for (char *line = strtok_r(s_variable_persist_buffer, "\n", &save);
         line != NULL; line = strtok_r(NULL, "\n", &save)) {
        char name[OPENCALC_VARIABLE_NAME_MAX];
        double real = 0.0, imag = 0.0;
        if (sscanf(line, "%15[^|]|%lf|%lf", name, &real, &imag) == 3) {
            opencalc_math_variable_set(name, real, imag);
        }
    }
}

static bool variable_store_into(const char *name)
{
    char expanded[CALC_EXPR_MAX + CALC_RESULT_MAX];
    calc_expand_ans_value(s_variable_store_expression, s_calc_ans, expanded, sizeof(expanded));
    double real = 0.0, imag = 0.0;
    if (!opencalc_math_eval_complex_expression(expanded, &real, &imag)) {
        snprintf(s_variable_status, sizeof(s_variable_status), "expression is not a storable value");
        return false;
    }
    if (!opencalc_math_variable_set(name, real, imag)) {
        snprintf(s_variable_status, sizeof(s_variable_status), "invalid name or variable storage full");
        return false;
    }
    variables_save_all();
    char value[40];
    variable_format_value(real, imag, value, sizeof(value));
    snprintf(s_calc_output, sizeof(s_calc_output), "%s -> %s", value, name);
    snprintf(s_calc_ans, sizeof(s_calc_ans), "%s", value);
    printf("stored variable %s=%s\n", name, value);
    return true;
}

static void variables_return_with_text(const char *text)
{
    page_id_t return_page = s_variable_return_page;
    s_page = return_page;
    s_current_app = return_page == PAGE_Y_EQUALS ? APP_GRAPH : APP_CALCULATOR;
    if (text != NULL && text[0] != '\0') expression_append(text);
    s_variable_status[0] = '\0';
    ui_draw_current();
}

static void variables_open_name_editor(const char *rename_from)
{
    s_variable_name[0] = '\0';
    snprintf(s_variable_rename_from, sizeof(s_variable_rename_from), "%s",
             rename_from != NULL ? rename_from : "");
    s_variable_status[0] = '\0';
    s_alpha_locked = true;
    s_alpha_active = true;
    s_page = PAGE_VARIABLE_NAME;
    ui_draw_current();
}

static void variables_activate_selected(void)
{
    variable_browser_item_t item;
    if (!variable_browser_item(s_variable_category, s_variable_selection, &item) || !item.selectable) {
        snprintf(s_variable_status, sizeof(s_variable_status), "nothing available to insert");
        ui_draw_current();
        return;
    }
    if (s_variable_action == VARIABLE_ACTION_STORE) {
        if (!item.user_variable) {
            snprintf(s_variable_status, sizeof(s_variable_status), "STO accepts user variables");
            ui_draw_current();
        } else if (item.name[0] == '+') {
            variables_open_name_editor(NULL);
        } else if (variable_store_into(item.name)) {
            variables_return_with_text(NULL);
        } else {
            ui_draw_current();
        }
        return;
    }
    const char *insert = s_variable_action == VARIABLE_ACTION_GET ? item.value : item.reference;
    if (insert[0] == '\0') {
        snprintf(s_variable_status, sizeof(s_variable_status), "value is not available");
        ui_draw_current();
        return;
    }
    char wrapped[CALC_EXPR_MAX];
    if (s_variable_action == VARIABLE_ACTION_GET &&
        (strchr(insert, '+') != NULL || (insert[0] == '-' && strchr(insert + 1, '-') != NULL))) {
        snprintf(wrapped, sizeof(wrapped), "(%s)", insert);
        insert = wrapped;
    }
    variables_return_with_text(insert);
}

static void variables_open(variable_action_t action)
{
    s_variable_return_page = expression_entry_active() ? s_page : PAGE_CALCULATOR;
    s_variable_action = action;
    s_variable_category = VARIABLE_CATEGORY_USER;
    s_variable_selection = 0;
    s_variable_scroll = 0;
    s_variable_status[0] = '\0';
    s_variable_rename_from[0] = '\0';
    if (action == VARIABLE_ACTION_STORE) {
        snprintf(s_variable_store_expression, sizeof(s_variable_store_expression), "%s",
                 s_calc_input[0] ? s_calc_input : s_calc_ans);
        if (s_variable_store_expression[0] == '\0') snprintf(s_variable_store_expression, sizeof(s_variable_store_expression), "0");
    }
    s_page = PAGE_VARIABLES;
    s_current_app = APP_CALCULATOR;
    ui_draw_current();
}

static bool variable_page_handle_key(int row, int col)
{
    if (s_page != PAGE_VARIABLES && s_page != PAGE_VARIABLE_NAME) return false;
    if (s_page == PAGE_VARIABLE_NAME) {
        if (row == 2 && col == 2) {
            s_page = PAGE_VARIABLES;
            s_variable_status[0] = '\0';
            ui_draw_current();
            return true;
        }
        if (row == 3 && col == 4) {
            size_t length = strlen(s_variable_name);
            if (s_second_active) s_variable_name[0] = '\0';
            else if (length > 0) s_variable_name[length - 1] = '\0';
            ui_draw_current();
            return true;
        }
        if (s_second_active && row == 9 && col == 2) {
            size_t length = strlen(s_variable_name);
            if (length > 0 && length + 1 < sizeof(s_variable_name)) {
                s_variable_name[length] = '_';
                s_variable_name[length + 1] = '\0';
            }
            ui_draw_current();
            return true;
        }
        if (row == 9 && col == 4) {
            if (!opencalc_math_variable_name_valid(s_variable_name)) {
                snprintf(s_variable_status, sizeof(s_variable_status), "invalid or reserved variable name");
            } else if (s_variable_rename_from[0]) {
                if (opencalc_math_variable_rename(s_variable_rename_from, s_variable_name)) {
                    variables_save_all();
                    s_page = PAGE_VARIABLES;
                    s_variable_status[0] = '\0';
                } else snprintf(s_variable_status, sizeof(s_variable_status), "name exists or rename failed");
            } else if (variable_store_into(s_variable_name)) {
                variables_return_with_text(NULL);
                return true;
            }
            ui_draw_current();
            return true;
        }
        const board_key_t *key = board_keypad_key_at(row, col);
        char converted[2];
        const char *alpha = alpha_key_text(key, converted);
        if ((s_alpha_active || s_alpha_locked) && alpha != NULL && strlen(alpha) == 1) {
            size_t length = strlen(s_variable_name);
            if (length + 1 < sizeof(s_variable_name)) {
                s_variable_name[length] = alpha[0];
                s_variable_name[length + 1] = '\0';
            }
            if (!s_alpha_locked) s_alpha_active = false;
            ui_draw_current();
            return true;
        }
        int digit = digit_for_key(row, col);
        if (digit >= 0) {
            size_t length = strlen(s_variable_name);
            if (length > 0 && length + 1 < sizeof(s_variable_name)) {
                s_variable_name[length] = (char)('0' + digit);
                s_variable_name[length + 1] = '\0';
            }
            ui_draw_current();
        }
        return true;
    }

    int count = variable_category_count(s_variable_category);
    if (row == 1 && col == 3) {
        s_variable_category = (variable_category_t)((s_variable_category + VARIABLE_CATEGORY_COUNT - 1) % VARIABLE_CATEGORY_COUNT);
        s_variable_selection = s_variable_scroll = 0;
    } else if (row == 2 && col == 4) {
        s_variable_category = (variable_category_t)((s_variable_category + 1) % VARIABLE_CATEGORY_COUNT);
        s_variable_selection = s_variable_scroll = 0;
    } else if (row == 1 && col == 4 && s_variable_selection > 0) {
        s_variable_selection--;
    } else if (row == 2 && col == 3 && s_variable_selection + 1 < count) {
        s_variable_selection++;
    } else if (row == 2 && col == 2) {
        variables_return_with_text(NULL);
        return true;
    } else if (row == 9 && col == 4) {
        if (s_second_active && s_variable_category == VARIABLE_CATEGORY_USER) {
            variable_browser_item_t item;
            if (variable_browser_item(s_variable_category, s_variable_selection, &item) &&
                item.user_variable && item.name[0] != '+' &&
                opencalc_math_variable_get(item.name, NULL, NULL)) {
                variables_open_name_editor(item.name);
                return true;
            }
            snprintf(s_variable_status, sizeof(s_variable_status), "select a stored user variable");
        } else variables_activate_selected();
    } else if (row == 3 && col == 4 && s_variable_category == VARIABLE_CATEGORY_USER) {
        variable_browser_item_t item;
        if (variable_browser_item(s_variable_category, s_variable_selection, &item) &&
            item.user_variable && item.name[0] != '+' && opencalc_math_variable_delete(item.name)) {
            variables_save_all();
            snprintf(s_variable_status, sizeof(s_variable_status), "%s deleted", item.name);
            count = variable_category_count(s_variable_category);
            if (s_variable_selection >= count) s_variable_selection = count - 1;
        } else snprintf(s_variable_status, sizeof(s_variable_status), "variable is already empty");
    }
    ui_draw_current();
    return true;
}

static int script_input_callback(char *buffer, size_t buffer_size, void *user_data)
{
    (void)user_data;
    if (buffer == NULL || buffer_size == 0) {
        return 0;
    }

    printf("Python worker: waiting for input, stack free=%u bytes\n",
           (unsigned)uxTaskGetStackHighWaterMark(NULL));

    s_script_input_active = true;
    s_script_input[0] = '\0';
    s_script_input_len = 0;
    s_script_input_submitted = false;
    s_script_input_cancelled = false;
    s_cursor_blink_visible = true;
    s_cursor_blink_last_visible = true;
    snprintf(s_script_status, sizeof(s_script_status), "type input, enter - submit");
    s_script_screen_dirty = true;

    while (!s_script_input_submitted && !s_script_input_cancelled) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
        if (s_script_stop_requested || s_script_py.abort_requested) {
            s_script_input_cancelled = true;
        }
    }

    s_script_input_active = false;
    if (s_script_input_cancelled) {
        snprintf(s_script_status, sizeof(s_script_status), "input cancelled");
        s_script_screen_dirty = true;
        return 0;
    }
    snprintf(buffer, buffer_size, "%s", s_script_input);
    script_output_append(s_script_input);
    script_output_append("\n");
    snprintf(s_script_status, sizeof(s_script_status), "running...");
    s_script_screen_dirty = true;
    return 1;
}

static void ui_draw_script_io(void)
{
    size_t graphics_count = 0;
    bool graphics_active = false;
    script_view_t view = s_script_view;
    if (s_script_output_mutex != NULL) {
        xSemaphoreTake(s_script_output_mutex, portMAX_DELAY);
    }
    memcpy(s_script_output_snapshot, s_script_output, sizeof(s_script_output_snapshot));
    const char *diagnostic = view == SCRIPT_VIEW_VARIABLES ? s_script_variables :
                             view == SCRIPT_VIEW_TRACEBACK ? s_script_traceback : "";
    snprintf(s_script_diagnostic_snapshot, sizeof(s_script_diagnostic_snapshot), "%s", diagnostic);
    graphics_count = s_script_graphics_count;
    graphics_active = s_script_graphics_active;
    if (graphics_count > SCRIPT_GRAPHICS_MAX) graphics_count = SCRIPT_GRAPHICS_MAX;
    memcpy(s_script_graphics_snapshot, s_script_graphics,
           graphics_count * sizeof(s_script_graphics_snapshot[0]));
    if (s_script_output_mutex != NULL) {
        xSemaphoreGive(s_script_output_mutex);
    }

    ui_clear(THEME_BG);
    if (view == SCRIPT_VIEW_CONSOLE && graphics_active) {
        for (size_t i = 0; i < graphics_count; i++) {
            const script_graphics_command_t *command = &s_script_graphics_snapshot[i];
            switch (command->kind) {
            case SCRIPT_GFX_CLEAR: ui_clear(command->color); break;
            case SCRIPT_GFX_PIXEL: ui_rect(command->x0, command->y0, 1, 1, command->color); break;
            case SCRIPT_GFX_LINE: ui_line(command->x0, command->y0, command->x1, command->y1, command->color); break;
            case SCRIPT_GFX_RECT: ui_rect(command->x0, command->y0, command->x1, command->y1, command->color); break;
            case SCRIPT_GFX_TEXT: ui_text(command->x0, command->y0, command->text, command->color, 1); break;
            }
        }
    }
    ui_header(&APPS[APP_PYTHON]);

    const char *view_name = view == SCRIPT_VIEW_VARIABLES ? "Variables" :
                            view == SCRIPT_VIEW_TRACEBACK ? "Traceback" :
                            view == SCRIPT_VIEW_PROFILE ? "Profile" : s_script_title;
    ui_text(10, 32, view_name, THEME_ACCENT, 1);
    ui_rect(8, 46, UI_W - 16, 1, THEME_BORDER);

    char lines[13][48];
    int line_count = 1;
    int col = 0;
    memset(lines, 0, sizeof(lines));
    char profile[192];
    if (view == SCRIPT_VIEW_PROFILE) {
        snprintf(profile, sizeof(profile),
                 "Statements: %lu\nFunction calls: %lu\nMax call depth: %lu\nElapsed: %lu ms\nStack free: see USB log",
                 s_script_profile.statements, s_script_profile.function_calls,
                 s_script_profile.max_call_depth, (unsigned long)s_script_elapsed_ms);
        snprintf(s_script_diagnostic_snapshot, sizeof(s_script_diagnostic_snapshot), "%s", profile);
    }
    const char *body = view == SCRIPT_VIEW_CONSOLE ? s_script_output_snapshot : s_script_diagnostic_snapshot;
    if (view != SCRIPT_VIEW_CONSOLE && body[0] == '\0') body = "No data available yet";
    for (size_t i = 0; body[i] != '\0'; i++) {
        char c = body[i];
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

    if (view == SCRIPT_VIEW_CONSOLE && s_script_input_active) {
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
    if (cursor_line >= s_script_editor_scroll_line + SCRIPT_EDITOR_VISIBLE_LINES) {
        s_script_editor_scroll_line = cursor_line - (SCRIPT_EDITOR_VISIBLE_LINES - 1);
    }
    if (cursor_col < s_script_editor_scroll_col) {
        s_script_editor_scroll_col = cursor_col;
    }
    if (cursor_col >= s_script_editor_scroll_col + SCRIPT_EDITOR_VISIBLE_COLS) {
        s_script_editor_scroll_col = cursor_col - (SCRIPT_EDITOR_VISIBLE_COLS - 1);
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

    char lines[SCRIPT_EDITOR_VISIBLE_LINES][SCRIPT_EDITOR_VISIBLE_COLS + 1];
    memset(lines, 0, sizeof(lines));
    for (int row = 0; row < SCRIPT_EDITOR_VISIBLE_LINES; row++) {
        int source_col = 0;
        int visible_col = 0;
        while (index < s_script_editor_len && s_script_editor[index] != '\n') {
            if (source_col >= s_script_editor_scroll_col &&
                visible_col < SCRIPT_EDITOR_VISIBLE_COLS) {
                lines[row][visible_col++] = s_script_editor[index];
            }
            source_col++;
            index++;
        }
        if (index < s_script_editor_len && s_script_editor[index] == '\n') {
            index++;
        }
    }

    for (int row = 0; row < SCRIPT_EDITOR_VISIBLE_LINES; row++) {
        int source_line = s_script_editor_scroll_line + row + 1;
        if (source_line < SCRIPT_BREAKPOINT_MAX && s_script_breakpoints[source_line]) {
            ui_rect(8, 57 + row * 12, 4, 4, THEME_BATTERY_RED);
        }
        ui_text(16, 54 + row * 12, lines[row], THEME_TEXT, 1);
    }

    int visible_line = cursor_line - s_script_editor_scroll_line;
    int visible_col = cursor_col - s_script_editor_scroll_col;
    if (visible_line >= 0 && visible_line < SCRIPT_EDITOR_VISIBLE_LINES &&
        visible_col >= 0 && visible_col < SCRIPT_EDITOR_VISIBLE_COLS) {
        ui_draw_script_cursor(16 + visible_col * 6, 54 + visible_line * 12);
    }

    ui_rect(0, 220, UI_W, 20, THEME_HEADER);
    ui_text(8, 226, "Trace-breakpoint  2nd Enter-save", THEME_MUTED, 1);
    if (s_script_status[0] != '\0') {
        ui_text(10, 208, s_script_status, THEME_MUTED, 1);
    }
    ui_present();
}

static bool script_editor_handle_key(int row, int col)
{
    s_cursor_blink_visible = true;
    s_cursor_blink_last_visible = true;

    if (row == 2 && col == 2) {
        scripts_scan();
        s_page = PAGE_SCRIPTS;
        s_current_app = APP_PYTHON;
        ui_draw_current();
        return true;
    }
    if (row == 0 && col == 3) {
        int line = 0;
        script_editor_cursor_line_col(&line, NULL);
        line++;
        if (line < SCRIPT_BREAKPOINT_MAX) {
            s_script_breakpoints[line] = !s_script_breakpoints[line];
            snprintf(s_script_status, sizeof(s_script_status), "%s breakpoint at line %d",
                     s_script_breakpoints[line] ? "set" : "cleared", line);
        }
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
    char converted[2];
    const char *alpha_text = alpha_key_text(key, converted);
    if ((s_alpha_active || s_alpha_locked) && alpha_text != NULL) {
        script_editor_insert_text(alpha_text);
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

static bool script_io_handle_key(int row, int col)
{
    if (s_page != PAGE_SCRIPT_IO || s_script_input_active) return false;

    if (row == 0 && col == 4) {
        s_script_view = (script_view_t)((s_script_view + 1) % SCRIPT_VIEW_COUNT);
        ui_draw_current();
        return true;
    }
    if (!s_script_running) return false;
    if ((row == 2 && col == 2) || (row == 3 && col == 4)) {
        s_script_stop_requested = true;
        py_request_abort(&s_script_py);
        s_script_debug_resume = 2;
        s_script_input_cancelled = true;
        if (s_script_task != NULL) xTaskNotifyGive(s_script_task);
        snprintf(s_script_status, sizeof(s_script_status), "stopping safely...");
        ui_draw_current();
        return true;
    }
    if (s_script_debug_paused && row == 9 && col == 4) {
        s_script_debug_resume = 1;
        if (s_script_task != NULL) xTaskNotifyGive(s_script_task);
        return true;
    }
    if (s_script_debug_paused && row == 0 && col == 3) {
        s_script_debug_resume = 2;
        if (s_script_task != NULL) xTaskNotifyGive(s_script_task);
        return true;
    }
    return true;
}

static void worksheet_capture_fixed(worksheet_fixed_t *fixed)
{
    memset(fixed, 0, sizeof(*fixed));
    fixed->fixed_size = (uint32_t)sizeof(*fixed);
    memcpy(fixed->calc_input, s_calc_input, sizeof(fixed->calc_input));
    memcpy(fixed->calc_output, s_calc_output, sizeof(fixed->calc_output));
    memcpy(fixed->calc_ans, s_calc_ans, sizeof(fixed->calc_ans));
    int history_count = 0;
    opencalc_calc_history_export(&history_count, fixed->calc_history_expr,
                                 fixed->calc_history_result);
    fixed->calc_history_count = history_count;

    memcpy(fixed->graph_exprs, s_graph_exprs, sizeof(fixed->graph_exprs));
    memcpy(fixed->graph_param_x, s_graph_param_x, sizeof(fixed->graph_param_x));
    memcpy(fixed->graph_param_y, s_graph_param_y, sizeof(fixed->graph_param_y));
    memcpy(fixed->graph_polar_exprs, s_graph_polar_exprs, sizeof(fixed->graph_polar_exprs));
    memcpy(fixed->graph_seq_exprs, s_graph_seq_exprs, sizeof(fixed->graph_seq_exprs));
    for (int i = 0; i < GRAPH_FUNC_COUNT; i++) fixed->graph_enabled[i] = s_graph_enabled[i];
    for (int i = 0; i < GRAPH_PARAM_COUNT; i++) fixed->graph_param_enabled[i] = s_graph_param_enabled[i];
    for (int i = 0; i < GRAPH_POLAR_COUNT; i++) fixed->graph_polar_enabled[i] = s_graph_polar_enabled[i];
    for (int i = 0; i < GRAPH_SEQ_COUNT; i++) fixed->graph_seq_enabled[i] = s_graph_seq_enabled[i];
    fixed->graph_tmin = s_graph_tmin; fixed->graph_tmax = s_graph_tmax;
    fixed->graph_nmin = s_graph_nmin; fixed->graph_nmax = s_graph_nmax;
    fixed->graph_xmin = s_graph_xmin; fixed->graph_xmax = s_graph_xmax;
    fixed->graph_ymin = s_graph_ymin; fixed->graph_ymax = s_graph_ymax;
    fixed->graph_xtick = s_graph_xtick; fixed->graph_ytick = s_graph_ytick;
    fixed->table_x_start = s_table_x_start; fixed->table_step = s_table_step;
    fixed->graph_selection = s_graph_selection;
    fixed->graph_window_selection = s_graph_window_selection;
    fixed->graph_calc_selection = s_graph_calc_selection;
    fixed->graph_format_selection = s_graph_format_selection;
    fixed->graph_style_series = s_graph_style_series;
    memcpy(fixed->graph_styles, s_graph_styles, sizeof(fixed->graph_styles));
    fixed->graph_split = s_graph_split;
    fixed->graph_background_enabled = s_graph_background_enabled;
    fixed->graph_grid = s_graph_grid;
    fixed->table_func_start = s_table_func_start;
    fixed->table_rows = s_table_rows;
    fixed->table_precision = s_table_precision;
    fixed->table_setup_selection = s_table_setup_selection;
    fixed->display_format = s_display_format;
    fixed->print_mode = s_print_mode;
    fixed->angle_mode = s_angle_mode;
    fixed->graphing_mode = s_graphing_mode;
    fixed->complex_mode = s_complex_mode;
    fixed->list_index = s_list_index;
    fixed->matrix_index = s_matrix_index;

    fixed->fin_n = s_fin_n; fixed->fin_i = s_fin_i; fixed->fin_pv = s_fin_pv;
    fixed->fin_pmt = s_fin_pmt; fixed->fin_fv = s_fin_fv;
    fixed->fin_py = s_fin_py; fixed->fin_cy = s_fin_cy;
    fixed->fin_begin = s_fin_begin;
    fixed->fin_selection = s_fin_selection;
    fixed->fin_cash_cursor = s_fin_cash_cursor;
    fixed->fin_cash_scroll = s_fin_cash_scroll;

    fixed->conic_type = s_conic_type;
    fixed->conic_h = s_conic_h; fixed->conic_k = s_conic_k;
    fixed->conic_a = s_conic_a; fixed->conic_b = s_conic_b;
    fixed->conic_r = s_conic_r; fixed->conic_angle = s_conic_angle;
    memcpy(fixed->conic_general, s_conic_general, sizeof(fixed->conic_general));
    fixed->conic_model = s_conic_model;
    memcpy(fixed->conic_overlays, s_conic_overlays, sizeof(fixed->conic_overlays));
    fixed->conic_overlay_count = s_conic_overlay_count;
    fixed->conic_overlay_selection = s_conic_overlay_selection;
    fixed->conic_selection = s_conic_selection;
}

static bool worksheet_fixed_valid(worksheet_fixed_t *fixed)
{
    if (fixed->fixed_size != (uint32_t)sizeof(*fixed) ||
        fixed->calc_history_count < 0 || fixed->calc_history_count > CALC_HISTORY_MAX ||
        fixed->graph_xmin >= fixed->graph_xmax || fixed->graph_ymin >= fixed->graph_ymax ||
        fixed->graph_tmin >= fixed->graph_tmax || fixed->graph_nmin > fixed->graph_nmax ||
        !isfinite(fixed->graph_tmin) || !isfinite(fixed->graph_tmax) ||
        !isfinite(fixed->graph_nmin) || !isfinite(fixed->graph_nmax) ||
        !isfinite(fixed->graph_xmin) || !isfinite(fixed->graph_xmax) ||
        !isfinite(fixed->graph_ymin) || !isfinite(fixed->graph_ymax) ||
        !isfinite(fixed->graph_xtick) || fixed->graph_xtick <= 0.0 ||
        !isfinite(fixed->graph_ytick) || fixed->graph_ytick <= 0.0 ||
        !isfinite(fixed->table_x_start) || !isfinite(fixed->table_step) || fixed->table_step == 0.0 ||
        fixed->graph_selection < 0 || fixed->graph_selection >= GRAPH_FUNC_COUNT ||
        fixed->graph_window_selection < 0 || fixed->graph_window_selection > 3 ||
        fixed->graph_calc_selection < 0 || fixed->graph_calc_selection >= GRAPH_CALC_COUNT ||
        fixed->graph_format_selection < 0 || fixed->graph_format_selection >= GRAPH_FORMAT_COUNT ||
        fixed->graph_style_series < 0 || fixed->graph_style_series >= GRAPH_FUNC_COUNT ||
        fixed->table_func_start < 0 || fixed->table_func_start >= GRAPH_FUNC_COUNT ||
        fixed->table_rows < 4 || fixed->table_rows > 10 ||
        fixed->table_precision < 0 || fixed->table_precision > 6 ||
        fixed->table_setup_selection < 0 || fixed->table_setup_selection > 3 ||
        fixed->display_format < 0 || fixed->display_format >= 3 ||
        fixed->print_mode < 0 || fixed->print_mode >= 2 ||
        fixed->angle_mode < 0 || fixed->angle_mode >= 2 ||
        fixed->graphing_mode < 0 || fixed->graphing_mode >= 4 ||
        fixed->complex_mode < 0 || fixed->complex_mode >= 3 ||
        fixed->list_index < 0 || fixed->list_index >= LIST_COUNT ||
        fixed->matrix_index < 0 || fixed->matrix_index >= MATRIX_COUNT ||
        !isfinite(fixed->fin_n) || !isfinite(fixed->fin_i) || !isfinite(fixed->fin_pv) ||
        !isfinite(fixed->fin_pmt) || !isfinite(fixed->fin_fv) ||
        !isfinite(fixed->fin_py) || !isfinite(fixed->fin_cy) ||
        fixed->fin_py <= 0.0 || fixed->fin_cy <= 0.0 ||
        fixed->fin_selection < 0 || fixed->fin_selection >= 7 ||
        fixed->conic_type < OPENCALC_CONIC_CIRCLE || fixed->conic_type > OPENCALC_CONIC_GENERAL ||
        !isfinite(fixed->conic_h) || !isfinite(fixed->conic_k) ||
        !isfinite(fixed->conic_a) || !isfinite(fixed->conic_b) ||
        !isfinite(fixed->conic_r) || !isfinite(fixed->conic_angle) ||
        fixed->conic_overlay_count < 0 || fixed->conic_overlay_count > 4 ||
        fixed->conic_overlay_selection < 0 || fixed->conic_overlay_selection > 3 ||
        fixed->conic_selection < 0 || fixed->conic_selection > 24) {
        return false;
    }

    for (int i = 0; i < 6; i++) {
        if (!isfinite(fixed->conic_general[i])) return false;
    }

    fixed->calc_input[CALC_EXPR_MAX - 1] = '\0';
    fixed->calc_output[CALC_RESULT_MAX - 1] = '\0';
    fixed->calc_ans[CALC_RESULT_MAX - 1] = '\0';
    for (int i = 0; i < CALC_HISTORY_MAX; i++) {
        fixed->calc_history_expr[i][CALC_EXPR_MAX - 1] = '\0';
        fixed->calc_history_result[i][CALC_RESULT_MAX - 1] = '\0';
    }
    for (int i = 0; i < GRAPH_FUNC_COUNT; i++) {
        fixed->graph_exprs[i][95] = '\0';
        if (fixed->graph_styles[i] >= GRAPH_STYLE_COUNT) fixed->graph_styles[i] = GRAPH_STYLE_LINE;
    }
    for (int i = 0; i < GRAPH_PARAM_COUNT; i++) {
        fixed->graph_param_x[i][95] = '\0';
        fixed->graph_param_y[i][95] = '\0';
    }
    for (int i = 0; i < GRAPH_POLAR_COUNT; i++) fixed->graph_polar_exprs[i][95] = '\0';
    for (int i = 0; i < GRAPH_SEQ_COUNT; i++) fixed->graph_seq_exprs[i][95] = '\0';
    return true;
}

static void worksheet_apply_fixed(const worksheet_fixed_t *fixed)
{
    memcpy(s_calc_input, fixed->calc_input, sizeof(s_calc_input));
    s_calc_cursor = strlen(s_calc_input);
    memcpy(s_calc_output, fixed->calc_output, sizeof(s_calc_output));
    memcpy(s_calc_ans, fixed->calc_ans, sizeof(s_calc_ans));
    (void)opencalc_calc_history_import(fixed->calc_history_count,
                                       fixed->calc_history_expr,
                                       fixed->calc_history_result);

    memcpy(s_graph_exprs, fixed->graph_exprs, sizeof(s_graph_exprs));
    memcpy(s_graph_param_x, fixed->graph_param_x, sizeof(s_graph_param_x));
    memcpy(s_graph_param_y, fixed->graph_param_y, sizeof(s_graph_param_y));
    memcpy(s_graph_polar_exprs, fixed->graph_polar_exprs, sizeof(s_graph_polar_exprs));
    memcpy(s_graph_seq_exprs, fixed->graph_seq_exprs, sizeof(s_graph_seq_exprs));
    for (int i = 0; i < GRAPH_FUNC_COUNT; i++) s_graph_enabled[i] = fixed->graph_enabled[i] != 0;
    for (int i = 0; i < GRAPH_PARAM_COUNT; i++) s_graph_param_enabled[i] = fixed->graph_param_enabled[i] != 0;
    for (int i = 0; i < GRAPH_POLAR_COUNT; i++) s_graph_polar_enabled[i] = fixed->graph_polar_enabled[i] != 0;
    for (int i = 0; i < GRAPH_SEQ_COUNT; i++) s_graph_seq_enabled[i] = fixed->graph_seq_enabled[i] != 0;
    s_graph_tmin = fixed->graph_tmin; s_graph_tmax = fixed->graph_tmax;
    s_graph_nmin = fixed->graph_nmin; s_graph_nmax = fixed->graph_nmax;
    s_graph_xmin = fixed->graph_xmin; s_graph_xmax = fixed->graph_xmax;
    s_graph_ymin = fixed->graph_ymin; s_graph_ymax = fixed->graph_ymax;
    s_graph_xtick = fixed->graph_xtick; s_graph_ytick = fixed->graph_ytick;
    s_table_x_start = fixed->table_x_start; s_table_step = fixed->table_step;
    s_graph_selection = fixed->graph_selection;
    s_graph_window_selection = fixed->graph_window_selection;
    s_graph_calc_selection = fixed->graph_calc_selection;
    s_graph_format_selection = fixed->graph_format_selection;
    s_graph_style_series = fixed->graph_style_series;
    memcpy(s_graph_styles, fixed->graph_styles, sizeof(s_graph_styles));
    s_graph_split = fixed->graph_split != 0;
    s_graph_background_enabled = fixed->graph_background_enabled != 0;
    s_graph_grid = fixed->graph_grid != 0;
    s_table_func_start = fixed->table_func_start;
    s_table_rows = fixed->table_rows;
    s_table_precision = fixed->table_precision;
    s_table_setup_selection = fixed->table_setup_selection;
    s_display_format = fixed->display_format;
    s_print_mode = fixed->print_mode;
    s_angle_mode = fixed->angle_mode == 0 ? 0 : 1;
    s_graphing_mode = fixed->graphing_mode >= 0 && fixed->graphing_mode <= 3 ? fixed->graphing_mode : 0;
    s_complex_mode = fixed->complex_mode;
    s_list_index = fixed->list_index;
    s_matrix_index = fixed->matrix_index;

    s_fin_n = fixed->fin_n; s_fin_i = fixed->fin_i; s_fin_pv = fixed->fin_pv;
    s_fin_pmt = fixed->fin_pmt; s_fin_fv = fixed->fin_fv;
    s_fin_py = fixed->fin_py; s_fin_cy = fixed->fin_cy;
    s_fin_begin = fixed->fin_begin != 0;
    s_fin_selection = fixed->fin_selection;
    s_fin_cash_cursor = fixed->fin_cash_cursor;
    s_fin_cash_scroll = fixed->fin_cash_scroll;

    s_conic_type = fixed->conic_type;
    s_conic_h = fixed->conic_h; s_conic_k = fixed->conic_k;
    s_conic_a = fixed->conic_a; s_conic_b = fixed->conic_b;
    s_conic_r = fixed->conic_r; s_conic_angle = fixed->conic_angle;
    memcpy(s_conic_general, fixed->conic_general, sizeof(s_conic_general));
    s_conic_model = fixed->conic_model;
    memcpy(s_conic_overlays, fixed->conic_overlays, sizeof(s_conic_overlays));
    s_conic_overlay_count = fixed->conic_overlay_count;
    s_conic_overlay_selection = fixed->conic_overlay_selection;
    s_conic_selection = fixed->conic_selection;
    opencalc_math_set_degrees(s_angle_mode == 0);
}

static bool worksheet_persist_save_path(const char *path)
{
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (sizeof(worksheet_fixed_t) > psram_free ||
        psram_free - sizeof(worksheet_fixed_t) < OPENCALC_PSRAM_RESERVE_BYTES) {
        return false;
    }
    worksheet_fixed_t *fixed = heap_caps_calloc(1, sizeof(*fixed), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (fixed == NULL) return false;
    worksheet_capture_fixed(fixed);

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        heap_caps_free(fixed);
        return false;
    }
    worksheet_header_t header = {.magic = WORKSHEET_MAGIC, .version = WORKSHEET_VERSION};
    bool ok = fwrite(&header, 1, sizeof(header), file) == sizeof(header);
    uint32_t crc = UINT32_MAX;
    uint32_t payload_size = 0;
    if (ok) ok = worksheet_write_payload(file, fixed, sizeof(*fixed), &crc, &payload_size);
    bool lists_locked = ok && script_lists_lock(0);
    ok = ok && lists_locked;
    for (int list = 0; ok && list < LIST_COUNT; list++) {
        uint32_t count = (uint32_t)s_list_counts[list];
        ok = count <= LIST_MAX_VALUES &&
            worksheet_write_payload(file, &count, sizeof(count), &crc, &payload_size) &&
            worksheet_write_payload(file, s_lists[list], count * sizeof(double), &crc, &payload_size);
    }
    if (lists_locked) script_lists_unlock();
    for (int matrix = 0; ok && matrix < MATRIX_COUNT; matrix++) {
        uint32_t rows = (uint32_t)s_matrix_rows_by_index[matrix];
        uint32_t cols = (uint32_t)s_matrix_cols_by_index[matrix];
        ok = rows <= MATRIX_MAX_N && cols <= MATRIX_MAX_N &&
            worksheet_write_payload(file, &rows, sizeof(rows), &crc, &payload_size) &&
            worksheet_write_payload(file, &cols, sizeof(cols), &crc, &payload_size);
        for (uint32_t row = 0; ok && row < rows; row++) {
            ok = worksheet_write_payload(file, s_matrices[matrix][row],
                                         cols * sizeof(double), &crc, &payload_size);
        }
    }
    header.payload_size = payload_size;
    header.payload_crc32 = crc ^ UINT32_MAX;
    if (ok) ok = fflush(file) == 0 && fseek(file, 0, SEEK_SET) == 0 &&
        fwrite(&header, 1, sizeof(header), file) == sizeof(header) && fflush(file) == 0;
    if (fclose(file) != 0) ok = false;
    heap_caps_free(fixed);
    return ok;
}

static bool worksheet_persist_load_path(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;
    worksheet_header_t header = {0};
    bool legacy_v1 = false;
    bool ok = fread(&header, 1, sizeof(header), file) == sizeof(header) &&
        header.magic == WORKSHEET_MAGIC &&
        (header.version == WORKSHEET_VERSION || header.version == 1u);
    if (ok) {
        legacy_v1 = header.version == 1u;
        size_t minimum_size = legacy_v1 ? sizeof(worksheet_fixed_v1_t) : sizeof(worksheet_fixed_t);
        ok = header.payload_size >= minimum_size && header.payload_size <= 2u * 1024u * 1024u;
    }
    uint8_t crc_buffer[256];
    uint32_t crc = UINT32_MAX;
    uint32_t crc_remaining = ok ? header.payload_size : 0;
    while (ok && crc_remaining > 0) {
        size_t chunk = crc_remaining < sizeof(crc_buffer) ? crc_remaining : sizeof(crc_buffer);
        if (fread(crc_buffer, 1, chunk, file) != chunk) {
            ok = false;
            break;
        }
        crc = worksheet_crc32_update(crc, crc_buffer, chunk);
        crc_remaining -= (uint32_t)chunk;
    }
    if (ok) ok = (crc ^ UINT32_MAX) == header.payload_crc32 &&
        fseek(file, sizeof(header), SEEK_SET) == 0;

    worksheet_fixed_t *fixed = NULL;
    worksheet_fixed_v1_t *legacy_fixed = NULL;
    double *values = NULL;
    uint32_t list_counts[LIST_COUNT] = {0};
    uint32_t matrix_rows[MATRIX_COUNT] = {0};
    uint32_t matrix_cols[MATRIX_COUNT] = {0};
    size_t value_count = 0;
    const size_t value_capacity = (size_t)LIST_COUNT * LIST_MAX_VALUES +
        (size_t)MATRIX_COUNT * MATRIX_MAX_N * MATRIX_MAX_N;
    if (ok) {
        size_t required = sizeof(*fixed) + value_capacity * sizeof(double);
        if (legacy_v1) required += sizeof(*legacy_fixed);
        size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        ok = required <= psram_free &&
             psram_free - required >= OPENCALC_PSRAM_RESERVE_BYTES;
    }
    if (ok) {
        fixed = heap_caps_calloc(1, sizeof(*fixed), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (legacy_v1) {
            legacy_fixed = heap_caps_calloc(1, sizeof(*legacy_fixed),
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        }
        values = heap_caps_malloc(value_capacity * sizeof(double), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        ok = fixed != NULL && values != NULL && (!legacy_v1 || legacy_fixed != NULL);
    }
    uint32_t remaining = header.payload_size;
    if (ok && legacy_v1) {
        ok = worksheet_read_payload(file, legacy_fixed, sizeof(*legacy_fixed), &remaining) &&
            legacy_fixed->fixed_size == sizeof(*legacy_fixed);
        if (ok) worksheet_migrate_v1(legacy_fixed, fixed);
    } else if (ok) {
        ok = worksheet_read_payload(file, fixed, sizeof(*fixed), &remaining);
    }
    if (ok) ok = worksheet_fixed_valid(fixed);
    for (int list = 0; ok && list < LIST_COUNT; list++) {
        ok = worksheet_read_payload(file, &list_counts[list], sizeof(list_counts[list]), &remaining) &&
            list_counts[list] <= LIST_MAX_VALUES && value_count + list_counts[list] <= value_capacity &&
            worksheet_read_payload(file, values + value_count,
                                   list_counts[list] * sizeof(double), &remaining);
        value_count += ok ? list_counts[list] : 0;
    }
    for (int matrix = 0; ok && matrix < MATRIX_COUNT; matrix++) {
        ok = worksheet_read_payload(file, &matrix_rows[matrix], sizeof(matrix_rows[matrix]), &remaining) &&
            worksheet_read_payload(file, &matrix_cols[matrix], sizeof(matrix_cols[matrix]), &remaining) &&
            matrix_rows[matrix] <= MATRIX_MAX_N && matrix_cols[matrix] <= MATRIX_MAX_N;
        size_t cells = (size_t)matrix_rows[matrix] * matrix_cols[matrix];
        ok = ok && value_count + cells <= value_capacity &&
            worksheet_read_payload(file, values + value_count, cells * sizeof(double), &remaining);
        value_count += ok ? cells : 0;
    }
    for (size_t i = 0; ok && i < value_count; i++) {
        if (!isfinite(values[i])) ok = false;
    }
    ok = ok && remaining == 0;

    if (ok) {
        worksheet_apply_fixed(fixed);
        memset(s_lists, 0, sizeof(s_lists));
        memset(s_matrices, 0, sizeof(s_matrices));
        size_t offset = 0;
        for (int list = 0; list < LIST_COUNT; list++) {
            s_list_counts[list] = (int)list_counts[list];
            memcpy(s_lists[list], values + offset, list_counts[list] * sizeof(double));
            offset += list_counts[list];
        }
        for (int matrix = 0; matrix < MATRIX_COUNT; matrix++) {
            s_matrix_rows_by_index[matrix] = (int)matrix_rows[matrix];
            s_matrix_cols_by_index[matrix] = (int)matrix_cols[matrix];
            for (uint32_t row = 0; row < matrix_rows[matrix]; row++) {
                memcpy(s_matrices[matrix][row], values + offset,
                       matrix_cols[matrix] * sizeof(double));
                offset += matrix_cols[matrix];
            }
        }
    }
    if (fixed != NULL) heap_caps_free(fixed);
    if (legacy_fixed != NULL) heap_caps_free(legacy_fixed);
    if (values != NULL) heap_caps_free(values);
    fclose(file);
    return ok;
}

static bool worksheet_journal_append_section(uint16_t section)
{
    FILE *file = fopen(WORKSHEET_JOURNAL_PATH, "rb+");
    if (file == NULL) file = fopen(WORKSHEET_JOURNAL_PATH, "wb+");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
        if (file != NULL) fclose(file);
        return false;
    }

    long header_offset = ftell(file);
    worksheet_journal_header_t header = {
        .magic = WORKSHEET_JOURNAL_MAGIC,
        .version = WORKSHEET_JOURNAL_VERSION,
        .section = section,
    };
    bool ok = header_offset >= 0 &&
        fwrite(&header, 1, sizeof(header), file) == sizeof(header);
    uint32_t crc = UINT32_MAX;
    uint32_t payload_size = 0;
    worksheet_fixed_t *fixed = NULL;

    if (ok && section == WORKSHEET_DIRTY_FIXED) {
        fixed = heap_caps_calloc(1, sizeof(*fixed), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        ok = fixed != NULL;
        if (ok) {
            worksheet_capture_fixed(fixed);
            ok = worksheet_write_payload(file, fixed, sizeof(*fixed), &crc, &payload_size);
        }
    } else if (ok && section == WORKSHEET_DIRTY_LISTS) {
        bool lists_locked = script_lists_lock(0);
        ok = lists_locked;
        for (int list = 0; ok && list < LIST_COUNT; list++) {
            uint32_t count = (uint32_t)s_list_counts[list];
            ok = count <= LIST_MAX_VALUES &&
                worksheet_write_payload(file, &count, sizeof(count), &crc, &payload_size) &&
                worksheet_write_payload(file, s_lists[list], count * sizeof(double),
                                        &crc, &payload_size);
        }
        if (lists_locked) script_lists_unlock();
    } else if (ok && section == WORKSHEET_DIRTY_MATRICES) {
        for (int matrix = 0; ok && matrix < MATRIX_COUNT; matrix++) {
            uint32_t rows = (uint32_t)s_matrix_rows_by_index[matrix];
            uint32_t cols = (uint32_t)s_matrix_cols_by_index[matrix];
            ok = rows <= MATRIX_MAX_N && cols <= MATRIX_MAX_N &&
                worksheet_write_payload(file, &rows, sizeof(rows), &crc, &payload_size) &&
                worksheet_write_payload(file, &cols, sizeof(cols), &crc, &payload_size);
            for (uint32_t row = 0; ok && row < rows; row++) {
                ok = worksheet_write_payload(file, s_matrices[matrix][row],
                                             cols * sizeof(double), &crc, &payload_size);
            }
        }
    } else if (ok) {
        ok = false;
    }

    if (fixed != NULL) heap_caps_free(fixed);
    header.payload_size = payload_size;
    header.payload_crc32 = crc ^ UINT32_MAX;
    long end_offset = ok ? ftell(file) : -1;
    if (ok) {
        ok = end_offset >= 0 && fseek(file, header_offset, SEEK_SET) == 0 &&
            fwrite(&header, 1, sizeof(header), file) == sizeof(header) &&
            fflush(file) == 0 && fseek(file, end_offset, SEEK_SET) == 0;
    }
    if (!ok && header_offset >= 0) {
        (void)fflush(file);
        (void)ftruncate(fileno(file), header_offset);
    }
    if (fclose(file) != 0) ok = false;
    return ok;
}

static bool worksheet_journal_apply_record(FILE *file,
                                           const worksheet_journal_header_t *header)
{
    uint32_t remaining = header->payload_size;
    if (header->section == WORKSHEET_DIRTY_FIXED) {
        if (remaining != sizeof(worksheet_fixed_t)) return false;
        worksheet_fixed_t *fixed = heap_caps_calloc(
            1, sizeof(*fixed), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        bool ok = fixed != NULL &&
            worksheet_read_payload(file, fixed, sizeof(*fixed), &remaining) &&
            remaining == 0 && worksheet_fixed_valid(fixed);
        if (ok) worksheet_apply_fixed(fixed);
        if (fixed != NULL) heap_caps_free(fixed);
        return ok;
    }

    size_t capacity = header->section == WORKSHEET_DIRTY_LISTS
        ? (size_t)LIST_COUNT * LIST_MAX_VALUES
        : (size_t)MATRIX_COUNT * MATRIX_MAX_N * MATRIX_MAX_N;
    if (header->section != WORKSHEET_DIRTY_LISTS &&
        header->section != WORKSHEET_DIRTY_MATRICES) return false;
    size_t bytes = capacity * sizeof(double);
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (bytes > psram_free || psram_free - bytes < OPENCALC_PSRAM_RESERVE_BYTES) return false;
    double *values = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (values == NULL) return false;

    bool ok = true;
    size_t value_count = 0;
    uint32_t counts[MATRIX_COUNT] = {0};
    uint32_t cols[MATRIX_COUNT] = {0};
    int item_count = header->section == WORKSHEET_DIRTY_LISTS ? LIST_COUNT : MATRIX_COUNT;
    for (int item = 0; ok && item < item_count; item++) {
        ok = worksheet_read_payload(file, &counts[item], sizeof(counts[item]), &remaining);
        if (header->section == WORKSHEET_DIRTY_MATRICES) {
            ok = ok && worksheet_read_payload(file, &cols[item], sizeof(cols[item]), &remaining) &&
                counts[item] <= MATRIX_MAX_N && cols[item] <= MATRIX_MAX_N;
        } else {
            ok = ok && counts[item] <= LIST_MAX_VALUES;
            cols[item] = 1;
        }
        size_t cells = (size_t)counts[item] * cols[item];
        ok = ok && cells <= capacity - value_count &&
            worksheet_read_payload(file, values + value_count,
                                   cells * sizeof(double), &remaining);
        value_count += ok ? cells : 0;
    }
    for (size_t i = 0; ok && i < value_count; i++) ok = isfinite(values[i]);
    ok = ok && remaining == 0;

    if (ok && header->section == WORKSHEET_DIRTY_LISTS) {
        memset(s_lists, 0, sizeof(s_lists));
        size_t offset = 0;
        for (int list = 0; list < LIST_COUNT; list++) {
            s_list_counts[list] = (int)counts[list];
            memcpy(s_lists[list], values + offset, counts[list] * sizeof(double));
            offset += counts[list];
        }
    } else if (ok) {
        memset(s_matrices, 0, sizeof(s_matrices));
        size_t offset = 0;
        for (int matrix = 0; matrix < MATRIX_COUNT; matrix++) {
            s_matrix_rows_by_index[matrix] = (int)counts[matrix];
            s_matrix_cols_by_index[matrix] = (int)cols[matrix];
            for (uint32_t row = 0; row < counts[matrix]; row++) {
                memcpy(s_matrices[matrix][row], values + offset,
                       cols[matrix] * sizeof(double));
                offset += cols[matrix];
            }
        }
    }
    heap_caps_free(values);
    return ok;
}

static bool worksheet_journal_load(bool *discarded_tail)
{
    if (discarded_tail != NULL) *discarded_tail = false;
    FILE *file = fopen(WORKSHEET_JOURNAL_PATH, "rb");
    if (file == NULL) return false;
    bool applied = false;
    bool invalid_tail = false;

    while (true) {
        worksheet_journal_header_t header = {0};
        size_t got = fread(&header, 1, sizeof(header), file);
        if (got == 0 && feof(file)) break;
        if (got != sizeof(header) || header.magic != WORKSHEET_JOURNAL_MAGIC ||
            header.version != WORKSHEET_JOURNAL_VERSION ||
            header.payload_size > 2u * 1024u * 1024u) {
            invalid_tail = true;
            break;
        }

        long payload_offset = ftell(file);
        uint8_t buffer[256];
        uint32_t crc = UINT32_MAX;
        uint32_t unread = header.payload_size;
        while (unread > 0) {
            size_t chunk = unread < sizeof(buffer) ? unread : sizeof(buffer);
            if (fread(buffer, 1, chunk, file) != chunk) {
                invalid_tail = true;
                break;
            }
            crc = worksheet_crc32_update(crc, buffer, chunk);
            unread -= (uint32_t)chunk;
        }
        if (invalid_tail || (crc ^ UINT32_MAX) != header.payload_crc32 ||
            fseek(file, payload_offset, SEEK_SET) != 0 ||
            !worksheet_journal_apply_record(file, &header)) {
            invalid_tail = true;
            break;
        }
        applied = true;
    }
    fclose(file);
    if (invalid_tail) {
        remove(WORKSHEET_JOURNAL_PATH);
        if (discarded_tail != NULL) *discarded_tail = true;
    }
    return applied;
}

static uint32_t worksheet_sections_for_page(page_id_t page)
{
    uint32_t sections = WORKSHEET_DIRTY_FIXED;
    if (page == PAGE_LIST_EDITOR || page == PAGE_STATS_ONE_VAR ||
        page == PAGE_STATS_SETUP || page == PAGE_STATS_RESULT ||
        page == PAGE_INEQ_EDITOR || page == PAGE_INEQ_RESULT) {
        sections |= WORKSHEET_DIRTY_LISTS;
    }
    if (page == PAGE_MATRIX_EDITOR || page == PAGE_MATRIX_SIZE ||
        page == PAGE_SOLVER_WORKFLOW || page == PAGE_SOLVER_SYSTEM_RESULT) {
        sections |= WORKSHEET_DIRTY_MATRICES;
    }
    return sections;
}

static void worksheet_mark_dirty(void)
{
    worksheet_mark_dirty_sections(worksheet_sections_for_page(s_page));
}

static void worksheet_mark_dirty_sections(uint32_t sections)
{
    s_worksheet_dirty_sections |= sections;
    s_worksheet_dirty_since_us = esp_timer_get_time();
}

static bool worksheet_persist_flush(void)
{
    uint32_t dirty = s_worksheet_dirty_sections;
    if (dirty == 0) return true;
    if (!usb_msc_app_storage_available()) return false;
    if (mkdir(WORKSHEET_DIR, 0775) != 0 && errno != EEXIST) return false;

    uint32_t saved = 0;
    const uint32_t sections[] = {
        WORKSHEET_DIRTY_FIXED,
        WORKSHEET_DIRTY_LISTS,
        WORKSHEET_DIRTY_MATRICES,
    };
    for (size_t i = 0; i < sizeof(sections) / sizeof(sections[0]); i++) {
        if ((dirty & sections[i]) == 0) continue;
        if (!worksheet_journal_append_section((uint16_t)sections[i])) break;
        saved |= sections[i];
    }
    s_worksheet_dirty_sections &= ~saved;
    if (saved != dirty) return false;

    struct stat journal_stat;
    if (stat(WORKSHEET_JOURNAL_PATH, &journal_stat) == 0 &&
        journal_stat.st_size >= WORKSHEET_CHECKPOINT_BYTES) {
        remove(WORKSHEET_TEMP_PATH);
        if (worksheet_persist_save_path(WORKSHEET_TEMP_PATH)) {
            remove(WORKSHEET_BACKUP_PATH);
            bool had_current = rename(WORKSHEET_PATH, WORKSHEET_BACKUP_PATH) == 0;
            if (rename(WORKSHEET_TEMP_PATH, WORKSHEET_PATH) == 0) {
                remove(WORKSHEET_JOURNAL_PATH);
                ESP_LOGI("persistence", "Worksheet journal compacted into checkpoint");
            } else {
                if (had_current) rename(WORKSHEET_BACKUP_PATH, WORKSHEET_PATH);
                remove(WORKSHEET_TEMP_PATH);
            }
        } else {
            remove(WORKSHEET_TEMP_PATH);
        }
    }
    printf("worksheet sections saved: 0x%02x\n", (unsigned)saved);
    return true;
}

static void worksheet_persist_poll(void)
{
    if (atomic_load(&s_sensor_capture_active)) return;
    if (atomic_exchange(&s_script_lists_dirty, false)) {
        worksheet_mark_dirty_sections(WORKSHEET_DIRTY_LISTS);
    }
    if (s_worksheet_dirty_sections != 0 &&
        esp_timer_get_time() - s_worksheet_dirty_since_us >= WORKSHEET_SAVE_DELAY_US) {
        (void)worksheet_persist_flush();
    }
}

static void worksheet_persist_load(void)
{
    bool loaded = worksheet_persist_load_path(WORKSHEET_PATH);
    bool recovered_backup = false;
    if (!loaded) {
        loaded = worksheet_persist_load_path(WORKSHEET_BACKUP_PATH);
        recovered_backup = loaded;
    }
    bool discarded_tail = false;
    bool replayed = worksheet_journal_load(&discarded_tail);
    s_worksheet_dirty_sections = 0;
    if (loaded) {
        printf("worksheet restored%s\n", recovered_backup ? " from backup" : "");
        if (recovered_backup) {
            remove(WORKSHEET_PATH);
            worksheet_mark_dirty_sections(WORKSHEET_DIRTY_ALL);
            (void)worksheet_persist_flush();
        }
    }
    if (replayed) printf("worksheet journal replayed\n");
    if (discarded_tail) {
        ESP_LOGW("persistence", "Discarded incomplete worksheet journal tail");
        worksheet_mark_dirty_sections(WORKSHEET_DIRTY_ALL);
    }
}

static void worksheet_persist_erase(void)
{
    remove(WORKSHEET_TEMP_PATH);
    remove(WORKSHEET_BACKUP_PATH);
    remove(WORKSHEET_PATH);
    remove(WORKSHEET_JOURNAL_PATH);
    s_worksheet_dirty_sections = 0;
}

static void factory_reset_runtime_state(void)
{
    s_current_app = APP_CALCULATOR;
    s_previous_app = APP_CALCULATOR;
    s_home_selection = APP_CALCULATOR;
    s_script_selection = 0;
    s_app_selection = 0;
    s_math_tab = 0;
    s_math_selection = 0;
    s_calc_input[0] = '\0';
    s_calc_cursor = 0;
    snprintf(s_calc_output, sizeof(s_calc_output), "0");
    snprintf(s_calc_ans, sizeof(s_calc_ans), "0");
    opencalc_calc_history_reset();
    opencalc_math_variables_reset();
    memset(s_lists, 0, sizeof(s_lists));
    memset(s_list_counts, 0, sizeof(s_list_counts));
    s_list_index = 0;
    s_list_cursor = 0;
    s_list_entry[0] = '\0';
    s_list_editing = false;
    s_mode_selection = 0;
    s_display_format = 0;
    s_print_mode = 0;
    s_angle_mode = 0;
    s_graphing_mode = 0;
    s_stats_result_title[0] = '\0';
    memset(s_stats_result_lines, 0, sizeof(s_stats_result_lines));
    s_stats_result_line_count = 0;
    s_stats_plot_mode = 0;
    s_complex_mode = 0;
    memset(s_matrices, 0, sizeof(s_matrices));
    memset(s_matrix_rows_by_index, 0, sizeof(s_matrix_rows_by_index));
    memset(s_matrix_cols_by_index, 0, sizeof(s_matrix_cols_by_index));
    s_matrix_index = 0;
    s_matrix_cursor_row = 0;
    s_matrix_cursor_col = 0;
    s_matrix_scroll_row = 0;
    s_matrix_scroll_col = 0;
    s_matrix_entry[0] = '\0';
    s_matrix_entry_active = false;
    s_matrix_status[0] = '\0';
    snprintf(s_solver_e1, sizeof(s_solver_e1), "x^2");
    snprintf(s_solver_e2, sizeof(s_solver_e2), "4");
    s_solver_guess = 1.0;
    s_solver_lower = -10.0;
    s_solver_upper = 10.0;
    s_solver_precision = 10;
    s_solver_result = 0.0;
    s_solver_result_imag = 0.0;
    s_solver_has_complex_result = false;
    s_solver_has_result = false;
    memset(s_solver_poly_root_real, 0, sizeof(s_solver_poly_root_real));
    memset(s_solver_poly_root_imag, 0, sizeof(s_solver_poly_root_imag));
    s_solver_poly_root_count = 0;
    s_solver_poly_root_selected = 0;
    s_solver_poly_source[0] = '\0';
    s_solver_roots_are_polynomial = true;
    s_solver_workflow = SOLVER_WORKFLOW_EQUATION;
    s_solver_workflow_selection = 0;
    s_solver_system_status = SOLVER_SYSTEM_NONE;
    s_solver_system_variables = 0;
    s_solver_system_rank = 0;
    s_solver_system_selected = 0;
    s_solver_symbolic_result[0] = '\0';
    s_solver_symbolic_scroll = 0;
    memset(s_solver_saved_e1, 0, sizeof(s_solver_saved_e1));
    memset(s_solver_saved_e2, 0, sizeof(s_solver_saved_e2));
    memset(s_solver_saved_type, 0, sizeof(s_solver_saved_type));
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
    s_conic_a = 2.0;
    s_conic_b = 3.0;
    s_conic_r = 5.0;
    s_conic_angle = 0.0;
    const double default_conic[6] = {1.0, 0.0, 1.0, 0.0, 0.0, -25.0};
    memcpy(s_conic_general, default_conic, sizeof(default_conic));
    memset(&s_conic_model, 0, sizeof(s_conic_model));
    memset(s_conic_overlays, 0, sizeof(s_conic_overlays));
    s_conic_overlay_count = 0;
    s_conic_overlay_selection = 0;
    s_conic_selection = 0;
    s_conic_entry[0] = '\0';
    s_conic_entry_active = false;
    s_conic_status[0] = '\0';
    opencalc_math_set_degrees(true);
    opencalc_giac_reset();
    opencalc_graph_model_reset();

    snprintf(s_graph_exprs[0], sizeof(s_graph_exprs[0]), "x");
    for (int i = 1; i < GRAPH_FUNC_COUNT; i++) {
        s_graph_exprs[i][0] = '\0';
    }
    for (int i = 0; i < GRAPH_FUNC_COUNT; i++) {
        s_graph_enabled[i] = i == 0;
    }
    snprintf(s_graph_param_x[0], sizeof(s_graph_param_x[0]), "cos(t)");
    snprintf(s_graph_param_y[0], sizeof(s_graph_param_y[0]), "sin(t)");
    for (int i = 1; i < GRAPH_PARAM_COUNT; i++) {
        s_graph_param_x[i][0] = '\0';
        s_graph_param_y[i][0] = '\0';
    }
    for (int i = 0; i < GRAPH_PARAM_COUNT; i++) {
        s_graph_param_enabled[i] = false;
    }
    snprintf(s_graph_polar_exprs[0], sizeof(s_graph_polar_exprs[0]), "1");
    for (int i = 1; i < GRAPH_POLAR_COUNT; i++) {
        s_graph_polar_exprs[i][0] = '\0';
    }
    for (int i = 0; i < GRAPH_POLAR_COUNT; i++) {
        s_graph_polar_enabled[i] = false;
    }
    snprintf(s_graph_seq_exprs[0], sizeof(s_graph_seq_exprs[0]), "n");
    for (int i = 1; i < GRAPH_SEQ_COUNT; i++) {
        s_graph_seq_exprs[i][0] = '\0';
    }
    for (int i = 0; i < GRAPH_SEQ_COUNT; i++) {
        s_graph_seq_enabled[i] = false;
    }
    s_graph_tmin = 0.0;
    s_graph_tmax = 360.0;
    s_graph_nmin = 0.0;
    s_graph_nmax = 20.0;
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
    s_table_x_start = 0.0;
    s_table_step = 1.0;
    s_table_func_start = 0;
    s_table_rows = 9;
    s_table_precision = 2;
    s_table_setup_selection = 0;
    inequality_clear_all();
    s_ineq_selection = 0;
    s_ineq_join_or = false;
    s_ineq_integer_mode = false;
    s_ineq_sign_chart = false;
    s_ineq_graph_editor = false;
    memset(&s_ineq_problem, 0, sizeof(s_ineq_problem));
    memset(&s_ineq_solution, 0, sizeof(s_ineq_solution));
    snprintf(s_ineq_problem_text, sizeof(s_ineq_problem_text), "x^2-4>=0");
    s_ineq_interval_text[0] = '\0';
    s_ineq_exact_text[0] = '\0';
    s_ineq_status[0] = '\0';
    s_ineq_symbolic_pending = false;
    s_ineq_result_scroll = 0;
    s_graph_trace = false;
    s_graph_trace_x = 0.0;

    s_second_active = false;
    s_alpha_active = false;
    s_alpha_locked = false;
    s_sleep_enabled = true;
    s_power_save_enabled = false;
    s_light_mode = false;
    s_audio_volume_percent = OPENCALC_AUDIO_VOLUME_PERCENT;
    opencalc_audio_set_volume_percent(s_audio_volume_percent);
    s_doom_high_score = 0;
    s_doom_last_saved_high_score = 0;
    opencalc_persist_factory_reset();
    worksheet_persist_erase();
    board_set_backlight_brightness(80);
    opencalc_persist_set_u32("brightness", 80);
    opencalc_persist_set_u32("auto_sleep", 1);
    opencalc_persist_set_u32("power_save", 0);
    opencalc_persist_set_u32("light_mode", 0);
    opencalc_persist_set_u32("audio_volume", (uint32_t)s_audio_volume_percent);
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
    int history_count = opencalc_calc_history_count();
    int history_selected = opencalc_calc_history_selected();
    bool answer_selected = opencalc_calc_history_answer_selected();
    int start = history_count > visible_rows ? history_count - visible_rows : 0;
    if (history_selected >= 0 && history_selected < start) {
        start = history_selected;
    }
    for (int i = start; i < history_count; i++) {
        int visible = i - start;
        int y = first_y + visible * row_h;
        bool selected = i == history_selected;
        uint32_t expression_color = selected && !answer_selected ? THEME_ACCENT : THEME_TEXT;
        uint32_t result_color = selected && answer_selected ? THEME_ACCENT : THEME_TEXT;
        const char *history_expression = opencalc_calc_history_expression(i);
        const char *history_result = opencalc_calc_history_result(i);
        char expression_preview[49];
        char result_preview[49];
        snprintf(expression_preview, sizeof(expression_preview), "%.45s%s", history_expression,
                 strlen(history_expression) > 45 ? "..." : "");
        snprintf(result_preview, sizeof(result_preview), "%.45s%s", history_result,
                 strlen(history_result) > 45 ? "..." : "");

        int expression_w = ui_math_text(0, 0, expression_preview, expression_color, false);
        if (selected && !answer_selected) {
            ui_rect(8, y - 5, expression_w + 4, 22, THEME_ACCENT_2);
        }
        ui_math_text(10, y + 2, expression_preview, expression_color, true);
        int result_w = ui_math_text(0, 0, result_preview, result_color, false);
        int result_x = UI_W - 10 - result_w;
        if (result_x < 120) {
            result_x = 120;
        }
        if (selected && answer_selected) {
            ui_rect(result_x - 2, y + 15, result_w + 4, 22, THEME_ACCENT_2);
        }
        ui_math_text(result_x, y + 22, result_preview, result_color, true);
    }

    ui_rect(0, 202, UI_W, 1, THEME_BORDER);
    ui_draw_calc_expression(10, 214, s_calc_input, s_calc_cursor);
    ui_present();
}

static void ui_draw_calc_result(void)
{
    enum { CHARS_PER_LINE = 46, VISIBLE_LINES = 11 };
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_CALCULATOR]);
    ui_text(12, 34, "Full result", THEME_ACCENT, 1);

    size_t length = strlen(s_calc_output);
    int line_count = length == 0 ? 1 : (int)((length + CHARS_PER_LINE - 1) / CHARS_PER_LINE);
    int max_scroll = line_count > VISIBLE_LINES ? line_count - VISIBLE_LINES : 0;
    if (s_calc_result_scroll > max_scroll) s_calc_result_scroll = max_scroll;
    if (s_calc_result_scroll < 0) s_calc_result_scroll = 0;
    for (int row = 0; row < VISIBLE_LINES; row++) {
        size_t offset = (size_t)(s_calc_result_scroll + row) * CHARS_PER_LINE;
        if (offset >= length) break;
        char line[CHARS_PER_LINE + 1];
        size_t count = length - offset;
        if (count > CHARS_PER_LINE) count = CHARS_PER_LINE;
        memcpy(line, s_calc_output + offset, count);
        line[count] = '\0';
        ui_text(12, 52 + row * 14, line, THEME_TEXT, 1);
    }
    ui_scrollbar(312, 50, 154, line_count, VISIBLE_LINES, s_calc_result_scroll);
    ui_rect(0, 211, UI_W, 1, THEME_BORDER);
    ui_text(12, 219, "Up/Down scroll  Enter copy  Back close", THEME_MUTED, 1);
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

    int tab_step = (UI_W - 20) / MATH_TAB_COUNT;
    for (int i = 0; i < MATH_TAB_COUNT; i++) {
        int x = 10 + i * tab_step;
        uint32_t bg = (i == s_math_tab) ? THEME_ACCENT : THEME_SURFACE;
        uint32_t fg = THEME_TEXT;
        ui_rect(x, 32, tab_step - 6, 18, bg);
        int label_x = x + (tab_step - 6 - ui_text_width(MATH_TAB_NAMES[i], 8, 1)) / 2;
        ui_text(label_x, 38, MATH_TAB_NAMES[i], fg, 1);
    }

    int count = math_menu_count(s_math_tab);
    const int visible_count = 10;
    int first = s_math_selection >= visible_count ? s_math_selection - visible_count + 1 : 0;
    if (first + visible_count > count) first = count > visible_count ? count - visible_count : 0;
    for (int row = 0; row < visible_count && first + row < count; row++) {
        int i = first + row;
        uint32_t bg = (i == s_math_selection) ? THEME_ACCENT_2 : THEME_SURFACE;
        ui_rect(18, 60 + row * 15, 284, 13, bg);
        char line[48];
        snprintf(line, sizeof(line), "%d %s", i + 1, MATH_MENU[s_math_tab][i].label);
        ui_text(26, 62 + row * 15, line, THEME_TEXT, 1);
    }
    ui_scrollbar(310, 60, 148, count, visible_count, first);

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

static void ui_draw_graph_format(void)
{
    char series[8];
    char rows[GRAPH_FORMAT_COUNT][48];
    graph_series_label(s_graph_style_series, series, sizeof(series));
    snprintf(rows[0], sizeof(rows[0]), "view       %s", s_graph_split ? "split" : "full");
    snprintf(rows[1], sizeof(rows[1]), "series     %s", series);
    snprintf(rows[2], sizeof(rows[2]), "style      %s",
             graph_style_name((graph_style_t)s_graph_styles[s_graph_style_series]));
    snprintf(rows[3], sizeof(rows[3]), "background %s", s_graph_background_enabled ? "graph.bmp" : "off");
    snprintf(rows[4], sizeof(rows[4]), "reload      graph.bmp");
    snprintf(rows[5], sizeof(rows[5]), "grid       %s", s_graph_grid ? "on" : "off");

    ui_clear(THEME_BG);
    ui_header(&APPS[APP_GRAPH]);
    ui_text(18, 34, "Format", THEME_TEXT, 2);
    for (int i = 0; i < GRAPH_FORMAT_COUNT; i++) {
        int y = 58 + i * 24;
        ui_rect(18, y, 284, 18, i == s_graph_format_selection ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_text(28, y + 6, rows[i], THEME_TEXT, 1);
    }
    ui_text(16, 220, "arrows - select/change  enter - apply", THEME_MUTED, 1);
    ui_present();
}

static void ui_graph_symbolic_value(int x, int y, const char *value, uint32_t color)
{
    char clipped[47];
    snprintf(clipped, sizeof(clipped), "%s", value != NULL && value[0] != '\0' ? value : "-");
    if (value != NULL && strlen(value) >= sizeof(clipped)) {
        memcpy(clipped + sizeof(clipped) - 4, "...", 4);
    }
    ui_text(x, y, clipped, color, 1);
}

static void ui_draw_graph_symbolic(void)
{
    char label[8];
    char expression[100];
    const char *derivative_label = "Derivative";
    const char *integral_label = "Integral";
    const char *roots_label = "Roots";
    const char *asymptotes_label = "Asymptotes";

    graph_series_label(s_graph_trace_fn, label, sizeof(label));
    switch (s_graphing_mode) {
    case 1:
        snprintf(expression, sizeof(expression), "x=%s  y=%s",
                 s_graph_param_x[s_graph_trace_fn], s_graph_param_y[s_graph_trace_fn]);
        derivative_label = "dy/dx";
        integral_label = "Integral y dx";
        roots_label = "x-axis t";
        asymptotes_label = "End behavior";
        break;
    case 2:
        snprintf(expression, sizeof(expression), "r=%s", s_graph_polar_exprs[s_graph_trace_fn]);
        derivative_label = "dy/dx";
        integral_label = "Area integral";
        roots_label = "Pole crossings";
        asymptotes_label = "End behavior";
        break;
    case 3:
        snprintf(expression, sizeof(expression), "%s", s_graph_seq_exprs[s_graph_trace_fn]);
        derivative_label = "Forward difference";
        integral_label = "Symbolic sum";
        roots_label = "Zero indices";
        asymptotes_label = "Limit";
        break;
    default:
        snprintf(expression, sizeof(expression), "%s", s_graph_exprs[s_graph_trace_fn]);
        break;
    }

    ui_clear(THEME_BG);
    ui_header(&APPS[APP_GRAPH]);
    ui_text(14, 34, "Linked Symbolic Analysis", THEME_TEXT, 1);
    ui_rect(14, 48, 292, 23, THEME_SURFACE);
    ui_rect(14, 48, 4, 23, s_graph_colors[s_graph_trace_fn % GRAPH_COLOR_COUNT]);
    ui_text(23, 53, label, s_graph_colors[s_graph_trace_fn % GRAPH_COLOR_COUNT], 1);
    ui_graph_symbolic_value(48, 53, expression, THEME_TEXT);

    const char *labels[] = {derivative_label, integral_label, roots_label, asymptotes_label};
    const char *values[] = {
        s_graph_symbolic_derivative,
        s_graph_symbolic_integral,
        s_graph_symbolic_roots,
        s_graph_symbolic_asymptotes,
    };
    for (int row = 0; row < 4; row++) {
        int y = 77 + row * 35;
        ui_text(16, y, labels[row], THEME_MUTED, 1);
        ui_graph_symbolic_value(22, y + 13, values[row], THEME_TEXT);
    }

    char controls[80];
    snprintf(controls, sizeof(controls), "Y= tangent %s  Window shade %s",
             s_graph_tangent_enabled && s_graph_overlay_fn == s_graph_trace_fn ? "on" : "off",
             s_graph_integral_shade_enabled && s_graph_overlay_fn == s_graph_trace_fn ? "on" : "off");
    ui_rect(0, 219, UI_W, 21, THEME_HEADER);
    ui_text(5, 221, controls, THEME_TEXT, 1);
    ui_text(5, 231, "left/right line  up/down point  enter graph", THEME_MUTED, 1);
    ui_present();
}

static void graph_draw_segment_if_visible(bool *have_prev, int *prev_x, int *prev_y,
                                          int px, int py, int top, int bottom, uint32_t color,
                                          graph_style_t style, int sample)
{
    if (px < -40 || px > UI_W + 40 || py < top - 40 || py > bottom + 40) {
        *have_prev = false;
        return;
    }
    if (style == GRAPH_STYLE_POINTS) {
        if ((sample % 5) == 0 && px >= 0 && px < UI_W && py >= top && py <= bottom) {
            ui_rect(px - 1, py - 1, 3, 3, color);
        }
    } else if (*have_prev && (style != GRAPH_STYLE_DOTTED || (sample % 8) < 4)) {
        ui_line(*prev_x, *prev_y, px, py, color);
        if (style == GRAPH_STYLE_THICK) {
            ui_line(*prev_x, *prev_y + 1, px, py + 1, color);
        }
    }
    *prev_x = px;
    *prev_y = py;
    *have_prev = true;
}

static uint32_t graph_dim_color(uint32_t color)
{
    uint32_t r = ((color >> 16) & 0xff) / 3;
    uint32_t g = ((color >> 8) & 0xff) / 3;
    uint32_t b = (color & 0xff) / 3;
    return (r << 16) | (g << 8) | b;
}

static void draw_graph_integral_shade(const graph_view_t *view, int top, int bottom)
{
    if (!s_graph_integral_shade_enabled || s_graphing_mode != 0 ||
        !graph_series_is_active(s_graph_overlay_fn)) return;

    double low = fmin(0.0, s_graph_overlay_x);
    double high = fmax(0.0, s_graph_overlay_x);
    int axis_y = graph_screen_y(view, 0.0);
    uint32_t color = graph_dim_color(s_graph_colors[s_graph_overlay_fn % GRAPH_COLOR_COUNT]);
    for (int px = 0; px < UI_W; px += 2) {
        double x = graph_world_x(view, px);
        double y = 0.0;
        if (x < low || x > high || !graph_eval_expression(s_graph_exprs[s_graph_overlay_fn], x, &y)) {
            continue;
        }
        int curve_y = graph_screen_y(view, y);
        int y0 = axis_y < curve_y ? axis_y : curve_y;
        int y1 = axis_y > curve_y ? axis_y : curve_y;
        if (y0 < top) y0 = top;
        if (y1 > bottom) y1 = bottom;
        if (y0 <= y1) ui_line(px, y0, px, y1, color);
    }
}

static void draw_graph_tangent(const graph_view_t *view, int top, int bottom)
{
    if (!s_graph_tangent_enabled || s_graphing_mode != 0 ||
        !graph_series_is_active(s_graph_overlay_fn)) return;

    double span = s_graph_xmax - s_graph_xmin;
    double h = fmax(fabs(span) * 1e-5, 1e-6);
    double y0 = 0.0, left = 0.0, right = 0.0;
    const char *expr = s_graph_exprs[s_graph_overlay_fn];
    if (!graph_eval_expression(expr, s_graph_overlay_x, &y0) ||
        !graph_eval_expression(expr, s_graph_overlay_x - h, &left) ||
        !graph_eval_expression(expr, s_graph_overlay_x + h, &right)) return;
    double slope = (right - left) / (2.0 * h);

    bool have_prev = false;
    int prev_x = 0, prev_y = 0;
    uint32_t color = THEME_BATTERY_YELLOW;
    for (int px = 0; px < UI_W; px++) {
        double x = graph_world_x(view, px);
        double y = y0 + slope * (x - s_graph_overlay_x);
        graph_draw_segment_if_visible(&have_prev, &prev_x, &prev_y, px,
                                      graph_screen_y(view, y), top, bottom,
                                      color, GRAPH_STYLE_LINE, px);
    }
}

static void draw_function_graphs(const graph_view_t *view, int top, int bottom)
{
    for (int fn = 0; fn < GRAPH_FUNC_COUNT; fn++) {
        if (!s_graph_enabled[fn] || s_graph_exprs[fn][0] == '\0') {
            continue;
        }

        bool have_prev = false;
        int prev_x = 0;
        int prev_y = 0;
        for (int px = 0; px < UI_W; px++) {
            double x = graph_world_x(view, px);
            double y = 0.0;
            if (!graph_eval_expression(s_graph_exprs[fn], x, &y)) {
                have_prev = false;
                continue;
            }
            graph_draw_segment_if_visible(&have_prev, &prev_x, &prev_y, px,
                                          graph_screen_y(view, y), top, bottom, s_graph_colors[fn],
                                          (graph_style_t)s_graph_styles[fn], px);
        }
    }
}

static void draw_parametric_graphs(const graph_view_t *view, int top, int bottom)
{
    const int steps = 240;
    for (int fn = 0; fn < GRAPH_PARAM_COUNT; fn++) {
        if (!s_graph_param_enabled[fn] || s_graph_param_x[fn][0] == '\0' || s_graph_param_y[fn][0] == '\0') {
            continue;
        }

        bool have_prev = false;
        int prev_x = 0;
        int prev_y = 0;
        for (int i = 0; i <= steps; i++) {
            double t = s_graph_tmin + (s_graph_tmax - s_graph_tmin) * (double)i / (double)steps;
            double x = 0.0;
            double y = 0.0;
            if (!graph_eval_expression_var(s_graph_param_x[fn], 't', t, &x) ||
                !graph_eval_expression_var(s_graph_param_y[fn], 't', t, &y)) {
                have_prev = false;
                continue;
            }
            graph_draw_segment_if_visible(&have_prev, &prev_x, &prev_y,
                                          graph_screen_x(view, x), graph_screen_y(view, y),
                                          top, bottom, s_graph_colors[fn],
                                          (graph_style_t)s_graph_styles[fn], i);
        }
    }
}

static void draw_polar_graphs(const graph_view_t *view, int top, int bottom)
{
    const int steps = 360;
    for (int fn = 0; fn < GRAPH_POLAR_COUNT; fn++) {
        if (!s_graph_polar_enabled[fn] || s_graph_polar_exprs[fn][0] == '\0') {
            continue;
        }

        bool have_prev = false;
        int prev_x = 0;
        int prev_y = 0;
        for (int i = 0; i <= steps; i++) {
            double theta = s_graph_tmin + (s_graph_tmax - s_graph_tmin) * (double)i / (double)steps;
            double r = 0.0;
            if (!graph_eval_expression_var(s_graph_polar_exprs[fn], 't', theta, &r)) {
                have_prev = false;
                continue;
            }
            double rad = graph_angle_to_radians_for_plot(theta);
            double x = r * cos(rad);
            double y = r * sin(rad);
            graph_draw_segment_if_visible(&have_prev, &prev_x, &prev_y,
                                          graph_screen_x(view, x), graph_screen_y(view, y),
                                          top, bottom, s_graph_colors[fn],
                                          (graph_style_t)s_graph_styles[fn], i);
        }
    }
}

static void draw_sequence_graphs(const graph_view_t *view, int top, int bottom)
{
    int n0 = (int)ceil(s_graph_nmin);
    int n1 = (int)floor(s_graph_nmax);
    if (n1 < n0) {
        return;
    }

    for (int fn = 0; fn < GRAPH_SEQ_COUNT; fn++) {
        if (!s_graph_seq_enabled[fn] || s_graph_seq_exprs[fn][0] == '\0') {
            continue;
        }

        bool have_prev = false;
        int prev_x = 0;
        int prev_y = 0;
        for (int n = n0; n <= n1; n++) {
            double y = 0.0;
            if (!graph_eval_expression_var(s_graph_seq_exprs[fn], 'n', (double)n, &y)) {
                have_prev = false;
                continue;
            }
            int px = graph_screen_x(view, (double)n);
            int py = graph_screen_y(view, y);
            graph_style_t style = (graph_style_t)s_graph_styles[fn];
            if (style != GRAPH_STYLE_POINTS) {
                ui_rect(px - 1, py - 1, 3, 3, s_graph_colors[fn]);
            }
            graph_draw_segment_if_visible(&have_prev, &prev_x, &prev_y, px, py, top, bottom,
                                          s_graph_colors[fn], style,
                                          style == GRAPH_STYLE_POINTS ? (n - n0) * 5 : n - n0);
        }
    }
}

static bool graph_eval_live_series_at(int fn, double input, double *x, double *y, double *metric)
{
    if (x == NULL || y == NULL || metric == NULL) {
        return false;
    }

    switch (s_graphing_mode) {
    case 1:
        if (fn < 0 || fn >= GRAPH_PARAM_COUNT || !s_graph_param_enabled[fn] ||
            s_graph_param_x[fn][0] == '\0' || s_graph_param_y[fn][0] == '\0' ||
            !graph_eval_expression_var(s_graph_param_x[fn], 't', input, x) ||
            !graph_eval_expression_var(s_graph_param_y[fn], 't', input, y)) {
            return false;
        }
        *metric = *y;
        return true;
    case 2: {
        double r = 0.0;
        if (fn < 0 || fn >= GRAPH_POLAR_COUNT || !s_graph_polar_enabled[fn] ||
            s_graph_polar_exprs[fn][0] == '\0' ||
            !graph_eval_expression_var(s_graph_polar_exprs[fn], 't', input, &r)) {
            return false;
        }
        double rad = graph_angle_to_radians_for_plot(input);
        *x = r * cos(rad);
        *y = r * sin(rad);
        *metric = r;
        return true;
    }
    case 3:
        if (fn < 0 || fn >= GRAPH_SEQ_COUNT || !s_graph_seq_enabled[fn] ||
            s_graph_seq_exprs[fn][0] == '\0' ||
            !graph_eval_expression_var(s_graph_seq_exprs[fn], 'n', input, y)) {
            return false;
        }
        *x = input;
        *metric = *y;
        return true;
    default:
        if (!graph_eval_fn_at(fn, input, y)) {
            return false;
        }
        *x = input;
        *metric = *y;
        return true;
    }
}

static int graph_table_series_count(void)
{
    switch (s_graphing_mode) {
    case 1: return GRAPH_PARAM_COUNT;
    case 2: return GRAPH_POLAR_COUNT;
    case 3: return GRAPH_SEQ_COUNT;
    default: return GRAPH_FUNC_COUNT;
    }
}

static void ui_draw_graph_split_table(int top)
{
    int series_count = graph_series_count();
    if (series_count <= 0) return;
    int fn = s_graph_trace_fn;
    if (fn < 0 || fn >= series_count) fn = 0;
    for (int offset = 0; offset < series_count; offset++) {
        int candidate = (fn + offset) % series_count;
        double x = 0.0, y = 0.0, metric = 0.0;
        if (graph_eval_live_series_at(candidate, s_table_x_start, &x, &y, &metric)) {
            fn = candidate;
            break;
        }
    }

    char label[8];
    char text[48];
    graph_series_label(fn, label, sizeof(label));
    ui_rect(0, top, UI_W, UI_H - top, THEME_SURFACE);
    ui_rect(0, top, UI_W, 1, THEME_BORDER);
    snprintf(text, sizeof(text), "Table %s", label);
    ui_text(8, top + 6, text, s_graph_colors[fn], 1);

    for (int row = 0; row < 3; row++) {
        double input = s_table_x_start + row;
        if (s_graphing_mode == 3) input = floor(input + 0.5);
        double x = 0.0, y = 0.0, metric = 0.0;
        int text_y = top + 23 + row * 17;
        if (!graph_eval_live_series_at(fn, input, &x, &y, &metric)) continue;
        if (s_graphing_mode == 1) {
            snprintf(text, sizeof(text), "t %.3g   x %.4g   y %.4g", input, x, y);
        } else if (s_graphing_mode == 2) {
            snprintf(text, sizeof(text), "t %.3g   r %.4g   (%.3g,%.3g)", input, metric, x, y);
        } else if (s_graphing_mode == 3) {
            snprintf(text, sizeof(text), "n %.0f   u %.6g", input, y);
        } else {
            snprintf(text, sizeof(text), "x %.3g   y %.6g", input, y);
        }
        ui_text(8, text_y, text, THEME_TEXT, 1);
    }
}

static void ui_draw_graph(void)
{
    if (s_graph_background_enabled && s_graph_background_loaded) {
        memcpy(opencalc_ui_canvas_pixels(), s_graph_background,
               opencalc_ui_canvas_size_bytes());
    } else {
        ui_clear(THEME_BG);
    }

    const int top = GRAPH_TOP;
    const int bottom = s_graph_split ? 146 : GRAPH_BOTTOM;
    graph_view_t view = current_graph_view();
    view.screen_bottom = bottom;
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
    draw_graph_integral_shade(&view, top, bottom);

    switch (s_graphing_mode) {
    case 1:
        draw_parametric_graphs(&view, top, bottom);
        break;
    case 2:
        draw_polar_graphs(&view, top, bottom);
        break;
    case 3:
        draw_sequence_graphs(&view, top, bottom);
        break;
    default:
        draw_function_graphs(&view, top, bottom);
        break;
    }
    draw_graph_tangent(&view, top, bottom);

    if (s_graph_trace) {
        graph_poi_t pois[GRAPH_POI_LIMIT];
        int poi_count = s_graphing_mode == 0 ? graph_collect_pois(pois, GRAPH_POI_LIMIT) : 0;
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

        int series_count = s_graphing_mode == 1 ? GRAPH_PARAM_COUNT :
            (s_graphing_mode == 2 ? GRAPH_POLAR_COUNT :
             (s_graphing_mode == 3 ? GRAPH_SEQ_COUNT : GRAPH_FUNC_COUNT));
        for (int offset = 0; offset < series_count; offset++) {
            int fn = (s_graph_trace_fn + offset) % series_count;
            double x = 0.0;
            double y = 0.0;
            double metric = 0.0;
            if (graph_eval_live_series_at(fn, s_graph_trace_x, &x, &y, &metric)) {
                s_graph_trace_fn = fn;
                int tx = graph_screen_x(&view, x);
                int ty = graph_screen_y(&view, y);
                if (s_graphing_mode == 0) {
                    ui_line(tx, top, tx, bottom, THEME_TEXT);
                }
                if (tx >= 0 && tx < UI_W && ty >= top && ty <= bottom) {
                    ui_rect(tx - 2, ty - 2, 5, 5, THEME_TEXT);
                }
                char buf[64];
                if (nearest_poi >= 0 && fabs(pois[nearest_poi].x - s_graph_trace_x) < (s_graph_xmax - s_graph_xmin) / 40.0) {
                    snprintf(buf, sizeof(buf), "%s x %.2f y %.2f",
                             graph_poi_label(pois[nearest_poi].type),
                             pois[nearest_poi].x,
                             pois[nearest_poi].y);
                } else if (s_graphing_mode == 1) {
                    snprintf(buf, sizeof(buf), "P%d t %.2f x %.2f y %.2f", fn + 1, s_graph_trace_x, x, y);
                } else if (s_graphing_mode == 2) {
                    snprintf(buf, sizeof(buf), "r%d t %.2f r %.2f", fn + 1, s_graph_trace_x, metric);
                } else if (s_graphing_mode == 3) {
                    snprintf(buf, sizeof(buf), "u%d n %.0f y %.2f", fn + 1, s_graph_trace_x, y);
                } else {
                    snprintf(buf, sizeof(buf), "Y%d x %.2f y %.2f", fn + 1, s_graph_trace_x, y);
                }
                ui_text(6, 6, buf, THEME_TEXT, 1);
                break;
            }
        }
    }

    if (s_graph_split) {
        ui_draw_graph_split_table(bottom + 1);
    } else {
        ui_rect(0, 229, UI_W, 11, THEME_HEADER);
        ui_text(4, 232,
                s_graph_status[0] != '\0' ? s_graph_status :
                    (s_graph_zoom_mode ? "zoom: up - in  down - out  zoom - done" :
                        (s_graph_trace ? "left/right move  trace line  alpha+graph info" :
                         "y= funcs  trace select  alpha+graph analysis")),
                THEME_TEXT, 1);
    }

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
    static const char *titles[] = {"Y=", "Param", "Polar", "Seq"};
    const char *title = s_graphing_mode >= 0 && s_graphing_mode < 4 ? titles[s_graphing_mode] : "Y=";
    ui_text(18, 32, title, THEME_TEXT, 2);

    int count = graph_entry_count();
    if (s_graph_selection >= count) {
        s_graph_selection = count - 1;
    }
    for (int i = 0; i < count && i < 12; i++) {
        int y = 55 + i * 14;
        uint32_t bg = (i == s_graph_selection) ? THEME_ACCENT_2 : THEME_SURFACE;
        char label[8];
        char *expr = graph_entry_buffer(i, NULL);
        graph_entry_label(i, label, sizeof(label));
        ui_rect(14, y - 2, 292, 13, bg);
        ui_rect(20, y + 1, 8, 8, graph_entry_is_enabled(i) ? graph_entry_color(i) : THEME_BORDER);
        char line[112];
        snprintf(line, sizeof(line), "%s=%s", label,
                 expr != NULL && expr[0] ? expr : "-");
        ui_text(36, y, line, THEME_TEXT, 1);
    }
    ui_text(16, 220, "enter - toggle  graph - draw", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_table_setup(void)
{
    char values[4][32];
    snprintf(values[0], sizeof(values[0]), "%.6g", s_table_x_start);
    snprintf(values[1], sizeof(values[1]), "%.6g", s_table_step);
    snprintf(values[2], sizeof(values[2]), "%d", s_table_rows);
    snprintf(values[3], sizeof(values[3]), "%d", s_table_precision);
    static const char *const labels[] = {"Table start", "Table step", "Visible rows", "Decimals"};

    ui_clear(THEME_BG);
    ui_header(&APPS[APP_TABLE]);
    ui_text(18, 34, "TABLE SETTINGS", THEME_TEXT, 2);
    for (int i = 0; i < 4; i++) {
        int y = 65 + i * 29;
        bool selected = i == s_table_setup_selection;
        ui_rect(18, y, 284, 23, selected ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_rect(18, y, 4, 23, selected ? THEME_ACCENT : THEME_BORDER);
        ui_text(30, y + 8, labels[i], THEME_TEXT, 1);
        int value_width = ui_text_width(values[i], 31, 1);
        ui_text(291 - value_width, y + 8, values[i], selected ? THEME_ACCENT : THEME_MUTED, 1);
    }
    ui_text(16, 205, "left/right adjust   enter view table", THEME_MUTED, 1);
    ui_text(16, 220, "up/down select   back previous app", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_table(void)
{
    ui_clear(THEME_BG);
    ui_header(&APPS[APP_TABLE]);
    char buf[64];

    if (s_graphing_mode == 1) {
        ui_text(10, 31, "t", THEME_TEXT, 1);
        for (int col = 0; col < 2; col++) {
            int fn = s_table_func_start + col;
            if (fn < GRAPH_PARAM_COUNT) {
                snprintf(buf, sizeof(buf), "P%d x/y", fn + 1);
                ui_text(78 + col * 112, 31, buf, s_graph_colors[fn], 1);
            }
        }
        for (int i = 0; i < s_table_rows; i++) {
            double t = s_table_x_start + i * s_table_step;
            int y = 50 + i * 15;
            snprintf(buf, sizeof(buf), "%.*f", s_table_precision, t);
            ui_text(10, y, buf, THEME_TEXT, 1);
            for (int col = 0; col < 2; col++) {
                int fn = s_table_func_start + col;
                double x = 0.0;
                double yv = 0.0;
                if (fn < GRAPH_PARAM_COUNT && s_graph_param_enabled[fn] &&
                    graph_eval_expression_var(s_graph_param_x[fn], 't', t, &x) &&
                    graph_eval_expression_var(s_graph_param_y[fn], 't', t, &yv)) {
                    snprintf(buf, sizeof(buf), "%.*f,%.*f", s_table_precision, x,
                             s_table_precision, yv);
                    ui_text(78 + col * 112, y, buf, THEME_TEXT, 1);
                }
            }
        }
        ui_text(8, 220, "up, down - t scroll  left, right - p cols", THEME_MUTED, 1);
    } else if (s_graphing_mode == 2) {
        ui_text(10, 31, "theta", THEME_TEXT, 1);
        for (int col = 0; col < 3; col++) {
            int fn = s_table_func_start + col;
            if (fn < GRAPH_POLAR_COUNT) {
                snprintf(buf, sizeof(buf), "r%d", fn + 1);
                ui_text(86 + col * 70, 31, buf, s_graph_colors[fn], 1);
            }
        }
        for (int i = 0; i < s_table_rows; i++) {
            double theta = s_table_x_start + i * s_table_step;
            int y = 50 + i * 15;
            snprintf(buf, sizeof(buf), "%.*f", s_table_precision, theta);
            ui_text(10, y, buf, THEME_TEXT, 1);
            for (int col = 0; col < 3; col++) {
                int fn = s_table_func_start + col;
                double r = 0.0;
                if (fn < GRAPH_POLAR_COUNT && s_graph_polar_enabled[fn] &&
                    graph_eval_expression_var(s_graph_polar_exprs[fn], 't', theta, &r)) {
                    snprintf(buf, sizeof(buf), "%.*f", s_table_precision, r);
                    ui_text(86 + col * 70, y, buf, THEME_TEXT, 1);
                }
            }
        }
        ui_text(8, 220, "up, down - theta scroll  left, right - r cols", THEME_MUTED, 1);
    } else if (s_graphing_mode == 3) {
        ui_text(10, 31, "n", THEME_TEXT, 1);
        for (int col = 0; col < 3; col++) {
            int fn = s_table_func_start + col;
            if (fn < GRAPH_SEQ_COUNT) {
                snprintf(buf, sizeof(buf), "u%d", fn + 1);
                ui_text(86 + col * 70, 31, buf, s_graph_colors[fn], 1);
            }
        }
        for (int i = 0; i < s_table_rows; i++) {
            double n = floor(s_table_x_start + i * s_table_step + 0.5);
            int y = 50 + i * 15;
            snprintf(buf, sizeof(buf), "%.0f", n);
            ui_text(10, y, buf, THEME_TEXT, 1);
            for (int col = 0; col < 3; col++) {
                int fn = s_table_func_start + col;
                double value = 0.0;
                if (fn < GRAPH_SEQ_COUNT && s_graph_seq_enabled[fn] &&
                    graph_eval_expression_var(s_graph_seq_exprs[fn], 'n', n, &value)) {
                    snprintf(buf, sizeof(buf), "%.*f", s_table_precision, value);
                    ui_text(86 + col * 70, y, buf, THEME_TEXT, 1);
                }
            }
        }
        ui_text(8, 220, "up, down - n scroll  left, right - u cols", THEME_MUTED, 1);
    } else {
        ui_text(10, 31, "x", THEME_TEXT, 1);
        static const char *headers[] = {"Y1", "Y2", "Y3", "Y4", "Y5", "Y6", "Y7", "Y8", "Y9", "Y10"};
        for (int fn = 0; fn < 3; fn++) {
            int graph_fn = s_table_func_start + fn;
            if (graph_fn < GRAPH_FUNC_COUNT) {
                ui_text(70 + fn * 75, 31, headers[graph_fn], s_graph_colors[graph_fn], 1);
            }
        }

        for (int i = 0; i < s_table_rows; i++) {
            double x = s_table_x_start + i * s_table_step;
            int y = 50 + i * 15;
            snprintf(buf, sizeof(buf), "%.*f", s_table_precision, x);
            ui_text(10, y, buf, THEME_TEXT, 1);
            for (int fn = 0; fn < 3; fn++) {
                int graph_fn = s_table_func_start + fn;
                if (graph_fn >= GRAPH_FUNC_COUNT) {
                    continue;
                }
                double value = 0.0;
                if (s_graph_enabled[graph_fn] && s_graph_exprs[graph_fn][0] != '\0' &&
                    graph_eval_expression(s_graph_exprs[graph_fn], x, &value)) {
                    snprintf(buf, sizeof(buf), "%.*f", s_table_precision, value);
                    ui_text(70 + fn * 75, y, buf, THEME_TEXT, 1);
                }
            }
        }
        ui_text(8, 220, "up, down - x scroll  left, right - y cols", THEME_MUTED, 1);
    }
    ui_present();
}

static void ui_draw_game_menu(void);

static void ui_reference_header(const char *title)
{
    ui_rect(0, UI_TOP_SAFE_Y, UI_W, UI_HEADER_H, THEME_HEADER);
    ui_rect(0, UI_HEADER_H - 2, UI_W, 2, THEME_ACCENT);
    ui_text_center(10, title, THEME_HEADER_TEXT, 1);
    ui_shift_indicators(7);
    ui_charge_indicator(276, 7);
    ui_battery_indicator(288, 9);
}

static uint32_t periodic_category_color(opencalc_element_category_t category)
{
    static const uint32_t colors[] = {
        0x5dd39e, 0x76d7ff, 0xff6b6b, 0xffb86b, 0xe6cf66, 0x8ee36b,
        0x9aa8b6, 0x6fa8ff, 0xd28cff, 0xff8cc6, 0x7b8490,
    };
    return category >= 0 && category <= OPENCALC_ELEMENT_UNKNOWN ? colors[category] : THEME_BORDER;
}

static void ui_draw_reference_home(void)
{
    static const char *const titles[] = {"Periodic Table", "Math", "Physics", "Engineering"};
    static const char *const details[] = {
        "118 elements and properties", "formulas and conversions",
        "mechanics, waves, electricity", "electronics and design",
    };
    ui_clear(THEME_BG);
    ui_reference_header("Reference Center");
    for (int i = 0; i < 4; ++i) {
        int y = 42 + i * 42;
        ui_rect(16, y, 288, 34, i == s_reference_home_selection ? THEME_ACCENT_2 : THEME_SURFACE);
        ui_text(26, y + 6, titles[i], THEME_TEXT, 1);
        ui_text(26, y + 19, details[i], THEME_MUTED, 1);
    }
    ui_text(16, 220, "up/down - select  enter - open", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_periodic_table(void)
{
    const int cell_w = 16;
    const int cell_h = 16;
    const int origin_x = 16;
    const int origin_y = 34;
    const opencalc_element_t *selected = opencalc_element_get(s_periodic_atomic_number);

    ui_clear(THEME_BG);
    ui_reference_header("Periodic Table");
    for (int row = 0; row < 9; ++row) {
        for (int col = 0; col < 18; ++col) {
            int atomic_number = opencalc_periodic_atomic_number_at(row, col);
            if (atomic_number == 0) continue;
            const opencalc_element_t *element = opencalc_element_get(atomic_number);
            int x = origin_x + col * cell_w;
            int y = origin_y + row * cell_h;
            bool is_selected = atomic_number == s_periodic_atomic_number;
            uint32_t category_color = periodic_category_color(element->category);
            ui_rect(x, y, cell_w - 1, cell_h - 1, is_selected ? THEME_ACCENT : THEME_SURFACE);
            ui_border(x, y, cell_w - 1, cell_h - 1, is_selected ? THEME_WHITE : category_color);
            int symbol_width = ui_text_width(element->symbol, 2, 1);
            ui_text(x + (cell_w - symbol_width) / 2, y + 5, element->symbol,
                    is_selected ? THEME_WHITE : category_color, 1);
        }
    }
    if (selected != NULL) {
        char summary[64];
        snprintf(summary, sizeof(summary), "%d  %s  %s", s_periodic_atomic_number,
                 selected->symbol, selected->name);
        ui_text(16, 184, summary, THEME_TEXT, 1);
        snprintf(summary, sizeof(summary), "mass %s   %s", selected->mass,
                 opencalc_element_category_name(selected->category));
        ui_text(16, 198, summary, THEME_MUTED, 1);
    }
    ui_text(16, 220, "arrows - move  enter - details", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_element_detail(void)
{
    const opencalc_element_t *element = opencalc_element_get(s_periodic_atomic_number);
    char line[96];
    ui_clear(THEME_BG);
    ui_reference_header("Element Details");
    if (element != NULL) {
        uint32_t color = periodic_category_color(element->category);
        ui_rect(18, 42, 78, 78, THEME_SURFACE);
        ui_border(18, 42, 78, 78, color);
        snprintf(line, sizeof(line), "%d", s_periodic_atomic_number);
        ui_text(25, 49, line, THEME_MUTED, 1);
        ui_text_centered_in_box(18, 67, 78, element->symbol, color, 3);
        ui_text(112, 47, element->name, THEME_TEXT, 2);
        snprintf(line, sizeof(line), "Atomic mass  %s", element->mass);
        ui_text(112, 78, line, THEME_TEXT, 1);
        snprintf(line, sizeof(line), "Group        %s", element->group == 0 ? "f-block" : "");
        if (element->group != 0) snprintf(line, sizeof(line), "Group        %u", element->group);
        ui_text(112, 95, line, THEME_TEXT, 1);
        snprintf(line, sizeof(line), "Period       %u", element->period);
        ui_text(112, 112, line, THEME_TEXT, 1);
        ui_rect(18, 138, 284, 40, THEME_SURFACE);
        ui_text(28, 147, "Classification", THEME_MUTED, 1);
        ui_text(28, 163, opencalc_element_category_name(element->category), color, 1);
    }
    ui_text(16, 201, "left/right - previous/next element", THEME_MUTED, 1);
    ui_text(16, 220, "back - periodic table", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_reference_list(void)
{
    size_t count = opencalc_reference_count(s_reference_category);
    int first = s_reference_selection - 4;
    if (first < 0) first = 0;
    if (first + 9 > (int)count) first = (int)count - 9;
    if (first < 0) first = 0;

    ui_clear(THEME_BG);
    ui_reference_header(opencalc_reference_category_name(s_reference_category));
    for (int row = 0; row < 9 && first + row < (int)count; ++row) {
        int index = first + row;
        const opencalc_reference_entry_t *entry = opencalc_reference_get(s_reference_category, (size_t)index);
        int y = 37 + row * 19;
        ui_rect(14, y, 292, 16, index == s_reference_selection ? THEME_ACCENT_2 : THEME_SURFACE);
        char label[64];
        snprintf(label, sizeof(label), "%2d  %s", index + 1, entry->title);
        ui_text(22, y + 4, label, THEME_TEXT, 1);
    }
    ui_scrollbar(312, 37, 168, (int)count, 9, first);
    ui_text(16, 211, "left/right - category", THEME_MUTED, 1);
    ui_text(16, 224, "up/down - select  enter - details", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_reference_detail(void)
{
    const opencalc_reference_entry_t *entry =
        opencalc_reference_get(s_reference_category, (size_t)s_reference_selection);
    ui_clear(THEME_BG);
    ui_reference_header(opencalc_reference_category_name(s_reference_category));
    if (entry != NULL) {
        ui_text(18, 40, entry->title, THEME_TEXT, 2);
        ui_rect(18, 67, 284, 48, THEME_SURFACE);
        ui_text(28, 75, "Formula", THEME_MUTED, 1);
        ui_text(28, 94, entry->formula, THEME_ACCENT, 1);
        ui_rect(18, 124, 284, 28, THEME_SURFACE);
        ui_text(28, 133, entry->units, THEME_TEXT, 1);
        ui_rect(18, 161, 284, 30, THEME_SURFACE);
        ui_text(28, 170, entry->note, THEME_TEXT, 1);
    }
    ui_text(16, 203, "left/right - previous/next", THEME_MUTED, 1);
    ui_text(16, 220, "back - reference list", THEME_MUTED, 1);
    ui_present();
}

static const app_info_t VARIABLES_APP = {
    "Variables", "V", 0x00d2c6, {"Browse values", NULL, NULL, NULL, NULL}
};

static const char *const VARIABLE_CATEGORY_NAMES[VARIABLE_CATEGORY_COUNT] = {
    "User Variables", "Lists", "Matrices", "Functions", "Strings",
    "Statistics", "Graph Variables", "System Variables"
};

static void variable_format_value(double real, double imag, char *out, size_t out_size)
{
    if (fabs(imag) <= 1e-12) snprintf(out, out_size, "%.10g", real);
    else if (fabs(real) <= 1e-12) snprintf(out, out_size, "%.10gi", imag);
    else snprintf(out, out_size, "%.10g%+.10gi", real, imag);
}

static int variable_custom_count(void)
{
    int count = 0;
    opencalc_variable_t variable;
    for (size_t i = 0; opencalc_math_variable_at(i, &variable); i++) {
        if (strlen(variable.name) > 1) count++;
    }
    return count;
}

static bool variable_custom_at(int wanted, opencalc_variable_t *out)
{
    opencalc_variable_t variable;
    for (size_t i = 0; opencalc_math_variable_at(i, &variable); i++) {
        if (strlen(variable.name) <= 1) continue;
        if (wanted-- == 0) { *out = variable; return true; }
    }
    return false;
}

static int variable_category_count(variable_category_t category)
{
    switch (category) {
    case VARIABLE_CATEGORY_USER:
        return 26 + variable_custom_count() + (s_variable_action == VARIABLE_ACTION_STORE ? 1 : 0);
    case VARIABLE_CATEGORY_LISTS: return LIST_COUNT;
    case VARIABLE_CATEGORY_MATRICES: return MATRIX_COUNT;
    case VARIABLE_CATEGORY_FUNCTIONS: return GRAPH_FUNC_COUNT;
    case VARIABLE_CATEGORY_STRINGS: return 1;
    case VARIABLE_CATEGORY_STATISTICS: return s_stats_result_line_count > 0 ? s_stats_result_line_count : 1;
    case VARIABLE_CATEGORY_GRAPH: return 10;
    case VARIABLE_CATEGORY_SYSTEM: return 4;
    default: return 0;
    }
}

static void variable_list_literal(int list, char *out, size_t out_size)
{
    size_t used = 0;
    if (out_size < 3) return;
    out[used++] = '{';
    for (int i = 0; i < s_list_counts[list]; i++) {
        int written = snprintf(out + used, out_size - used, "%s%.10g", i ? "," : "", s_lists[list][i]);
        if (written < 0 || (size_t)written >= out_size - used) break;
        used += (size_t)written;
    }
    if (used + 2 > out_size) used = out_size - 2;
    out[used++] = '}';
    out[used] = '\0';
}

static bool variable_browser_item(variable_category_t category, int index, variable_browser_item_t *item)
{
    if (item == NULL || index < 0 || index >= variable_category_count(category)) return false;
    memset(item, 0, sizeof(*item));
    item->selectable = true;
    switch (category) {
    case VARIABLE_CATEGORY_USER: {
        if (index < 26) {
            item->name[0] = (char)('A' + index);
            item->name[1] = '\0';
            double real = 0.0, imag = 0.0;
            bool set = opencalc_math_variable_get(item->name, &real, &imag);
            snprintf(item->type, sizeof(item->type), "%s", set ? (fabs(imag) > 1e-12 ? "Complex" : "Number") : "Unset");
            if (set) variable_format_value(real, imag, item->preview, sizeof(item->preview));
            else snprintf(item->preview, sizeof(item->preview), "not assigned");
            snprintf(item->reference, sizeof(item->reference), "%s", item->name);
            if (set) variable_format_value(real, imag, item->value, sizeof(item->value));
            item->user_variable = true;
            return true;
        }
        int custom = index - 26;
        opencalc_variable_t variable;
        if (variable_custom_at(custom, &variable)) {
            snprintf(item->name, sizeof(item->name), "%s", variable.name);
            snprintf(item->type, sizeof(item->type), "%s", fabs(variable.imag) > 1e-12 ? "Complex" : "Number");
            variable_format_value(variable.real, variable.imag, item->preview, sizeof(item->preview));
            snprintf(item->reference, sizeof(item->reference), "%s", variable.name);
            variable_format_value(variable.real, variable.imag, item->value, sizeof(item->value));
            item->user_variable = true;
            return true;
        }
        snprintf(item->name, sizeof(item->name), "+ New name");
        snprintf(item->type, sizeof(item->type), "Custom");
        snprintf(item->preview, sizeof(item->preview), "store with a descriptive name");
        item->user_variable = true;
        return true;
    }
    case VARIABLE_CATEGORY_LISTS:
        snprintf(item->name, sizeof(item->name), "L%d", index + 1);
        snprintf(item->type, sizeof(item->type), "List");
        snprintf(item->preview, sizeof(item->preview), "%d values%s", s_list_counts[index], s_list_counts[index] ? "" : " (empty)");
        snprintf(item->reference, sizeof(item->reference), "L%d", index + 1);
        variable_list_literal(index, item->value, sizeof(item->value));
        return true;
    case VARIABLE_CATEGORY_MATRICES:
        snprintf(item->name, sizeof(item->name), "Matrix %c", 'A' + index);
        snprintf(item->type, sizeof(item->type), "Matrix");
        snprintf(item->preview, sizeof(item->preview), "%d x %d", s_matrix_rows_by_index[index], s_matrix_cols_by_index[index]);
        snprintf(item->reference, sizeof(item->reference), "mat%c", 'A' + index);
        if (!calc_matrix_literal(index, item->value, sizeof(item->value))) {
            snprintf(item->value, sizeof(item->value), "%s", item->reference);
        }
        return true;
    case VARIABLE_CATEGORY_FUNCTIONS:
        snprintf(item->name, sizeof(item->name), "Y%d", index + 1);
        snprintf(item->type, sizeof(item->type), "Function");
        snprintf(item->preview, sizeof(item->preview), "%s", s_graph_exprs[index][0] ? s_graph_exprs[index] : "not defined");
        snprintf(item->reference, sizeof(item->reference), "Y%d", index + 1);
        snprintf(item->value, sizeof(item->value), "%s", s_graph_exprs[index]);
        return true;
    case VARIABLE_CATEGORY_STRINGS:
        snprintf(item->name, sizeof(item->name), "No strings");
        snprintf(item->type, sizeof(item->type), "String");
        snprintf(item->preview, sizeof(item->preview), "no string variables stored");
        item->selectable = false;
        return true;
    case VARIABLE_CATEGORY_STATISTICS:
        if (s_stats_result_line_count <= 0) {
            snprintf(item->name, sizeof(item->name), "No results");
            snprintf(item->type, sizeof(item->type), "Statistics");
            snprintf(item->preview, sizeof(item->preview), "run a statistics calculation");
            item->selectable = false;
        } else {
            snprintf(item->name, sizeof(item->name), "Stat%d", index + 1);
            snprintf(item->type, sizeof(item->type), "Statistics");
            snprintf(item->preview, sizeof(item->preview), "%s", s_stats_result_lines[index]);
            const char *equals = strchr(s_stats_result_lines[index], '=');
            if (equals != NULL) snprintf(item->value, sizeof(item->value), "%s", equals + 1);
            else item->selectable = false;
            snprintf(item->reference, sizeof(item->reference), "%s", item->name);
        }
        return true;
    case VARIABLE_CATEGORY_GRAPH: {
        static const char *const names[] = {"Xmin","Xmax","Ymin","Ymax","Xscl","Yscl","Tmin","Tmax","nMin","nMax"};
        double values[] = {s_graph_xmin,s_graph_xmax,s_graph_ymin,s_graph_ymax,s_graph_xtick,s_graph_ytick,s_graph_tmin,s_graph_tmax,s_graph_nmin,s_graph_nmax};
        snprintf(item->name, sizeof(item->name), "%s", names[index]);
        snprintf(item->type, sizeof(item->type), "Graph");
        snprintf(item->preview, sizeof(item->preview), "%.10g", values[index]);
        snprintf(item->reference, sizeof(item->reference), "%s", names[index]);
        snprintf(item->value, sizeof(item->value), "%.10g", values[index]);
        return true;
    }
    case VARIABLE_CATEGORY_SYSTEM:
        if (index == 0) {
            snprintf(item->name, sizeof(item->name), "Ans"); snprintf(item->type, sizeof(item->type), "System");
            snprintf(item->preview, sizeof(item->preview), "%s", s_calc_ans); snprintf(item->reference, sizeof(item->reference), "ANS");
            snprintf(item->value, sizeof(item->value), "%s", s_calc_ans);
        } else if (index == 1) {
            snprintf(item->name, sizeof(item->name), "Angle"); snprintf(item->type, sizeof(item->type), "Mode");
            snprintf(item->preview, sizeof(item->preview), "%s", s_angle_mode == 0 ? "Degrees" : "Radians"); item->selectable = false;
        } else if (index == 2) {
            snprintf(item->name, sizeof(item->name), "Display"); snprintf(item->type, sizeof(item->type), "Mode");
            snprintf(item->preview, sizeof(item->preview), "%s", s_display_format == 0 ? "Normal" : "Formatted"); item->selectable = false;
        } else {
            snprintf(item->name, sizeof(item->name), "Brightness"); snprintf(item->type, sizeof(item->type), "System");
            snprintf(item->preview, sizeof(item->preview), "%d%%", board_get_backlight_brightness());
            snprintf(item->value, sizeof(item->value), "%d", board_get_backlight_brightness());
            snprintf(item->reference, sizeof(item->reference), "Brightness");
        }
        return true;
    default: return false;
    }
}

static void ui_draw_variables(void)
{
    ui_clear(THEME_BG);
    ui_header(&VARIABLES_APP);
    const char *action = s_variable_action == VARIABLE_ACTION_STORE ? "STO" :
                         (s_variable_action == VARIABLE_ACTION_GET ? "GET" : "VARS");
    char heading[48];
    snprintf(heading, sizeof(heading), "%s  %s", action, VARIABLE_CATEGORY_NAMES[s_variable_category]);
    ui_text(12, 35, heading, THEME_ACCENT, 1);
    int count = variable_category_count(s_variable_category);
    if (s_variable_selection >= count) s_variable_selection = count > 0 ? count - 1 : 0;
    if (s_variable_selection < s_variable_scroll) s_variable_scroll = s_variable_selection;
    if (s_variable_selection >= s_variable_scroll + 5) s_variable_scroll = s_variable_selection - 4;
    for (int row = 0; row < 5; row++) {
        int index = s_variable_scroll + row;
        if (index >= count) break;
        variable_browser_item_t item;
        if (!variable_browser_item(s_variable_category, index, &item)) continue;
        int y = 53 + row * 29;
        bool selected = index == s_variable_selection;
        if (selected) ui_rect(8, y - 2, 304, 27, THEME_ACCENT_2);
        ui_text(14, y, item.name, selected ? THEME_ACCENT : THEME_TEXT, 1);
        ui_text(118, y, item.type, THEME_MUTED, 1);
        ui_text(14, y + 13, item.preview, item.selectable ? THEME_TEXT : THEME_MUTED, 1);
    }
    ui_scrollbar(314, 51, 143, count, 5, s_variable_scroll);
    ui_rect(0, 201, UI_W, 1, THEME_BORDER);
    ui_text(10, 207, s_variable_status[0] ? s_variable_status : "left/right category  enter select", s_variable_status[0] ? THEME_BATTERY_YELLOW : THEME_MUTED, 1);
    ui_text(10, 222, "Del delete  2nd+Enter rename  Back close", THEME_MUTED, 1);
    ui_present();
}

static void ui_draw_variable_name(void)
{
    ui_clear(THEME_BG);
    ui_header(&VARIABLES_APP);
    ui_text(18, 43, s_variable_rename_from[0] ? "Rename variable" : "New variable name", THEME_TEXT, 2);
    ui_rect(18, 81, 284, 42, THEME_BORDER);
    ui_rect(20, 83, 280, 38, THEME_SURFACE);
    ui_text(29, 96, s_variable_name, THEME_ACCENT, 2);
    if (s_cursor_blink_visible) ui_rect(29 + ui_text_width(s_variable_name, 99, 2), 94, 3, 18, THEME_ACCENT);
    ui_text(18, 139, "Letters, digits, underscore; max 15", THEME_MUTED, 1);
    ui_text(18, 158, s_variable_status, THEME_BATTERY_YELLOW, 1);
    ui_text(18, 211, "Enter save   Del erase   Back cancel", THEME_MUTED, 1);
    ui_present();
}

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
    case PAGE_CALC_RESULT: ui_draw_calc_result(); break;
    case PAGE_MATH_MENU: ui_draw_math_menu(); break;
    case PAGE_GRAPH: ui_draw_graph(); break;
    case PAGE_Y_EQUALS: ui_draw_y_equals(); break;
    case PAGE_TABLE: ui_draw_table(); break;
    case PAGE_TABLE_SETUP: ui_draw_table_setup(); break;
    case PAGE_GRAPH_WINDOW: ui_draw_graph_window(); break;
    case PAGE_GRAPH_CALC: ui_draw_graph_calc_menu(); break;
    case PAGE_GRAPH_SYMBOLIC: ui_draw_graph_symbolic(); break;
    case PAGE_GRAPH_FORMAT: ui_draw_graph_format(); break;
    case PAGE_LIST_EDITOR: ui_draw_list_editor(); break;
    case PAGE_MATRIX_EDITOR: ui_draw_matrix_editor(); break;
    case PAGE_MATRIX_VIEWER: ui_draw_matrix_viewer(); break;
    case PAGE_MATRIX_SIZE: ui_draw_matrix_size(); break;
    case PAGE_FINANCE_TVM: ui_draw_finance_tvm(); break;
    case PAGE_FINANCE_CASHFLOW: ui_draw_finance_cashflow(); break;
    case PAGE_FINANCE_RESULT: ui_draw_finance_result(); break;
    case PAGE_MODE_MENU: ui_draw_mode_menu(); break;
    case PAGE_PROGRAM_MENU: ui_draw_program_menu(); break;
    case PAGE_GAME_MENU: ui_draw_game_menu(); break;
    case PAGE_SCRIPT_IO: ui_draw_script_io(); break;
    case PAGE_SCRIPT_EDITOR: ui_draw_script_editor(); break;
    case PAGE_SOLVER_WORKFLOW: ui_draw_solver_workflow(); break;
    case PAGE_SOLVER_SYSTEM_RESULT: ui_draw_solver_system_result(); break;
    case PAGE_SOLVER_SYMBOLIC_RESULT: ui_draw_solver_symbolic_result(); break;
    case PAGE_SOLVER_SAVED: ui_draw_solver_saved(); break;
    case PAGE_SOLVER_ROOTS: ui_draw_solver_roots(); break;
    case PAGE_STATS_CATEGORY: ui_draw_stats_category(); break;
    case PAGE_STATS_ONE_VAR: ui_draw_stats_one_var(); break;
    case PAGE_STATS_SETUP: ui_draw_stats_setup(); break;
    case PAGE_STATS_RESULT: ui_draw_stats_result(); break;
    case PAGE_STATS_PLOT: ui_draw_stats_plot(); break;
    case PAGE_REFERENCE_HOME: ui_draw_reference_home(); break;
    case PAGE_PERIODIC_TABLE: ui_draw_periodic_table(); break;
    case PAGE_ELEMENT_DETAIL: ui_draw_element_detail(); break;
    case PAGE_REFERENCE_LIST: ui_draw_reference_list(); break;
    case PAGE_REFERENCE_DETAIL: ui_draw_reference_detail(); break;
    case PAGE_CONIC_EDITOR: ui_draw_conic_editor(); break;
    case PAGE_CONIC_RESULT: ui_draw_conic_result(); break;
    case PAGE_CONIC_GRAPHS: ui_draw_conic_graphs(); break;
    case PAGE_INEQ_EDITOR: ui_draw_inequality_editor(); break;
    case PAGE_INEQ_RESULT: ui_draw_inequality_result(); break;
    case PAGE_VARIABLES: ui_draw_variables(); break;
    case PAGE_VARIABLE_NAME: ui_draw_variable_name(); break;
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

static const char *game_validation_status(game_id_t game)
{
    switch (game) {
    case GAME_DOOM:
        return "HW check: LCD/input/storage/audio";
    case GAME_MARIO:
        return "HW check: LCD/input/timing/audio";
    default:
        return "ready";
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

    ui_text(12, 205, game_validation_status(selected_game_id()), THEME_MUTED, 1);
    ui_text(12, 218, "enter - play  back - home", THEME_MUTED, 1);
    ui_present();
}

static void remember_app_transition(app_id_t app)
{
    if (app != s_current_app) s_previous_app = s_current_app;
}

static void ui_open_selected_app(void)
{
    app_id_t selected = (app_id_t)s_home_selection;
#if OPENCALC_EXPORT_USB_STORAGE_TO_HOST
    if (s_usb_storage_enabled && usb_msc_mount_app()) s_usb_storage_enabled = false;
#endif
    remember_app_transition(selected);
    s_current_app = selected;
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

    opencalc_audio_game_end();

    s_active_game = GAME_NONE;
    vTaskDelay(pdMS_TO_TICKS(30));
#if OPENCALC_EXPORT_USB_STORAGE_TO_HOST
    (void)worksheet_persist_flush();
    s_usb_storage_enabled = usb_msc_mount_usb();
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

    (void)worksheet_persist_flush();

    if (game == GAME_TETRIS) {
        opencalc_audio_game_begin();
        opencalc_tetris_enter();
        s_active_game = GAME_TETRIS;
        printf("tetris on\n");
        return;
    }
    if (game == GAME_SNAKE) {
        opencalc_audio_game_begin();
        opencalc_snake_enter();
        s_active_game = GAME_SNAKE;
        printf("snake on\n");
        return;
    }
    if (game == GAME_BREAKOUT) {
        opencalc_audio_game_begin();
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
        s_usb_storage_enabled = false;
        if (!opencalc_doom_wad_available()) {
            printf("Doom WAD missing: copy doom1.wad to the USB drive root\n");
            s_page = PAGE_GAME_MENU;
            ui_draw_current();
#if OPENCALC_EXPORT_USB_STORAGE_TO_HOST
            (void)worksheet_persist_flush();
            s_usb_storage_enabled = usb_msc_mount_usb();
#endif
            return;
        }
        opencalc_audio_game_begin();
        if (opencalc_doom_start()) {
            s_active_game = GAME_DOOM;
            printf("doom on\n");
            printf("doom validation: check LCD colors/cropping, held keys, storage reads, audio\n");
            return;
        }
        printf("Doom initialization failed; see resource logs above\n");
        opencalc_audio_game_end();
        s_page = PAGE_GAME_MENU;
        ui_draw_current();
#if OPENCALC_EXPORT_USB_STORAGE_TO_HOST
        (void)worksheet_persist_flush();
        s_usb_storage_enabled = usb_msc_mount_usb();
#endif
#else
        printf("doom disabled\n");
#endif
        return;
    }

    if (game == GAME_MARIO) {
        if (!usb_msc_mount_app()) {
            printf("Mario could not take ownership of USB storage\n");
            s_page = PAGE_GAME_MENU;
            ui_draw_current();
            return;
        }
        s_usb_storage_enabled = false;
        if (!opencalc_mario_rom_available()) {
            printf("Mario ROM missing: copy mario.nes to the USB drive root\n");
            s_page = PAGE_GAME_MENU;
            ui_draw_current();
#if OPENCALC_EXPORT_USB_STORAGE_TO_HOST
            (void)worksheet_persist_flush();
            s_usb_storage_enabled = usb_msc_mount_usb();
#endif
            return;
        }
        opencalc_audio_game_begin();
        opencalc_mario_enter();
        if (!opencalc_mario_active()) {
            opencalc_audio_game_end();
            s_page = PAGE_GAME_MENU;
            ui_draw_current();
#if OPENCALC_EXPORT_USB_STORAGE_TO_HOST
            (void)worksheet_persist_flush();
            s_usb_storage_enabled = usb_msc_mount_usb();
#endif
            return;
        }
        s_active_game = GAME_MARIO;
        printf("mario on\n");
        printf("mario validation: check LCD scaling, held keys, timing, audio\n");
        return;
    }
}

static void power_off_calculator(void)
{
    printf("software off\n");
    opencalc_audio_game_end();
    (void)worksheet_persist_flush();
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

static void adjust_audio_volume(int delta)
{
    int next = s_audio_volume_percent + delta;
    if (next < 0) next = 0;
    if (next > 100) next = 100;
    next = (next / 5) * 5;
    s_audio_volume_percent = next;
    opencalc_audio_set_volume_percent(s_audio_volume_percent);
    opencalc_persist_set_u32("audio_volume", (uint32_t)s_audio_volume_percent);
    printf("audio volume %d%%\n", s_audio_volume_percent);
    ui_draw_current();
}

static void status_message(const char *text)
{
    snprintf(s_calc_output, sizeof(s_calc_output), "%s", text);
    printf("%s\n", s_calc_output);
    ui_draw_current();
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

static void open_graph_format_menu(void)
{
    int count = graph_series_count();
    if (s_graph_style_series >= count) s_graph_style_series = count - 1;
    if (s_graph_style_series < 0) s_graph_style_series = 0;
    s_graph_format_selection = 0;
    s_graph_zoom_mode = false;
    s_page = PAGE_GRAPH_FORMAT;
    s_current_app = APP_GRAPH;
    ui_draw_current();
}

static void adjust_graph_format_value(int delta)
{
    int count = graph_series_count();
    switch (s_graph_format_selection) {
    case 0:
        s_graph_split = !s_graph_split;
        break;
    case 1:
        if (count > 0) s_graph_style_series = (s_graph_style_series + delta + count) % count;
        break;
    case 2:
        s_graph_styles[s_graph_style_series] =
            (uint8_t)((s_graph_styles[s_graph_style_series] + delta + GRAPH_STYLE_COUNT) % GRAPH_STYLE_COUNT);
        break;
    case 3:
        s_graph_background_enabled = !s_graph_background_enabled;
        if (s_graph_background_enabled && !s_graph_background_loaded) {
            s_graph_background_loaded = graph_background_load();
            if (!s_graph_background_loaded) s_graph_background_enabled = false;
        }
        break;
    case 4:
        s_graph_background_loaded = graph_background_load();
        if (s_graph_background_loaded) s_graph_background_enabled = true;
        break;
    case 5:
        s_graph_grid = !s_graph_grid;
        break;
    default:
        break;
    }
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
    if (s_mode_selection == 3) {
        s_graph_selection = 0;
        s_graph_style_series = 0;
        s_graph_trace = false;
        s_graph_zoom_mode = false;
        s_graph_status[0] = '\0';
        s_table_x_start = 0.0;
        s_table_func_start = 0;
        s_graph_tmax = s_angle_mode == 0 ? 360.0 : 6.28318530717958647692;
    } else if (s_mode_selection == 2 && s_graphing_mode == 2) {
        s_graph_tmin = 0.0;
        s_graph_tmax = s_angle_mode == 0 ? 360.0 : 6.28318530717958647692;
    }
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

static double *matrix_alloc_values(size_t count)
{
    if (count > SIZE_MAX / sizeof(double)) return NULL;
    size_t bytes = count * sizeof(double);
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (bytes > psram_free || psram_free - bytes < OPENCALC_PSRAM_RESERVE_BYTES) {
        return NULL;
    }
    double *values = heap_caps_calloc(count, sizeof(double), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return values;
}

static double *matrix_at(double *matrix, int cols, int row, int col)
{
    return &matrix[(size_t)row * (size_t)cols + (size_t)col];
}

static bool matrix_parse_calc_input(void)
{
    if (s_calc_input[0] == '\0') {
        return false;
    }

    double *temp = matrix_alloc_values((size_t)MATRIX_MAX_N * (size_t)MATRIX_MAX_N);
    if (temp == NULL) {
        return false;
    }
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
                free(temp);
                return false;
            }
            if (cols < 0) {
                cols = current_cols;
            } else if (current_cols != cols) {
                free(temp);
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
            free(temp);
            return false;
        }

        char *end = NULL;
        double value = strtod(p, &end);
        if (end == p || !isfinite(value)) {
            free(temp);
            return false;
        }
        *matrix_at(temp, MATRIX_MAX_N, rows, current_cols++) = value;
        p = end;
    }

    if (current_cols > 0) {
        if (cols < 0) {
            cols = current_cols;
        } else if (current_cols != cols) {
            free(temp);
            return false;
        }
        rows++;
    }

    if (rows <= 0 || cols <= 0 || rows > MATRIX_MAX_N || cols > MATRIX_MAX_N) {
        free(temp);
        return false;
    }

    memset(s_matrix_a, 0, sizeof(s_matrix_a));
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            s_matrix_a[r][c] = *matrix_at(temp, MATRIX_MAX_N, r, c);
        }
    }
    free(temp);
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
    double *m = matrix_alloc_values((size_t)n * (size_t)n);
    if (m == NULL) {
        return false;
    }
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            *matrix_at(m, n, r, c) = s_matrix_a[r][c];
        }
    }
    double result = 1.0;
    int sign = 1;

    for (int col = 0; col < n; col++) {
        int pivot = col;
        for (int r = col + 1; r < n; r++) {
            if (fabs(*matrix_at(m, n, r, col)) > fabs(*matrix_at(m, n, pivot, col))) {
                pivot = r;
            }
        }
        if (fabs(*matrix_at(m, n, pivot, col)) < 1e-12) {
            *det = 0.0;
            free(m);
            return true;
        }
        if (pivot != col) {
            for (int c = 0; c < n; c++) {
                double tmp = *matrix_at(m, n, col, c);
                *matrix_at(m, n, col, c) = *matrix_at(m, n, pivot, c);
                *matrix_at(m, n, pivot, c) = tmp;
            }
            sign = -sign;
        }
        double pivot_value = *matrix_at(m, n, col, col);
        result *= pivot_value;
        for (int r = col + 1; r < n; r++) {
            double factor = *matrix_at(m, n, r, col) / pivot_value;
            for (int c = col; c < n; c++) {
                *matrix_at(m, n, r, c) -= factor * *matrix_at(m, n, col, c);
            }
        }
    }

    *det = result * (double)sign;
    free(m);
    return true;
}

static bool matrix_inverse_a(void)
{
    if (s_matrix_rows == 0 || s_matrix_rows != s_matrix_cols) {
        return false;
    }

    int n = s_matrix_rows;
    int aug_cols = n * 2;
    double *aug = matrix_alloc_values((size_t)n * (size_t)aug_cols);
    if (aug == NULL) {
        return false;
    }
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            *matrix_at(aug, aug_cols, r, c) = s_matrix_a[r][c];
        }
        *matrix_at(aug, aug_cols, r, n + r) = 1.0;
    }

    for (int col = 0; col < n; col++) {
        int pivot = col;
        for (int r = col + 1; r < n; r++) {
            if (fabs(*matrix_at(aug, aug_cols, r, col)) > fabs(*matrix_at(aug, aug_cols, pivot, col))) {
                pivot = r;
            }
        }
        if (fabs(*matrix_at(aug, aug_cols, pivot, col)) < 1e-12) {
            free(aug);
            return false;
        }
        if (pivot != col) {
            for (int c = 0; c < n * 2; c++) {
                double tmp = *matrix_at(aug, aug_cols, col, c);
                *matrix_at(aug, aug_cols, col, c) = *matrix_at(aug, aug_cols, pivot, c);
                *matrix_at(aug, aug_cols, pivot, c) = tmp;
            }
        }
        double pivot_value = *matrix_at(aug, aug_cols, col, col);
        for (int c = 0; c < n * 2; c++) {
            *matrix_at(aug, aug_cols, col, c) /= pivot_value;
        }
        for (int r = 0; r < n; r++) {
            if (r == col) {
                continue;
            }
            double factor = *matrix_at(aug, aug_cols, r, col);
            for (int c = 0; c < n * 2; c++) {
                *matrix_at(aug, aug_cols, r, c) -= factor * *matrix_at(aug, aug_cols, col, c);
            }
        }
    }

    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            s_matrix_a[r][c] = *matrix_at(aug, aug_cols, r, n + c);
        }
    }
    free(aug);
    return true;
}

static int matrix_next_index(void)
{
    return (s_matrix_index + 1) % MATRIX_COUNT;
}

static bool matrix_add_subtract_next(bool subtract)
{
    int other = matrix_next_index();
    if (s_matrix_rows <= 0 || s_matrix_cols <= 0 ||
        s_matrix_rows != s_matrix_rows_by_index[other] ||
        s_matrix_cols != s_matrix_cols_by_index[other]) {
        return false;
    }

    size_t cells = (size_t)s_matrix_rows * (size_t)s_matrix_cols;
    double *result = matrix_alloc_values(cells);
    if (result == NULL) return false;
    for (int row = 0; row < s_matrix_rows; row++) {
        for (int col = 0; col < s_matrix_cols; col++) {
            double rhs = s_matrices[other][row][col];
            double value = s_matrix_a[row][col] + (subtract ? -rhs : rhs);
            if (!isfinite(value)) {
                free(result);
                return false;
            }
            *matrix_at(result, s_matrix_cols, row, col) = value;
        }
    }
    for (int row = 0; row < s_matrix_rows; row++) {
        for (int col = 0; col < s_matrix_cols; col++) {
            s_matrix_a[row][col] = *matrix_at(result, s_matrix_cols, row, col);
        }
    }
    free(result);
    return true;
}

static bool matrix_multiply_next(void)
{
    int other = matrix_next_index();
    int right_rows = s_matrix_rows_by_index[other];
    int right_cols = s_matrix_cols_by_index[other];
    if (s_matrix_rows <= 0 || s_matrix_cols <= 0 || right_rows <= 0 ||
        right_cols <= 0 || s_matrix_cols != right_rows) {
        return false;
    }

    const int result_rows = s_matrix_rows;
    const int shared = s_matrix_cols;
    double *result = matrix_alloc_values((size_t)result_rows * (size_t)right_cols);
    if (result == NULL) return false;

    for (int row = 0; row < result_rows; row++) {
        for (int inner = 0; inner < shared; inner++) {
            double left = s_matrix_a[row][inner];
            for (int col = 0; col < right_cols; col++) {
                *matrix_at(result, right_cols, row, col) +=
                    left * s_matrices[other][inner][col];
                if (!isfinite(*matrix_at(result, right_cols, row, col))) {
                    free(result);
                    return false;
                }
            }
        }
    }

    memset(s_matrix_a, 0, sizeof(s_matrix_a));
    for (int row = 0; row < result_rows; row++) {
        for (int col = 0; col < right_cols; col++) {
            s_matrix_a[row][col] = *matrix_at(result, right_cols, row, col);
        }
    }
    free(result);
    s_matrix_rows = result_rows;
    s_matrix_cols = right_cols;
    return true;
}

static bool matrix_calc_value(double *value)
{
    if (value == NULL) return false;
    const char *source = s_calc_input[0] != '\0' ? s_calc_input : s_calc_ans;
    if (source[0] == '\0') return false;

    char expression[CALC_EXPR_MAX];
    calc_expand_ans(source, expression, sizeof(expression));
    return opencalc_math_eval_expression(expression, value) && isfinite(*value);
}

static bool matrix_scalar_multiply(double scalar)
{
    if (s_matrix_rows <= 0 || s_matrix_cols <= 0 || !isfinite(scalar)) return false;
    for (int row = 0; row < s_matrix_rows; row++) {
        for (int col = 0; col < s_matrix_cols; col++) {
            if (!isfinite(s_matrix_a[row][col] * scalar)) return false;
        }
    }
    for (int row = 0; row < s_matrix_rows; row++) {
        for (int col = 0; col < s_matrix_cols; col++) {
            s_matrix_a[row][col] *= scalar;
        }
    }
    return true;
}

static bool matrix_square_multiply(const double *left, const double *right,
                                   double *result, int size)
{
    memset(result, 0, (size_t)size * (size_t)size * sizeof(*result));
    for (int row = 0; row < size; row++) {
        for (int inner = 0; inner < size; inner++) {
            double value = *matrix_at((double *)left, size, row, inner);
            for (int col = 0; col < size; col++) {
                *matrix_at(result, size, row, col) +=
                    value * *matrix_at((double *)right, size, inner, col);
                if (!isfinite(*matrix_at(result, size, row, col))) return false;
            }
        }
    }
    return true;
}

static bool matrix_power_a(int exponent)
{
    if (s_matrix_rows <= 0 || s_matrix_rows != s_matrix_cols ||
        exponent < -999 || exponent > 999) {
        return false;
    }

    int size = s_matrix_rows;
    size_t cells = (size_t)size * (size_t)size;
    double *base = matrix_alloc_values(cells);
    double *result = matrix_alloc_values(cells);
    double *scratch = matrix_alloc_values(cells);
    if (base == NULL || result == NULL || scratch == NULL) {
        free(base);
        free(result);
        free(scratch);
        return false;
    }

    if (exponent < 0) {
        double *original = matrix_alloc_values(cells);
        if (original == NULL) {
            free(base);
            free(result);
            free(scratch);
            return false;
        }
        for (int row = 0; row < size; row++) {
            for (int col = 0; col < size; col++) {
                *matrix_at(original, size, row, col) = s_matrix_a[row][col];
            }
        }
        if (!matrix_inverse_a()) {
            free(original);
            free(base);
            free(result);
            free(scratch);
            return false;
        }
        for (int row = 0; row < size; row++) {
            for (int col = 0; col < size; col++) {
                *matrix_at(base, size, row, col) = s_matrix_a[row][col];
                s_matrix_a[row][col] = *matrix_at(original, size, row, col);
            }
        }
        free(original);
        exponent = -exponent;
    } else {
        for (int row = 0; row < size; row++) {
            for (int col = 0; col < size; col++) {
                *matrix_at(base, size, row, col) = s_matrix_a[row][col];
            }
        }
    }
    for (int row = 0; row < size; row++) {
        *matrix_at(result, size, row, row) = 1.0;
    }

    while (exponent > 0) {
        if ((exponent & 1) != 0) {
            if (!matrix_square_multiply(result, base, scratch, size)) {
                free(base);
                free(result);
                free(scratch);
                return false;
            }
            double *swap = result;
            result = scratch;
            scratch = swap;
        }
        exponent >>= 1;
        if (exponent > 0) {
            if (!matrix_square_multiply(base, base, scratch, size)) {
                free(base);
                free(result);
                free(scratch);
                return false;
            }
            double *swap = base;
            base = scratch;
            scratch = swap;
        }
    }

    memset(s_matrix_a, 0, sizeof(s_matrix_a));
    for (int row = 0; row < size; row++) {
        for (int col = 0; col < size; col++) {
            s_matrix_a[row][col] = *matrix_at(result, size, row, col);
        }
    }
    free(base);
    free(result);
    free(scratch);
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

static bool matrix_ref_a(void)
{
    if (s_matrix_rows == 0 || s_matrix_cols == 0) return false;

    int lead = 0;
    for (int row = 0; row < s_matrix_rows && lead < s_matrix_cols; row++) {
        int pivot = row;
        while (pivot < s_matrix_rows && fabs(s_matrix_a[pivot][lead]) < 1e-12) pivot++;
        if (pivot == s_matrix_rows) {
            lead++;
            row--;
            continue;
        }
        if (pivot != row) {
            for (int col = 0; col < s_matrix_cols; col++) {
                double swap = s_matrix_a[row][col];
                s_matrix_a[row][col] = s_matrix_a[pivot][col];
                s_matrix_a[pivot][col] = swap;
            }
        }
        double pivot_value = s_matrix_a[row][lead];
        for (int col = lead; col < s_matrix_cols; col++) {
            s_matrix_a[row][col] /= pivot_value;
            if (fabs(s_matrix_a[row][col]) < 1e-10) s_matrix_a[row][col] = 0.0;
        }
        for (int lower = row + 1; lower < s_matrix_rows; lower++) {
            double factor = s_matrix_a[lower][lead];
            for (int col = lead; col < s_matrix_cols; col++) {
                s_matrix_a[lower][col] -= factor * s_matrix_a[row][col];
                if (fabs(s_matrix_a[lower][col]) < 1e-10) s_matrix_a[lower][col] = 0.0;
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

    double *temp = matrix_alloc_values((size_t)s_matrix_cols * (size_t)s_matrix_rows);
    if (temp == NULL) {
        return false;
    }
    for (int r = 0; r < s_matrix_rows; r++) {
        for (int c = 0; c < s_matrix_cols; c++) {
            *matrix_at(temp, s_matrix_rows, c, r) = s_matrix_a[r][c];
        }
    }
    int old_rows = s_matrix_rows;
    s_matrix_rows = s_matrix_cols;
    s_matrix_cols = old_rows;
    memset(s_matrix_a, 0, sizeof(s_matrix_a));
    for (int r = 0; r < s_matrix_rows; r++) {
        for (int c = 0; c < s_matrix_cols; c++) {
            s_matrix_a[r][c] = *matrix_at(temp, s_matrix_cols, r, c);
        }
    }
    free(temp);
    return true;
}

static bool matrix_set_identity(void)
{
    int size = 3;
    if (s_matrix_rows > 0 || s_matrix_cols > 0) {
        if (s_matrix_rows <= 0 || s_matrix_rows != s_matrix_cols) return false;
        size = s_matrix_rows;
    }
    memset(s_matrix_a, 0, sizeof(s_matrix_a));
    s_matrix_rows = size;
    s_matrix_cols = size;
    for (int i = 0; i < size; i++) {
        s_matrix_a[i][i] = 1.0;
    }
    return true;
}

static bool matrix_augment_with_next(void)
{
    int other = matrix_next_index();
    int other_rows = s_matrix_rows_by_index[other];
    int other_cols = s_matrix_cols_by_index[other];
    if (s_matrix_rows == 0 || other_rows == 0 || s_matrix_rows != other_rows ||
        s_matrix_cols + other_cols > MATRIX_MAX_N) {
        return false;
    }

    for (int r = 0; r < s_matrix_rows; r++) {
        for (int c = 0; c < other_cols; c++) {
            s_matrix_a[r][s_matrix_cols + c] = s_matrices[other][r][c];
        }
    }
    s_matrix_cols += other_cols;
    return true;
}

static bool matrix_extract_from_calc(void)
{
    if (s_calc_input[0] == '\0' || s_matrix_rows <= 0 || s_matrix_cols <= 0) return false;

    char operation[8] = "";
    int first = 0;
    int second = 0;
    int third = 0;
    int fourth = 0;
    int parsed = sscanf(s_calc_input, " %7[^, ] , %d , %d , %d , %d",
                        operation, &first, &second, &third, &fourth);
    for (char *ch = operation; *ch != '\0'; ch++) {
        *ch = (char)tolower((unsigned char)*ch);
    }

    int row_start = 0;
    int row_end = s_matrix_rows - 1;
    int col_start = 0;
    int col_end = s_matrix_cols - 1;
    if (strcmp(operation, "row") == 0 && parsed == 2) {
        row_start = row_end = first - 1;
    } else if (strcmp(operation, "col") == 0 && parsed == 2) {
        col_start = col_end = first - 1;
    } else if (strcmp(operation, "sub") == 0 && parsed == 5) {
        row_start = first - 1;
        col_start = second - 1;
        row_end = third - 1;
        col_end = fourth - 1;
    } else {
        return false;
    }
    if (row_start < 0 || col_start < 0 || row_end < row_start || col_end < col_start ||
        row_end >= s_matrix_rows || col_end >= s_matrix_cols) {
        return false;
    }

    int result_rows = row_end - row_start + 1;
    int result_cols = col_end - col_start + 1;
    double *result = matrix_alloc_values((size_t)result_rows * (size_t)result_cols);
    if (result == NULL) return false;
    for (int row = 0; row < result_rows; row++) {
        for (int col = 0; col < result_cols; col++) {
            *matrix_at(result, result_cols, row, col) =
                s_matrix_a[row_start + row][col_start + col];
        }
    }
    memset(s_matrix_a, 0, sizeof(s_matrix_a));
    for (int row = 0; row < result_rows; row++) {
        for (int col = 0; col < result_cols; col++) {
            s_matrix_a[row][col] = *matrix_at(result, result_cols, row, col);
        }
    }
    free(result);
    s_matrix_rows = result_rows;
    s_matrix_cols = result_cols;
    return true;
}

static bool matrix_to_selected_list(void)
{
    if (s_matrix_rows == 0 || s_matrix_cols == 0 || s_matrix_rows > LIST_MAX_VALUES) {
        return false;
    }
    for (int r = 0; r < s_matrix_rows; r++) {
        s_lists[s_list_index][r] = s_matrix_a[r][0];
    }
    s_list_counts[s_list_index] = s_matrix_rows;
    if (s_list_cursor > s_list_counts[s_list_index]) {
        s_list_cursor = s_list_counts[s_list_index];
    }
    return true;
}

static bool matrix_from_selected_list(void)
{
    if (s_list_counts[s_list_index] <= 0 || s_list_counts[s_list_index] > MATRIX_MAX_N) {
        return false;
    }
    memset(s_matrix_a, 0, sizeof(s_matrix_a));
    s_matrix_rows = s_list_counts[s_list_index];
    s_matrix_cols = 1;
    for (int r = 0; r < s_matrix_rows; r++) {
        s_matrix_a[r][0] = s_lists[s_list_index][r];
    }
    return true;
}

static void matrix_reset_navigation(void)
{
    s_matrix_cursor_row = 0;
    s_matrix_cursor_col = 0;
    s_matrix_scroll_row = 0;
    s_matrix_scroll_col = 0;
    s_matrix_entry[0] = '\0';
    s_matrix_entry_active = false;
    s_matrix_status[0] = '\0';
}

static void matrix_open_editor(void)
{
    if (s_matrix_rows <= 0 || s_matrix_cols <= 0) {
        memset(s_matrix_a, 0, sizeof(s_matrix_a));
        s_matrix_rows = 3;
        s_matrix_cols = 3;
        matrix_reset_navigation();
    }
    matrix_keep_cursor_visible(6, 4);
    s_matrix_entry[0] = '\0';
    s_matrix_entry_active = false;
    s_page = PAGE_MATRIX_EDITOR;
    s_current_app = APP_MATRICES;
    ui_draw_current();
}

static void matrix_open_viewer(void)
{
    if (s_matrix_rows <= 0 || s_matrix_cols <= 0) {
        snprintf(s_calc_output, sizeof(s_calc_output), "Matrix %c is empty", 'A' + s_matrix_index);
        ui_draw_current();
        return;
    }
    matrix_keep_cursor_visible(7, 4);
    s_page = PAGE_MATRIX_VIEWER;
    s_current_app = APP_MATRICES;
    ui_draw_current();
}

static void matrix_open_size(void)
{
    s_matrix_size_rows = s_matrix_rows > 0 ? s_matrix_rows : 3;
    s_matrix_size_cols = s_matrix_cols > 0 ? s_matrix_cols : 3;
    s_matrix_size_selection = 0;
    s_matrix_size_typing = false;
    s_page = PAGE_MATRIX_SIZE;
    s_current_app = APP_MATRICES;
    ui_draw_current();
}

static void matrix_apply_size(void)
{
    if (s_matrix_size_rows < 1) s_matrix_size_rows = 1;
    if (s_matrix_size_cols < 1) s_matrix_size_cols = 1;
    if (s_matrix_size_rows > MATRIX_MAX_N) s_matrix_size_rows = MATRIX_MAX_N;
    if (s_matrix_size_cols > MATRIX_MAX_N) s_matrix_size_cols = MATRIX_MAX_N;

    int old_rows = s_matrix_rows;
    int old_cols = s_matrix_cols;
    for (int row = 0; row < MATRIX_MAX_N; row++) {
        for (int col = 0; col < MATRIX_MAX_N; col++) {
            if (row >= s_matrix_size_rows || col >= s_matrix_size_cols ||
                row >= old_rows || col >= old_cols) {
                s_matrix_a[row][col] = 0.0;
            }
        }
    }
    s_matrix_rows = s_matrix_size_rows;
    s_matrix_cols = s_matrix_size_cols;
    matrix_reset_navigation();
    snprintf(s_calc_output, sizeof(s_calc_output), "Matrix %c resized to %dx%d",
             'A' + s_matrix_index, s_matrix_rows, s_matrix_cols);
    matrix_open_editor();
}

bool opencalc_ui_matrix_boot_stress_test(void)
{
    const int saved_index = s_matrix_index;
    const size_t internal_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t psram_before = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    bool passed = true;

    s_matrix_index = MATRIX_COUNT - 1;
    memset(s_matrix_a, 0, sizeof(s_matrix_a));
    s_matrix_rows = MATRIX_MAX_N;
    s_matrix_cols = MATRIX_MAX_N;
    for (int index = 0; index < MATRIX_MAX_N; index++) {
        s_matrix_a[index][index] = (index & 1) != 0 ? 2.0 : 1.0;
    }

    double determinant = 0.0;
    passed = matrix_det_a(&determinant) && isfinite(determinant) && determinant > 1.0;
    passed = matrix_inverse_a() && passed;
    passed = fabs(s_matrix_a[0][0] - 1.0) < 1e-12 &&
             fabs(s_matrix_a[MATRIX_MAX_N - 1][MATRIX_MAX_N - 1] - 1.0) < 1e-12 && passed;
    passed = matrix_transpose_a() && matrix_rref_a() && passed;
    passed = fabs(s_matrix_a[0][0] - 1.0) < 1e-12 &&
             fabs(s_matrix_a[MATRIX_MAX_N - 1][MATRIX_MAX_N - 1] - 1.0) < 1e-12 &&
             fabs(s_matrix_a[0][MATRIX_MAX_N - 1]) < 1e-12 && passed;

    s_matrix_index = MATRIX_COUNT - 3;
    memset(s_matrix_a, 0, sizeof(s_matrix_a));
    memset(s_matrices[matrix_next_index()], 0, sizeof(s_matrices[matrix_next_index()]));
    s_matrix_rows = 2;
    s_matrix_cols = 2;
    s_matrix_a[0][0] = 1.0;
    s_matrix_a[0][1] = 2.0;
    s_matrix_a[1][0] = 3.0;
    s_matrix_a[1][1] = 4.0;
    s_matrix_rows_by_index[matrix_next_index()] = 2;
    s_matrix_cols_by_index[matrix_next_index()] = 2;
    s_matrices[matrix_next_index()][0][0] = 1.0;
    s_matrices[matrix_next_index()][1][1] = 1.0;
    passed = matrix_add_subtract_next(false) &&
             fabs(s_matrix_a[0][0] - 2.0) < 1e-12 && passed;
    passed = matrix_add_subtract_next(true) &&
             fabs(s_matrix_a[1][1] - 4.0) < 1e-12 && passed;
    passed = matrix_multiply_next() &&
             fabs(s_matrix_a[0][1] - 2.0) < 1e-12 && passed;
    passed = matrix_scalar_multiply(2.0) && matrix_scalar_multiply(0.5) && passed;
    passed = matrix_power_a(2) && fabs(s_matrix_a[0][0] - 7.0) < 1e-12 &&
             fabs(s_matrix_a[1][1] - 22.0) < 1e-12 && passed;
    s_matrix_a[0][0] = 1.0;
    s_matrix_a[0][1] = 2.0;
    s_matrix_a[1][0] = 3.0;
    s_matrix_a[1][1] = 4.0;
    passed = matrix_ref_a() && fabs(s_matrix_a[0][0] - 1.0) < 1e-12 &&
             fabs(s_matrix_a[1][0]) < 1e-12 && passed;

    s_matrix_cursor_row = MATRIX_MAX_N - 1;
    s_matrix_cursor_col = MATRIX_MAX_N - 1;
    s_matrix_scroll_row = 0;
    s_matrix_scroll_col = 0;
    matrix_keep_cursor_visible(7, 4);
    passed = s_matrix_scroll_row == MATRIX_MAX_N - 7 &&
             s_matrix_scroll_col == MATRIX_MAX_N - 4 && passed;

    memset(s_matrix_a, 0, sizeof(s_matrix_a));
    s_matrix_rows = 0;
    s_matrix_cols = 0;
    memset(s_matrices[matrix_next_index()], 0, sizeof(s_matrices[matrix_next_index()]));
    s_matrix_rows_by_index[matrix_next_index()] = 0;
    s_matrix_cols_by_index[matrix_next_index()] = 0;
    matrix_reset_navigation();
    s_matrix_index = saved_index;

    const size_t internal_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t psram_after = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    passed = internal_after == internal_before && psram_after == psram_before && passed;
    printf("Matrix 99x99 stress test: %s (internal %u->%u, PSRAM %u->%u)\n",
           passed ? "PASS" : "FAIL",
           (unsigned)internal_before, (unsigned)internal_after,
           (unsigned)psram_before, (unsigned)psram_after);
    return passed;
}

static bool matrix_apply_row_command(void)
{
    if (s_calc_input[0] == '\0' || s_matrix_rows == 0 || s_matrix_cols == 0) {
        return false;
    }

    char op[12];
    const char *p = s_calc_input;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    int op_len = 0;
    while (p[op_len] != '\0' && p[op_len] != ',' && p[op_len] != ' ' && p[op_len] != '\t' &&
           op_len < (int)sizeof(op) - 1) {
        op[op_len] = p[op_len];
        op_len++;
    }
    op[op_len] = '\0';
    p += op_len;

    for (char *q = op; *q != '\0'; q++) {
        *q = (char)tolower((unsigned char)*q);
    }

    double values[3] = {0.0};
    int value_count = 0;
    while (*p != '\0' && value_count < 3) {
        while (*p == ' ' || *p == '\t' || *p == ',') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        char *end = NULL;
        values[value_count] = strtod(p, &end);
        if (end == p) {
            return false;
        }
        value_count++;
        p = end;
    }

    if (strcmp(op, "swap") == 0) {
        int row_a = (int)llround(values[0]);
        int row_b = (int)llround(values[1]);
        if (value_count < 2 || row_a < 1 || row_a > s_matrix_rows || row_b < 1 || row_b > s_matrix_rows) {
            return false;
        }
        row_a--;
        row_b--;
        for (int c = 0; c < s_matrix_cols; c++) {
            double tmp = s_matrix_a[row_a][c];
            s_matrix_a[row_a][c] = s_matrix_a[row_b][c];
            s_matrix_a[row_b][c] = tmp;
        }
        return true;
    }

    if (strcmp(op, "scale") == 0) {
        int row_a = (int)llround(values[0]);
        double factor = values[1];
        if (value_count < 2 || row_a < 1 || row_a > s_matrix_rows || !isfinite(factor)) {
            return false;
        }
        row_a--;
        for (int c = 0; c < s_matrix_cols; c++) {
            s_matrix_a[row_a][c] *= factor;
        }
        return true;
    }

    if (strcmp(op, "add") == 0) {
        int row_a = (int)llround(values[0]);
        int row_b = (int)llround(values[1]);
        double factor = values[2];
        if (value_count < 3 || row_a < 1 || row_a > s_matrix_rows || row_b < 1 || row_b > s_matrix_rows ||
            !isfinite(factor)) {
            return false;
        }
        int src = row_a - 1;
        int dst = row_b - 1;
        for (int c = 0; c < s_matrix_cols; c++) {
            s_matrix_a[dst][c] += factor * s_matrix_a[src][c];
            if (fabs(s_matrix_a[dst][c]) < 1e-10) {
                s_matrix_a[dst][c] = 0.0;
            }
        }
        return true;
    }

    return false;
}

static bool solver_set_guess_from_calc(void)
{
    const char *source = s_calc_input[0] ? s_calc_input : s_calc_ans;
    char eval_expr[CALC_EXPR_MAX + CALC_RESULT_MAX];
    double value = 0.0;
    calc_expand_ans(source, eval_expr, sizeof(eval_expr));
    if (!opencalc_math_eval_expression(eval_expr, &value)) {
        return false;
    }
    s_solver_guess = value;
    return true;
}

static bool solver_number_from_calc(double *value)
{
    if (value == NULL) return false;
    const char *source = s_calc_input[0] ? s_calc_input : s_calc_ans;
    char eval_expr[CALC_EXPR_MAX + CALC_RESULT_MAX];
    calc_expand_ans(source, eval_expr, sizeof(eval_expr));
    return opencalc_math_eval_expression(eval_expr, value) && isfinite(*value);
}

static double complex poly_eval_complex_coeffs(const double *coeffs, int count, double complex x)
{
    double complex y = 0.0;
    for (int i = 0; i < count; i++) {
        y = y * x + coeffs[i];
    }
    return y;
}

static void solver_sort_complex_roots(double *real, double *imag, int count)
{
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            bool swap = real[j] < real[i] ||
                (fabs(real[j] - real[i]) < 1e-9 && imag[j] < imag[i]);
            if (swap) {
                double tr = real[i];
                double ti = imag[i];
                real[i] = real[j];
                imag[i] = imag[j];
                real[j] = tr;
                imag[j] = ti;
            }
        }
    }
}

static int solver_find_poly_roots(const double *coeffs, int count, double *real, double *imag, int max_roots)
{
    if (coeffs == NULL || real == NULL || imag == NULL || count < 2 || count > 11 || max_roots <= 0) {
        return -1;
    }

    int start = 0;
    while (start < count - 1 && fabs(coeffs[start]) < 1e-12) {
        start++;
    }
    int degree = count - start - 1;
    if (degree < 1 || degree > max_roots || fabs(coeffs[start]) < 1e-12) {
        return -1;
    }

    if (degree == 1) {
        real[0] = -coeffs[start + 1] / coeffs[start];
        imag[0] = 0.0;
        return 1;
    }

    double radius = 1.0;
    for (int i = start + 1; i < count; i++) {
        radius = fmax(radius, 1.0 + fabs(coeffs[i] / coeffs[start]));
    }

    double complex roots[10];
    for (int i = 0; i < degree; i++) {
        double angle = 2.0 * 3.14159265358979323846 * (double)i / (double)degree + 0.23;
        roots[i] = radius * (cos(angle) + sin(angle) * I);
    }

    for (int iter = 0; iter < 96; iter++) {
        double max_delta = 0.0;
        for (int i = 0; i < degree; i++) {
            double complex denom = 1.0;
            for (int j = 0; j < degree; j++) {
                if (i == j) {
                    continue;
                }
                double complex diff = roots[i] - roots[j];
                if (cabs(diff) < 1e-12) {
                    diff += (1e-6 + 1e-6 * I) * (double)(i + 1);
                }
                denom *= diff;
            }
            double complex delta = poly_eval_complex_coeffs(coeffs + start, degree + 1, roots[i]) / denom;
            roots[i] -= delta;
            max_delta = fmax(max_delta, cabs(delta));
        }
        if (max_delta < 1e-10) {
            break;
        }
    }

    for (int i = 0; i < degree; i++) {
        real[i] = creal(roots[i]);
        imag[i] = cimag(roots[i]);
        if (fabs(real[i]) < 1e-8) real[i] = 0.0;
        if (fabs(imag[i]) < 1e-8) imag[i] = 0.0;
    }
    solver_sort_complex_roots(real, imag, degree);
    return degree;
}

static bool solver_system_from_selected_matrix(char *out, size_t out_size)
{
    if (out == NULL || out_size == 0 || s_matrix_rows == 0 || s_matrix_cols != s_matrix_rows + 1) {
        s_solver_system_status = SOLVER_SYSTEM_NONE;
        return false;
    }
    s_solver_system_matrix = s_matrix_index;
    s_solver_system_variables = s_matrix_cols - 1;
    s_solver_system_selected = 0;
    if (!matrix_rref_a()) {
        s_solver_system_status = SOLVER_SYSTEM_NONE;
        return false;
    }

    int vars = s_matrix_cols - 1;
    int rank = 0;
    for (int r = 0; r < s_matrix_rows; r++) {
        int pivot_col = -1;
        for (int c = 0; c < vars; c++) {
            if (fabs(s_matrix_a[r][c]) > 1e-9) {
                pivot_col = c;
                break;
            }
        }
        if (pivot_col < 0) {
            if (fabs(s_matrix_a[r][vars]) > 1e-9) {
                s_solver_system_rank = rank;
                s_solver_system_status = SOLVER_SYSTEM_INCONSISTENT;
                snprintf(out, out_size, "inconsistent system");
                return true;
            }
            continue;
        }
        rank++;
    }

    s_solver_system_rank = rank;
    if (rank < vars) {
        s_solver_system_status = SOLVER_SYSTEM_DEPENDENT;
        snprintf(out, out_size, "dependent: %d free variable%s",
                 vars - rank, vars - rank == 1 ? "" : "s");
        return true;
    }

    s_solver_system_status = SOLVER_SYSTEM_UNIQUE;
    snprintf(out, out_size, "unique solution: %d variable%s", vars, vars == 1 ? "" : "s");
    return true;
}

static void solver_saved_key(int slot, const char *suffix, char *out, size_t out_size)
{
    snprintf(out, out_size, "solve%d%s", slot, suffix);
}

static void solver_saved_load_all(void)
{
    for (int slot = 0; slot < SOLVER_SAVED_MAX; slot++) {
        char key[16];
        solver_saved_key(slot, "a", key, sizeof(key));
        opencalc_persist_get_string(key, s_solver_saved_e1[slot], sizeof(s_solver_saved_e1[slot]));
        solver_saved_key(slot, "b", key, sizeof(key));
        opencalc_persist_get_string(key, s_solver_saved_e2[slot], sizeof(s_solver_saved_e2[slot]));
        solver_saved_key(slot, "t", key, sizeof(key));
        s_solver_saved_type[slot] = opencalc_persist_get_u32(key, SOLVER_WORKFLOW_EQUATION);
        if (s_solver_saved_type[slot] > SOLVER_WORKFLOW_NUMERIC) {
            s_solver_saved_type[slot] = SOLVER_WORKFLOW_EQUATION;
        }
    }
}

static bool solver_save_problem(const char *left, const char *right)
{
    if (left == NULL || left[0] == '\0') {
        app_output("nothing to save");
        return false;
    }
    int slot = s_solver_saved_selection;
    snprintf(s_solver_saved_e1[slot], sizeof(s_solver_saved_e1[slot]), "%s", left);
    snprintf(s_solver_saved_e2[slot], sizeof(s_solver_saved_e2[slot]), "%s", right != NULL ? right : "");
    char key[16];
    solver_saved_key(slot, "a", key, sizeof(key));
    bool ok = opencalc_persist_set_string(key, s_solver_saved_e1[slot]);
    solver_saved_key(slot, "b", key, sizeof(key));
    ok = opencalc_persist_set_string(key, s_solver_saved_e2[slot]) && ok;
    s_solver_saved_type[slot] = (uint32_t)s_solver_workflow;
    solver_saved_key(slot, "t", key, sizeof(key));
    ok = opencalc_persist_set_u32(key, s_solver_saved_type[slot]) && ok;
    char status[48];
    snprintf(status, sizeof(status), ok ? "saved problem in slot %d" : "could not save slot %d", slot + 1);
    app_output(status);
    return ok;
}

static void solver_delete_saved(int slot)
{
    if (slot < 0 || slot >= SOLVER_SAVED_MAX) return;
    s_solver_saved_e1[slot][0] = '\0';
    s_solver_saved_e2[slot][0] = '\0';
    char key[16];
    solver_saved_key(slot, "a", key, sizeof(key));
    opencalc_persist_erase(key);
    solver_saved_key(slot, "b", key, sizeof(key));
    opencalc_persist_erase(key);
    solver_saved_key(slot, "t", key, sizeof(key));
    opencalc_persist_erase(key);
    s_solver_saved_type[slot] = SOLVER_WORKFLOW_EQUATION;
}

static void solver_plot_sides(void)
{
    snprintf(s_graph_exprs[0], sizeof(s_graph_exprs[0]), "%s", s_solver_e1);
    snprintf(s_graph_exprs[1], sizeof(s_graph_exprs[1]), "%s", s_solver_e2);
    s_graph_enabled[0] = true;
    s_graph_enabled[1] = true;
    s_graphing_mode = 0;
    s_graph_trace = false;
    s_current_app = APP_GRAPH;
    s_page = PAGE_GRAPH;
    ui_draw_current();
}

static bool solver_set_side_from_calculator(char *side, size_t side_size, const char *name)
{
    if (s_calc_input[0] == '\0') {
        app_output("enter expression in Calculator first");
        return false;
    }
    snprintf(side, side_size, "%s", s_calc_input);
    s_solver_has_result = false;
    char status[64];
    snprintf(status, sizeof(status), "%s set to %.40s", name, side);
    app_output(status);
    return true;
}

static void solver_open_workflow(solver_workflow_t workflow)
{
    s_solver_workflow = workflow;
    s_solver_workflow_selection = 0;
    s_page = PAGE_SOLVER_WORKFLOW;
    s_current_app = APP_SOLVER;
    ui_draw_current();
}

static void solver_open_home_selection(void)
{
    if (s_app_selection >= 0 && s_app_selection <= 3) {
        solver_open_workflow((solver_workflow_t)s_app_selection);
    } else if (s_app_selection == 4) {
        s_page = PAGE_SOLVER_SAVED;
        s_current_app = APP_SOLVER;
        ui_draw_current();
    }
}

static void solver_open_symbolic(const char *command, const char *title)
{
    char expression[256];
    int written = snprintf(expression, sizeof(expression), "%s", command != NULL ? command : "");
    if (written < 0 || written >= (int)sizeof(expression)) {
        app_output("symbolic expression too long");
        return;
    }
    submit_solver_symbolic_job(expression, title);
}

static void solver_exact_equation(void)
{
    char expression[256];
    int written = snprintf(expression, sizeof(expression), "solve((%s)=(%s),x)", s_solver_e1, s_solver_e2);
    if (written < 0 || written >= (int)sizeof(expression)) {
        app_output("equation too long");
        return;
    }
    solver_open_symbolic(expression, "Exact Equation Solutions");
}

static bool solver_export_unique_solution(void)
{
    if (s_solver_system_status != SOLVER_SYSTEM_UNIQUE || s_solver_system_variables <= 0 ||
        s_solver_system_variables > LIST_MAX_VALUES) return false;
    s_matrix_index = s_solver_system_matrix;
    for (int variable = 0; variable < s_solver_system_variables; variable++) {
        int row = solver_system_pivot_row(variable);
        if (row < 0) return false;
        s_lists[s_list_index][variable] = s_matrix_a[row][s_solver_system_variables];
    }
    s_list_counts[s_list_index] = s_solver_system_variables;
    return true;
}

static void solver_run_workflow_action(void)
{
    char expression[256];
    char status[96];
    int action = s_solver_workflow_selection;
    if (s_solver_workflow == SOLVER_WORKFLOW_EQUATION) {
        if (action == 0) solver_set_side_from_calculator(s_solver_e1, sizeof(s_solver_e1), "left side");
        else if (action == 1) solver_set_side_from_calculator(s_solver_e2, sizeof(s_solver_e2), "right side");
        else if (action == 2) solver_exact_equation();
        else if (action == 3) {
            if (s_calc_input[0] == '\0') app_output("enter solve(...) in Calculator");
            else solver_open_symbolic(s_calc_input, "Advanced Symbolic Result");
        }
        else if (action == 4) submit_solver_solve_job();
        else if (action == 5) solver_plot_sides();
        else solver_save_problem(s_solver_e1, s_solver_e2);
        return;
    }
    if (s_solver_workflow == SOLVER_WORKFLOW_SYSTEM) {
        if (action == 0) {
            s_solver_matrix_editing = true;
            matrix_open_editor();
        } else if (action == 1) {
            if (solver_system_from_selected_matrix(status, sizeof(status))) {
                snprintf(s_calc_output, sizeof(s_calc_output), "%s", status);
                s_page = PAGE_SOLVER_SYSTEM_RESULT;
                s_current_app = APP_SOLVER;
                ui_draw_current();
            } else app_output("select an n x (n+1) augmented matrix");
        } else if (action == 2) {
            if (s_calc_input[0] == '\0') app_output("enter equations/list in Calculator");
            else {
                int written = snprintf(expression, sizeof(expression),
                                       strncmp(s_calc_input, "solve(", 6) == 0 ? "%s" : "solve(%s)", s_calc_input);
                if (written < 0 || written >= (int)sizeof(expression)) app_output("system expression too long");
                else solver_open_symbolic(expression, "Symbolic System Solutions");
            }
        } else if (action == 3) {
            if (s_solver_system_status == SOLVER_SYSTEM_NONE) app_output("solve a matrix system first");
            else {
                s_matrix_index = s_solver_system_matrix;
                s_page = PAGE_SOLVER_SYSTEM_RESULT;
                ui_draw_current();
            }
        } else if (action == 4) {
            if (solver_export_unique_solution()) {
                snprintf(status, sizeof(status), "solution -> L%d", s_list_index + 1);
                app_output(status);
            } else app_output("requires a unique matrix solution");
        } else {
            solver_save_problem(s_calc_input, "");
        }
        return;
    }
    if (s_solver_workflow == SOLVER_WORKFLOW_POLYNOMIAL) {
        if (action == 0) solver_set_side_from_calculator(s_solver_e1, sizeof(s_solver_e1), "P(x)");
        else if (action == 1) {
            int written = snprintf(expression, sizeof(expression), "solve((%s)=0,x)", s_solver_e1);
            if (written < 0 || written >= (int)sizeof(expression)) app_output("polynomial too long");
            else solver_open_symbolic(expression, "Exact Polynomial Roots");
        } else if (action == 2) {
            int written = snprintf(expression, sizeof(expression), "factor(%s)", s_solver_e1);
            if (written < 0 || written >= (int)sizeof(expression)) app_output("polynomial too long");
            else solver_open_symbolic(expression, "Polynomial Factorization");
        } else if (action == 3) {
            double coeffs[11] = {0.0};
            int coeff_count = parse_csv_numbers(s_calc_input, coeffs, 11);
            int roots = solver_find_poly_roots(coeffs, coeff_count, s_solver_poly_root_real,
                                                s_solver_poly_root_imag, SOLVER_POLY_ROOT_MAX);
            if (roots < 0) app_output("use coefficients: highest,...,constant");
            else {
                s_solver_poly_root_count = roots;
                s_solver_poly_root_selected = 0;
                snprintf(s_solver_poly_source, sizeof(s_solver_poly_source), "%s", s_calc_input);
                s_solver_roots_are_polynomial = true;
                s_page = PAGE_SOLVER_ROOTS;
                s_current_app = APP_SOLVER;
                ui_draw_current();
            }
        } else if (action == 4) {
            if (s_solver_poly_root_count <= 0) app_output("find numeric roots first");
            else { s_page = PAGE_SOLVER_ROOTS; ui_draw_current(); }
        } else solver_save_problem(s_solver_e1, "0");
        return;
    }

    if (action == 0) solver_set_side_from_calculator(s_solver_e1, sizeof(s_solver_e1), "left side");
    else if (action == 1) solver_set_side_from_calculator(s_solver_e2, sizeof(s_solver_e2), "right side");
    else if (action == 2) {
        if (solver_set_guess_from_calc()) {
            snprintf(status, sizeof(status), "initial guess %.10g", s_solver_guess);
            app_output(status);
        } else app_output("guess must be numeric");
    } else if (action == 3 || action == 4) {
        double value = 0.0;
        if (!solver_number_from_calc(&value)) {
            app_output("bound must be numeric");
        } else if (action == 3) {
            s_solver_lower = value;
            snprintf(status, sizeof(status), "lower bound %.10g", s_solver_lower);
            app_output(status);
        } else {
            s_solver_upper = value;
            snprintf(status, sizeof(status), "upper bound %.10g", s_solver_upper);
            app_output(status);
        }
    } else if (action == 5) {
        double value = 0.0;
        if (!solver_number_from_calc(&value)) {
            app_output("precision must be 4 through 12");
        } else {
            int digits = (int)llround(value);
            if (digits < 4 || digits > 12) app_output("precision must be 4 through 12");
            else {
                s_solver_precision = digits;
                snprintf(status, sizeof(status), "precision %d digits", s_solver_precision);
                app_output(status);
            }
        }
    } else if (action == 6) submit_solver_solve_job();
    else if (action == 7) submit_solver_scan_job();
    else if (action == 8) solver_plot_sides();
    else solver_save_problem(s_solver_e1, s_solver_e2);
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

static double finance_periodic_rate(void)
{
    double payments = s_fin_py > 0.0 ? s_fin_py : 1.0;
    double compounds = s_fin_cy > 0.0 ? s_fin_cy : payments;
    double compound_base = 1.0 + (s_fin_i / 100.0) / compounds;
    if (compound_base <= 0.0) return NAN;
    return pow(compound_base, compounds / payments) - 1.0;
}

static double finance_balance_for_rate(double rate)
{
    if (rate <= -1.0 || s_fin_n < 0.0) return NAN;
    double compound = pow(1.0 + rate, s_fin_n);
    if (!isfinite(compound)) return NAN;
    double annuity = fabs(rate) < 1e-12 ? s_fin_n : (compound - 1.0) / rate;
    double due = s_fin_begin ? (1.0 + rate) : 1.0;
    return s_fin_pv * compound + s_fin_pmt * due * annuity + s_fin_fv;
}

static double finance_nominal_rate_from_periodic(double periodic_rate)
{
    double payments = s_fin_py > 0.0 ? s_fin_py : 1.0;
    double compounds = s_fin_cy > 0.0 ? s_fin_cy : payments;
    if (periodic_rate <= -1.0) return NAN;
    return (pow(1.0 + periodic_rate, payments / compounds) - 1.0) * compounds * 100.0;
}

static bool finance_solve_interest(double *interest_percent)
{
    if (interest_percent == NULL || s_fin_n <= 0.0 || s_fin_py <= 0.0 || s_fin_cy <= 0.0) {
        return false;
    }

    const int sample_count = 640;
    double guess = finance_periodic_rate();
    if (!isfinite(guess) || guess <= -1.0) guess = 0.0;
    bool bracketed = false;
    double best_lo = 0.0;
    double best_hi = 0.0;
    double best_distance = INFINITY;
    double previous_rate = -0.9999;
    double previous_value = finance_balance_for_rate(previous_rate);

    for (int sample = 1; sample <= sample_count; sample++) {
        double rate;
        if (sample <= 160) {
            rate = -0.9999 + 0.9999 * (double)sample / 160.0;
        } else {
            double position = (double)(sample - 160) / (double)(sample_count - 160);
            rate = exp(log(101.0) * position) - 1.0;
        }
        double value = finance_balance_for_rate(rate);
        if (isfinite(value) && fabs(value) < 1e-10) {
            *interest_percent = finance_nominal_rate_from_periodic(rate);
            return isfinite(*interest_percent);
        }
        if (isfinite(previous_value) && isfinite(value) &&
            ((previous_value < 0.0 && value > 0.0) ||
             (previous_value > 0.0 && value < 0.0))) {
            double midpoint = (previous_rate + rate) * 0.5;
            double distance = fabs(midpoint - guess);
            if (!bracketed || distance < best_distance) {
                bracketed = true;
                best_lo = previous_rate;
                best_hi = rate;
                best_distance = distance;
            }
        }
        previous_rate = rate;
        previous_value = value;
    }
    if (!bracketed) return false;

    double lo = best_lo;
    double hi = best_hi;
    double f_lo = finance_balance_for_rate(lo);
    for (int iteration = 0; iteration < 100; iteration++) {
        double mid = (lo + hi) * 0.5;
        double f_mid = finance_balance_for_rate(mid);
        if (!isfinite(f_mid)) return false;
        if (fabs(f_mid) < 1e-10 || fabs(hi - lo) < 1e-13) {
            lo = hi = mid;
            break;
        }
        if ((f_lo < 0.0 && f_mid > 0.0) || (f_lo > 0.0 && f_mid < 0.0)) {
            hi = mid;
        } else {
            lo = mid;
            f_lo = f_mid;
        }
    }
    *interest_percent = finance_nominal_rate_from_periodic((lo + hi) * 0.5);
    return isfinite(*interest_percent);
}

static double finance_solve_pmt(void)
{
    double r = finance_periodic_rate();
    if (s_fin_n <= 0.0 || !isfinite(r)) return NAN;
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
    if (!isfinite(r)) return NAN;
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
    if (!isfinite(r)) return NAN;
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
    if (n == NULL || !isfinite(r) || r <= -1.0) {
        return false;
    }
    if (fabs(s_fin_pmt) < 1e-12) {
        if (fabs(r) < 1e-12 || fabs(s_fin_pv) < 1e-12 || -s_fin_fv / s_fin_pv <= 0.0) {
            return false;
        }
        *n = log(-s_fin_fv / s_fin_pv) / log(1.0 + r);
        return *n >= 0.0 && isfinite(*n);
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

static bool finance_solve_selected(void)
{
    double value = NAN;
    switch (s_fin_selection) {
    case 0:
        if (!finance_solve_n(&value)) return false;
        s_fin_n = value;
        break;
    case 1:
        if (!finance_solve_interest(&value)) return false;
        s_fin_i = value;
        break;
    case 2:
        value = finance_solve_pv();
        if (!isfinite(value)) return false;
        s_fin_pv = value;
        break;
    case 3:
        value = finance_solve_pmt();
        if (!isfinite(value)) return false;
        s_fin_pmt = value;
        break;
    case 4:
        value = finance_solve_fv();
        if (!isfinite(value)) return false;
        s_fin_fv = value;
        break;
    default:
        return false;
    }
    snprintf(s_fin_status, sizeof(s_fin_status), "%s solved: %.10g",
             finance_selected_name(), value);
    return true;
}

static double finance_npv_from_list(int list, double rate)
{
    if (list < 0 || list >= LIST_COUNT || !isfinite(rate) || rate <= -1.0) return NAN;
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

static void finance_reset_entry(void)
{
    s_fin_entry[0] = '\0';
    s_fin_entry_active = false;
    s_fin_status[0] = '\0';
}

static void finance_open_tvm(void)
{
    finance_reset_entry();
    if (s_fin_selection < 0 || s_fin_selection >= 7) s_fin_selection = 3;
    s_page = PAGE_FINANCE_TVM;
    s_current_app = APP_FINANCE;
    ui_draw_current();
}

static void finance_open_cashflow(void)
{
    finance_reset_entry();
    s_fin_cash_cursor = 0;
    s_fin_cash_scroll = 0;
    s_page = PAGE_FINANCE_CASHFLOW;
    s_current_app = APP_FINANCE;
    ui_draw_current();
}

static void finance_open_result(const char *title, double value, const char *detail)
{
    snprintf(s_fin_result_title, sizeof(s_fin_result_title), "%s", title);
    s_fin_result_value = value;
    snprintf(s_fin_result_detail, sizeof(s_fin_result_detail), "%s", detail);
    s_page = PAGE_FINANCE_RESULT;
    s_current_app = APP_FINANCE;
    ui_draw_current();
}

static bool finance_commit_tvm_entry(void)
{
    if (!s_fin_entry_active) return true;
    if (s_fin_entry[0] == '\0' || strcmp(s_fin_entry, "-") == 0 ||
        strcmp(s_fin_entry, ".") == 0 || strcmp(s_fin_entry, "-.") == 0) {
        snprintf(s_fin_status, sizeof(s_fin_status), "Enter a valid number");
        return false;
    }
    char *end = NULL;
    double value = strtod(s_fin_entry, &end);
    if (end == s_fin_entry || *end != '\0' || !isfinite(value)) {
        snprintf(s_fin_status, sizeof(s_fin_status), "Enter a valid number");
        return false;
    }
    if (s_fin_selection == 0 && value < 0.0) {
        snprintf(s_fin_status, sizeof(s_fin_status), "N cannot be negative");
        return false;
    }
    if ((s_fin_selection == 5 || s_fin_selection == 6) &&
        (value < 1.0 || value > 365.0)) {
        snprintf(s_fin_status, sizeof(s_fin_status), "P/Y and C/Y must be 1..365");
        return false;
    }
    if (s_fin_selection == 5 || s_fin_selection == 6) value = round(value);
    double *field = finance_selected_value();
    if (field == NULL) return false;
    double previous = *field;
    *field = value;
    if (!isfinite(finance_periodic_rate())) {
        *field = previous;
        snprintf(s_fin_status, sizeof(s_fin_status), "I% is outside the compounding domain");
        return false;
    }
    finance_reset_entry();
    return true;
}

static bool finance_commit_cash_entry(void)
{
    if (!s_fin_entry_active) return true;
    if (s_fin_entry[0] == '\0' || strcmp(s_fin_entry, "-") == 0 ||
        strcmp(s_fin_entry, ".") == 0 || strcmp(s_fin_entry, "-.") == 0) {
        snprintf(s_fin_status, sizeof(s_fin_status), "Enter a valid cash flow");
        return false;
    }
    char *end = NULL;
    double value = strtod(s_fin_entry, &end);
    if (end == s_fin_entry || *end != '\0' || !isfinite(value)) {
        snprintf(s_fin_status, sizeof(s_fin_status), "Enter a valid cash flow");
        return false;
    }
    if (s_fin_cash_cursor >= LIST_MAX_VALUES) {
        snprintf(s_fin_status, sizeof(s_fin_status), "Cash-flow list is full");
        return false;
    }
    s_lists[s_list_index][s_fin_cash_cursor] = value;
    if (s_fin_cash_cursor == s_list_counts[s_list_index]) {
        s_list_counts[s_list_index]++;
    }
    finance_reset_entry();
    return true;
}

static bool finance_append_entry_char(char value)
{
    if (!s_fin_entry_active) {
        s_fin_entry[0] = '\0';
        s_fin_entry_active = true;
    }
    size_t length = strlen(s_fin_entry);
    if (length + 1 >= sizeof(s_fin_entry)) return false;
    s_fin_entry[length] = value;
    s_fin_entry[length + 1] = '\0';
    s_fin_status[0] = '\0';
    ui_draw_current();
    return true;
}

static void finance_toggle_entry_sign(void)
{
    if (!s_fin_entry_active) {
        double current = s_page == PAGE_FINANCE_TVM ? *finance_selected_value() :
            (s_fin_cash_cursor < s_list_counts[s_list_index] ?
             s_lists[s_list_index][s_fin_cash_cursor] : 0.0);
        finance_format_value(current, s_fin_entry, sizeof(s_fin_entry));
        s_fin_entry_active = true;
    }
    size_t length = strlen(s_fin_entry);
    if (s_fin_entry[0] == '-') {
        memmove(s_fin_entry, s_fin_entry + 1, length);
    } else if (length + 1 < sizeof(s_fin_entry)) {
        memmove(s_fin_entry + 1, s_fin_entry, length + 1);
        s_fin_entry[0] = '-';
    }
    s_fin_status[0] = '\0';
    ui_draw_current();
}

static void finance_delete_cash_flow(void)
{
    int count = s_list_counts[s_list_index];
    if (s_fin_cash_cursor >= count) return;
    for (int index = s_fin_cash_cursor; index + 1 < count; index++) {
        s_lists[s_list_index][index] = s_lists[s_list_index][index + 1];
    }
    s_lists[s_list_index][count - 1] = 0.0;
    s_list_counts[s_list_index]--;
    if (s_fin_cash_cursor > s_list_counts[s_list_index]) {
        s_fin_cash_cursor = s_list_counts[s_list_index];
    }
    finance_reset_entry();
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
    int count = 0;
    list_sorted_copy(list, s_stats_sort_scratch, &count);
    return median_of_sorted_values(s_stats_sort_scratch, count);
}

static void list_quartiles(int list, double *q1, double *q3)
{
    int count = 0;
    list_sorted_copy(list, s_stats_sort_scratch, &count);
    if (count < 2) {
        *q1 = count == 1 ? s_stats_sort_scratch[0] : 0.0;
        *q3 = *q1;
        return;
    }

    int upper_start = (count + 1) / 2;
    int lower_count = count / 2;
    int upper_count = count / 2;
    *q1 = median_of_sorted_values(s_stats_sort_scratch, lower_count);
    *q3 = median_of_sorted_values(s_stats_sort_scratch + upper_start, upper_count);
}

static double stats_weighted_value_at_rank(const stats_weighted_item_t *items, int count,
                                           int64_t rank)
{
    int64_t position = 0;
    for (int i = 0; i < count; i++) {
        position += items[i].frequency;
        if (rank < position) return items[i].value;
    }
    return count > 0 ? items[count - 1].value : 0.0;
}

static double stats_weighted_median_range(const stats_weighted_item_t *items, int count,
                                          int64_t start, int64_t length)
{
    if (length <= 0) return 0.0;
    if ((length & 1) != 0) {
        return stats_weighted_value_at_rank(items, count, start + length / 2);
    }
    double low = stats_weighted_value_at_rank(items, count, start + length / 2 - 1);
    double high = stats_weighted_value_at_rank(items, count, start + length / 2);
    return (low + high) / 2.0;
}

static void stats_format_one_var_value(double value, char *out, size_t out_size)
{
    if (s_stats_one_var_number_format != 0) {
        snprintf(out, out_size, "%.8g", value);
        return;
    }

    for (long long denominator = 1; denominator <= 1000; denominator++) {
        long long numerator = llround(value * (double)denominator);
        if (fabs(value - (double)numerator / (double)denominator) <= 1e-10) {
            if (denominator == 1) snprintf(out, out_size, "%lld", numerator);
            else snprintf(out, out_size, "%lld/%lld", numerator, denominator);
            return;
        }
    }
    snprintf(out, out_size, "%.10g", value);
}

static void stats_open_one_var(void)
{
    s_stats_one_var_selection = 0;
    s_stats_one_var_error[0] = '\0';
    s_stats_return_page = PAGE_STATS_ONE_VAR;
    s_page = PAGE_STATS_ONE_VAR;
    s_current_app = APP_STATS;
    ui_draw_current();
}

static void stats_calculate_one_var(void)
{
    int data = s_stats_one_var_data_list;
    int frequency = s_stats_one_var_frequency_list;
    int count = s_list_counts[data];
    if (count <= 0) {
        snprintf(s_stats_one_var_error, sizeof(s_stats_one_var_error),
                 "L%d has no data", data + 1);
        ui_draw_current();
        return;
    }
    if (frequency >= 0 && s_list_counts[frequency] != count) {
        snprintf(s_stats_one_var_error, sizeof(s_stats_one_var_error),
                 "L%d frequency length must match", frequency + 1);
        ui_draw_current();
        return;
    }

    int64_t total = 0;
    double mean = 0.0;
    double m2 = 0.0;
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        double value = s_lists[data][i];
        if (!isfinite(value)) {
            snprintf(s_stats_one_var_error, sizeof(s_stats_one_var_error),
                     "Data must contain finite values");
            ui_draw_current();
            return;
        }
        int64_t weight = 1;
        if (frequency >= 0) {
            double raw = s_lists[frequency][i];
            weight = llround(raw);
            if (!isfinite(raw) || raw < 0.0 || fabs(raw - (double)weight) > 1e-9) {
                snprintf(s_stats_one_var_error, sizeof(s_stats_one_var_error),
                         "Frequencies must be whole and >=0");
                ui_draw_current();
                return;
            }
        }
        if (weight > 1000000000LL - total) {
            snprintf(s_stats_one_var_error, sizeof(s_stats_one_var_error),
                     "Frequency total is too large");
            ui_draw_current();
            return;
        }
        s_stats_weighted_items[i].value = value;
        s_stats_weighted_items[i].frequency = weight;
        if (weight > 0) {
            int64_t next_total = total + weight;
            double delta = value - mean;
            mean += delta * (double)weight / (double)next_total;
            m2 += delta * delta * (double)total * (double)weight / (double)next_total;
            sum += value * (double)weight;
            total = next_total;
        }
    }
    if (total <= 0) {
        snprintf(s_stats_one_var_error, sizeof(s_stats_one_var_error),
                 "Frequency total must be positive");
        ui_draw_current();
        return;
    }

    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (s_stats_weighted_items[j].value < s_stats_weighted_items[i].value) {
                stats_weighted_item_t swap = s_stats_weighted_items[i];
                s_stats_weighted_items[i] = s_stats_weighted_items[j];
                s_stats_weighted_items[j] = swap;
            }
        }
    }

    double median = stats_weighted_median_range(s_stats_weighted_items, count, 0, total);
    int64_t half = total / 2;
    int64_t quartile_count = half;
    int64_t upper_start = (total + 1) / 2;
    if (s_stats_one_var_quartile_method != 0 && (total & 1) != 0) {
        quartile_count = half + 1;
        upper_start = half;
    }
    double q1 = total < 2 ? median :
        stats_weighted_median_range(s_stats_weighted_items, count, 0, quartile_count);
    double q3 = total < 2 ? median :
        stats_weighted_median_range(s_stats_weighted_items, count, upper_start, quartile_count);
    double minimum = stats_weighted_value_at_rank(s_stats_weighted_items, count, 0);
    double maximum = stats_weighted_value_at_rank(s_stats_weighted_items, count, total - 1);
    double stdev = total > 1 ? sqrt(fmax(0.0, m2 / (double)(total - 1))) : 0.0;

    char sum_text[24], mean_text[24], stdev_text[24], min_text[24];
    char q1_text[24], median_text[24], q3_text[24], max_text[24];
    stats_format_one_var_value(sum, sum_text, sizeof(sum_text));
    stats_format_one_var_value(mean, mean_text, sizeof(mean_text));
    stats_format_one_var_value(stdev, stdev_text, sizeof(stdev_text));
    stats_format_one_var_value(minimum, min_text, sizeof(min_text));
    stats_format_one_var_value(q1, q1_text, sizeof(q1_text));
    stats_format_one_var_value(median, median_text, sizeof(median_text));
    stats_format_one_var_value(q3, q3_text, sizeof(q3_text));
    stats_format_one_var_value(maximum, max_text, sizeof(max_text));

    char title[32];
    snprintf(title, sizeof(title), "One-Variable Stats L%d", data + 1);
    stats_result_begin(title);
    stats_result_line("n=%lld  sum=%s", (long long)total, sum_text);
    stats_result_line("mean %s", mean_text);
    stats_result_line("sample sx %s", stdev_text);
    stats_result_line("min %s  Q1 %s", min_text, q1_text);
    stats_result_line("median %s", median_text);
    stats_result_line("Q3 %s  max %s", q3_text, max_text);
    char frequency_text[8];
    if (frequency < 0) snprintf(frequency_text, sizeof(frequency_text), "None");
    else snprintf(frequency_text, sizeof(frequency_text), "L%d", frequency + 1);
    stats_result_line("frequency %s", frequency_text);
    stats_result_line("%s quartiles, %s format",
                      s_stats_one_var_quartile_method == 0 ? "median" : "inclusive",
                      s_stats_one_var_number_format == 0 ? "exact" : "decimal");
    s_stats_one_var_error[0] = '\0';
    s_stats_return_page = PAGE_STATS_ONE_VAR;
    stats_result_show();
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
    s_list_editing = false;
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

static int parse_csv_numbers(const char *text, double *values, int max_values)
{
    if (text == NULL || values == NULL || max_values <= 0) {
        return 0;
    }

    int count = 0;
    const char *p = text;
    while (*p != '\0' && count < max_values) {
        while (*p == ' ' || *p == '\t' || *p == ',') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        char *end = NULL;
        double value = strtod(p, &end);
        if (end == p) {
            break;
        }
        values[count++] = value;
        p = end;
    }
    return count;
}

static bool solve_3x3(double a[3][4], double out[3])
{
    for (int col = 0; col < 3; col++) {
        int pivot = col;
        for (int r = col + 1; r < 3; r++) {
            if (fabs(a[r][col]) > fabs(a[pivot][col])) {
                pivot = r;
            }
        }
        if (fabs(a[pivot][col]) < 1e-12) {
            return false;
        }
        if (pivot != col) {
            for (int c = col; c < 4; c++) {
                double tmp = a[col][c];
                a[col][c] = a[pivot][c];
                a[pivot][c] = tmp;
            }
        }
        double div = a[col][col];
        for (int c = col; c < 4; c++) {
            a[col][c] /= div;
        }
        for (int r = 0; r < 3; r++) {
            if (r == col) {
                continue;
            }
            double factor = a[r][col];
            for (int c = col; c < 4; c++) {
                a[r][c] -= factor * a[col][c];
            }
        }
    }
    for (int i = 0; i < 3; i++) {
        out[i] = a[i][3];
    }
    return true;
}

static bool quadreg_l1_l2(double *a, double *b, double *c)
{
    int n = s_list_counts[0] < s_list_counts[1] ? s_list_counts[0] : s_list_counts[1];
    if (n < 3 || a == NULL || b == NULL || c == NULL) {
        return false;
    }

    double sx = 0.0;
    double sx2 = 0.0;
    double sx3 = 0.0;
    double sx4 = 0.0;
    double sy = 0.0;
    double sxy = 0.0;
    double sx2y = 0.0;
    for (int i = 0; i < n; i++) {
        double x = s_lists[0][i];
        double y = s_lists[1][i];
        double x2 = x * x;
        sx += x;
        sx2 += x2;
        sx3 += x2 * x;
        sx4 += x2 * x2;
        sy += y;
        sxy += x * y;
        sx2y += x2 * y;
    }

    double system[3][4] = {
        {sx4, sx3, sx2, sx2y},
        {sx3, sx2, sx, sxy},
        {sx2, sx, (double)n, sy},
    };
    double out[3] = {0.0};
    if (!solve_3x3(system, out)) {
        return false;
    }
    *a = out[0];
    *b = out[1];
    *c = out[2];
    return true;
}

static bool expreg_l1_l2(double *a, double *b)
{
    int n = s_list_counts[0] < s_list_counts[1] ? s_list_counts[0] : s_list_counts[1];
    if (n < 2 || a == NULL || b == NULL) {
        return false;
    }

    double sx = 0.0;
    double sy = 0.0;
    double sxx = 0.0;
    double sxy = 0.0;
    for (int i = 0; i < n; i++) {
        if (s_lists[1][i] <= 0.0) {
            return false;
        }
        double x = s_lists[0][i];
        double y = log(s_lists[1][i]);
        sx += x;
        sy += y;
        sxx += x * x;
        sxy += x * y;
    }

    double denom = (double)n * sxx - sx * sx;
    if (fabs(denom) < 1e-12) {
        return false;
    }
    double slope = ((double)n * sxy - sx * sy) / denom;
    double intercept = (sy - slope * sx) / (double)n;
    *a = exp(intercept);
    *b = exp(slope);
    return true;
}

static bool transformed_linreg_l1_l2(bool log_x, bool log_y, double *a, double *b)
{
    int n = s_list_counts[0] < s_list_counts[1] ? s_list_counts[0] : s_list_counts[1];
    if (n < 2 || a == NULL || b == NULL) {
        return false;
    }

    double sx = 0.0;
    double sy = 0.0;
    double sxx = 0.0;
    double sxy = 0.0;
    for (int i = 0; i < n; i++) {
        if ((log_x && s_lists[0][i] <= 0.0) || (log_y && s_lists[1][i] <= 0.0)) {
            return false;
        }
        double x = log_x ? log(s_lists[0][i]) : s_lists[0][i];
        double y = log_y ? log(s_lists[1][i]) : s_lists[1][i];
        sx += x;
        sy += y;
        sxx += x * x;
        sxy += x * y;
    }

    double denom = (double)n * sxx - sx * sx;
    if (fabs(denom) < 1e-12) {
        return false;
    }
    *b = ((double)n * sxy - sx * sy) / denom;
    *a = (sy - *b * sx) / (double)n;
    if (log_y) {
        *a = exp(*a);
    }
    return true;
}

static bool polyreg_l1_l2(int degree, double *coeffs, int max_coeffs)
{
    int n = s_list_counts[0] < s_list_counts[1] ? s_list_counts[0] : s_list_counts[1];
    int terms = degree + 1;
    if (degree < 1 || degree > 4 || coeffs == NULL || max_coeffs < terms || n < terms) {
        return false;
    }

    double aug[5][6] = {0};
    for (int row = 0; row < terms; row++) {
        for (int col = 0; col < terms; col++) {
            double sum = 0.0;
            for (int i = 0; i < n; i++) {
                sum += pow(s_lists[0][i], (double)(row + col));
            }
            aug[row][col] = sum;
        }
        double rhs = 0.0;
        for (int i = 0; i < n; i++) {
            rhs += s_lists[1][i] * pow(s_lists[0][i], (double)row);
        }
        aug[row][terms] = rhs;
    }

    for (int col = 0; col < terms; col++) {
        int pivot = col;
        for (int r = col + 1; r < terms; r++) {
            if (fabs(aug[r][col]) > fabs(aug[pivot][col])) {
                pivot = r;
            }
        }
        if (fabs(aug[pivot][col]) < 1e-12) {
            return false;
        }
        if (pivot != col) {
            for (int c = col; c <= terms; c++) {
                double tmp = aug[col][c];
                aug[col][c] = aug[pivot][c];
                aug[pivot][c] = tmp;
            }
        }
        double div = aug[col][col];
        for (int c = col; c <= terms; c++) {
            aug[col][c] /= div;
        }
        for (int r = 0; r < terms; r++) {
            if (r == col) {
                continue;
            }
            double factor = aug[r][col];
            for (int c = col; c <= terms; c++) {
                aug[r][c] -= factor * aug[col][c];
            }
        }
    }

    for (int i = 0; i < terms; i++) {
        coeffs[i] = aug[i][terms];
    }
    return true;
}

static double normal_pdf(double x, double mu, double sigma)
{
    return opencalc_stats_normal_pdf(x, mu, sigma);
}

static double normal_cdf(double x, double mu, double sigma)
{
    return opencalc_stats_normal_cdf(x, mu, sigma);
}

static double t_pdf_core(double x, double df, double unused)
{
    (void)unused;
    return opencalc_stats_t_pdf(x, df);
}

static double t_cdf(double x, double df)
{
    return opencalc_stats_t_cdf(x, df);
}

static double chi2_pdf_core(double x, double df, double unused)
{
    (void)unused;
    return opencalc_stats_chi_square_pdf(x, df);
}

static double chi2_cdf(double x, double df)
{
    return opencalc_stats_chi_square_cdf(x, df);
}

static double f_pdf_core(double x, double df1, double df2)
{
    return opencalc_stats_f_pdf(x, df1, df2);
}

static double f_cdf(double x, double df1, double df2)
{
    return opencalc_stats_f_cdf(x, df1, df2);
}

static double inv_norm(double p, double mu, double sigma)
{
    return opencalc_stats_inverse_normal(p, mu, sigma);
}

static double binom_pdf(int n, double p, int x)
{
    return opencalc_stats_binomial_pdf(n, p, x);
}

static double poisson_pdf(double lambda, int x)
{
    return opencalc_stats_poisson_pdf(lambda, x);
}

static void stats_setup_field(int index, const char *label, stats_field_kind_t kind, double value)
{
    s_stats_setup_labels[index] = label;
    s_stats_setup_kinds[index] = kind;
    s_stats_setup_values[index] = value;
}

static void stats_setup_open(int tool)
{
    s_stats_setup_tool = tool;
    s_stats_setup_selection = 0;
    s_stats_setup_field_count = 0;
    s_stats_setup_entry[0] = '\0';
    s_stats_setup_error[0] = '\0';

#define SF(label, kind, value) stats_setup_field(s_stats_setup_field_count++, label, kind, value)
    switch (tool) {
    case STATS_TOOL_Z_TEST: SF("Data", STATS_FIELD_LIST, 1); SF("mu0", STATS_FIELD_NUMBER, 0); SF("sigma", STATS_FIELD_NUMBER, 1); SF("alternative", STATS_FIELD_TAIL, 0); break;
    case STATS_TOOL_T_TEST: SF("Data", STATS_FIELD_LIST, 1); SF("mu0", STATS_FIELD_NUMBER, 0); SF("alternative", STATS_FIELD_TAIL, 0); break;
    case STATS_TOOL_CHI_GOF: SF("Observed", STATS_FIELD_LIST, 1); SF("Expected", STATS_FIELD_LIST, 2); break;
    case STATS_TOOL_ONE_PROP_TEST: SF("p0", STATS_FIELD_NUMBER, 0.5); SF("successes", STATS_FIELD_INTEGER, 5); SF("n", STATS_FIELD_INTEGER, 10); SF("alternative", STATS_FIELD_TAIL, 0); break;
    case STATS_TOOL_TWO_PROP_TEST: SF("successes 1", STATS_FIELD_INTEGER, 5); SF("n1", STATS_FIELD_INTEGER, 10); SF("successes 2", STATS_FIELD_INTEGER, 5); SF("n2", STATS_FIELD_INTEGER, 10); SF("alternative", STATS_FIELD_TAIL, 0); break;
    case STATS_TOOL_TWO_SAMPLE_T_TEST: SF("Sample 1", STATS_FIELD_LIST, 1); SF("Sample 2", STATS_FIELD_LIST, 2); SF("alternative", STATS_FIELD_TAIL, 0); break;
    case STATS_TOOL_Z_INTERVAL: SF("Data", STATS_FIELD_LIST, 1); SF("sigma", STATS_FIELD_NUMBER, 1); SF("confidence", STATS_FIELD_CONFIDENCE, 0.95); break;
    case STATS_TOOL_T_INTERVAL: SF("Data", STATS_FIELD_LIST, 1); SF("confidence", STATS_FIELD_CONFIDENCE, 0.95); break;
    case STATS_TOOL_ONE_PROP_INTERVAL: SF("successes", STATS_FIELD_INTEGER, 5); SF("n", STATS_FIELD_INTEGER, 10); SF("confidence", STATS_FIELD_CONFIDENCE, 0.95); break;
    case STATS_TOOL_TWO_PROP_INTERVAL: SF("successes 1", STATS_FIELD_INTEGER, 5); SF("n1", STATS_FIELD_INTEGER, 10); SF("successes 2", STATS_FIELD_INTEGER, 5); SF("n2", STATS_FIELD_INTEGER, 10); SF("confidence", STATS_FIELD_CONFIDENCE, 0.95); break;
    case STATS_TOOL_TWO_SAMPLE_Z_INTERVAL: SF("Sample 1", STATS_FIELD_LIST, 1); SF("Sample 2", STATS_FIELD_LIST, 2); SF("sigma 1", STATS_FIELD_NUMBER, 1); SF("sigma 2", STATS_FIELD_NUMBER, 1); SF("confidence", STATS_FIELD_CONFIDENCE, 0.95); break;
    case STATS_TOOL_TWO_SAMPLE_T_INTERVAL: SF("Sample 1", STATS_FIELD_LIST, 1); SF("Sample 2", STATS_FIELD_LIST, 2); SF("confidence", STATS_FIELD_CONFIDENCE, 0.95); break;
    case STATS_TOOL_ANOVA: SF("Group 1", STATS_FIELD_LIST, 1); SF("Group 2", STATS_FIELD_LIST, 2); SF("Group 3", STATS_FIELD_LIST, 3); break;
    case STATS_TOOL_NORMAL_PDF: SF("x", STATS_FIELD_NUMBER, 0); SF("mean", STATS_FIELD_NUMBER, 0); SF("stdev", STATS_FIELD_NUMBER, 1); break;
    case STATS_TOOL_NORMAL_CDF: SF("lower", STATS_FIELD_NUMBER, -1); SF("upper", STATS_FIELD_NUMBER, 1); SF("mean", STATS_FIELD_NUMBER, 0); SF("stdev", STATS_FIELD_NUMBER, 1); break;
    case STATS_TOOL_INV_NORMAL: SF("area", STATS_FIELD_NUMBER, 0.95); SF("mean", STATS_FIELD_NUMBER, 0); SF("stdev", STATS_FIELD_NUMBER, 1); break;
    case STATS_TOOL_T_PDF: SF("x", STATS_FIELD_NUMBER, 0); SF("df", STATS_FIELD_NUMBER, 10); break;
    case STATS_TOOL_T_CDF: SF("lower", STATS_FIELD_NUMBER, -1); SF("upper", STATS_FIELD_NUMBER, 1); SF("df", STATS_FIELD_NUMBER, 10); break;
    case STATS_TOOL_CHI_PDF: SF("x", STATS_FIELD_NUMBER, 1); SF("df", STATS_FIELD_NUMBER, 5); break;
    case STATS_TOOL_CHI_CDF: SF("lower", STATS_FIELD_NUMBER, 0); SF("upper", STATS_FIELD_NUMBER, 5); SF("df", STATS_FIELD_NUMBER, 5); break;
    case STATS_TOOL_F_PDF: SF("x", STATS_FIELD_NUMBER, 1); SF("df numerator", STATS_FIELD_NUMBER, 5); SF("df denominator", STATS_FIELD_NUMBER, 10); break;
    case STATS_TOOL_F_CDF: SF("lower", STATS_FIELD_NUMBER, 0); SF("upper", STATS_FIELD_NUMBER, 3); SF("df numerator", STATS_FIELD_NUMBER, 5); SF("df denominator", STATS_FIELD_NUMBER, 10); break;
    case STATS_TOOL_BINOM_PDF: case STATS_TOOL_BINOM_CDF: SF("trials", STATS_FIELD_INTEGER, 10); SF("probability", STATS_FIELD_NUMBER, 0.5); SF("successes", STATS_FIELD_INTEGER, 5); break;
    case STATS_TOOL_POISSON_PDF: case STATS_TOOL_POISSON_CDF: SF("mean", STATS_FIELD_NUMBER, 3); SF("count", STATS_FIELD_INTEGER, 2); break;
    default: return;
    }
#undef SF
    s_page = PAGE_STATS_SETUP;
    s_current_app = APP_STATS;
    ui_draw_current();
}

static void stats_setup_commit_entry(void)
{
    if (s_stats_setup_entry[0] == '\0') return;
    char *end = NULL;
    double value = strtod(s_stats_setup_entry, &end);
    if (end != s_stats_setup_entry && *end == '\0' && isfinite(value)) {
        stats_field_kind_t kind = s_stats_setup_kinds[s_stats_setup_selection];
        if (kind == STATS_FIELD_INTEGER || kind == STATS_FIELD_LIST) value = llround(value);
        s_stats_setup_values[s_stats_setup_selection] = value;
        s_stats_setup_error[0] = '\0';
    } else {
        snprintf(s_stats_setup_error, sizeof(s_stats_setup_error), "Enter a valid number");
    }
    s_stats_setup_entry[0] = '\0';
}

static bool stats_setup_list(int field, int minimum, int *list)
{
    int index = (int)llround(s_stats_setup_values[field]) - 1;
    if (index < 0 || index >= LIST_COUNT) {
        snprintf(s_stats_setup_error, sizeof(s_stats_setup_error), "List must be L1 through L6");
        return false;
    }
    if (s_list_counts[index] < minimum) {
        snprintf(s_stats_setup_error, sizeof(s_stats_setup_error), "L%d needs at least %d values", index + 1, minimum);
        return false;
    }
    *list = index;
    return true;
}

static opencalc_stats_tail_t stats_setup_tail(int field)
{
    int value = (int)llround(s_stats_setup_values[field]);
    return value < 0 ? OPENCALC_STATS_TAIL_LESS :
        (value > 0 ? OPENCALC_STATS_TAIL_GREATER : OPENCALC_STATS_TAIL_NOT_EQUAL);
}

static void stats_interval_show(const char *title, const opencalc_stats_interval_t *result,
                                double confidence, const char *warning)
{
    stats_result_begin(title);
    stats_result_line("estimate %.10g", result->estimate);
    stats_result_line("%.1f%% CI", confidence * 100.0);
    stats_result_line("low %.10g", result->low);
    stats_result_line("high %.10g", result->high);
    stats_result_line("SE %.10g", result->standard_error);
    if (isfinite(result->df)) stats_result_line("df %.8g", result->df);
    if (warning != NULL) stats_result_line("warning: %s", warning);
    stats_result_show();
}

static void stats_setup_fail(const char *message)
{
    snprintf(s_stats_setup_error, sizeof(s_stats_setup_error), "%s", message);
    ui_draw_current();
}

static void stats_setup_execute(void)
{
    stats_setup_commit_entry();
    if (s_stats_setup_error[0] != '\0') { ui_draw_current(); return; }
    double *v = s_stats_setup_values;
    int tool = s_stats_setup_tool;
    int l1 = 0, l2 = 0, l3 = 0;

    if (tool == STATS_TOOL_Z_TEST || tool == STATS_TOOL_T_TEST ||
        tool == STATS_TOOL_Z_INTERVAL || tool == STATS_TOOL_T_INTERVAL) {
        if (!stats_setup_list(0, tool == STATS_TOOL_Z_TEST || tool == STATS_TOOL_Z_INTERVAL ? 1 : 2, &l1)) { ui_draw_current(); return; }
    }

    if (tool == STATS_TOOL_Z_TEST) {
        if (!(v[2] > 0.0)) { stats_setup_fail("sigma must be positive"); return; }
        double z = (list_mean(l1) - v[1]) / (v[2] / sqrt((double)s_list_counts[l1]));
        double p = opencalc_stats_p_value(normal_cdf(z, 0, 1), stats_setup_tail(3));
        stats_result_begin("Z-Test"); stats_result_line("z %.10g", z); stats_result_line("p %.10g", p);
        stats_result_line("xbar %.10g", list_mean(l1)); stats_result_line("n %d", s_list_counts[l1]); stats_result_line("alt %s", stats_tail_text(v[3])); stats_result_show(); return;
    }
    if (tool == STATS_TOOL_T_TEST) {
        double sd = list_stdev(l1);
        if (!(sd > 0.0)) { stats_setup_fail("sample variance is zero"); return; }
        double df = s_list_counts[l1] - 1.0;
        double t = (list_mean(l1) - v[1]) / (sd / sqrt((double)s_list_counts[l1]));
        double p = opencalc_stats_p_value(t_cdf(t, df), stats_setup_tail(2));
        stats_result_begin("T-Test"); stats_result_line("t %.10g", t); stats_result_line("p %.10g", p);
        stats_result_line("df %.0f", df); stats_result_line("xbar %.10g", list_mean(l1)); stats_result_line("sx %.10g", sd); stats_result_line("alt %s", stats_tail_text(v[2]));
        if (s_list_counts[l1] < 30) {
            stats_result_line("check normality/outliers");
        }
        stats_result_show();
        return;
    }
    if (tool == STATS_TOOL_CHI_GOF) {
        if (!stats_setup_list(0, 2, &l1) || !stats_setup_list(1, 2, &l2)) { ui_draw_current(); return; }
        if (s_list_counts[l1] != s_list_counts[l2]) { stats_setup_fail("Observed/expected sizes differ"); return; }
        double chi = 0.0; bool small = false;
        for (int i = 0; i < s_list_counts[l1]; ++i) {
            if (!(s_lists[l2][i] > 0.0) || s_lists[l1][i] < 0.0) { stats_setup_fail("Counts must be nonnegative; E>0"); return; }
            double d = s_lists[l1][i] - s_lists[l2][i]; chi += d * d / s_lists[l2][i];
            if (s_lists[l2][i] < 5.0) small = true;
        }
        double df = s_list_counts[l1] - 1.0;
        stats_result_begin("Chi-Square GOF"); stats_result_line("chi2 %.10g", chi); stats_result_line("p %.10g", 1.0 - chi2_cdf(chi, df));
        stats_result_line("df %.0f", df); stats_result_line("Observed L%d, Expected L%d", l1 + 1, l2 + 1); if (small) stats_result_line("warning: expected count <5"); stats_result_show(); return;
    }
    if (tool == STATS_TOOL_ONE_PROP_TEST) {
        int x = (int)llround(v[1]), n = (int)llround(v[2]);
        if (!(v[0] > 0.0 && v[0] < 1.0) || n <= 0 || x < 0 || x > n) { stats_setup_fail("Require 0<p0<1 and 0<=x<=n"); return; }
        double se = sqrt(v[0] * (1.0 - v[0]) / n);
        double z = ((double)x / n - v[0]) / se;
        stats_result_begin("1-Prop Z-Test"); stats_result_line("z %.10g", z); stats_result_line("p %.10g", opencalc_stats_p_value(normal_cdf(z, 0, 1), stats_setup_tail(3)));
        stats_result_line("phat %.10g", (double)x / n); stats_result_line("n %d", n); if (n * v[0] < 10 || n * (1-v[0]) < 10) stats_result_line("warning: null counts <10"); stats_result_show(); return;
    }
    if (tool == STATS_TOOL_TWO_PROP_TEST) {
        int x1=(int)llround(v[0]), n1=(int)llround(v[1]), x2=(int)llround(v[2]), n2=(int)llround(v[3]);
        if (n1<=0 || n2<=0 || x1<0 || x1>n1 || x2<0 || x2>n2) { stats_setup_fail("Require 0<=successes<=n"); return; }
        double pooled=(double)(x1+x2)/(n1+n2), se=sqrt(pooled*(1-pooled)*(1.0/n1+1.0/n2));
        if (!(se > 0.0)) { stats_setup_fail("Pooled standard error is zero"); return; }
        double diff=(double)x1/n1-(double)x2/n2, z=diff/se;
        stats_result_begin("2-Prop Z-Test"); stats_result_line("z %.10g", z); stats_result_line("p %.10g", opencalc_stats_p_value(normal_cdf(z,0,1), stats_setup_tail(4)));
        stats_result_line("p1-p2 %.10g", diff); stats_result_line("pooled p %.10g", pooled); if ((x1+x2)<10 || (n1+n2-x1-x2)<10) stats_result_line("warning: pooled counts <10"); stats_result_show(); return;
    }
    if (tool == STATS_TOOL_TWO_SAMPLE_T_TEST) {
        if (!stats_setup_list(0, 2, &l1) || !stats_setup_list(1, 2, &l2)) { ui_draw_current(); return; }
        double s1=list_stdev(l1), s2=list_stdev(l2), q1=s1*s1/s_list_counts[l1], q2=s2*s2/s_list_counts[l2];
        if (!(q1+q2 > 0.0)) { stats_setup_fail("Both sample variances are zero"); return; }
        double df=(q1+q2)*(q1+q2)/(q1*q1/(s_list_counts[l1]-1.0)+q2*q2/(s_list_counts[l2]-1.0));
        double t=(list_mean(l1)-list_mean(l2))/sqrt(q1+q2);
        stats_result_begin("2-Samp T-Test"); stats_result_line("t %.10g", t); stats_result_line("p %.10g", opencalc_stats_p_value(t_cdf(t,df), stats_setup_tail(2)));
        stats_result_line("df %.8g", df); stats_result_line("xbar1-xbar2 %.10g", list_mean(l1)-list_mean(l2)); stats_result_line("Welch (not pooled)");
        if (s_list_counts[l1] < 30 || s_list_counts[l2] < 30) {
            stats_result_line("check normality/outliers");
        }
        stats_result_show();
        return;
    }

    opencalc_stats_interval_t interval;
    bool interval_ok = false;
    if (tool == STATS_TOOL_Z_INTERVAL) interval_ok = opencalc_stats_z_interval(list_mean(l1), v[1], s_list_counts[l1], v[2], &interval);
    else if (tool == STATS_TOOL_T_INTERVAL) interval_ok = opencalc_stats_t_interval(list_mean(l1), list_stdev(l1), s_list_counts[l1], v[1], &interval);
    else if (tool == STATS_TOOL_ONE_PROP_INTERVAL) interval_ok = opencalc_stats_one_prop_interval((int)llround(v[0]), (int)llround(v[1]), v[2], &interval);
    else if (tool == STATS_TOOL_TWO_PROP_INTERVAL) interval_ok = opencalc_stats_two_prop_interval((int)llround(v[0]), (int)llround(v[1]), (int)llround(v[2]), (int)llround(v[3]), v[4], &interval);
    else if (tool == STATS_TOOL_TWO_SAMPLE_Z_INTERVAL) {
        if (!stats_setup_list(0,1,&l1) || !stats_setup_list(1,1,&l2)) { ui_draw_current(); return; }
        interval_ok = opencalc_stats_two_mean_z_interval(list_mean(l1),v[2],s_list_counts[l1],list_mean(l2),v[3],s_list_counts[l2],v[4],&interval);
    } else if (tool == STATS_TOOL_TWO_SAMPLE_T_INTERVAL) {
        if (!stats_setup_list(0,2,&l1) || !stats_setup_list(1,2,&l2)) { ui_draw_current(); return; }
        interval_ok = opencalc_stats_two_mean_t_interval(list_mean(l1),list_stdev(l1),s_list_counts[l1],list_mean(l2),list_stdev(l2),s_list_counts[l2],v[2],&interval);
    }
    if (tool >= STATS_TOOL_Z_INTERVAL && tool <= STATS_TOOL_TWO_SAMPLE_T_INTERVAL) {
        int confidence_field = s_stats_setup_field_count - 1;
        if (!interval_ok) { stats_setup_fail("Check n, spread, sigma, confidence"); return; }
        const char *warning = NULL;
        if (tool == STATS_TOOL_ONE_PROP_INTERVAL) {
            int x = (int)llround(v[0]), n = (int)llround(v[1]);
            if (x < 10 || n - x < 10) warning = "success/failure <10";
        } else if (tool == STATS_TOOL_TWO_PROP_INTERVAL) {
            int x1=(int)llround(v[0]), n1=(int)llround(v[1]), x2=(int)llround(v[2]), n2=(int)llround(v[3]);
            if (x1 < 10 || n1-x1 < 10 || x2 < 10 || n2-x2 < 10) warning = "success/failure <10";
        } else if (tool == STATS_TOOL_T_INTERVAL && s_list_counts[l1] < 30) {
            warning = "check normality/outliers";
        } else if (tool == STATS_TOOL_TWO_SAMPLE_T_INTERVAL &&
                   (s_list_counts[l1] < 30 || s_list_counts[l2] < 30)) {
            warning = "check normality/outliers";
        }
        stats_interval_show(STATS_TOOLS[tool].label, &interval, v[confidence_field], warning); return;
    }

    if (tool == STATS_TOOL_ANOVA) {
        if (!stats_setup_list(0,2,&l1) || !stats_setup_list(1,2,&l2) || !stats_setup_list(2,2,&l3)) { ui_draw_current(); return; }
        int lists[3]={l1,l2,l3}, total=s_list_counts[l1]+s_list_counts[l2]+s_list_counts[l3]; double grand=(list_sum(l1)+list_sum(l2)+list_sum(l3))/total, ssb=0, ssw=0;
        for(int g=0;g<3;g++){double mean=list_mean(lists[g]);ssb+=s_list_counts[lists[g]]*(mean-grand)*(mean-grand);for(int i=0;i<s_list_counts[lists[g]];i++){double d=s_lists[lists[g]][i]-mean;ssw+=d*d;}}
        if (!(ssw > 0.0)) { stats_setup_fail("Within-group variance is zero"); return; }
        double f=(ssb/2.0)/(ssw/(total-3.0));
        stats_result_begin("One-Way ANOVA"); stats_result_line("F %.10g",f); stats_result_line("p %.10g",1.0-f_cdf(f,2,total-3)); stats_result_line("df 2, %d",total-3); stats_result_line("SSB %.10g",ssb); stats_result_line("SSW %.10g",ssw); stats_result_show(); return;
    }

    double result=NAN;
    switch(tool){
    case STATS_TOOL_NORMAL_PDF: result=normal_pdf(v[0],v[1],v[2]); break;
    case STATS_TOOL_NORMAL_CDF: if(v[1]>=v[0]) result=normal_cdf(v[1],v[2],v[3])-normal_cdf(v[0],v[2],v[3]); break;
    case STATS_TOOL_INV_NORMAL: result=inv_norm(v[0],v[1],v[2]); break;
    case STATS_TOOL_T_PDF: result=t_pdf_core(v[0],v[1],0); break;
    case STATS_TOOL_T_CDF: if(v[1]>=v[0]) result=t_cdf(v[1],v[2])-t_cdf(v[0],v[2]); break;
    case STATS_TOOL_CHI_PDF: result=chi2_pdf_core(v[0],v[1],0); break;
    case STATS_TOOL_CHI_CDF: if(v[1]>=v[0]) result=chi2_cdf(v[1],v[2])-chi2_cdf(v[0],v[2]); break;
    case STATS_TOOL_F_PDF: result=f_pdf_core(v[0],v[1],v[2]); break;
    case STATS_TOOL_F_CDF: if(v[1]>=v[0]) result=f_cdf(v[1],v[2],v[3])-f_cdf(v[0],v[2],v[3]); break;
    case STATS_TOOL_BINOM_PDF: result=binom_pdf((int)llround(v[0]),v[1],(int)llround(v[2])); break;
    case STATS_TOOL_BINOM_CDF: result=opencalc_stats_binomial_cdf((int)llround(v[0]),v[1],(int)llround(v[2])); break;
    case STATS_TOOL_POISSON_PDF: result=poisson_pdf(v[0],(int)llround(v[1])); break;
    case STATS_TOOL_POISSON_CDF: result=opencalc_stats_poisson_cdf(v[0],(int)llround(v[1])); break;
    default: break;
    }
    if (!isfinite(result)) { stats_setup_fail("Parameters are outside the valid domain"); return; }
    stats_result_begin(STATS_TOOLS[tool].label); stats_result_line("result %.12g",result); stats_result_line("validated domain inputs"); stats_result_show();
}

static bool expression_entry_active(void)
{
    return s_page == PAGE_CALCULATOR || s_page == PAGE_Y_EQUALS;
}

static void open_app(app_id_t app)
{
#if OPENCALC_EXPORT_USB_STORAGE_TO_HOST
    if (s_usb_storage_enabled && usb_msc_mount_app()) s_usb_storage_enabled = false;
#endif
    remember_app_transition(app);
    s_current_app = app;
    s_app_selection = 0;
    if (app == APP_CALCULATOR) {
        s_page = PAGE_CALCULATOR;
    } else if (app == APP_GRAPH) {
        s_page = PAGE_GRAPH;
    } else if (app == APP_TABLE) {
        s_page = PAGE_TABLE;
    } else if (app == APP_PYTHON) {
        s_program_selection = 0;
        s_page = PAGE_PROGRAM_MENU;
    } else if (app == APP_SETTINGS) {
        s_script_selection = 0;
        s_page = PAGE_SETTINGS;
    } else if (app == APP_LISTS) {
        snprintf(s_calc_output, sizeof(s_calc_output), "Select an action for L%d",
                 s_list_index + 1);
        s_page = PAGE_APP;
    } else if (app == APP_MATRICES) {
        snprintf(s_calc_output, sizeof(s_calc_output), "Select an action for Matrix %c",
                 'A' + s_matrix_index);
        s_page = PAGE_APP;
    } else if (app == APP_SOLVER) {
        s_app_selection = 0;
        s_page = PAGE_APP;
    } else if (app == APP_FINANCE) {
        snprintf(s_calc_output, sizeof(s_calc_output), "TVM and cash-flow workspace");
        s_page = PAGE_APP;
    } else if (app == APP_STATS) {
        s_stats_home_selection = 0;
        s_stats_return_page = PAGE_APP;
        s_page = PAGE_APP;
    } else {
        s_page = PAGE_APP;
    }
    ui_draw_current();
}

static bool open_previous_app(void)
{
    if (s_previous_app == s_current_app) return false;
    app_id_t previous = s_previous_app;
    if (previous == APP_GRAPH) {
        s_previous_app = s_current_app;
        s_current_app = APP_GRAPH;
        s_app_selection = 0;
        s_page = PAGE_GRAPH;
        ui_draw_current();
        return true;
    }
    open_app(previous);
    return true;
}

static void clear_graph_expressions(void)
{
    for (int i = 0; i < GRAPH_FUNC_COUNT; i++) {
        s_graph_exprs[i][0] = '\0';
        s_graph_enabled[i] = false;
    }
    for (int i = 0; i < GRAPH_PARAM_COUNT; i++) {
        s_graph_param_x[i][0] = '\0';
        s_graph_param_y[i][0] = '\0';
        s_graph_param_enabled[i] = false;
    }
    for (int i = 0; i < GRAPH_POLAR_COUNT; i++) {
        s_graph_polar_exprs[i][0] = '\0';
        s_graph_polar_enabled[i] = false;
    }
    for (int i = 0; i < GRAPH_SEQ_COUNT; i++) {
        s_graph_seq_exprs[i][0] = '\0';
        s_graph_seq_enabled[i] = false;
    }
    s_graph_selection = 0;
}

static void conic_rebuild_model(void)
{
    if (s_conic_type == OPENCALC_CONIC_CIRCLE) {
        opencalc_conic_circle(&s_conic_model, s_conic_h, s_conic_k, s_conic_r);
    } else if (s_conic_type == OPENCALC_CONIC_GENERAL) {
        opencalc_conic_general(&s_conic_model, s_conic_general[0], s_conic_general[1],
                               s_conic_general[2], s_conic_general[3],
                               s_conic_general[4], s_conic_general[5]);
    } else {
        opencalc_conic_axis(&s_conic_model, (opencalc_conic_kind_t)s_conic_type,
                            s_conic_h, s_conic_k, s_conic_a, s_conic_b, s_conic_angle);
    }
    if (s_conic_model.valid) {
        snprintf(s_conic_equation, sizeof(s_conic_equation),
                 "%.8g*x^2%+.8g*x*y%+.8g*y^2%+.8g*x%+.8g*y%+.8g=0",
                 s_conic_model.A, s_conic_model.B, s_conic_model.C,
                 s_conic_model.D, s_conic_model.E, s_conic_model.F);
    } else {
        snprintf(s_conic_equation, sizeof(s_conic_equation), "invalid conic parameters");
    }
}

static void conic_open_editor(int type)
{
    s_conic_type = type;
    s_conic_selection = 0;
    s_conic_entry[0] = '\0';
    s_conic_entry_active = false;
    s_conic_status[0] = '\0';
    if (type == OPENCALC_CONIC_CIRCLE) {
        s_conic_h = 0.0; s_conic_k = 0.0; s_conic_r = 5.0; s_conic_angle = 0.0;
    } else if (type == OPENCALC_CONIC_PARABOLA) {
        s_conic_h = 0.0; s_conic_k = 0.0; s_conic_a = 1.0; s_conic_angle = 90.0;
    } else if (type == OPENCALC_CONIC_ELLIPSE) {
        s_conic_h = 0.0; s_conic_k = 0.0; s_conic_a = 6.0; s_conic_b = 4.0; s_conic_angle = 0.0;
    } else if (type == OPENCALC_CONIC_HYPERBOLA) {
        s_conic_h = 0.0; s_conic_k = 0.0; s_conic_a = 4.0; s_conic_b = 2.0; s_conic_angle = 0.0;
    }
    conic_rebuild_model();
    s_page = PAGE_CONIC_EDITOR;
    s_current_app = APP_CONICS;
    ui_draw_current();
}

static void conic_result_begin(const char *title)
{
    snprintf(s_conic_result_title, sizeof(s_conic_result_title), "%s", title);
    memset(s_conic_result_lines, 0, sizeof(s_conic_result_lines));
    s_conic_result_count = 0;
    s_conic_result_scroll = 0;
}

static void conic_result_line(const char *format, ...)
{
    if (s_conic_result_count >= (int)(sizeof(s_conic_result_lines) / sizeof(s_conic_result_lines[0]))) return;
    va_list args;
    va_start(args, format);
    vsnprintf(s_conic_result_lines[s_conic_result_count], sizeof(s_conic_result_lines[0]), format, args);
    va_end(args);
    s_conic_result_count++;
}

static void conic_add_intercepts(double quadratic, double linear, double constant, const char *axis)
{
    if (fabs(quadratic) < 1e-12) {
        if (fabs(linear) > 1e-12) conic_result_line("%s-intercept %.8g", axis, -constant / linear);
        else conic_result_line("%s-intercepts: %s", axis, fabs(constant) < 1e-12 ? "all" : "none");
        return;
    }
    double discriminant = linear * linear - 4.0 * quadratic * constant;
    if (discriminant < -1e-10) conic_result_line("%s-intercepts: none real", axis);
    else if (fabs(discriminant) <= 1e-10) conic_result_line("%s-intercept %.8g", axis, -linear / (2.0 * quadratic));
    else {
        double root = sqrt(discriminant);
        conic_result_line("%s-intercepts %.8g, %.8g", axis,
                          (-linear - root) / (2.0 * quadratic),
                          (-linear + root) / (2.0 * quadratic));
    }
}

static void conic_analyze_current(void)
{
    conic_rebuild_model();
    opencalc_conic_analysis_t a;
    if (!opencalc_conic_analyze(&s_conic_model, &a)) {
        snprintf(s_conic_status, sizeof(s_conic_status), "Invalid or incomplete parameters");
        ui_draw_current();
        return;
    }

    conic_result_begin("CONIC ANALYSIS");
    conic_result_line("Type: %s%s", opencalc_conic_kind_name(a.kind), a.degenerate ? " (degenerate)" : "");
    conic_result_line("B^2-4AC = %.10g", a.discriminant);
    conic_result_line("xy term: %s   rotation %.8g deg", a.rotated ? "yes" : "no", a.rotation_degrees);
    conic_result_line("Real graph: %s", a.real_graph ? "yes" : "no");
    if (a.degenerate) {
        conic_result_line("Degenerate case: %s", a.kind == OPENCALC_CONIC_HYPERBOLA ?
                          "intersecting lines" : (a.kind == OPENCALC_CONIC_ELLIPSE ? "point/empty" : "line pair"));
    }
    if (a.center_valid) {
        conic_result_line("Center (%.8g, %.8g)", a.center_x, a.center_y);
    }
    conic_result_line("General: %.7g x2 %+.7g xy %+.7g y2", s_conic_model.A, s_conic_model.B, s_conic_model.C);
    conic_result_line("         %+.7g x %+.7g y %+.7g = 0", s_conic_model.D, s_conic_model.E, s_conic_model.F);

    double theta = s_conic_angle * 3.14159265358979323846 / 180.0;
    double ux = cos(theta), uy = sin(theta);
    double vx = -uy, vy = ux;
    if (s_conic_type == OPENCALC_CONIC_CIRCLE) {
        conic_result_line("Standard: (x-h)^2+(y-k)^2=r^2");
        conic_result_line("Radius %.8g   diameter %.8g", s_conic_r, 2.0 * s_conic_r);
        conic_result_line("Area %.8g   circumference %.8g", 3.14159265358979323846 * s_conic_r * s_conic_r,
                          2.0 * 3.14159265358979323846 * s_conic_r);
        conic_result_line("Domain [%.8g, %.8g]", s_conic_h - s_conic_r, s_conic_h + s_conic_r);
        conic_result_line("Range [%.8g, %.8g]", s_conic_k - s_conic_r, s_conic_k + s_conic_r);
        conic_result_line("Parametric: h+r cos(t), k+r sin(t)");
        if (fabs(s_conic_h) < 1e-12 && fabs(s_conic_k) < 1e-12) {
            conic_result_line("Polar: rho=%.8g", s_conic_r);
        } else {
            conic_result_line("Polar: (rho cos(t)-h)^2+(rho sin(t)-k)^2=r^2");
        }
    } else if (s_conic_type == OPENCALC_CONIC_PARABOLA) {
        conic_result_line("Standard local: v^2=%.8g*u", 4.0 * s_conic_a);
        conic_result_line("Vertex (%.8g, %.8g)", s_conic_h, s_conic_k);
        conic_result_line("Focus (%.8g, %.8g)", s_conic_h + s_conic_a * ux, s_conic_k + s_conic_a * uy);
        conic_result_line("Axis direction %.8g deg", s_conic_angle + (s_conic_a < 0.0 ? 180.0 : 0.0));
        conic_result_line("Directrix: %.5g(x-h)%+.5g(y-k)=%.8g", ux, uy, -s_conic_a);
        conic_result_line("Latus rectum length %.8g", 4.0 * fabs(s_conic_a));
        conic_result_line("Latus endpoints local: (p, +/-2|p|)");
        if (fabs(uy) < 1e-8) {
            conic_result_line("Domain x %s %.8g; range all", s_conic_a > 0.0 ? ">=" : "<=", s_conic_h);
        } else if (fabs(ux) < 1e-8) {
            conic_result_line("Domain all; range y %s %.8g", s_conic_a * uy > 0.0 ? ">=" : "<=", s_conic_k);
        } else {
            conic_result_line("Domain/range: unbounded after rotation");
        }
        conic_result_line("Eccentricity 1   focus-directrix definition");
        conic_result_line("Parametric local: u=p*t^2, v=2p*t");
    } else if (s_conic_type == OPENCALC_CONIC_ELLIPSE) {
        double major = fmax(s_conic_a, s_conic_b);
        double minor = fmin(s_conic_a, s_conic_b);
        double mux = s_conic_a >= s_conic_b ? ux : vx;
        double muy = s_conic_a >= s_conic_b ? uy : vy;
        double nux = s_conic_a >= s_conic_b ? vx : ux;
        double nuy = s_conic_a >= s_conic_b ? vy : uy;
        double c = sqrt(fmax(0.0, major * major - minor * minor));
        double circumference = 3.14159265358979323846 * (3.0 * (major + minor) -
            sqrt((3.0 * major + minor) * (major + 3.0 * minor)));
        conic_result_line("Vertices (%.6g,%.6g), (%.6g,%.6g)", s_conic_h - major * mux,
                          s_conic_k - major * muy, s_conic_h + major * mux, s_conic_k + major * muy);
        conic_result_line("Co-vertices (%.6g,%.6g), (%.6g,%.6g)", s_conic_h - minor * nux,
                          s_conic_k - minor * nuy, s_conic_h + minor * nux, s_conic_k + minor * nuy);
        conic_result_line("Foci (%.6g,%.6g), (%.6g,%.6g)", s_conic_h - c * mux, s_conic_k - c * muy,
                          s_conic_h + c * mux, s_conic_k + c * muy);
        conic_result_line("Major %.8g   minor %.8g   focal c %.8g", 2.0 * major, 2.0 * minor, c);
        conic_result_line("Eccentricity %.8g   latus %.8g", c / major,
                          2.0 * minor * minor / major);
        if (c > 1e-12) conic_result_line("Directrices local: u=+/-%g", major * major / c);
        double x_extent = hypot(s_conic_a * ux, s_conic_b * vx);
        double y_extent = hypot(s_conic_a * uy, s_conic_b * vy);
        conic_result_line("Domain [%.6g,%.6g] range [%.6g,%.6g]",
                          s_conic_h - x_extent, s_conic_h + x_extent,
                          s_conic_k - y_extent, s_conic_k + y_extent);
        conic_result_line("Area %.8g   circumference ~%.8g",
                          3.14159265358979323846 * major * minor, circumference);
        conic_result_line("Parametric: center + rotated (a cos t,b sin t)");
    } else if (s_conic_type == OPENCALC_CONIC_HYPERBOLA) {
        double c = hypot(s_conic_a, s_conic_b);
        conic_result_line("Vertices (%.6g,%.6g), (%.6g,%.6g)", s_conic_h - s_conic_a * ux,
                          s_conic_k - s_conic_a * uy, s_conic_h + s_conic_a * ux, s_conic_k + s_conic_a * uy);
        conic_result_line("Foci (%.6g,%.6g), (%.6g,%.6g)", s_conic_h - c * ux, s_conic_k - c * uy,
                          s_conic_h + c * ux, s_conic_k + c * uy);
        conic_result_line("Transverse %.8g   conjugate %.8g", 2.0 * s_conic_a, 2.0 * s_conic_b);
        conic_result_line("Eccentricity %.8g   latus %.8g", c / s_conic_a,
                          2.0 * s_conic_b * s_conic_b / s_conic_a);
        conic_result_line("Directrices local: u=+/-%g", s_conic_a * s_conic_a / c);
        conic_result_line("Latus endpoints local: (+/-c,+/-b^2/a)");
        conic_result_line("Asymptotes local: v=+/-%g*u", s_conic_b / s_conic_a);
        conic_result_line("Parametric local: u=a sec(t), v=b tan(t)");
    } else if (a.center_valid) {
        conic_result_line("Principal coefficients %.8g, %.8g", a.eigenvalue_1, a.eigenvalue_2);
        conic_result_line("Semi-axis estimates %.8g, %.8g", a.semi_axis_1, a.semi_axis_2);
        if (a.eccentricity > 0.0) conic_result_line("Focal length %.8g   eccentricity %.8g", a.focal_length, a.eccentricity);
        conic_result_line("Complete square after rotating %.8g deg", a.rotation_degrees);
    }
    conic_add_intercepts(s_conic_model.A, s_conic_model.D, s_conic_model.F, "x");
    conic_add_intercepts(s_conic_model.C, s_conic_model.E, s_conic_model.F, "y");
    s_page = PAGE_CONIC_RESULT;
    s_current_app = APP_CONICS;
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

static bool conic_graph_model(const opencalc_conic_t *model, bool clear_existing)
{
    char upper[96];
    char lower[96];
    int needed = opencalc_conic_y_expressions(model, upper, sizeof(upper), lower, sizeof(lower));
    if (needed <= 0) return false;
    if (clear_existing) clear_graph_expressions();
    int slot = graph_first_free_slot(needed);
    if (slot < 0) {
        return false;
    }
    snprintf(s_graph_exprs[slot], sizeof(s_graph_exprs[slot]), "%s", upper);
    s_graph_enabled[slot] = true;
    if (needed == 2) {
        snprintf(s_graph_exprs[slot + 1], sizeof(s_graph_exprs[slot + 1]), "%s", lower);
        s_graph_enabled[slot + 1] = true;
    }
    opencalc_conic_analysis_t analysis;
    if (opencalc_conic_analyze(model, &analysis) && analysis.center_valid) {
        double span = fmax(analysis.semi_axis_1, analysis.semi_axis_2);
        if (!isfinite(span) || span < 2.0) span = 5.0;
        span *= 1.5;
        s_graph_xmin = analysis.center_x - span;
        s_graph_xmax = analysis.center_x + span;
        s_graph_ymin = analysis.center_y - span;
        s_graph_ymax = analysis.center_y + span;
    } else if (model->source_kind == OPENCALC_CONIC_PARABOLA) {
        double span = 10.0;
        s_graph_xmin = s_conic_h - span;
        s_graph_xmax = s_conic_h + span;
        s_graph_ymin = s_conic_k - span;
        s_graph_ymax = s_conic_k + span;
    }
    s_graphing_mode = 0;
    s_graph_trace = false;
    return true;
}

static void conic_graph_current(void)
{
    conic_rebuild_model();
    if (!conic_graph_model(&s_conic_model, true)) {
        snprintf(s_conic_status, sizeof(s_conic_status), "Cannot solve this conic for y");
        ui_draw_current();
        return;
    }
    open_app(APP_GRAPH);
}

static bool conic_commit_entry(void)
{
    if (!s_conic_entry_active) return true;
    char *end = NULL;
    double value = strtod(s_conic_entry, &end);
    if (end == s_conic_entry || *end != '\0' || !isfinite(value)) {
        snprintf(s_conic_status, sizeof(s_conic_status), "Enter a valid number");
        return false;
    }
    int field = s_conic_selection;
    double *target = conic_field_value(field);
    if ((s_conic_type == OPENCALC_CONIC_CIRCLE && field == 2 && value <= 0.0) ||
        ((s_conic_type == OPENCALC_CONIC_ELLIPSE || s_conic_type == OPENCALC_CONIC_HYPERBOLA) &&
         (field == 2 || field == 3) && value <= 0.0) ||
        (s_conic_type == OPENCALC_CONIC_PARABOLA && field == 2 && fabs(value) < 1e-12)) {
        snprintf(s_conic_status, sizeof(s_conic_status), "Radius/axes > 0; parabola p != 0");
        return false;
    }
    *target = value;
    s_conic_entry[0] = '\0';
    s_conic_entry_active = false;
    s_conic_status[0] = '\0';
    conic_rebuild_model();
    return true;
}

static void conic_copy_equation(void)
{
    conic_rebuild_model();
    snprintf(s_calc_input, sizeof(s_calc_input), "%s", s_conic_equation);
    s_calc_cursor = strlen(s_calc_input);
    s_current_app = APP_CALCULATOR;
    s_page = PAGE_CALCULATOR;
    ui_draw_current();
}

static void conic_copy_selected_result(void)
{
    if (s_conic_result_count <= 0) return;
    snprintf(s_calc_input, sizeof(s_calc_input), "%s", s_conic_result_lines[s_conic_result_scroll]);
    s_calc_cursor = strlen(s_calc_input);
    s_current_app = APP_CALCULATOR;
    s_page = PAGE_CALCULATOR;
    ui_draw_current();
}

static void conic_add_overlay(void)
{
    conic_rebuild_model();
    if (!s_conic_model.valid) {
        snprintf(s_conic_status, sizeof(s_conic_status), "Invalid conic cannot be added");
    } else if (s_conic_overlay_count < 4) {
        s_conic_overlays[s_conic_overlay_count++] = s_conic_model;
        s_conic_overlay_selection = s_conic_overlay_count - 1;
        snprintf(s_conic_status, sizeof(s_conic_status), "Added overlay %d", s_conic_overlay_count);
    } else {
        s_conic_overlays[s_conic_overlay_selection] = s_conic_model;
        snprintf(s_conic_status, sizeof(s_conic_status), "Replaced overlay %d", s_conic_overlay_selection + 1);
    }
    ui_draw_current();
}

static void conic_point_tangent(void)
{
    double point[2];
    if (parse_csv_numbers(s_calc_input, point, 2) != 2) {
        snprintf(s_conic_status, sizeof(s_conic_status), "Calculator input must be x,y");
        ui_draw_current();
        return;
    }
    conic_rebuild_model();
    double value = opencalc_conic_evaluate(&s_conic_model, point[0], point[1]);
    double gx = 0.0, gy = 0.0;
    conic_result_begin("POINT AND TANGENT");
    conic_result_line("Point (%.8g, %.8g)", point[0], point[1]);
    conic_result_line("Conic residual %.10g", value);
    if (opencalc_conic_tangent(&s_conic_model, point[0], point[1], 1e-6, &gx, &gy)) {
        conic_result_line("Point lies on conic: yes");
        conic_result_line("Tangent: %.7g(x%+.7g)%+.7g(y%+.7g)=0",
                          gx, -point[0], gy, -point[1]);
        if (fabs(gy) > 1e-12) conic_result_line("Tangent slope %.8g", -gx / gy);
        if (fabs(gx) > 1e-12) conic_result_line("Normal slope %.8g", gy / gx);
        conic_result_line("Normal direction <%.8g, %.8g>", gx, gy);
    } else {
        conic_result_line("Point lies on conic: no");
        conic_result_line("Use a point with residual near zero");
    }
    s_page = PAGE_CONIC_RESULT;
    ui_draw_current();
}

static void conic_construct_from_calculator(void)
{
    double values[6];
    if (s_conic_type == OPENCALC_CONIC_CIRCLE) {
        if (parse_csv_numbers(s_calc_input, values, 6) != 6 ||
            !opencalc_conic_circle_through_points(&s_conic_model, values[0], values[1],
                                                  values[2], values[3], values[4], values[5])) {
            snprintf(s_conic_status, sizeof(s_conic_status), "Calc: x1,y1,x2,y2,x3,y3");
            ui_draw_current();
            return;
        }
        opencalc_conic_analysis_t analysis;
        opencalc_conic_analyze(&s_conic_model, &analysis);
        s_conic_h = analysis.center_x;
        s_conic_k = analysis.center_y;
        s_conic_r = analysis.semi_axis_1;
        snprintf(s_conic_status, sizeof(s_conic_status), "Circle through three points");
    } else if (s_conic_type == OPENCALC_CONIC_PARABOLA) {
        if (parse_csv_numbers(s_calc_input, values, 4) != 4) {
            snprintf(s_conic_status, sizeof(s_conic_status), "Calc: vertex x,y, focus x,y");
            ui_draw_current();
            return;
        }
        double dx = values[2] - values[0];
        double dy = values[3] - values[1];
        if (hypot(dx, dy) < 1e-10) {
            snprintf(s_conic_status, sizeof(s_conic_status), "Focus must differ from vertex");
            ui_draw_current();
            return;
        }
        s_conic_h = values[0];
        s_conic_k = values[1];
        s_conic_a = hypot(dx, dy);
        s_conic_angle = atan2(dy, dx) * 180.0 / 3.14159265358979323846;
        snprintf(s_conic_status, sizeof(s_conic_status), "Parabola from vertex and focus");
    } else if (s_conic_type == OPENCALC_CONIC_ELLIPSE ||
               s_conic_type == OPENCALC_CONIC_HYPERBOLA) {
        if (parse_csv_numbers(s_calc_input, values, 6) != 6) {
            snprintf(s_conic_status, sizeof(s_conic_status), "Calc: center x,y, vertex x,y, focus x,y");
            ui_draw_current();
            return;
        }
        double vx = values[2] - values[0];
        double vy = values[3] - values[1];
        double fx = values[4] - values[0];
        double fy = values[5] - values[1];
        double axis = hypot(vx, vy);
        double focal = hypot(fx, fy);
        bool valid = axis > 1e-10 &&
            ((s_conic_type == OPENCALC_CONIC_ELLIPSE && focal < axis) ||
             (s_conic_type == OPENCALC_CONIC_HYPERBOLA && focal > axis));
        if (!valid) {
            snprintf(s_conic_status, sizeof(s_conic_status),
                     s_conic_type == OPENCALC_CONIC_ELLIPSE ? "Ellipse focus must be inside vertex" :
                                                              "Hyperbola focus must be beyond vertex");
            ui_draw_current();
            return;
        }
        s_conic_h = values[0];
        s_conic_k = values[1];
        s_conic_a = axis;
        s_conic_b = s_conic_type == OPENCALC_CONIC_ELLIPSE ?
                    sqrt(axis * axis - focal * focal) : sqrt(focal * focal - axis * axis);
        s_conic_angle = atan2(vy, vx) * 180.0 / 3.14159265358979323846;
        snprintf(s_conic_status, sizeof(s_conic_status), "%s from center, vertex, focus",
                 opencalc_conic_kind_name((opencalc_conic_kind_t)s_conic_type));
    } else {
        double points[10];
        if (parse_csv_numbers(s_calc_input, points, 10) != 10 ||
            !opencalc_conic_through_five_points(&s_conic_model, points)) {
            snprintf(s_conic_status, sizeof(s_conic_status), "Calc: x1,y1,...,x5,y5 (independent points)");
            ui_draw_current();
            return;
        }
        s_conic_general[0] = s_conic_model.A;
        s_conic_general[1] = s_conic_model.B;
        s_conic_general[2] = s_conic_model.C;
        s_conic_general[3] = s_conic_model.D;
        s_conic_general[4] = s_conic_model.E;
        s_conic_general[5] = s_conic_model.F;
        snprintf(s_conic_status, sizeof(s_conic_status), "General conic fitted through five points");
    }
    conic_rebuild_model();
    conic_analyze_current();
}

static void conic_run_action(int action)
{
    if (action == 0) conic_analyze_current();
    else if (action == 1) conic_graph_current();
    else if (action == 2) conic_add_overlay();
    else if (action == 3) conic_point_tangent();
    else if (action == 4) conic_copy_equation();
    else conic_construct_from_calculator();
}

static void conic_graph_all(void)
{
    clear_graph_expressions();
    bool graphed = false;
    double xmin = INFINITY, xmax = -INFINITY, ymin = INFINITY, ymax = -INFINITY;
    for (int i = 0; i < s_conic_overlay_count; i++) {
        if (!s_conic_overlays[i].valid || !conic_graph_model(&s_conic_overlays[i], false)) continue;
        graphed = true;
        xmin = fmin(xmin, s_graph_xmin);
        xmax = fmax(xmax, s_graph_xmax);
        ymin = fmin(ymin, s_graph_ymin);
        ymax = fmax(ymax, s_graph_ymax);
    }
    if (!graphed) {
        snprintf(s_conic_status, sizeof(s_conic_status), "No graphable overlays");
        ui_draw_current();
        return;
    }
    if (isfinite(xmin) && isfinite(xmax) && isfinite(ymin) && isfinite(ymax)) {
        s_graph_xmin = xmin;
        s_graph_xmax = xmax;
        s_graph_ymin = ymin;
        s_graph_ymax = ymax;
    }
    open_app(APP_GRAPH);
}

static void conic_remove_overlay(void)
{
    if (s_conic_overlay_selection < 0 || s_conic_overlay_selection >= s_conic_overlay_count) return;
    for (int i = s_conic_overlay_selection + 1; i < s_conic_overlay_count; i++) {
        s_conic_overlays[i - 1] = s_conic_overlays[i];
    }
    memset(&s_conic_overlays[--s_conic_overlay_count], 0, sizeof(s_conic_overlays[0]));
    if (s_conic_overlay_selection >= s_conic_overlay_count && s_conic_overlay_selection > 0) {
        s_conic_overlay_selection--;
    }
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
    opencalc_calc_history_clear_selection();

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
        char *graph_buffer = graph_entry_buffer(s_graph_selection, size);
        if (graph_buffer != NULL) {
            return graph_buffer;
        }
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
            s_list_editing = false;
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
            s_list_editing = false;
            app_output("ClearList done");
        } else if (s_app_selection == 4) {
            if (s_list_counts[0] == 0) {
                app_output("1-var needs L1");
            } else {
                double q1 = 0.0;
                double q3 = 0.0;
                list_quartiles(0, &q1, &q3);
                double min = 0.0;
                double max = 0.0;
                list_minmax(0, &min, &max);
                stats_result_begin("1-Var Stats L1");
                stats_result_line("n=%d  sum %.10g", s_list_counts[0], list_sum(0));
                stats_result_line("mean %.10g", list_mean(0));
                stats_result_line("sx %.10g", list_stdev(0));
                stats_result_line("min %.10g", min);
                stats_result_line("Q1 %.10g", q1);
                stats_result_line("median %.10g", list_median_l1());
                stats_result_line("Q3 %.10g", q3);
                stats_result_line("max %.10g", max);
                stats_result_show();
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
                stats_result_begin("2-Var Stats L1,L2");
                stats_result_line("n=%d", count);
                stats_result_line("xbar %.10g", xbar);
                stats_result_line("ybar %.10g", ybar);
                stats_result_line("sx %.10g", sx);
                stats_result_line("sy %.10g", sy);
                stats_result_show();
                printf("2-var L1,L2: n=%d xbar=%.10g ybar=%.10g sx=%.10g sy=%.10g\n",
                       count, xbar, ybar, sx, sy);
            } else {
                app_output("2-var needs L1,L2");
            }
        } else if (s_app_selection == 6) {
            double a = 0.0;
            double b = 0.0;
            double r = 0.0;
            if (linreg_l1_l2(&a, &b, &r)) {
                stats_result_begin("LinReg L1,L2");
                stats_result_line("y=a+bx");
                stats_result_line("a %.10g", a);
                stats_result_line("b %.10g", b);
                stats_result_line("r %.10g", r);
                stats_result_show();
                printf("LinReg L1,L2: a=%.10g b=%.10g r=%.10g\n", a, b, r);
            } else {
                app_output("LinReg needs L1,L2");
            }
        } else if (s_app_selection == 7) {
            double a = 0.0;
            double b = 0.0;
            double c = 0.0;
            if (quadreg_l1_l2(&a, &b, &c)) {
                stats_result_begin("QuadReg L1,L2");
                stats_result_line("y=ax^2+bx+c");
                stats_result_line("a %.10g", a);
                stats_result_line("b %.10g", b);
                stats_result_line("c %.10g", c);
                stats_result_show();
                printf("QuadReg L1,L2: a=%.10g b=%.10g c=%.10g\n", a, b, c);
            } else {
                app_output("QuadReg needs L1,L2");
            }
        } else if (s_app_selection == 8) {
            double a = 0.0;
            double b = 0.0;
            if (expreg_l1_l2(&a, &b)) {
                stats_result_begin("ExpReg L1,L2");
                stats_result_line("y=a*b^x");
                stats_result_line("a %.10g", a);
                stats_result_line("b %.10g", b);
                stats_result_show();
                printf("ExpReg L1,L2: a=%.10g b=%.10g\n", a, b);
            } else {
                app_output("ExpReg needs y>0");
            }
        } else if (s_app_selection == 9) {
            double a = 0.0;
            double b = 0.0;
            if (transformed_linreg_l1_l2(true, false, &a, &b)) {
                stats_result_begin("LogReg L1,L2");
                stats_result_line("y=a+b*ln(x)");
                stats_result_line("a %.10g", a);
                stats_result_line("b %.10g", b);
                stats_result_show();
            } else {
                app_output("LogReg needs x>0");
            }
        } else if (s_app_selection == 10) {
            double a = 0.0;
            double b = 0.0;
            if (transformed_linreg_l1_l2(true, true, &a, &b)) {
                stats_result_begin("PwrReg L1,L2");
                stats_result_line("y=a*x^b");
                stats_result_line("a %.10g", a);
                stats_result_line("b %.10g", b);
                stats_result_show();
            } else {
                app_output("PwrReg needs x,y>0");
            }
        } else if (s_app_selection == 11 || s_app_selection == 12) {
            int degree = s_app_selection == 11 ? 3 : 4;
            double coeffs[5] = {0.0};
            if (polyreg_l1_l2(degree, coeffs, 5)) {
                snprintf(line, sizeof(line), "%dReg L1,L2", degree);
                stats_result_begin(line);
                stats_result_line("degree %d polynomial", degree);
                for (int i = degree; i >= 0; i--) {
                    stats_result_line("a%d %.10g", i, coeffs[i]);
                }
                stats_result_show();
            } else {
                app_output("poly reg needs L1,L2");
            }
        } else if (s_app_selection == 13) {
            double a = 0.0;
            double b = 0.0;
            double r = 0.0;
            if (linreg_l1_l2(&a, &b, &r)) {
                stats_result_begin("Med-Med Approx");
                stats_result_line("current approx uses LinReg");
                stats_result_line("a %.10g", a);
                stats_result_line("b %.10g", b);
                stats_result_line("r %.10g", r);
                stats_result_show();
            } else {
                app_output("Med-Med needs L1,L2");
            }
        } else if (s_app_selection >= STATS_TOOL_Z_TEST && s_app_selection <= STATS_TOOL_POISSON_CDF) {
            stats_setup_open(s_app_selection);
        } else if (s_app_selection == STATS_TOOL_PLOT) {
            s_page = PAGE_STATS_PLOT;
            s_current_app = APP_STATS;
            ui_draw_current();
        } else if (s_app_selection == 14) {
            double p[2] = {0.0};
            int n = parse_csv_numbers(s_calc_input, p, 2);
            if (n < 2 || s_list_counts[0] == 0 || p[1] <= 0.0) {
                app_output("calc: mu0,sigma");
            } else {
                double z = (list_mean(0) - p[0]) / (p[1] / sqrt((double)s_list_counts[0]));
                stats_result_begin("Z-Test L1");
                stats_result_line("mu0 %.10g", p[0]);
                stats_result_line("sigma %.10g", p[1]);
                stats_result_line("xbar %.10g", list_mean(0));
                stats_result_line("n %d", s_list_counts[0]);
                stats_result_line("z %.10g", z);
                stats_result_line("p %.10g", 2.0 * (1.0 - normal_cdf(fabs(z), 0.0, 1.0)));
                stats_result_show();
            }
        } else if (s_app_selection == 15) {
            double p[1] = {0.0};
            int n = parse_csv_numbers(s_calc_input, p, 1);
            if (n < 1 || s_list_counts[0] < 2) {
                app_output("calc: mu0, needs L1");
            } else {
                double t = (list_mean(0) - p[0]) / (list_stdev(0) / sqrt((double)s_list_counts[0]));
                double p_value = 2.0 * (1.0 - t_cdf(fabs(t), (double)(s_list_counts[0] - 1)));
                stats_result_begin("T-Test L1");
                stats_result_line("mu0 %.10g", p[0]);
                stats_result_line("xbar %.10g", list_mean(0));
                stats_result_line("sx %.10g", list_stdev(0));
                stats_result_line("df %d", s_list_counts[0] - 1);
                stats_result_line("t %.10g", t);
                stats_result_line("p %.10g", p_value);
                stats_result_show();
            }
        } else if (s_app_selection == 16) {
            int n = s_list_counts[0] < s_list_counts[1] ? s_list_counts[0] : s_list_counts[1];
            if (n == 0) {
                app_output("ChiSq needs L1,L2");
            } else {
                double chi = 0.0;
                bool ok = true;
                for (int i = 0; i < n; i++) {
                    if (s_lists[1][i] <= 0.0) {
                        ok = false;
                        break;
                    }
                    double d = s_lists[0][i] - s_lists[1][i];
                    chi += d * d / s_lists[1][i];
                }
                if (ok) {
                    double p_value = 1.0 - chi2_cdf(chi, (double)(n - 1));
                    stats_result_begin("Chi-Square GOF");
                    stats_result_line("observed L1");
                    stats_result_line("expected L2");
                    stats_result_line("df %d", n - 1);
                    stats_result_line("chi2 %.10g", chi);
                    stats_result_line("p %.10g", p_value);
                    stats_result_show();
                } else {
                    app_output("expected must be >0");
                }
            }
        } else if (s_app_selection == 17) {
            double p[1] = {0.0};
            int n = parse_csv_numbers(s_calc_input, p, 1);
            if (n < 1 || p[0] <= 0.0 || s_list_counts[0] == 0) {
                app_output("calc: sigma, needs L1");
            } else {
                double mean = list_mean(0);
                double margin = 1.96 * p[0] / sqrt((double)s_list_counts[0]);
                stats_result_begin("Z-Interval L1");
                stats_result_line("sigma %.10g", p[0]);
                stats_result_line("xbar %.10g", mean);
                stats_result_line("n %d", s_list_counts[0]);
                stats_result_line("low %.10g", mean - margin);
                stats_result_line("high %.10g", mean + margin);
                stats_result_show();
            }
        } else if (s_app_selection == 18) {
            if (s_list_counts[0] < 2) {
                app_output("T-Int needs L1");
            } else {
                double mean = list_mean(0);
                double margin = 1.96 * list_stdev(0) / sqrt((double)s_list_counts[0]);
                stats_result_begin("T-Interval L1");
                stats_result_line("xbar %.10g", mean);
                stats_result_line("sx %.10g", list_stdev(0));
                stats_result_line("df %d", s_list_counts[0] - 1);
                stats_result_line("low %.10g", mean - margin);
                stats_result_line("high %.10g", mean + margin);
                stats_result_show();
            }
        } else if (s_app_selection == 19) {
            int groups = 0;
            int total_n = 0;
            double grand_sum = 0.0;
            for (int list = 0; list < 3; list++) {
                if (s_list_counts[list] > 0) {
                    groups++;
                    total_n += s_list_counts[list];
                    grand_sum += list_sum(list);
                }
            }
            if (groups < 2 || total_n <= groups) {
                app_output("ANOVA needs L1-L3");
            } else {
                double grand_mean = grand_sum / (double)total_n;
                double ss_between = 0.0;
                double ss_within = 0.0;
                for (int list = 0; list < 3; list++) {
                    if (s_list_counts[list] == 0) {
                        continue;
                    }
                    double mean = list_mean(list);
                    ss_between += (double)s_list_counts[list] * (mean - grand_mean) * (mean - grand_mean);
                    for (int i = 0; i < s_list_counts[list]; i++) {
                        double d = s_lists[list][i] - mean;
                        ss_within += d * d;
                    }
                }
                double df_between = (double)(groups - 1);
                double df_within = (double)(total_n - groups);
                double f = (ss_between / df_between) / (ss_within / df_within);
                double p_value = 1.0 - f_cdf(f, df_between, df_within);
                stats_result_begin("ANOVA L1-L3");
                stats_result_line("groups %d  n %d", groups, total_n);
                stats_result_line("dfB %.0f  dfW %.0f", df_between, df_within);
                stats_result_line("SSB %.10g", ss_between);
                stats_result_line("SSW %.10g", ss_within);
                stats_result_line("F %.10g", f);
                stats_result_line("p %.10g", p_value);
                stats_result_show();
            }
        } else if (s_app_selection >= 20 && s_app_selection <= 32) {
            double p[4] = {0.0};
            int n = parse_csv_numbers(s_calc_input, p, 4);
            double result = NAN;
            const char *dist_name = STATS_TOOLS[s_app_selection].label;
            if (s_app_selection == 20 && n >= 3) {
                result = normal_pdf(p[0], p[1], p[2]);
            } else if (s_app_selection == 21 && n >= 4) {
                result = normal_cdf(p[1], p[2], p[3]) - normal_cdf(p[0], p[2], p[3]);
            } else if (s_app_selection == 22 && n >= 3) {
                result = inv_norm(p[0], p[1], p[2]);
            } else if (s_app_selection == 23 && n >= 2) {
                result = t_pdf_core(p[0], p[1], 0.0);
            } else if (s_app_selection == 24 && n >= 3) {
                result = t_cdf(p[1], p[2]) - t_cdf(p[0], p[2]);
            } else if (s_app_selection == 25 && n >= 2) {
                result = chi2_pdf_core(p[0], p[1], 0.0);
            } else if (s_app_selection == 26 && n >= 3) {
                result = chi2_cdf(p[1], p[2]) - chi2_cdf(p[0], p[2]);
            } else if (s_app_selection == 27 && n >= 3) {
                result = f_pdf_core(p[0], p[1], p[2]);
            } else if (s_app_selection == 28 && n >= 4) {
                result = f_cdf(p[1], p[2], p[3]) - f_cdf(p[0], p[2], p[3]);
            } else if (s_app_selection == 29 && n >= 3) {
                result = binom_pdf((int)llround(p[0]), p[1], (int)llround(p[2]));
            } else if (s_app_selection == 30 && n >= 3) {
                result = 0.0;
                for (int x = 0; x <= (int)llround(p[2]); x++) {
                    result += binom_pdf((int)llround(p[0]), p[1], x);
                }
            } else if (s_app_selection == 31 && n >= 2) {
                result = poisson_pdf(p[0], (int)llround(p[1]));
            } else if (s_app_selection == 32 && n >= 2) {
                result = 0.0;
                for (int x = 0; x <= (int)llround(p[1]); x++) {
                    result += poisson_pdf(p[0], x);
                }
            }

            if (isfinite(result)) {
                stats_result_begin(dist_name);
                stats_result_line("result %.10g", result);
                stats_result_line("input %.34s", s_calc_input);
                stats_result_line("%s", STATS_TOOLS[s_app_selection].detail);
                stats_result_show();
            } else {
                app_output("enter params in Calc");
            }
        } else if (s_app_selection == 33) {
            s_page = PAGE_STATS_PLOT;
            s_current_app = APP_STATS;
            ui_draw_current();
        }
        break;
    case APP_LISTS:
        if (s_app_selection == 0) {
            s_page = PAGE_LIST_EDITOR;
            s_current_app = APP_LISTS;
            s_list_entry[0] = '\0';
            s_list_editing = false;
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
            if (s_list_counts[s_list_index] == 0) {
                snprintf(line, sizeof(line), "sum L%d empty", s_list_index + 1);
                app_output(line);
            } else {
                snprintf(line, sizeof(line), "sum L%d %.10g", s_list_index + 1, list_sum(s_list_index));
                app_output(line);
            }
        } else if (s_app_selection == 3) {
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
            s_list_editing = false;
            snprintf(line, sizeof(line), "L%d cleared", s_list_index + 1);
            app_output(line);
        }
        break;
    case APP_MATRICES:
        if (s_app_selection == MATRIX_TOOL_EDIT) {
            matrix_open_editor();
        } else if (s_app_selection == MATRIX_TOOL_BROWSE) {
            matrix_open_viewer();
        } else if (s_app_selection == MATRIX_TOOL_DIMENSIONS) {
            matrix_open_size();
        } else if (s_app_selection == MATRIX_TOOL_ADD || s_app_selection == MATRIX_TOOL_SUBTRACT) {
            bool subtract = s_app_selection == MATRIX_TOOL_SUBTRACT;
            if (matrix_add_subtract_next(subtract)) {
                snprintf(line, sizeof(line), "%c %s %c stored", 'A' + s_matrix_index,
                         subtract ? "minus" : "plus", 'A' + matrix_next_index());
                snprintf(s_calc_output, sizeof(s_calc_output), "%s", line);
                matrix_reset_navigation();
                matrix_open_viewer();
            } else {
                app_output("matrices need matching dimensions");
            }
        } else if (s_app_selection == MATRIX_TOOL_MULTIPLY) {
            int other = matrix_next_index();
            if (matrix_multiply_next()) {
                snprintf(line, sizeof(line), "%c x %c stored", 'A' + s_matrix_index, 'A' + other);
                snprintf(s_calc_output, sizeof(s_calc_output), "%s", line);
                matrix_reset_navigation();
                matrix_open_viewer();
            } else {
                app_output("left columns must equal right rows");
            }
        } else if (s_app_selection == MATRIX_TOOL_SCALAR) {
            double scalar = 0.0;
            if (matrix_calc_value(&scalar) && matrix_scalar_multiply(scalar)) {
                snprintf(line, sizeof(line), "%c x %.8g stored", 'A' + s_matrix_index, scalar);
                snprintf(s_calc_output, sizeof(s_calc_output), "%s", line);
                matrix_reset_navigation();
                matrix_open_viewer();
            } else {
                app_output("put scalar in Calc or Ans");
            }
        } else if (s_app_selection == MATRIX_TOOL_POWER) {
            double value = 0.0;
            int exponent = 0;
            if (matrix_calc_value(&value) && value >= -999.0 && value <= 999.0 &&
                fabs(value - round(value)) < 1e-10) {
                exponent = (int)llround(value);
            } else {
                value = NAN;
            }
            if (isfinite(value) && matrix_power_a(exponent)) {
                snprintf(line, sizeof(line), "%c^%d stored", 'A' + s_matrix_index, exponent);
                snprintf(s_calc_output, sizeof(s_calc_output), "%s", line);
                matrix_reset_navigation();
                matrix_open_viewer();
            } else {
                app_output("needs square matrix and integer -999..999");
            }
        } else if (s_app_selection == MATRIX_TOOL_TRANSPOSE) {
            if (matrix_transpose_a()) {
                snprintf(line, sizeof(line), "%c transposed", 'A' + s_matrix_index);
                snprintf(s_calc_output, sizeof(s_calc_output), "%s", line);
                matrix_reset_navigation();
                matrix_open_viewer();
            } else {
                app_output("matrix empty");
            }
        } else if (s_app_selection == MATRIX_TOOL_DETERMINANT) {
            double det = 0.0;
            if (matrix_det_a(&det)) {
                snprintf(line, sizeof(line), "det %c %.10g", 'A' + s_matrix_index, det);
                app_output(line);
            } else {
                app_output("det needs square matrix");
            }
        } else if (s_app_selection == MATRIX_TOOL_INVERSE) {
            if (matrix_inverse_a()) {
                snprintf(line, sizeof(line), "%c inverse stored", 'A' + s_matrix_index);
                snprintf(s_calc_output, sizeof(s_calc_output), "%s", line);
                matrix_reset_navigation();
                matrix_open_viewer();
            } else {
                app_output("inverse needs nonsingular square matrix");
            }
        } else if (s_app_selection == MATRIX_TOOL_RREF || s_app_selection == MATRIX_TOOL_REF) {
            bool reduced = s_app_selection == MATRIX_TOOL_RREF;
            if (reduced ? matrix_rref_a() : matrix_ref_a()) {
                snprintf(line, sizeof(line), "%c %s stored", 'A' + s_matrix_index,
                         reduced ? "rref" : "ref");
                snprintf(s_calc_output, sizeof(s_calc_output), "%s", line);
                matrix_reset_navigation();
                matrix_open_viewer();
            } else {
                app_output("matrix empty");
            }
        } else if (s_app_selection == MATRIX_TOOL_IDENTITY) {
            if (matrix_set_identity()) {
                snprintf(line, sizeof(line), "%c=identity %dx%d", 'A' + s_matrix_index,
                         s_matrix_rows, s_matrix_cols);
                snprintf(s_calc_output, sizeof(s_calc_output), "%s", line);
                matrix_reset_navigation();
                matrix_open_viewer();
            } else {
                app_output("identity needs square dimensions");
            }
        } else if (s_app_selection == MATRIX_TOOL_ZERO) {
            if (s_matrix_rows <= 0 || s_matrix_cols <= 0) {
                s_matrix_rows = 3;
                s_matrix_cols = 3;
            }
            memset(s_matrix_a, 0, sizeof(s_matrix_a));
            snprintf(line, sizeof(line), "%c zero %dx%d", 'A' + s_matrix_index,
                     s_matrix_rows, s_matrix_cols);
            snprintf(s_calc_output, sizeof(s_calc_output), "%s", line);
            matrix_reset_navigation();
            matrix_open_viewer();
        } else if (s_app_selection == MATRIX_TOOL_AUGMENT) {
            if (matrix_augment_with_next()) {
                snprintf(line, sizeof(line), "%c augmented with %c",
                         'A' + s_matrix_index, 'A' + matrix_next_index());
                snprintf(s_calc_output, sizeof(s_calc_output), "%s", line);
                matrix_reset_navigation();
                matrix_open_viewer();
            } else {
                app_output("need same rows and <=99 columns");
            }
        } else if (s_app_selection == MATRIX_TOOL_EXTRACT) {
            if (matrix_extract_from_calc()) {
                snprintf(line, sizeof(line), "%c extract stored %dx%d", 'A' + s_matrix_index,
                         s_matrix_rows, s_matrix_cols);
                snprintf(s_calc_output, sizeof(s_calc_output), "%s", line);
                matrix_reset_navigation();
                matrix_open_viewer();
            } else {
                app_output("Calc: row,2  col,3  sub,1,2,3,4");
            }
        } else if (s_app_selection == MATRIX_TOOL_IMPORT) {
            if (matrix_parse_calc_input()) {
                snprintf(line, sizeof(line), "%c set %dx%d", 'A' + s_matrix_index, s_matrix_rows, s_matrix_cols);
                snprintf(s_calc_output, sizeof(s_calc_output), "%s", line);
                matrix_reset_navigation();
                matrix_open_viewer();
            } else {
                app_output("use calc: 1,2;3,4");
            }
        } else if (s_app_selection == MATRIX_TOOL_ROW_OP) {
            if (matrix_apply_row_command()) {
                snprintf(s_calc_output, sizeof(s_calc_output), "row op applied");
                matrix_reset_navigation();
                matrix_open_viewer();
            } else {
                app_output("use swap,1,2 scale,1,2 add,1,2,-3");
            }
        } else if (s_app_selection == MATRIX_TOOL_TO_LIST) {
            if (matrix_to_selected_list()) {
                snprintf(line, sizeof(line), "%c col1 -> L%d", 'A' + s_matrix_index, s_list_index + 1);
                app_output(line);
            } else {
                app_output("matrix/list too large");
            }
        } else if (s_app_selection == MATRIX_TOOL_FROM_LIST) {
            if (matrix_from_selected_list()) {
                snprintf(line, sizeof(line), "L%d -> %c column", s_list_index + 1, 'A' + s_matrix_index);
                snprintf(s_calc_output, sizeof(s_calc_output), "%s", line);
                matrix_reset_navigation();
                matrix_open_viewer();
            } else {
                app_output("selected list empty");
            }
        }
        break;
    case APP_SOLVER:
        if (s_page == PAGE_APP) {
            solver_open_home_selection();
            break;
        }
        if (s_app_selection == 0) {
            if (s_calc_input[0] == '\0') {
                app_output("type E1 in Calc first");
            } else {
                snprintf(s_solver_e1, sizeof(s_solver_e1), "%s", s_calc_input);
                s_solver_has_result = false;
                s_solver_result_imag = 0.0;
                s_solver_has_complex_result = false;
                snprintf(line, sizeof(line), "E1=%.40s", s_solver_e1);
                app_output(line);
            }
        } else if (s_app_selection == 1) {
            if (s_calc_input[0] == '\0') {
                app_output("type E2 in Calc first");
            } else {
                snprintf(s_solver_e2, sizeof(s_solver_e2), "%s", s_calc_input);
                s_solver_has_result = false;
                s_solver_result_imag = 0.0;
                s_solver_has_complex_result = false;
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
            if (s_solver_has_result && s_solver_has_complex_result) {
                app_output("frac needs real result");
            } else if (s_solver_has_result && calc_format_fraction_value(s_solver_result, line, sizeof(line))) {
                char out[96];
                snprintf(out, sizeof(out), "x=%.90s", line);
                app_output(out);
            } else {
                app_output("solve first");
            }
        } else if (s_app_selection == 5) {
            double coeffs[11] = {0.0};
            int coeff_count = parse_csv_numbers(s_calc_input, coeffs, 11);
            int root_count = solver_find_poly_roots(coeffs, coeff_count,
                                                    s_solver_poly_root_real,
                                                    s_solver_poly_root_imag,
                                                    SOLVER_POLY_ROOT_MAX);
            if (root_count < 0) {
                app_output("coeffs high..constant");
            } else {
                s_solver_poly_root_count = root_count;
                s_solver_poly_root_selected = 0;
                snprintf(s_solver_poly_source, sizeof(s_solver_poly_source), "%s", s_calc_input);
                snprintf(s_calc_output, sizeof(s_calc_output), "%d roots", root_count);
                s_page = PAGE_SOLVER_ROOTS;
                s_current_app = APP_SOLVER;
                ui_draw_current();
            }
        } else if (s_app_selection == 6) {
            if (solver_system_from_selected_matrix(line, sizeof(line))) {
                app_output(line);
            } else {
                app_output("need augmented matrix");
            }
        } else if (s_app_selection == 7) {
            snprintf(s_solver_e1, sizeof(s_solver_e1), "x^2");
            snprintf(s_solver_e2, sizeof(s_solver_e2), "4");
            s_solver_guess = 1.0;
            s_solver_result = 0.0;
            s_solver_result_imag = 0.0;
            s_solver_has_complex_result = false;
            s_solver_has_result = false;
            app_output("example E1 x^2 E2 4");
        } else {
            snprintf(s_solver_e1, sizeof(s_solver_e1), "x");
            snprintf(s_solver_e2, sizeof(s_solver_e2), "0");
            s_solver_guess = 0.0;
            s_solver_result = 0.0;
            s_solver_result_imag = 0.0;
            s_solver_has_complex_result = false;
            s_solver_has_result = false;
            app_output("solver cleared");
        }
        break;
    case APP_FINANCE:
        if (s_app_selection == 0) {
            finance_open_tvm();
        } else if (s_app_selection == 1) {
            finance_open_cashflow();
        } else if (s_app_selection == 2) {
            if (s_list_counts[s_list_index] == 0) {
                app_output("Add cash flows first");
            } else {
                double rate = finance_periodic_rate();
                double npv = finance_npv_from_list(s_list_index, rate);
                if (isfinite(npv)) {
                    char detail[64];
                    snprintf(detail, sizeof(detail), "Present value at %.6g%% per period", rate * 100.0);
                    finance_open_result("NET PRESENT VALUE", npv, detail);
                } else {
                    app_output("Discount rate is invalid");
                }
            }
        } else if (s_app_selection == 3) {
            if (s_list_counts[s_list_index] < 2) {
                app_output("IRR needs at least 2 flows");
            } else {
                double irr = 0.0;
                if (finance_irr_from_list(s_list_index, &irr)) {
                    finance_open_result("INTERNAL RATE", irr * 100.0, "Percent return per cash-flow period");
                } else {
                    app_output("No bracketed IRR found");
                }
            }
        } else if (s_app_selection == 4) {
            s_fin_begin = !s_fin_begin;
            app_output(s_fin_begin ? "payments BEGIN" : "payments END");
        } else if (s_app_selection == 5) {
            s_list_index = (s_list_index + 1) % LIST_COUNT;
            snprintf(line, sizeof(line), "Cash-flow list L%d", s_list_index + 1);
            app_output(line);
        } else if (s_app_selection == 6) {
            s_list_index = (s_list_index + LIST_COUNT - 1) % LIST_COUNT;
            snprintf(line, sizeof(line), "Cash-flow list L%d", s_list_index + 1);
            app_output(line);
        } else {
            s_fin_n = 12.0;
            s_fin_i = 5.0;
            s_fin_pv = 1000.0;
            s_fin_pmt = 0.0;
            s_fin_fv = 0.0;
            s_fin_py = 12.0;
            s_fin_cy = 12.0;
    s_fin_begin = false;
    s_fin_selection = 3;
    s_fin_entry[0] = '\0';
    s_fin_entry_active = false;
    s_fin_status[0] = '\0';
    s_fin_cash_cursor = 0;
    s_fin_cash_scroll = 0;
    s_fin_result_title[0] = '\0';
    s_fin_result_value = 0.0;
    s_fin_result_detail[0] = '\0';
            finance_reset_entry();
            app_output("TVM worksheet reset");
        }
        break;
    case APP_CONICS:
        if (s_app_selection >= 0 && s_app_selection <= OPENCALC_CONIC_GENERAL) {
            conic_open_editor(s_app_selection);
        } else {
            s_conic_overlay_selection = 0;
            s_page = PAGE_CONIC_GRAPHS;
            s_current_app = APP_CONICS;
            ui_draw_current();
        }
        break;
    case APP_INEQUALITY:
        if (s_app_selection == 0) {
            if (!inequality_solve_from_calc(false)) ui_draw_current();
        } else if (s_app_selection == 1) {
            inequality_open_editor(false);
        } else if (s_app_selection == 2) {
            inequality_open_editor(true);
        } else {
            if (!inequality_solve_from_calc(true)) ui_draw_current();
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
    char converted[2];
    const char *alpha_text = alpha_key_text(key, converted);
    if (alpha_text == NULL) {
        return false;
    }

    expression_append(alpha_text);
    ui_draw_current();
    return true;
}

static void calc_expand_ans_value(const char *input, const char *ans, char *out, size_t out_size)
{
    size_t out_len = 0;
    if (out_size == 0) {
        return;
    }

    for (size_t i = 0; input[i] != '\0' && out_len + 1 < out_size;) {
        bool left_boundary = i == 0 ||
            (!isalnum((unsigned char)input[i - 1]) && input[i - 1] != '_');
        bool has_ans = input[i + 1] != '\0' && input[i + 2] != '\0' &&
            (input[i] == 'A' || input[i] == 'a') &&
            (input[i + 1] == 'N' || input[i + 1] == 'n') &&
            (input[i + 2] == 'S' || input[i + 2] == 's');
        bool right_boundary = has_ans &&
            (!isalnum((unsigned char)input[i + 3]) && input[i + 3] != '_');
        if (left_boundary && has_ans && right_boundary) {
            size_t ans_len = strlen(ans);
            if (out_len + ans_len + 2 >= out_size) {
                break;
            }
            out[out_len++] = '(';
            memcpy(out + out_len, ans, ans_len);
            out_len += ans_len;
            out[out_len++] = ')';
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

static bool calc_matrix_literal(int matrix, char *out, size_t out_size)
{
    if (matrix < 0 || matrix >= MATRIX_COUNT || out == NULL || out_size < 3 ||
        s_matrix_rows_by_index[matrix] <= 0 || s_matrix_cols_by_index[matrix] <= 0) return false;
    size_t used = 0;
    out[used++] = '[';
    for (int row = 0; row < s_matrix_rows_by_index[matrix]; row++) {
        if (used + 2 >= out_size) return false;
        if (row) out[used++] = ',';
        out[used++] = '[';
        for (int col = 0; col < s_matrix_cols_by_index[matrix]; col++) {
            int written = snprintf(out + used, out_size - used, "%s%.10g", col ? "," : "",
                                   s_matrices[matrix][row][col]);
            if (written < 0 || (size_t)written >= out_size - used) return false;
            used += (size_t)written;
        }
        if (used + 1 >= out_size) return false;
        out[used++] = ']';
    }
    if (used + 2 > out_size) return false;
    out[used++] = ']';
    out[used] = '\0';
    return true;
}

static bool calc_catalog_value(const char *name, char *out, size_t out_size)
{
    if (name == NULL || out == NULL || out_size == 0) return false;
    size_t name_len = strlen(name);
    if (name_len == 2 && (name[0] == 'L' || name[0] == 'l') &&
        name[1] >= '1' && name[1] <= '6') {
        variable_list_literal(name[1] - '1', out, out_size);
        return true;
    }
    if (name_len == 4 && strncasecmp(name, "mat", 3) == 0) {
        int matrix = toupper((unsigned char)name[3]) - 'A';
        return calc_matrix_literal(matrix, out, out_size);
    }
    if (name_len >= 2 && (name[0] == 'Y' || name[0] == 'y') &&
        isdigit((unsigned char)name[1])) {
        char *end = NULL;
        long number = strtol(name + 1, &end, 10);
        if (*end == '\0' && number >= 1 && number <= GRAPH_FUNC_COUNT && s_graph_exprs[number - 1][0]) {
            snprintf(out, out_size, "(%s)", s_graph_exprs[number - 1]);
            return true;
        }
    }
    static const char *const graph_names[] = {"Xmin","Xmax","Ymin","Ymax","Xscl","Yscl","Tmin","Tmax","nMin","nMax"};
    double graph_values[] = {s_graph_xmin,s_graph_xmax,s_graph_ymin,s_graph_ymax,s_graph_xtick,s_graph_ytick,s_graph_tmin,s_graph_tmax,s_graph_nmin,s_graph_nmax};
    for (int i = 0; i < 10; i++) {
        if (strcasecmp(name, graph_names[i]) == 0) {
            snprintf(out, out_size, "(%.17g)", graph_values[i]);
            return true;
        }
    }
    if (name_len >= 5 && strncasecmp(name, "Stat", 4) == 0 &&
        isdigit((unsigned char)name[4])) {
        char *end = NULL;
        long number = strtol(name + 4, &end, 10);
        if (*end == '\0' && number >= 1 && number <= s_stats_result_line_count) {
            const char *equals = strchr(s_stats_result_lines[number - 1], '=');
            if (equals != NULL) {
                snprintf(out, out_size, "(%s)", equals + 1);
                return true;
            }
        }
    }
    if (strcasecmp(name, "Brightness") == 0) {
        snprintf(out, out_size, "(%d)", board_get_backlight_brightness());
        return true;
    }
    return false;
}

static bool calc_expand_catalog_variables(const char *input, char *out, size_t out_size)
{
    if (input == NULL || out == NULL || out_size == 0) return false;
    size_t used = 0;
    const char *p = input;
    while (*p) {
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            char name[OPENCALC_VARIABLE_NAME_MAX] = {0};
            size_t length = 0;
            while (isalnum((unsigned char)*p) || *p == '_') {
                if (length + 1 < sizeof(name)) name[length++] = *p;
                p++;
            }
            name[length] = '\0';
            char value[CALC_EXPR_MAX + CALC_RESULT_MAX];
            if (length == (size_t)(p - start) && calc_catalog_value(name, value, sizeof(value))) {
                size_t add = strlen(value);
                if (used + add + 1 > out_size) return false;
                memcpy(out + used, value, add);
                used += add;
            } else {
                size_t add = (size_t)(p - start);
                if (used + add + 1 > out_size) return false;
                memcpy(out + used, start, add);
                used += add;
            }
        } else {
            if (used + 2 > out_size) return false;
            out[used++] = *p++;
        }
    }
    out[used] = '\0';
    return true;
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

static bool expression_contains_complex_unit(const char *expr)
{
    if (expr == NULL) {
        return false;
    }
    for (const char *p = expr; *p != '\0'; p++) {
        if ((*p == 'i' || *p == 'I') &&
            (p == expr || !isalpha((unsigned char)p[-1])) &&
            !isalpha((unsigned char)p[1])) {
            return true;
        }
    }
    return false;
}

static void format_complex_value(double real, double imag, int mode, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }

    if (fabs(imag) < 1e-10) {
        snprintf(out, out_size, "%.10g", real);
        return;
    }

    if (mode == 2) {
        double radius = hypot(real, imag);
        double theta = atan2(imag, real);
        if (s_angle_mode == 0) {
            theta *= 180.0 / 3.14159265358979323846;
        }
        snprintf(out, out_size, "%.10g e^(%.10g i)", radius, theta);
        return;
    }

    snprintf(out, out_size, "%.10g%+.10gi", real, imag);
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

static bool work_solver_eval_complex_difference(const ui_work_job_t *job, double complex x, double complex *out)
{
    double left_real = 0.0;
    double left_imag = 0.0;
    double right_real = 0.0;
    double right_imag = 0.0;
    if (!opencalc_math_eval_complex_expression_var(job->solver.e1, 'x', creal(x), cimag(x),
                                                   &left_real, &left_imag) ||
        !opencalc_math_eval_complex_expression_var(job->solver.e2, 'x', creal(x), cimag(x),
                                                   &right_real, &right_imag)) {
        return false;
    }

    *out = (left_real - right_real) + (left_imag - right_imag) * I;
    return isfinite(creal(*out)) && isfinite(cimag(*out));
}

static bool work_solver_solve_complex_near_guess(const ui_work_job_t *job, double *root_real, double *root_imag)
{
    if (job == NULL || root_real == NULL || root_imag == NULL) {
        return false;
    }

    double complex z = job->solver.guess;
    double complex fz = 0.0;
    if (!work_solver_eval_complex_difference(job, z, &fz)) {
        return false;
    }
    if (cabs(fz) < 1e-10) {
        *root_real = creal(z);
        *root_imag = cimag(z);
        return true;
    }

    const double complex seeds[] = {
        0.0, 1.0, -1.0, I, -I, 1.0 + I, 1.0 - I, -1.0 + I, -1.0 - I,
    };
    double complex best = z;
    double best_abs = cabs(fz);

    for (size_t seed = 0; seed < sizeof(seeds) / sizeof(seeds[0]); seed++) {
        z = job->solver.guess + seeds[seed];
        for (int iter = 0; iter < 48; iter++) {
            if (!work_solver_eval_complex_difference(job, z, &fz)) {
                break;
            }
            double abs_f = cabs(fz);
            if (abs_f < best_abs) {
                best_abs = abs_f;
                best = z;
            }
            if (abs_f < 1e-9) {
                *root_real = fabs(creal(z)) < 5e-10 ? 0.0 : creal(z);
                *root_imag = fabs(cimag(z)) < 5e-10 ? 0.0 : cimag(z);
                return true;
            }

            double h = fmax(1e-5, cabs(z) * 1e-5);
            double complex fp = 0.0;
            double complex fm = 0.0;
            if (!work_solver_eval_complex_difference(job, z + h, &fp) ||
                !work_solver_eval_complex_difference(job, z - h, &fm)) {
                break;
            }
            double complex slope = (fp - fm) / (2.0 * h);
            if (cabs(slope) < 1e-12) {
                break;
            }
            z -= fz / slope;
            if (!isfinite(creal(z)) || !isfinite(cimag(z)) || cabs(z) > 1e12) {
                break;
            }
        }
    }

    if (best_abs < 1e-6) {
        *root_real = fabs(creal(best)) < 5e-10 ? 0.0 : creal(best);
        *root_imag = fabs(cimag(best)) < 5e-10 ? 0.0 : cimag(best);
        return true;
    }
    return false;
}

static bool work_solver_solve_near_guess(const ui_work_job_t *job, double guess, double *root)
{
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

static bool work_solver_bisect(const ui_work_job_t *job, double low, double high,
                               double tolerance, double *root)
{
    double f_low = 0.0;
    double f_high = 0.0;
    if (!work_solver_eval_difference(job, low, &f_low) ||
        !work_solver_eval_difference(job, high, &f_high)) return false;
    if (fabs(f_low) <= tolerance) { *root = low; return true; }
    if (fabs(f_high) <= tolerance) { *root = high; return true; }
    if ((f_low < 0.0) == (f_high < 0.0)) return false;

    for (int iteration = 0; iteration < 80; iteration++) {
        double mid = (low + high) * 0.5;
        double f_mid = 0.0;
        if (!work_solver_eval_difference(job, mid, &f_mid)) return false;
        if (fabs(f_mid) <= tolerance || fabs(high - low) <= tolerance) {
            *root = mid;
            return true;
        }
        if ((f_low < 0.0) != (f_mid < 0.0)) {
            high = mid;
        } else {
            low = mid;
            f_low = f_mid;
        }
    }
    *root = (low + high) * 0.5;
    return true;
}

static void work_solver_add_root(double root, double *roots, int *count, int max_roots,
                                 double merge_tolerance)
{
    if (!isfinite(root) || roots == NULL || count == NULL || *count >= max_roots) return;
    for (int i = 0; i < *count; i++) {
        if (fabs(roots[i] - root) <= merge_tolerance) return;
    }
    roots[(*count)++] = fabs(root) < merge_tolerance ? 0.0 : root;
}

static int work_solver_collect_real_roots(const ui_work_job_t *job, double *roots, int max_roots)
{
    if (job == NULL || roots == NULL || max_roots <= 0 ||
        !(job->solver.upper > job->solver.lower)) return 0;
    const int samples = 384;
    double step = (job->solver.upper - job->solver.lower) / samples;
    double tolerance = fmax(job->solver.tolerance, 1e-12);
    double merge = fmax(step * 0.25, tolerance * 20.0);
    int count = 0;
    double x0 = job->solver.lower;
    double f0 = 0.0;
    bool ok0 = work_solver_eval_difference(job, x0, &f0);
    double x1 = x0;
    double f1 = f0;
    bool ok1 = ok0;

    if (ok0 && fabs(f0) <= tolerance) work_solver_add_root(x0, roots, &count, max_roots, merge);
    for (int sample = 1; sample <= samples && count < max_roots; sample++) {
        double x2 = sample == samples ? job->solver.upper : job->solver.lower + sample * step;
        double f2 = 0.0;
        bool ok2 = work_solver_eval_difference(job, x2, &f2);
        if (ok1 && ok2 && (f1 < 0.0) != (f2 < 0.0)) {
            double root = 0.0;
            if (work_solver_bisect(job, x1, x2, tolerance, &root)) {
                double residual = 0.0;
                if (work_solver_eval_difference(job, root, &residual) && fabs(residual) <= tolerance * 10.0) {
                    work_solver_add_root(root, roots, &count, max_roots, merge);
                }
            }
        }
        if (sample >= 2 && ok0 && ok1 && ok2 && fabs(f1) <= fabs(f0) && fabs(f1) <= fabs(f2) &&
            fabs(f1) < fmax(0.05, sqrt(tolerance))) {
            double root = 0.0;
            if (work_solver_solve_near_guess(job, x1, &root) &&
                root >= job->solver.lower - merge && root <= job->solver.upper + merge) {
                double residual = 0.0;
                if (work_solver_eval_difference(job, root, &residual) && fabs(residual) <= tolerance * 10.0) {
                    work_solver_add_root(root, roots, &count, max_roots, merge);
                }
            }
        }
        f0 = f1; ok0 = ok1;
        x1 = x2; f1 = f2; ok1 = ok2;
    }
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (roots[j] < roots[i]) {
                double swap = roots[i]; roots[i] = roots[j]; roots[j] = swap;
            }
        }
    }
    return count;
}

static bool work_calc_should_cancel(void *context)
{
    const ui_work_job_t *job = context;
    return job == NULL || job->request_id != atomic_load(&s_calc_eval_generation);
}

static bool work_calc_expand_catalog(const char *input, char *out, size_t out_size,
                                     void *context)
{
    (void)context;
    return calc_expand_catalog_variables(input, out, out_size);
}

static void work_calc_eval(const ui_work_job_t *job, ui_work_result_t *result)
{
    result->request_id = job->request_id;
    snprintf(result->calc.expr, sizeof(result->calc.expr), "%s", job->calc.expr);
    opencalc_calc_eval_request_t request = {
        .expression = job->calc.expr,
        .ans = job->calc.ans,
        .complex_mode = job->calc.complex_mode,
        .display_format = job->calc.display_format,
        .print_mode = job->calc.print_mode,
        .degrees = job->degrees,
        .timeout_ms = OPENCALC_CAS_TIMEOUT_MS,
        .should_cancel = work_calc_should_cancel,
        .cancel_context = (void *)job,
        .expand_catalog = work_calc_expand_catalog,
        .catalog_context = NULL,
    };
    opencalc_calc_eval_result_t evaluated;
    opencalc_calc_evaluate(&request, &evaluated);
    result->ok = evaluated.ok;
    result->calc.update_ans = evaluated.update_ans;
    snprintf(result->calc.output, sizeof(result->calc.output), "%s", evaluated.output);
}

static void work_graph_symbolic_eval(const char *expression, bool degrees,
                                     char *out, size_t out_size)
{
    if (!opencalc_giac_eval(expression, degrees, out, out_size) &&
        !opencalc_cas_eval(expression, out, out_size)) {
        snprintf(out, out_size, "not available");
    }
}

static void work_graph_symbolic(const ui_work_job_t *job, ui_work_result_t *result)
{
    const char *primary = job->graph_symbolic.primary;
    const char *secondary = job->graph_symbolic.secondary;
    char command[384];
    char first[72];
    char second[72];
    char third[72];

    result->ok = true;
    result->graph_symbolic.graphing_mode = job->graph_symbolic.graphing_mode;
    result->graph_symbolic.series = job->graph_symbolic.series;
    result->graph_symbolic.fingerprint = job->graph_symbolic.fingerprint;

    switch (job->graph_symbolic.graphing_mode) {
    case 1:
        snprintf(command, sizeof(command), "normal(diff((%s),t)/diff((%s),t))", secondary, primary);
        work_graph_symbolic_eval(command, job->degrees, result->graph_symbolic.derivative,
                                 sizeof(result->graph_symbolic.derivative));
        snprintf(command, sizeof(command), "integrate((%s)*diff((%s),t),t)", secondary, primary);
        work_graph_symbolic_eval(command, job->degrees, result->graph_symbolic.integral,
                                 sizeof(result->graph_symbolic.integral));
        snprintf(command, sizeof(command), "solve((%s)=0,t)", secondary);
        work_graph_symbolic_eval(command, job->degrees, result->graph_symbolic.roots,
                                 sizeof(result->graph_symbolic.roots));
        snprintf(command, sizeof(command), "limit([(%s),(%s)],t,infinity)", primary, secondary);
        work_graph_symbolic_eval(command, job->degrees, result->graph_symbolic.asymptotes,
                                 sizeof(result->graph_symbolic.asymptotes));
        break;
    case 2:
        snprintf(command, sizeof(command),
                 "normal(((diff((%s),t)*sin(t)+(%s)*cos(t)))/(diff((%s),t)*cos(t)-(%s)*sin(t)))",
                 primary, primary, primary, primary);
        work_graph_symbolic_eval(command, job->degrees, result->graph_symbolic.derivative,
                                 sizeof(result->graph_symbolic.derivative));
        snprintf(command, sizeof(command), "integrate((%s)^2/2,t)", primary);
        work_graph_symbolic_eval(command, job->degrees, result->graph_symbolic.integral,
                                 sizeof(result->graph_symbolic.integral));
        snprintf(command, sizeof(command), "solve((%s)=0,t)", primary);
        work_graph_symbolic_eval(command, job->degrees, result->graph_symbolic.roots,
                                 sizeof(result->graph_symbolic.roots));
        snprintf(command, sizeof(command), "limit((%s),t,infinity)", primary);
        work_graph_symbolic_eval(command, job->degrees, result->graph_symbolic.asymptotes,
                                 sizeof(result->graph_symbolic.asymptotes));
        break;
    case 3:
        snprintf(command, sizeof(command), "simplify(subst((%s),n,n+1)-(%s))", primary, primary);
        work_graph_symbolic_eval(command, job->degrees, result->graph_symbolic.derivative,
                                 sizeof(result->graph_symbolic.derivative));
        snprintf(command, sizeof(command), "sum((%s),n)", primary);
        work_graph_symbolic_eval(command, job->degrees, result->graph_symbolic.integral,
                                 sizeof(result->graph_symbolic.integral));
        snprintf(command, sizeof(command), "solve((%s)=0,n)", primary);
        work_graph_symbolic_eval(command, job->degrees, result->graph_symbolic.roots,
                                 sizeof(result->graph_symbolic.roots));
        snprintf(command, sizeof(command), "limit((%s),n,infinity)", primary);
        work_graph_symbolic_eval(command, job->degrees, result->graph_symbolic.asymptotes,
                                 sizeof(result->graph_symbolic.asymptotes));
        break;
    default:
        snprintf(command, sizeof(command), "diff((%s),x)", primary);
        work_graph_symbolic_eval(command, job->degrees, result->graph_symbolic.derivative,
                                 sizeof(result->graph_symbolic.derivative));
        snprintf(command, sizeof(command), "integrate((%s),x)", primary);
        work_graph_symbolic_eval(command, job->degrees, result->graph_symbolic.integral,
                                 sizeof(result->graph_symbolic.integral));
        snprintf(command, sizeof(command), "solve((%s)=0,x)", primary);
        work_graph_symbolic_eval(command, job->degrees, result->graph_symbolic.roots,
                                 sizeof(result->graph_symbolic.roots));
        snprintf(command, sizeof(command), "solve(denom(normal((%s)))=0,x)", primary);
        work_graph_symbolic_eval(command, job->degrees, first, sizeof(first));
        snprintf(command, sizeof(command),
                 "[limit((%s)/x,x,infinity),limit((%s)-limit((%s)/x,x,infinity)*x,x,infinity)]",
                 primary, primary, primary);
        work_graph_symbolic_eval(command, job->degrees, second, sizeof(second));
        snprintf(command, sizeof(command),
                 "[limit((%s)/x,x,-infinity),limit((%s)-limit((%s)/x,x,-infinity)*x,x,-infinity)]",
                 primary, primary, primary);
        work_graph_symbolic_eval(command, job->degrees, third, sizeof(third));
        snprintf(result->graph_symbolic.asymptotes,
                 sizeof(result->graph_symbolic.asymptotes),
                 "vertical %s; [slope,offset] +%s -%s", first, second, third);
        break;
    }
}

static void ui_work_execute(const void *job_data, void *result_data)
{
    const ui_work_job_t *job = job_data;
    ui_work_result_t *result = result_data;
    opencalc_math_set_degrees(job->degrees);
    result->type = job->type;
    result->request_id = job->request_id;

    switch (job->type) {
    case UI_WORK_CALC_EVAL:
        work_calc_eval(job, result);
        break;
    case UI_WORK_SOLVER_SOLVE:
        result->solver.multiple = job->solver.scan_all;
        result->solver.complex_root = !job->solver.scan_all &&
            (job->solver.complex_mode != 0 ||
             expression_contains_complex_unit(job->solver.e1) ||
             expression_contains_complex_unit(job->solver.e2));
        if (job->solver.scan_all) {
            result->solver.root_count = work_solver_collect_real_roots(
                job, result->solver.roots, SOLVER_POLY_ROOT_MAX);
            result->ok = result->solver.root_count > 0;
        } else if (result->solver.complex_root) {
            result->ok = work_solver_solve_complex_near_guess(
                job, &result->solver.root, &result->solver.root_imag);
        } else {
            result->ok = work_solver_solve_near_guess(
                job, job->solver.guess, &result->solver.root);
            result->solver.root_imag = 0.0;
        }
        break;
    case UI_WORK_SOLVER_SYMBOLIC:
        snprintf(result->symbolic.title, sizeof(result->symbolic.title), "%s", job->symbolic.title);
        result->ok = opencalc_giac_eval(job->symbolic.expression, job->degrees,
                                        result->symbolic.output, sizeof(result->symbolic.output));
        if (!result->ok) {
            result->ok = opencalc_cas_eval(job->symbolic.expression,
                                           result->symbolic.output,
                                           sizeof(result->symbolic.output));
        }
        if (!result->ok) {
            snprintf(result->symbolic.output, sizeof(result->symbolic.output),
                     "No symbolic solution. Check syntax, domain, and variable list; the result may require a numerical method.");
        }
        break;
    case UI_WORK_GRAPH_CALC:
        result->ok = work_graph_calc_run(job, result);
        if (!result->ok) {
            snprintf(result->graph.status, sizeof(result->graph.status), "graph calc: no result");
        }
        break;
    case UI_WORK_GRAPH_SYMBOLIC:
        work_graph_symbolic(job, result);
        break;
    default:
        result->ok = false;
        break;
    }
}

static bool ui_work_submit(const ui_work_job_t *job)
{
    return job != NULL && opencalc_ui_work_submit(job);
}

static bool submit_calc_eval_job(void)
{
    if (s_calc_eval_pending) {
        atomic_fetch_add(&s_calc_eval_generation, 1);
        s_calc_eval_pending = false;
    }

    ui_work_job_t job;
    memset(&job, 0, sizeof(job));
    job.type = UI_WORK_CALC_EVAL;
    job.request_id = (uint32_t)atomic_fetch_add(&s_calc_eval_generation, 1) + 1u;
    job.degrees = s_angle_mode == 0;
    snprintf(job.calc.expr, sizeof(job.calc.expr), "%s", s_calc_input);
    snprintf(job.calc.ans, sizeof(job.calc.ans), "%s", s_calc_ans);
    job.calc.complex_mode = s_complex_mode;
    job.calc.display_format = s_display_format;
    job.calc.print_mode = s_print_mode;

    if (!ui_work_submit(&job)) {
        snprintf(s_calc_output, sizeof(s_calc_output), "math worker unavailable");
        return false;
    }
    s_calc_eval_pending = true;
    snprintf(s_calc_output, sizeof(s_calc_output), "calculating...  Back cancels");
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
    job.solver.lower = s_solver_lower;
    job.solver.upper = s_solver_upper;
    job.solver.tolerance = pow(10.0, -(double)s_solver_precision);
    job.solver.complex_mode = s_complex_mode;
    job.solver.scan_all = false;

    if (!ui_work_submit(&job)) {
        app_output("worker unavailable");
        return false;
    }

    s_solver_solve_pending = true;
    app_output("solving...");
    return true;
}

static bool submit_solver_scan_job(void)
{
    if (s_solver_solve_pending) {
        app_output("solver busy");
        return false;
    }
    if (!(s_solver_upper > s_solver_lower)) {
        app_output("upper bound must exceed lower");
        return false;
    }

    ui_work_job_t job;
    memset(&job, 0, sizeof(job));
    job.type = UI_WORK_SOLVER_SOLVE;
    job.degrees = s_angle_mode == 0;
    snprintf(job.solver.e1, sizeof(job.solver.e1), "%s", s_solver_e1);
    snprintf(job.solver.e2, sizeof(job.solver.e2), "%s", s_solver_e2);
    job.solver.guess = s_solver_guess;
    job.solver.lower = s_solver_lower;
    job.solver.upper = s_solver_upper;
    job.solver.tolerance = pow(10.0, -(double)s_solver_precision);
    job.solver.complex_mode = 0;
    job.solver.scan_all = true;

    if (!ui_work_submit(&job)) {
        app_output("worker unavailable");
        return false;
    }
    s_solver_solve_pending = true;
    app_output("scanning interval...");
    return true;
}

static bool submit_solver_symbolic_job(const char *expression, const char *title)
{
    if (s_solver_symbolic_pending) {
        app_output("symbolic solver busy");
        return false;
    }
    if (expression == NULL || expression[0] == '\0') {
        app_output("enter an equation first");
        return false;
    }

    ui_work_job_t job;
    memset(&job, 0, sizeof(job));
    job.type = UI_WORK_SOLVER_SYMBOLIC;
    job.degrees = s_angle_mode == 0;
    snprintf(job.symbolic.expression, sizeof(job.symbolic.expression), "%s", expression);
    snprintf(job.symbolic.title, sizeof(job.symbolic.title), "%s", title != NULL ? title : "Symbolic Result");
    if (!ui_work_submit(&job)) {
        app_output("worker unavailable");
        return false;
    }
    s_solver_symbolic_pending = true;
    app_output("symbolic CAS working...");
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
    job.graph.graphing_mode = s_graphing_mode;
    memcpy(job.graph.exprs, s_graph_exprs, sizeof(job.graph.exprs));
    memcpy(job.graph.enabled, s_graph_enabled, sizeof(job.graph.enabled));
    memcpy(job.graph.param_x, s_graph_param_x, sizeof(job.graph.param_x));
    memcpy(job.graph.param_y, s_graph_param_y, sizeof(job.graph.param_y));
    memcpy(job.graph.param_enabled, s_graph_param_enabled, sizeof(job.graph.param_enabled));
    memcpy(job.graph.polar_exprs, s_graph_polar_exprs, sizeof(job.graph.polar_exprs));
    memcpy(job.graph.polar_enabled, s_graph_polar_enabled, sizeof(job.graph.polar_enabled));
    memcpy(job.graph.seq_exprs, s_graph_seq_exprs, sizeof(job.graph.seq_exprs));
    memcpy(job.graph.seq_enabled, s_graph_seq_enabled, sizeof(job.graph.seq_enabled));
    job.graph.xmin = s_graph_xmin;
    job.graph.xmax = s_graph_xmax;
    job.graph.ymin = s_graph_ymin;
    job.graph.ymax = s_graph_ymax;
    job.graph.tmin = s_graph_tmin;
    job.graph.tmax = s_graph_tmax;
    job.graph.nmin = s_graph_nmin;
    job.graph.nmax = s_graph_nmax;
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

static bool submit_graph_symbolic_job(void)
{
    if (s_graph_symbolic_pending || !graph_series_is_active(s_graph_trace_fn)) {
        return false;
    }

    ui_work_job_t job;
    memset(&job, 0, sizeof(job));
    job.type = UI_WORK_GRAPH_SYMBOLIC;
    job.degrees = s_angle_mode == 0;
    job.graph_symbolic.graphing_mode = s_graphing_mode;
    job.graph_symbolic.series = s_graph_trace_fn;
    job.graph_symbolic.fingerprint = graph_symbolic_fingerprint(s_graph_trace_fn);

    switch (s_graphing_mode) {
    case 1:
        snprintf(job.graph_symbolic.primary, sizeof(job.graph_symbolic.primary),
                 "%s", s_graph_param_x[s_graph_trace_fn]);
        snprintf(job.graph_symbolic.secondary, sizeof(job.graph_symbolic.secondary),
                 "%s", s_graph_param_y[s_graph_trace_fn]);
        break;
    case 2:
        snprintf(job.graph_symbolic.primary, sizeof(job.graph_symbolic.primary),
                 "%s", s_graph_polar_exprs[s_graph_trace_fn]);
        break;
    case 3:
        snprintf(job.graph_symbolic.primary, sizeof(job.graph_symbolic.primary),
                 "%s", s_graph_seq_exprs[s_graph_trace_fn]);
        break;
    default:
        snprintf(job.graph_symbolic.primary, sizeof(job.graph_symbolic.primary),
                 "%s", s_graph_exprs[s_graph_trace_fn]);
        break;
    }

    if (!ui_work_submit(&job)) {
        snprintf(s_graph_symbolic_derivative, sizeof(s_graph_symbolic_derivative), "worker unavailable");
        return false;
    }
    s_graph_symbolic_pending = true;
    snprintf(s_graph_symbolic_derivative, sizeof(s_graph_symbolic_derivative), "working...");
    s_graph_symbolic_integral[0] = '\0';
    s_graph_symbolic_roots[0] = '\0';
    s_graph_symbolic_asymptotes[0] = '\0';
    return true;
}

static void open_graph_symbolic(void)
{
    if (!graph_series_is_active(s_graph_trace_fn)) {
        s_graph_trace = false;
        if (!graph_trace_cycle_line()) {
            snprintf(s_graph_status, sizeof(s_graph_status), "enable a graph first");
            s_page = PAGE_GRAPH;
            s_current_app = APP_GRAPH;
            ui_draw_current();
            return;
        }
    }
    s_graph_trace = true;
    s_page = PAGE_GRAPH_SYMBOLIC;
    s_current_app = APP_GRAPH;
    (void)submit_graph_symbolic_job();
    ui_draw_current();
}

static void ui_work_apply_result(const ui_work_result_t *result)
{
    if (result == NULL) {
        return;
    }

    switch (result->type) {
    case UI_WORK_CALC_EVAL:
        if (result->request_id != atomic_load(&s_calc_eval_generation)) {
            break;
        }
        s_calc_eval_pending = false;
        snprintf(s_calc_output, sizeof(s_calc_output), "%s", result->calc.output);
        if (result->calc.update_ans) {
            strncpy(s_calc_ans, result->calc.output, sizeof(s_calc_ans) - 1);
            s_calc_ans[sizeof(s_calc_ans) - 1] = '\0';
        }
        opencalc_calc_history_push(result->calc.expr, s_calc_output);
        worksheet_mark_dirty();
        char assignment_name[OPENCALC_VARIABLE_NAME_MAX];
        if (result->calc.update_ans &&
            opencalc_math_assignment_name(result->calc.expr, assignment_name, sizeof(assignment_name))) {
            variables_save_all();
        }
        printf("calc %s => %s\n", result->calc.expr, s_calc_output);
        s_calc_input[0] = '\0';
        s_calc_cursor = 0;
        if (s_page == PAGE_CALCULATOR && strlen(s_calc_output) > 80) {
            s_calc_result_scroll = 0;
            s_page = PAGE_CALC_RESULT;
            s_current_app = APP_CALCULATOR;
        }
        ui_draw_current();
        break;
    case UI_WORK_SOLVER_SOLVE:
        s_solver_solve_pending = false;
        if (result->solver.multiple) {
            s_solver_has_result = false;
            s_solver_has_complex_result = false;
            if (result->ok) {
                s_solver_poly_root_count = result->solver.root_count;
                s_solver_poly_root_selected = 0;
                for (int i = 0; i < result->solver.root_count; i++) {
                    s_solver_poly_root_real[i] = result->solver.roots[i];
                    s_solver_poly_root_imag[i] = 0.0;
                }
                snprintf(s_solver_poly_source, sizeof(s_solver_poly_source),
                         "%.42s = %.42s", s_solver_e1, s_solver_e2);
                s_solver_roots_are_polynomial = false;
                s_page = PAGE_SOLVER_ROOTS;
                s_current_app = APP_SOLVER;
                ui_draw_current();
            } else {
                app_output("no real roots in bounds");
            }
        } else if (result->ok) {
            s_solver_result = result->solver.root;
            s_solver_result_imag = result->solver.root_imag;
            s_solver_has_complex_result = result->solver.complex_root &&
                fabs(result->solver.root_imag) > 1e-10;
            s_solver_guess = result->solver.root;
            s_solver_has_result = true;
            char line[96];
            if (result->solver.complex_root) {
                char root[48];
                format_complex_value(result->solver.root, result->solver.root_imag,
                                     s_complex_mode, root, sizeof(root));
                snprintf(line, sizeof(line), "x=%.42s", root);
            } else {
                snprintf(line, sizeof(line), "x=%.*g", s_solver_precision, result->solver.root);
            }
            app_output(line);
        } else {
            s_solver_result_imag = 0.0;
            s_solver_has_complex_result = false;
            s_solver_has_result = false;
            app_output("no solution near guess");
        }
        break;
    case UI_WORK_SOLVER_SYMBOLIC:
        s_solver_symbolic_pending = false;
        if (s_ineq_symbolic_pending) {
            s_ineq_symbolic_pending = false;
            snprintf(s_ineq_exact_text, sizeof(s_ineq_exact_text), "%s", result->symbolic.output);
            snprintf(s_ineq_status, sizeof(s_ineq_status), "%s",
                     result->ok ? "Exact CAS result ready" : "CAS unavailable; numerical result retained");
            s_ineq_result_scroll = 0;
            s_page = PAGE_INEQ_RESULT;
            s_current_app = APP_INEQUALITY;
            ui_draw_current();
            break;
        }
        snprintf(s_solver_symbolic_title, sizeof(s_solver_symbolic_title), "%s", result->symbolic.title);
        snprintf(s_solver_symbolic_result, sizeof(s_solver_symbolic_result), "%s", result->symbolic.output);
        s_solver_symbolic_scroll = 0;
        s_page = PAGE_SOLVER_SYMBOLIC_RESULT;
        s_current_app = APP_SOLVER;
        ui_draw_current();
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
    case UI_WORK_GRAPH_SYMBOLIC:
        s_graph_symbolic_pending = false;
        if (result->graph_symbolic.graphing_mode == s_graphing_mode &&
            result->graph_symbolic.series == s_graph_trace_fn &&
            result->graph_symbolic.fingerprint == graph_symbolic_fingerprint(s_graph_trace_fn)) {
            snprintf(s_graph_symbolic_derivative, sizeof(s_graph_symbolic_derivative),
                     "%s", result->graph_symbolic.derivative);
            snprintf(s_graph_symbolic_integral, sizeof(s_graph_symbolic_integral),
                     "%s", result->graph_symbolic.integral);
            snprintf(s_graph_symbolic_roots, sizeof(s_graph_symbolic_roots),
                     "%s", result->graph_symbolic.roots);
            snprintf(s_graph_symbolic_asymptotes, sizeof(s_graph_symbolic_asymptotes),
                     "%s", result->graph_symbolic.asymptotes);
        } else if (s_page == PAGE_GRAPH_SYMBOLIC) {
            (void)submit_graph_symbolic_job();
        }
        if (s_page == PAGE_GRAPH_SYMBOLIC) ui_draw_current();
        break;
    default:
        break;
    }
}

static void ui_work_poll_results(void)
{
    ui_work_result_t *result = NULL;
    while ((result = opencalc_ui_work_take_result()) != NULL) {
        ui_work_apply_result(result);
        heap_caps_free(result);
    }
}

static void cancel_calc_eval(void)
{
    if (!s_calc_eval_pending) return;
    atomic_fetch_add(&s_calc_eval_generation, 1);
    s_calc_eval_pending = false;
    snprintf(s_calc_output, sizeof(s_calc_output), "cancelled");
    ui_draw_current();
}

static void calc_eval(void)
{
    char expr[CALC_EXPR_MAX];
    snprintf(expr, sizeof(expr), "%s", s_calc_input);
    if (expr[0] == '\0') {
        return;
    }
    submit_calc_eval_job();
}

static void script_worker_task(void *arg)
{
    (void)arg;
    script_run_request_t request;

    for (;;) {
        if (xQueueReceive(s_script_request_queue, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        script_run_result_t *result = &s_script_worker_result;
        memset(result, 0, sizeof(*result));
        printf("Python worker: request received for %s, stack free=%u bytes\n",
               request.path, (unsigned)uxTaskGetStackHighWaterMark(NULL));
        py_init(&s_script_py);
        py_set_output_callback(&s_script_py, script_output_callback, NULL);
        py_set_input_callback(&s_script_py, script_input_callback, NULL);
        py_set_debug_callback(&s_script_py, script_debug_callback, NULL);
        py_set_native_callback(&s_script_py, script_native_callback, NULL);
        py_set_gpio_callbacks(&s_script_py, script_sensor_gpio_mode,
                              script_sensor_gpio_write, script_sensor_gpio_read, NULL);
        py_set_execution_limits(&s_script_py, OPENCALC_SCRIPT_STATEMENT_LIMIT,
                                OPENCALC_SCRIPT_CALL_DEPTH_LIMIT);
        s_script_debug_mode = request.debug;
        s_script_debug_step = request.debug;
        s_script_started_us = esp_timer_get_time();
        printf("Python worker: interpreter initialized; executing file\n");
        result->ok = py_run_file(&s_script_py, request.path, NULL, 0);
        printf("Python worker: file execution returned %s, stack free=%u bytes\n",
               result->ok ? "ok" : "error",
               (unsigned)uxTaskGetStackHighWaterMark(NULL));
        result->elapsed_ms = (uint32_t)((esp_timer_get_time() - s_script_started_us) / 1000);
        snprintf(result->error, sizeof(result->error), "%s", s_script_py.error);
        snprintf(result->traceback, sizeof(result->traceback), "%s", py_get_traceback(&s_script_py));
        py_format_variables(&s_script_py, result->variables, sizeof(result->variables));
        const py_profile_t *profile = py_get_profile(&s_script_py);
        if (profile != NULL) result->profile = *profile;
        result->stack_high_water = uxTaskGetStackHighWaterMark(NULL);
        py_deinit(&s_script_py);

        uint8_t completed = 1;
        xQueueSend(s_script_result_queue, &completed, portMAX_DELAY);
    }
}

static bool script_worker_start(void)
{
    if (s_script_output_mutex == NULL) {
        s_script_output_mutex = xSemaphoreCreateMutex();
    }
    opencalc_worksheet_model_init();
    if (s_script_request_queue == NULL) {
        s_script_request_queue = xQueueCreate(1, sizeof(script_run_request_t));
    }
    if (s_script_result_queue == NULL) {
        s_script_result_queue = xQueueCreate(1, sizeof(uint8_t));
    }
    if (s_script_task == NULL && s_script_request_queue != NULL && s_script_result_queue != NULL) {
        BaseType_t created = pdFAIL;
        uint32_t stack_size = OPENCALC_SCRIPT_TASK_STACK;
        while (stack_size >= OPENCALC_SCRIPT_TASK_STACK_MIN) {
            created = xTaskCreatePinnedToCore(
                script_worker_task,
                "opencalc_script",
                stack_size,
                NULL,
                5,
                &s_script_task,
                OPENCALC_WORKER_CORE);
            if (created == pdPASS) {
                s_script_task_stack_size = stack_size;
                break;
            }
            s_script_task = NULL;
            if (stack_size < OPENCALC_SCRIPT_TASK_STACK_MIN + 4096) break;
            stack_size -= 4096;
        }
        if (created != pdPASS) {
            s_script_task = NULL;
            s_script_task_stack_size = 0;
            printf("ERROR: failed to start script worker (internal=%u largest=%u psram=%u)\n",
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                   (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        } else {
            printf("Script worker ready: %u-byte internal stack on core %d\n",
                   (unsigned)s_script_task_stack_size, OPENCALC_WORKER_CORE);
        }
    }
    return s_script_task != NULL && s_script_output_mutex != NULL &&
           s_script_request_queue != NULL &&
           s_script_result_queue != NULL;
}

bool opencalc_ui_prepare_script_worker(void)
{
    return script_worker_start();
}

static void script_worker_poll_result(void)
{
    if (!s_script_running || s_script_result_queue == NULL) return;

    uint8_t completed = 0;
    if (xQueueReceive(s_script_result_queue, &completed, 0) != pdTRUE || completed == 0) return;
    const script_run_result_t *result = &s_script_worker_result;

    printf("Python finished: %s%s%s\n",
           result->ok ? "ok" : "error",
           result->error[0] != '\0' ? " - " : "",
           result->error);
    printf("Python worker stack high-water mark: %u bytes\n",
           (unsigned)result->stack_high_water);
    if (result->stack_high_water < 2048) {
        printf("WARNING: Python worker stack margin is below 2048 bytes\n");
    }
    s_script_running = false;
    s_script_debug_paused = false;
    s_script_profile = result->profile;
    s_script_elapsed_ms = result->elapsed_ms;
    if (s_script_output_mutex != NULL) xSemaphoreTake(s_script_output_mutex, portMAX_DELAY);
    snprintf(s_script_traceback, sizeof(s_script_traceback), "%s", result->traceback);
    snprintf(s_script_variables, sizeof(s_script_variables), "%s", result->variables);
    if (s_script_output_mutex != NULL) xSemaphoreGive(s_script_output_mutex);

    if (!result->ok && result->error[0] != '\0') {
        script_output_append("\npython error: ");
        script_output_append(result->error);
        script_output_append("\n");
    } else if (s_script_output[0] == '\0') {
        script_output_append("Done\n");
    }
    snprintf(s_script_title, sizeof(s_script_title), "%.47s", s_scripts[s_script_selection]);
    if (!result->ok) s_script_view = SCRIPT_VIEW_TRACEBACK;
    snprintf(s_script_status, sizeof(s_script_status), result->ok ?
             "done - Graph views  Enter exits" : "error - Graph views  Enter exits");
    snprintf(s_calc_output, sizeof(s_calc_output), result->ok ? "ran %s" : "script error",
             s_scripts[s_script_selection]);
    s_script_screen_dirty = false;
    if (s_page == PAGE_SCRIPT_IO) ui_draw_current();
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

    if (!usb_msc_mount_app()) {
        snprintf(s_script_status, sizeof(s_script_status), "storage busy - eject USB drive");
        ui_draw_current();
        return;
    }
    s_usb_storage_enabled = false;

    if (!script_worker_start()) {
        snprintf(s_script_status, sizeof(s_script_status), "script worker unavailable");
        ui_draw_current();
        return;
    }

    script_run_request_t request = {0};
    snprintf(request.path, sizeof(request.path), "/data/scripts/%s", s_scripts[s_script_selection]);
    request.debug = s_script_action == SCRIPT_ACTION_DEBUG;
    printf("Running %s\n", request.path);
    s_script_output[0] = '\0';
    s_script_variables[0] = '\0';
    s_script_traceback[0] = '\0';
    memset(&s_script_profile, 0, sizeof(s_script_profile));
    s_script_elapsed_ms = 0;
    s_script_view = SCRIPT_VIEW_CONSOLE;
    s_script_graphics_count = 0;
    s_script_graphics_active = false;
    s_script_first_output_logged = false;
    snprintf(s_script_title, sizeof(s_script_title), request.debug ? "Debugging %.34s" : "Running %.36s",
             s_scripts[s_script_selection]);
    snprintf(s_script_status, sizeof(s_script_status), request.debug ?
             "debug starting..." : "running...");
    s_page = PAGE_SCRIPT_IO;
    s_current_app = APP_PYTHON;
    ui_draw_current();

    s_script_running = true;
    s_script_input_active = false;
    s_script_input_submitted = false;
    s_script_input_cancelled = false;
    s_script_stop_requested = false;
    s_script_debug_paused = false;
    s_script_debug_resume = 0;
    printf("python memory: stack_free=%u internal_free=%u psram_free=%u\n",
           (unsigned)uxTaskGetStackHighWaterMark(NULL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    uint8_t stale_completion;
    while (xQueueReceive(s_script_result_queue, &stale_completion, 0) == pdTRUE) {
    }
    if (xQueueSend(s_script_request_queue, &request, pdMS_TO_TICKS(100)) != pdTRUE) {
        s_script_running = false;
        snprintf(s_script_status, sizeof(s_script_status), "script worker communication failed");
        script_output_append("python error: script worker communication failed\n");
        ui_draw_current();
    }
}

static void list_editor_save_entry(void)
{
    if (!s_list_editing) {
        if (s_list_cursor < s_list_counts[s_list_index]) {
            snprintf(s_list_entry, sizeof(s_list_entry), "%.12g",
                     s_lists[s_list_index][s_list_cursor]);
        } else {
            s_list_entry[0] = '\0';
        }
        s_list_editing = true;
        ui_draw_current();
        return;
    }
    if (s_list_entry[0] == '\0' || s_list_cursor >= LIST_MAX_VALUES) {
        s_list_editing = false;
        ui_draw_current();
        return;
    }

    char *end = NULL;
    double value = strtod(s_list_entry, &end);
    if (end == s_list_entry || *end != '\0' || !isfinite(value)) {
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
    s_list_editing = false;
    ui_draw_current();
}

static void list_editor_delete(void)
{
    if (s_list_editing) {
        size_t len = strlen(s_list_entry);
        if (len > 0) {
            s_list_entry[len - 1] = '\0';
        } else {
            s_list_editing = false;
        }
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
    if (!s_list_editing) {
        s_list_entry[0] = '\0';
        len = 0;
        s_list_editing = true;
    }
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
    if (s_script_running) {
        s_script_stop_requested = true;
        py_request_abort(&s_script_py);
        s_script_debug_resume = 2;
        s_script_input_cancelled = true;
        if (s_script_task != NULL) xTaskNotifyGive(s_script_task);
        snprintf(s_script_status, sizeof(s_script_status), "stopping safely...");
        ui_draw_current();
        return;
    }
    s_home_selection = APP_CALCULATOR;
    s_page = PAGE_HOME;
#if OPENCALC_EXPORT_USB_STORAGE_TO_HOST
    (void)worksheet_persist_flush();
    s_usb_storage_enabled = usb_msc_mount_usb();
#else
    s_usb_storage_enabled = false;
#endif
    ui_draw_current();
}

static bool reference_page_active(void)
{
    return s_page == PAGE_REFERENCE_HOME || s_page == PAGE_PERIODIC_TABLE ||
        s_page == PAGE_ELEMENT_DETAIL || s_page == PAGE_REFERENCE_LIST ||
        s_page == PAGE_REFERENCE_DETAIL;
}

static void open_reference_center(void)
{
    if (!reference_page_active()) {
        s_reference_return_page = s_page;
        s_reference_return_app = s_current_app;
    }
    s_reference_home_selection = 0;
    s_page = PAGE_REFERENCE_HOME;
    s_alpha_active = false;
    s_alpha_locked = false;
    ui_draw_current();
}

static void periodic_move_selection(int dx, int dy)
{
    int row = 0;
    int column = 0;
    if (!opencalc_periodic_find_position(s_periodic_atomic_number, &row, &column)) return;

    int best_atomic_number = 0;
    int best_score = 100000;
    for (int candidate_row = 0; candidate_row < 9; ++candidate_row) {
        for (int candidate_column = 0; candidate_column < 18; ++candidate_column) {
            int atomic_number = opencalc_periodic_atomic_number_at(candidate_row, candidate_column);
            if (atomic_number == 0) continue;
            int score;
            if (dx != 0) {
                if (candidate_row != row || (candidate_column - column) * dx <= 0) continue;
                score = abs(candidate_column - column);
            } else {
                if ((candidate_row - row) * dy <= 0) continue;
                score = abs(candidate_row - row) * 32 + abs(candidate_column - column);
            }
            if (score < best_score) {
                best_score = score;
                best_atomic_number = atomic_number;
            }
        }
    }
    if (best_atomic_number != 0) s_periodic_atomic_number = best_atomic_number;
}

static void reference_change_category(int delta)
{
    int category = (int)s_reference_category + delta;
    if (category < 0) category = OPENCALC_REFERENCE_CATEGORY_COUNT - 1;
    if (category >= OPENCALC_REFERENCE_CATEGORY_COUNT) category = 0;
    s_reference_category = (opencalc_reference_category_t)category;
    s_reference_selection = 0;
}

static void key_back(void)
{
    if (s_script_running) {
        s_script_stop_requested = true;
        py_request_abort(&s_script_py);
        s_script_debug_resume = 2;
        s_script_input_cancelled = true;
        if (s_script_task != NULL) xTaskNotifyGive(s_script_task);
        snprintf(s_script_status, sizeof(s_script_status), "stopping safely...");
        ui_draw_current();
        return;
    }
    if (s_page == PAGE_CALCULATOR && s_calc_eval_pending) {
        cancel_calc_eval();
        return;
    }

    if (s_page == PAGE_GRAPH && (s_graph_zoom_mode || s_graph_trace)) {
        s_graph_zoom_mode = false;
        s_graph_trace = false;
        ui_draw_current();
        return;
    }

    if (s_page == PAGE_CALC_RESULT) {
        s_page = PAGE_CALCULATOR;
        s_current_app = APP_CALCULATOR;
    } else if (s_page == PAGE_VARIABLE_NAME) {
        s_page = PAGE_VARIABLES;
        s_variable_status[0] = '\0';
    } else if (s_page == PAGE_VARIABLES) {
        page_id_t return_page = s_variable_return_page;
        s_page = return_page;
        s_current_app = return_page == PAGE_Y_EQUALS ? APP_GRAPH : APP_CALCULATOR;
        s_variable_status[0] = '\0';
    } else if (s_page == PAGE_REFERENCE_HOME) {
        s_page = s_reference_return_page;
        s_current_app = s_reference_return_app;
    } else if (s_page == PAGE_PERIODIC_TABLE) {
        s_page = PAGE_REFERENCE_HOME;
        s_reference_home_selection = 0;
    } else if (s_page == PAGE_ELEMENT_DETAIL) {
        s_page = PAGE_PERIODIC_TABLE;
    } else if (s_page == PAGE_REFERENCE_LIST) {
        s_page = PAGE_REFERENCE_HOME;
        s_reference_home_selection = (int)s_reference_category + 1;
    } else if (s_page == PAGE_REFERENCE_DETAIL) {
        s_page = PAGE_REFERENCE_LIST;
    } else if (s_page == PAGE_HOME) {
        s_page = PAGE_CALCULATOR;
        s_current_app = APP_CALCULATOR;
    } else if (s_page == PAGE_CALCULATOR) {
        if (!open_previous_app()) {
            s_page = PAGE_HOME;
            s_home_selection = APP_CALCULATOR;
        } else {
            return;
        }
    } else if (s_page == PAGE_MATH_MENU) {
        s_page = s_math_return_page;
        s_current_app = s_page == PAGE_Y_EQUALS ? APP_GRAPH : APP_CALCULATOR;
    } else if (s_page == PAGE_GRAPH_WINDOW || s_page == PAGE_GRAPH_CALC ||
               s_page == PAGE_GRAPH_SYMBOLIC ||
               s_page == PAGE_GRAPH_FORMAT || s_page == PAGE_Y_EQUALS) {
        s_page = PAGE_GRAPH;
        s_current_app = APP_GRAPH;
    } else if (s_page == PAGE_TABLE_SETUP) {
        if (open_previous_app()) return;
        s_page = PAGE_TABLE;
        s_current_app = APP_TABLE;
    } else if (s_page == PAGE_LIST_EDITOR) {
        s_page = PAGE_APP;
        s_current_app = APP_LISTS;
    } else if (s_page == PAGE_MATRIX_EDITOR || s_page == PAGE_MATRIX_VIEWER ||
               s_page == PAGE_MATRIX_SIZE) {
        s_matrix_entry[0] = '\0';
        s_matrix_entry_active = false;
        s_matrix_status[0] = '\0';
        if (s_solver_matrix_editing) {
            s_solver_matrix_editing = false;
            s_solver_workflow = SOLVER_WORKFLOW_SYSTEM;
            s_page = PAGE_SOLVER_WORKFLOW;
            s_current_app = APP_SOLVER;
        } else {
            s_page = PAGE_APP;
            s_current_app = APP_MATRICES;
        }
    } else if (s_page == PAGE_FINANCE_TVM || s_page == PAGE_FINANCE_CASHFLOW ||
               s_page == PAGE_FINANCE_RESULT) {
        finance_reset_entry();
        s_page = PAGE_APP;
        s_current_app = APP_FINANCE;
    } else if (s_page == PAGE_SCRIPT_IO) {
        scripts_scan();
        s_page = PAGE_SCRIPTS;
        s_current_app = APP_PYTHON;
    } else if (s_page == PAGE_SCRIPT_EDITOR) {
        scripts_scan();
        s_page = PAGE_SCRIPTS;
        s_current_app = APP_PYTHON;
    } else if (s_page == PAGE_SOLVER_ROOTS) {
        s_solver_workflow = s_solver_roots_are_polynomial ?
            SOLVER_WORKFLOW_POLYNOMIAL : SOLVER_WORKFLOW_NUMERIC;
        s_page = PAGE_SOLVER_WORKFLOW;
        s_current_app = APP_SOLVER;
    } else if (s_page == PAGE_STATS_SETUP || s_page == PAGE_STATS_RESULT || s_page == PAGE_STATS_PLOT) {
        if (s_stats_return_page == PAGE_GRAPH && open_previous_app()) return;
        s_page = s_stats_return_page;
        s_current_app = s_page == PAGE_GRAPH ? APP_GRAPH : APP_STATS;
    } else if (s_page == PAGE_SCRIPTS) {
        s_page = PAGE_PROGRAM_MENU;
        s_current_app = APP_PYTHON;
    } else if (s_page == PAGE_PROGRAM_MENU) {
        if (open_previous_app()) return;
        s_page = PAGE_HOME;
    } else if (s_page == PAGE_GAME_MENU) {
        if (s_current_app == APP_GRAPH) {
            s_page = PAGE_GRAPH;
            ui_draw_current();
            return;
        }
        open_app(s_current_app);
        return;
    } else if (s_page == PAGE_MODE_MENU) {
        s_page = PAGE_CALCULATOR;
        s_current_app = APP_CALCULATOR;
    } else if (s_page == PAGE_GRAPH || s_page == PAGE_TABLE || s_page == PAGE_SETTINGS ||
               s_page == PAGE_APP) {
        if (open_previous_app()) return;
        s_page = PAGE_HOME;
    } else {
        if (open_previous_app()) return;
        s_page = PAGE_HOME;
    }

    ui_draw_current();
}

static void key_enter(void)
{
    if (s_page == PAGE_TABLE_SETUP) {
        s_page = PAGE_TABLE;
        s_current_app = APP_TABLE;
        ui_draw_current();
    } else if (s_page == PAGE_REFERENCE_HOME) {
        if (s_reference_home_selection == 0) {
            s_page = PAGE_PERIODIC_TABLE;
        } else {
            s_reference_category = (opencalc_reference_category_t)(s_reference_home_selection - 1);
            s_reference_selection = 0;
            s_page = PAGE_REFERENCE_LIST;
        }
        ui_draw_current();
    } else if (s_page == PAGE_PERIODIC_TABLE) {
        s_page = PAGE_ELEMENT_DETAIL;
        ui_draw_current();
    } else if (s_page == PAGE_ELEMENT_DETAIL) {
        s_page = PAGE_PERIODIC_TABLE;
        ui_draw_current();
    } else if (s_page == PAGE_REFERENCE_LIST) {
        s_page = PAGE_REFERENCE_DETAIL;
        ui_draw_current();
    } else if (s_page == PAGE_REFERENCE_DETAIL) {
        s_page = PAGE_REFERENCE_LIST;
        ui_draw_current();
    } else if (s_page == PAGE_HOME) {
        ui_open_selected_app();
    } else if (s_page == PAGE_GAME_MENU) {
        launch_selected_game();
    } else if (s_page == PAGE_PROGRAM_MENU) {
        if (s_program_selection == 0) {
            open_scripts_browser_for(SCRIPT_ACTION_RUN);
            ui_draw_current();
        } else if (s_program_selection == 1) {
            open_scripts_browser_for(SCRIPT_ACTION_DEBUG);
            ui_draw_current();
        } else if (s_program_selection == 2) {
            open_scripts_browser_for(SCRIPT_ACTION_EDIT);
            ui_draw_current();
        } else if (s_program_selection == 3) {
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
    } else if (s_page == PAGE_SOLVER_ROOTS) {
        solver_copy_selected_root_to_calc();
        ui_draw_current();
    } else if (s_page == PAGE_STATS_SETUP) {
        stats_setup_commit_entry();
        if (s_stats_setup_error[0] != '\0') {
            ui_draw_current();
        } else if (s_stats_setup_selection + 1 < s_stats_setup_field_count) {
            s_stats_setup_selection++;
            ui_draw_current();
        } else {
            stats_setup_execute();
        }
    } else if (s_page == PAGE_STATS_RESULT) {
        s_page = s_stats_return_page;
        s_current_app = s_page == PAGE_GRAPH ? APP_GRAPH : APP_STATS;
        ui_draw_current();
    } else if (s_page == PAGE_CALCULATOR) {
        const char *copy = opencalc_calc_history_selected_text();
        if (copy != NULL) {
            opencalc_calc_history_clear_selection();
            s_calc_cursor = strlen(s_calc_input);
            expression_append(copy);
            ui_draw_current();
            return;
        }
        calc_eval();
    } else if (s_page == PAGE_MATH_MENU) {
        math_menu_insert_selected();
    } else if (s_page == PAGE_Y_EQUALS) {
        graph_entry_toggle(s_graph_selection);
        ui_draw_current();
    } else if (s_page == PAGE_GRAPH) {
        if (!graph_jump_to_nearest_intersection()) {
            s_graph_trace = !s_graph_trace;
            s_graph_trace_x = (s_graph_xmin + s_graph_xmax) / 2.0;
        }
        ui_draw_current();
    } else if (s_page == PAGE_GRAPH_CALC) {
        graph_calc_run_selected();
    } else if (s_page == PAGE_GRAPH_FORMAT) {
        adjust_graph_format_value(1);
        ui_draw_current();
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
            adjust_audio_volume(5);
        } else if (s_script_selection == 5) {
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

static bool stats_setup_handle_key(int row, int col)
{
    if (s_page != PAGE_STATS_SETUP) return false;

    int digit = digit_for_key(row, col);
    if (digit >= 0) {
        stats_field_kind_t kind = s_stats_setup_kinds[s_stats_setup_selection];
        if (kind != STATS_FIELD_TAIL) {
            size_t length = strlen(s_stats_setup_entry);
            if (length + 1 < sizeof(s_stats_setup_entry)) {
                s_stats_setup_entry[length] = (char)('0' + digit);
                s_stats_setup_entry[length + 1] = '\0';
                s_stats_setup_error[0] = '\0';
                ui_draw_current();
            }
        }
        return true;
    }
    if (row == 9 && col == 2) {
        if (s_stats_setup_kinds[s_stats_setup_selection] != STATS_FIELD_TAIL &&
            strchr(s_stats_setup_entry, '.') == NULL &&
            strlen(s_stats_setup_entry) + 1 < sizeof(s_stats_setup_entry)) {
            strcat(s_stats_setup_entry, ".");
            ui_draw_current();
        }
        return true;
    }
    if (row == 9 && col == 3) {
        if (s_stats_setup_kinds[s_stats_setup_selection] == STATS_FIELD_NUMBER &&
            strlen(s_stats_setup_entry) + 1 < sizeof(s_stats_setup_entry)) {
            if (s_stats_setup_entry[0] == '-') {
                memmove(s_stats_setup_entry, s_stats_setup_entry + 1, strlen(s_stats_setup_entry));
            } else {
                memmove(s_stats_setup_entry + 1, s_stats_setup_entry, strlen(s_stats_setup_entry) + 1);
                s_stats_setup_entry[0] = '-';
            }
            ui_draw_current();
        }
        return true;
    }
    if (row == 3 && col == 4) {
        size_t length = strlen(s_stats_setup_entry);
        if (s_second_active) s_stats_setup_entry[0] = '\0';
        else if (length > 0) s_stats_setup_entry[length - 1] = '\0';
        else s_stats_setup_values[s_stats_setup_selection] = 0.0;
        s_stats_setup_error[0] = '\0';
        ui_draw_current();
        return true;
    }
    if ((row == 1 && col == 4) || (row == 2 && col == 3)) {
        stats_setup_commit_entry();
        s_stats_setup_selection += row == 1 ? -1 : 1;
        if (s_stats_setup_selection < 0) s_stats_setup_selection = s_stats_setup_field_count - 1;
        if (s_stats_setup_selection >= s_stats_setup_field_count) s_stats_setup_selection = 0;
        ui_draw_current();
        return true;
    }
    if ((row == 1 && col == 3) || (row == 2 && col == 4)) {
        stats_setup_commit_entry();
        int delta = row == 1 ? -1 : 1;
        stats_field_kind_t kind = s_stats_setup_kinds[s_stats_setup_selection];
        double *value = &s_stats_setup_values[s_stats_setup_selection];
        if (kind == STATS_FIELD_LIST) {
            *value += delta;
            if (*value < 1) *value = LIST_COUNT;
            if (*value > LIST_COUNT) *value = 1;
        } else if (kind == STATS_FIELD_TAIL) {
            *value += delta;
            if (*value < -1) *value = 1;
            if (*value > 1) *value = -1;
        } else if (kind == STATS_FIELD_CONFIDENCE) {
            *value += delta * 0.01;
            if (*value < 0.50) *value = 0.50;
            if (*value > 0.999) *value = 0.999;
        } else {
            *value += delta;
        }
        s_stats_setup_error[0] = '\0';
        ui_draw_current();
        return true;
    }
    if (row == 9 && col == 4) { key_enter(); return true; }
    if (row == 2 && col == 2) { key_back(); return true; }
    return true;
}

static void stats_open_category(stats_category_t category)
{
    s_stats_category = category;
    s_stats_category_selection = 0;
    s_stats_return_page = PAGE_STATS_CATEGORY;
    s_page = PAGE_STATS_CATEGORY;
    s_current_app = APP_STATS;
    ui_draw_current();
}

static void stats_open_home_selection(void)
{
    switch (s_stats_home_selection) {
    case 0: stats_open_category(STATS_CATEGORY_SUMMARY); return;
    case 1: stats_open_category(STATS_CATEGORY_REGRESSION); return;
    case 2:
        s_stats_return_page = PAGE_APP;
        s_page = PAGE_STATS_PLOT;
        s_current_app = APP_STATS;
        ui_draw_current();
        return;
    case 3: stats_open_category(STATS_CATEGORY_DISTRIBUTIONS); return;
    case 4: stats_open_category(STATS_CATEGORY_INTERVALS); return;
    case 5: stats_open_category(STATS_CATEGORY_TESTS); return;
    case 6:
        s_stats_one_var_parent_page = PAGE_APP;
        stats_open_one_var();
        return;
    default: return;
    }
}

static void stats_run_category_selection(void)
{
    int count = 0;
    const char *title = NULL;
    const int *tools = stats_category_tools(s_stats_category, &count, &title);
    (void)title;
    if (s_stats_category_selection < 0 || s_stats_category_selection >= count) return;

    int tool = tools[s_stats_category_selection];
    if (tool == 4) {
        s_stats_one_var_parent_page = PAGE_STATS_CATEGORY;
        stats_open_one_var();
        return;
    }
    s_stats_return_page = PAGE_STATS_CATEGORY;
    s_app_selection = tool;
    run_home_app_tool();
}

static bool stats_page_handle_key(int row, int col)
{
    bool dashboard = s_page == PAGE_APP && s_current_app == APP_STATS;
    if (!dashboard && s_page != PAGE_STATS_CATEGORY && s_page != PAGE_STATS_ONE_VAR) {
        return false;
    }

    int digit = digit_for_key(row, col);
    bool up = row == 1 && col == 4;
    bool down = row == 2 && col == 3;
    bool left = row == 1 && col == 3;
    bool right = row == 2 && col == 4;
    bool enter = row == 9 && col == 4;
    bool back = row == 2 && col == 2;

    if (dashboard) {
        int count = (int)(sizeof(STATS_HOME_TOOLS) / sizeof(STATS_HOME_TOOLS[0]));
        if (digit >= 1 && digit <= count) {
            s_stats_home_selection = digit - 1;
            stats_open_home_selection();
        } else if (up && s_stats_home_selection > 0) {
            s_stats_home_selection--;
            ui_draw_current();
        } else if (down && s_stats_home_selection + 1 < count) {
            s_stats_home_selection++;
            ui_draw_current();
        } else if (enter) {
            stats_open_home_selection();
        } else if (back) {
            key_back();
        }
        return true;
    }

    if (s_page == PAGE_STATS_CATEGORY) {
        int count = 0;
        const char *title = NULL;
        (void)stats_category_tools(s_stats_category, &count, &title);
        if (digit >= 1 && digit <= count) {
            s_stats_category_selection = digit - 1;
            stats_run_category_selection();
        } else if (up && s_stats_category_selection > 0) {
            s_stats_category_selection--;
            ui_draw_current();
        } else if (down && s_stats_category_selection + 1 < count) {
            s_stats_category_selection++;
            ui_draw_current();
        } else if (enter) {
            stats_run_category_selection();
        } else if (back) {
            s_page = PAGE_APP;
            s_current_app = APP_STATS;
            ui_draw_current();
        }
        return true;
    }

    if (digit >= 1 && digit <= LIST_COUNT &&
        (s_stats_one_var_selection == 0 || s_stats_one_var_selection == 1)) {
        if (s_stats_one_var_selection == 0) s_stats_one_var_data_list = digit - 1;
        else s_stats_one_var_frequency_list = digit - 1;
        s_stats_one_var_error[0] = '\0';
        ui_draw_current();
        return true;
    }
    if (up) {
        s_stats_one_var_selection = (s_stats_one_var_selection + 4) % 5;
    } else if (down) {
        s_stats_one_var_selection = (s_stats_one_var_selection + 1) % 5;
    } else if (left || right) {
        int delta = right ? 1 : -1;
        if (s_stats_one_var_selection == 0) {
            s_stats_one_var_data_list =
                (s_stats_one_var_data_list + LIST_COUNT + delta) % LIST_COUNT;
        } else if (s_stats_one_var_selection == 1) {
            int option = s_stats_one_var_frequency_list + 1;
            option = (option + LIST_COUNT + 1 + delta) % (LIST_COUNT + 1);
            s_stats_one_var_frequency_list = option - 1;
        } else if (s_stats_one_var_selection == 2) {
            s_stats_one_var_quartile_method ^= 1;
        } else if (s_stats_one_var_selection == 3) {
            s_stats_one_var_number_format ^= 1;
        }
        s_stats_one_var_error[0] = '\0';
    } else if (enter) {
        if (s_stats_one_var_selection < 4) s_stats_one_var_selection++;
        else stats_calculate_one_var();
    } else if (back) {
        s_page = s_stats_one_var_parent_page;
        s_current_app = APP_STATS;
    }
    ui_draw_current();
    return true;
}

static bool solver_page_handle_key(int row, int col)
{
    bool dashboard = s_page == PAGE_APP && s_current_app == APP_SOLVER;
    bool solver_page = dashboard || s_page == PAGE_SOLVER_WORKFLOW ||
        s_page == PAGE_SOLVER_SYSTEM_RESULT || s_page == PAGE_SOLVER_SYMBOLIC_RESULT ||
        s_page == PAGE_SOLVER_SAVED;
    if (!solver_page) return false;

    int digit = digit_for_key(row, col);
    bool up = row == 1 && col == 4;
    bool down = row == 2 && col == 3;
    bool enter = row == 9 && col == 4;
    bool back = row == 2 && col == 2;
    bool del = row == 3 && col == 4;

    if (dashboard) {
        int count = (int)(sizeof(SOLVER_TOOLS) / sizeof(SOLVER_TOOLS[0]));
        if (digit >= 1 && digit <= count) {
            s_app_selection = digit - 1;
            solver_open_home_selection();
        } else if (up && s_app_selection > 0) {
            s_app_selection--;
            ui_draw_current();
        } else if (down && s_app_selection + 1 < count) {
            s_app_selection++;
            ui_draw_current();
        } else if (enter) {
            solver_open_home_selection();
        } else if (back) {
            key_back();
        }
        return true;
    }

    if (s_page == PAGE_SOLVER_WORKFLOW) {
        int count = 0;
        const char *title = NULL;
        (void)solver_workflow_actions(s_solver_workflow, &count, &title);
        if (digit >= 1 && digit <= count) {
            s_solver_workflow_selection = digit - 1;
            solver_run_workflow_action();
        } else if (up && s_solver_workflow_selection > 0) {
            s_solver_workflow_selection--;
            ui_draw_current();
        } else if (down && s_solver_workflow_selection + 1 < count) {
            s_solver_workflow_selection++;
            ui_draw_current();
        } else if (enter) {
            solver_run_workflow_action();
        } else if (back) {
            s_page = PAGE_APP;
            s_current_app = APP_SOLVER;
            ui_draw_current();
        }
        return true;
    }

    if (s_page == PAGE_SOLVER_SYSTEM_RESULT) {
        if (up && s_solver_system_selected > 0) {
            s_solver_system_selected--;
        } else if (down && s_solver_system_selected + 1 < s_solver_system_variables) {
            s_solver_system_selected++;
        } else if (enter && s_solver_system_status != SOLVER_SYSTEM_INCONSISTENT &&
                   s_solver_system_variables > 0) {
            char solution[96];
            solver_system_variable_text(s_solver_system_selected, solution, sizeof(solution));
            const char *equals = strstr(solution, " = ");
            snprintf(s_calc_input, sizeof(s_calc_input), "%s", equals != NULL ? equals + 3 : solution);
            s_calc_cursor = strlen(s_calc_input);
            s_current_app = APP_CALCULATOR;
            s_page = PAGE_CALCULATOR;
        } else if (back) {
            solver_open_workflow(SOLVER_WORKFLOW_SYSTEM);
            return true;
        }
        ui_draw_current();
        return true;
    }

    if (s_page == PAGE_SOLVER_SYMBOLIC_RESULT) {
        int offsets[64];
        int lines = solver_symbolic_wrap(offsets, 64);
        if (up && s_solver_symbolic_scroll > 0) s_solver_symbolic_scroll--;
        else if (down && s_solver_symbolic_scroll + 1 < lines) s_solver_symbolic_scroll++;
        else if (enter) {
            snprintf(s_calc_input, sizeof(s_calc_input), "%s", s_solver_symbolic_result);
            s_calc_cursor = strlen(s_calc_input);
            s_current_app = APP_CALCULATOR;
            s_page = PAGE_CALCULATOR;
        } else if (back) {
            s_page = PAGE_SOLVER_WORKFLOW;
            s_current_app = APP_SOLVER;
        }
        ui_draw_current();
        return true;
    }

    if (digit >= 1 && digit <= SOLVER_SAVED_MAX) s_solver_saved_selection = digit - 1;
    else if (up && s_solver_saved_selection > 0) s_solver_saved_selection--;
    else if (down && s_solver_saved_selection + 1 < SOLVER_SAVED_MAX) s_solver_saved_selection++;
    else if (del) solver_delete_saved(s_solver_saved_selection);
    else if (enter && s_solver_saved_e1[s_solver_saved_selection][0] != '\0') {
        snprintf(s_solver_e1, sizeof(s_solver_e1), "%s", s_solver_saved_e1[s_solver_saved_selection]);
        snprintf(s_solver_e2, sizeof(s_solver_e2), "%s",
                 s_solver_saved_e2[s_solver_saved_selection][0] ? s_solver_saved_e2[s_solver_saved_selection] : "0");
        solver_workflow_t workflow = (solver_workflow_t)s_solver_saved_type[s_solver_saved_selection];
        if (workflow == SOLVER_WORKFLOW_SYSTEM && s_solver_saved_e2[s_solver_saved_selection][0] == '\0') {
            snprintf(s_calc_input, sizeof(s_calc_input), "%s", s_solver_saved_e1[s_solver_saved_selection]);
            s_calc_cursor = strlen(s_calc_input);
        }
        solver_open_workflow(workflow);
        return true;
    } else if (back) {
        s_page = PAGE_APP;
        s_current_app = APP_SOLVER;
    }
    ui_draw_current();
    return true;
}

static bool matrix_editor_commit_entry(void)
{
    if (!s_matrix_entry_active) return true;
    if (s_matrix_entry[0] == '\0' || strcmp(s_matrix_entry, "-") == 0 ||
        strcmp(s_matrix_entry, ".") == 0 || strcmp(s_matrix_entry, "-.") == 0) {
        snprintf(s_matrix_status, sizeof(s_matrix_status), "Enter a valid number");
        return false;
    }

    char *end = NULL;
    double value = strtod(s_matrix_entry, &end);
    if (end == s_matrix_entry || *end != '\0' || !isfinite(value)) {
        snprintf(s_matrix_status, sizeof(s_matrix_status), "Invalid number");
        return false;
    }
    s_matrix_a[s_matrix_cursor_row][s_matrix_cursor_col] = value;
    s_matrix_entry[0] = '\0';
    s_matrix_entry_active = false;
    s_matrix_status[0] = '\0';
    return true;
}

static bool matrix_editor_append_char(char c)
{
    if (!s_matrix_entry_active) {
        s_matrix_entry[0] = '\0';
        s_matrix_entry_active = true;
    }
    size_t length = strlen(s_matrix_entry);
    if (length + 1 >= sizeof(s_matrix_entry)) return false;
    s_matrix_entry[length] = c;
    s_matrix_entry[length + 1] = '\0';
    s_matrix_status[0] = '\0';
    ui_draw_current();
    return true;
}

static void matrix_editor_move(int row_delta, int col_delta)
{
    if (!matrix_editor_commit_entry()) {
        ui_draw_current();
        return;
    }
    int row = s_matrix_cursor_row + row_delta;
    int col = s_matrix_cursor_col + col_delta;
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    if (row >= s_matrix_rows) row = s_matrix_rows - 1;
    if (col >= s_matrix_cols) col = s_matrix_cols - 1;
    s_matrix_cursor_row = row;
    s_matrix_cursor_col = col;
    ui_draw_current();
}

static void matrix_editor_advance(void)
{
    if (s_matrix_cursor_col + 1 < s_matrix_cols) {
        s_matrix_cursor_col++;
    } else if (s_matrix_cursor_row + 1 < s_matrix_rows) {
        s_matrix_cursor_row++;
        s_matrix_cursor_col = 0;
    } else {
        snprintf(s_matrix_status, sizeof(s_matrix_status), "Last cell");
    }
}

static bool matrix_page_handle_key(int row, int col)
{
    if (s_page != PAGE_MATRIX_EDITOR && s_page != PAGE_MATRIX_VIEWER &&
        s_page != PAGE_MATRIX_SIZE) {
        return false;
    }

    int digit = digit_for_key(row, col);
    if (s_page == PAGE_MATRIX_SIZE) {
        int *value = s_matrix_size_selection == 0 ? &s_matrix_size_rows : &s_matrix_size_cols;
        if (digit >= 0) {
            int candidate = s_matrix_size_typing ? (*value * 10 + digit) : digit;
            if (candidate >= 1 && candidate <= MATRIX_MAX_N) {
                *value = candidate;
                s_matrix_size_typing = true;
            }
            ui_draw_current();
            return true;
        }
        if (row == 1 && col == 4) {
            s_matrix_size_selection = 0;
            s_matrix_size_typing = false;
        } else if (row == 2 && col == 3) {
            s_matrix_size_selection = 1;
            s_matrix_size_typing = false;
        } else if (row == 1 && col == 3) {
            if (*value > 1) (*value)--;
            s_matrix_size_typing = false;
        } else if (row == 2 && col == 4) {
            if (*value < MATRIX_MAX_N) (*value)++;
            s_matrix_size_typing = false;
        } else if (row == 3 && col == 4) {
            *value = 1;
            s_matrix_size_typing = false;
        } else if (row == 9 && col == 4) {
            matrix_apply_size();
            return true;
        } else if (row == 2 && col == 2) {
            key_back();
            return true;
        }
        ui_draw_current();
        return true;
    }

    if (s_page == PAGE_MATRIX_VIEWER) {
        if (row == 1 && col == 3) matrix_editor_move(0, -1);
        else if (row == 2 && col == 4) matrix_editor_move(0, 1);
        else if (row == 1 && col == 4) matrix_editor_move(-1, 0);
        else if (row == 2 && col == 3) matrix_editor_move(1, 0);
        else if (row == 9 && col == 4) matrix_open_editor();
        else if (row == 2 && col == 2) key_back();
        return true;
    }

    if (digit >= 0) {
        matrix_editor_append_char((char)('0' + digit));
        return true;
    }
    if (row == 9 && col == 2) {
        if (!s_matrix_entry_active) matrix_editor_append_char('0');
        if (strchr(s_matrix_entry, '.') == NULL && strchr(s_matrix_entry, 'e') == NULL &&
            strchr(s_matrix_entry, 'E') == NULL) {
            matrix_editor_append_char('.');
        }
        return true;
    }
    if (row == 9 && col == 3) {
        if (!s_matrix_entry_active) {
            matrix_format_cell(s_matrix_a[s_matrix_cursor_row][s_matrix_cursor_col],
                               s_matrix_entry, sizeof(s_matrix_entry));
            s_matrix_entry_active = true;
        }
        size_t length = strlen(s_matrix_entry);
        if (s_matrix_entry[0] == '-') memmove(s_matrix_entry, s_matrix_entry + 1, length);
        else if (length + 1 < sizeof(s_matrix_entry)) {
            memmove(s_matrix_entry + 1, s_matrix_entry, length + 1);
            s_matrix_entry[0] = '-';
        }
        s_matrix_status[0] = '\0';
        ui_draw_current();
        return true;
    }
    if (s_second_active && row == 5 && col == 1) {
        if (!s_matrix_entry_active) {
            matrix_format_cell(s_matrix_a[s_matrix_cursor_row][s_matrix_cursor_col],
                               s_matrix_entry, sizeof(s_matrix_entry));
            s_matrix_entry_active = true;
        }
        if (strchr(s_matrix_entry, 'e') == NULL && strchr(s_matrix_entry, 'E') == NULL) {
            matrix_editor_append_char('E');
        }
        return true;
    }
    if (row == 3 && col == 4) {
        if (s_second_active) {
            s_matrix_a[s_matrix_cursor_row][s_matrix_cursor_col] = 0.0;
            s_matrix_entry[0] = '\0';
            s_matrix_entry_active = false;
        } else if (s_matrix_entry_active && s_matrix_entry[0] != '\0') {
            s_matrix_entry[strlen(s_matrix_entry) - 1] = '\0';
        } else {
            s_matrix_a[s_matrix_cursor_row][s_matrix_cursor_col] = 0.0;
        }
        s_matrix_status[0] = '\0';
        ui_draw_current();
        return true;
    }
    if (row == 1 && col == 3) matrix_editor_move(0, -1);
    else if (row == 2 && col == 4) matrix_editor_move(0, 1);
    else if (row == 1 && col == 4) matrix_editor_move(-1, 0);
    else if (row == 2 && col == 3) matrix_editor_move(1, 0);
    else if (row == 9 && col == 4) {
        if (!matrix_editor_commit_entry()) {
            ui_draw_current();
        } else if (s_second_active) {
            s_page = PAGE_APP;
            s_current_app = APP_MATRICES;
            ui_draw_current();
        } else {
            matrix_editor_advance();
            ui_draw_current();
        }
    } else if (row == 2 && col == 2) {
        if (matrix_editor_commit_entry()) key_back();
        else ui_draw_current();
    }
    return true;
}

static bool finance_page_handle_key(int row, int col)
{
    if (s_page != PAGE_FINANCE_TVM && s_page != PAGE_FINANCE_CASHFLOW &&
        s_page != PAGE_FINANCE_RESULT) {
        return false;
    }

    if (s_page == PAGE_FINANCE_RESULT) {
        if ((row == 9 && col == 4) || (row == 2 && col == 2)) key_back();
        return true;
    }

    int digit = digit_for_key(row, col);
    if (digit >= 0) {
        (void)finance_append_entry_char((char)('0' + digit));
        return true;
    }
    if (row == 9 && col == 2) {
        if (!s_fin_entry_active) (void)finance_append_entry_char('0');
        if (strchr(s_fin_entry, '.') == NULL && strchr(s_fin_entry, 'e') == NULL &&
            strchr(s_fin_entry, 'E') == NULL) {
            (void)finance_append_entry_char('.');
        }
        return true;
    }
    if (row == 9 && col == 3) {
        finance_toggle_entry_sign();
        return true;
    }
    if (s_second_active && row == 5 && col == 1) {
        if (!s_fin_entry_active) (void)finance_append_entry_char('1');
        if (strchr(s_fin_entry, 'e') == NULL && strchr(s_fin_entry, 'E') == NULL) {
            (void)finance_append_entry_char('E');
        }
        return true;
    }

    if (row == 3 && col == 4) {
        if (s_second_active) {
            if (s_page == PAGE_FINANCE_CASHFLOW) {
                memset(s_lists[s_list_index], 0, sizeof(s_lists[s_list_index]));
                s_list_counts[s_list_index] = 0;
                s_fin_cash_cursor = 0;
                s_fin_cash_scroll = 0;
            } else {
                double *field = finance_selected_value();
                if (field != NULL) *field = s_fin_selection >= 5 ? 1.0 : 0.0;
            }
            finance_reset_entry();
        } else if (s_fin_entry_active && s_fin_entry[0] != '\0') {
            s_fin_entry[strlen(s_fin_entry) - 1] = '\0';
            s_fin_status[0] = '\0';
        } else if (s_page == PAGE_FINANCE_CASHFLOW) {
            finance_delete_cash_flow();
        }
        ui_draw_current();
        return true;
    }

    if (s_page == PAGE_FINANCE_TVM) {
        int next = s_fin_selection;
        if (row == 1 && col == 3) {
            if (next >= 4) next -= 4;
        } else if (row == 2 && col == 4) {
            if (next < 4) {
                next += 4;
                if (next > 6) next = 6;
            }
        } else if (row == 1 && col == 4) {
            if ((next > 0 && next < 4) || next > 4) next--;
        } else if (row == 2 && col == 3) {
            if (next < 3 || (next >= 4 && next < 6)) next++;
        } else if (row == 9 && col == 4) {
            if (s_second_active) {
                if (s_fin_entry_active && !finance_commit_tvm_entry()) {
                    ui_draw_current();
                    return true;
                }
                if (!finance_solve_selected()) {
                    snprintf(s_fin_status, sizeof(s_fin_status),
                             s_fin_selection >= 5 ? "P/Y and C/Y are settings" : "No valid solution found");
                }
                ui_draw_current();
                return true;
            }
            if (finance_commit_tvm_entry()) {
                s_fin_selection = (s_fin_selection + 1) % 7;
            }
            ui_draw_current();
            return true;
        } else if (row == 2 && col == 2) {
            if (finance_commit_tvm_entry()) key_back();
            else ui_draw_current();
            return true;
        } else {
            return true;
        }
        if (next != s_fin_selection && finance_commit_tvm_entry()) {
            s_fin_selection = next;
            finance_reset_entry();
        }
        ui_draw_current();
        return true;
    }

    if ((row == 1 && col == 3) || (row == 2 && col == 4)) {
        if (finance_commit_cash_entry()) {
            int delta = row == 1 ? -1 : 1;
            s_list_index = (s_list_index + delta + LIST_COUNT) % LIST_COUNT;
            s_fin_cash_cursor = 0;
            s_fin_cash_scroll = 0;
            finance_reset_entry();
        }
    } else if (row == 1 && col == 4) {
        if (finance_commit_cash_entry() && s_fin_cash_cursor > 0) s_fin_cash_cursor--;
    } else if (row == 2 && col == 3) {
        if (finance_commit_cash_entry() && s_fin_cash_cursor < s_list_counts[s_list_index]) {
            s_fin_cash_cursor++;
        }
    } else if (row == 9 && col == 4) {
        if (finance_commit_cash_entry()) {
            if (s_second_active) {
                key_back();
                return true;
            }
            if (s_fin_cash_cursor < LIST_MAX_VALUES - 1) s_fin_cash_cursor++;
        }
    } else if (row == 2 && col == 2) {
        if (finance_commit_cash_entry()) key_back();
        else ui_draw_current();
        return true;
    }
    ui_draw_current();
    return true;
}

static bool conic_page_handle_key(int row, int col)
{
    bool dashboard = s_page == PAGE_APP && s_current_app == APP_CONICS;
    if (!dashboard && s_page != PAGE_CONIC_EDITOR && s_page != PAGE_CONIC_RESULT &&
        s_page != PAGE_CONIC_GRAPHS) return false;

    int digit = digit_for_key(row, col);
    bool up = row == 1 && col == 4;
    bool down = row == 2 && col == 3;
    bool left = row == 1 && col == 3;
    bool right = row == 2 && col == 4;
    bool back = row == 2 && col == 2;
    bool enter = row == 9 && col == 4;
    bool del = row == 3 && col == 4;
    bool graph = row == 0 && col == 4;

    if (dashboard) {
        if (digit >= 1 && digit <= 6) {
            s_app_selection = digit - 1;
            run_home_app_tool();
        } else if (up && s_app_selection > 0) {
            s_app_selection--;
            ui_draw_current();
        } else if (down && s_app_selection < 5) {
            s_app_selection++;
            ui_draw_current();
        } else if (enter) run_home_app_tool();
        else if (back) {
            key_back();
        }
        return true;
    }

    if (s_page == PAGE_CONIC_RESULT) {
        if (up && s_conic_result_scroll > 0) s_conic_result_scroll--;
        else if (down && s_conic_result_scroll + 1 < s_conic_result_count) s_conic_result_scroll++;
        else if (enter) { conic_copy_selected_result(); return true; }
        else if (graph) { conic_graph_current(); return true; }
        else if (back) s_page = PAGE_CONIC_EDITOR;
        ui_draw_current();
        return true;
    }

    if (s_page == PAGE_CONIC_GRAPHS) {
        if (up && s_conic_overlay_selection > 0) s_conic_overlay_selection--;
        else if (down && s_conic_overlay_selection < 3) s_conic_overlay_selection++;
        else if (enter || graph) { conic_graph_all(); return true; }
        else if (del) conic_remove_overlay();
        else if (back) {
            s_page = PAGE_APP;
            s_current_app = APP_CONICS;
        }
        ui_draw_current();
        return true;
    }

    int fields = conic_field_count();
    int total = fields + conic_action_count();
    if (s_conic_selection < fields && digit >= 0) {
        size_t length = strlen(s_conic_entry);
        if (!s_conic_entry_active) {
            s_conic_entry[0] = '\0';
            length = 0;
            s_conic_entry_active = true;
        }
        if (length + 1 < sizeof(s_conic_entry)) {
            s_conic_entry[length] = (char)('0' + digit);
            s_conic_entry[length + 1] = '\0';
        }
        s_conic_status[0] = '\0';
    } else if (s_conic_selection < fields && row == 9 && col == 2) {
        if (!s_conic_entry_active) { snprintf(s_conic_entry, sizeof(s_conic_entry), "0"); s_conic_entry_active = true; }
        if (strchr(s_conic_entry, '.') == NULL && strlen(s_conic_entry) + 1 < sizeof(s_conic_entry)) strcat(s_conic_entry, ".");
    } else if (s_conic_selection < fields && row == 9 && col == 3) {
        if (!s_conic_entry_active) { s_conic_entry[0] = '\0'; s_conic_entry_active = true; }
        if (s_conic_entry[0] == '-') memmove(s_conic_entry, s_conic_entry + 1, strlen(s_conic_entry));
        else if (strlen(s_conic_entry) + 1 < sizeof(s_conic_entry)) {
            memmove(s_conic_entry + 1, s_conic_entry, strlen(s_conic_entry) + 1);
            s_conic_entry[0] = '-';
        }
    } else if (del && s_conic_selection < fields) {
        size_t length = strlen(s_conic_entry);
        if (s_conic_entry_active && length > 0) s_conic_entry[length - 1] = '\0';
        else s_conic_entry_active = false;
    } else if ((up || down) && conic_commit_entry()) {
        if (up && s_conic_selection > 0) s_conic_selection--;
        if (down && s_conic_selection + 1 < total) s_conic_selection++;
    } else if ((left || right) && s_conic_selection < fields && conic_commit_entry()) {
        double step = conic_field_label(s_conic_selection) && strstr(conic_field_label(s_conic_selection), "rotation") ? 15.0 : 1.0;
        *conic_field_value(s_conic_selection) += right ? step : -step;
        conic_rebuild_model();
    } else if (enter) {
        if (s_conic_selection < fields) {
            if (conic_commit_entry() && s_conic_selection + 1 < total) s_conic_selection++;
        } else {
            conic_run_action(s_conic_selection - fields);
            return true;
        }
    } else if (graph) {
        if (conic_commit_entry()) conic_graph_current();
        return true;
    } else if (back) {
        s_conic_entry[0] = '\0';
        s_conic_entry_active = false;
        s_page = PAGE_APP;
        s_current_app = APP_CONICS;
    }
    ui_draw_current();
    return true;
}

static void inequality_toggle_domain(void)
{
    s_ineq_integer_mode = !s_ineq_integer_mode;
    opencalc_persist_set_u32("ineq_int", s_ineq_integer_mode ? 1 : 0);
    snprintf(s_ineq_status, sizeof(s_ineq_status), "%s-number solution mode",
             s_ineq_integer_mode ? "Integer" : "Real");
}

static bool inequality_page_handle_key(int row, int col)
{
    bool dashboard = s_page == PAGE_APP && s_current_app == APP_INEQUALITY;
    if (!dashboard && s_page != PAGE_INEQ_EDITOR && s_page != PAGE_INEQ_RESULT) return false;

    int digit = digit_for_key(row, col);
    bool up = row == 1 && col == 4;
    bool down = row == 2 && col == 3;
    bool left = row == 1 && col == 3;
    bool right = row == 2 && col == 4;
    bool back = row == 2 && col == 2;
    bool enter = row == 9 && col == 4;
    bool del = row == 3 && col == 4;
    bool y_equals = row == 0 && col == 0;
    bool window = row == 0 && col == 1;
    bool zoom = row == 0 && col == 2;
    bool trace = row == 0 && col == 3;
    bool graph = row == 0 && col == 4;

    if (dashboard) {
        if (s_second_active && window) {
            inequality_toggle_domain();
            ui_draw_current();
        } else if (digit >= 1 && digit <= 4) {
            s_app_selection = digit - 1;
            run_home_app_tool();
        } else if (up && s_app_selection > 0) {
            s_app_selection--;
            ui_draw_current();
        } else if (down && s_app_selection < 3) {
            s_app_selection++;
            ui_draw_current();
        } else if (enter) run_home_app_tool();
        else if (back) {
            key_back();
        }
        return true;
    }

    if (s_page == PAGE_INEQ_RESULT) {
        if (up && s_ineq_result_scroll >= 40) s_ineq_result_scroll -= 40;
        else if (down && s_ineq_exact_text[0] &&
                 s_ineq_result_scroll + 48 < (int)strlen(s_ineq_exact_text)) s_ineq_result_scroll += 40;
        else if (left || right) {
            s_ineq_notation = (s_ineq_notation + (right ? 1 : 2)) % 3;
            opencalc_persist_set_u32("ineq_note", (uint32_t)s_ineq_notation);
        }
        else if (enter) inequality_request_exact();
        else if (y_equals) { inequality_copy_result_to_calculator(); return true; }
        else if (trace) { inequality_save_intervals_to_list(); return true; }
        else if (window) {
            inequality_toggle_domain();
            inequality_solve_text(s_ineq_problem_text, s_ineq_sign_chart);
            return true;
        } else if (back) {
            s_page = PAGE_APP;
            s_current_app = APP_INEQUALITY;
        }
        ui_draw_current();
        return true;
    }

    if (up && s_ineq_selection > 0) s_ineq_selection--;
    else if (down && s_ineq_selection + 1 < INEQ_MAX) s_ineq_selection++;
    else if (enter && s_second_active && s_ineqs[s_ineq_selection].text[0]) {
        s_ineqs[s_ineq_selection].enabled = !s_ineqs[s_ineq_selection].enabled;
        char key[16];
        snprintf(key, sizeof(key), "ineq_on_%d", s_ineq_selection);
        opencalc_persist_set_u32(key, s_ineqs[s_ineq_selection].enabled ? 1 : 0);
        snprintf(s_ineq_status, sizeof(s_ineq_status), "I%d %s", s_ineq_selection + 1,
                 s_ineqs[s_ineq_selection].enabled ? "enabled" : "disabled");
    } else if (enter) {
        if (!inequality_load_from_calc()) {
            snprintf(s_ineq_status, sizeof(s_ineq_status), "Calculator input is not a valid inequality");
        }
    } else if (del) {
        inequality_delete_slot(s_ineq_selection);
        snprintf(s_ineq_status, sizeof(s_ineq_status), "I%d removed", s_ineq_selection + 1);
    } else if (window) {
        s_ineq_join_or = !s_ineq_join_or;
        opencalc_persist_set_u32("ineq_or", s_ineq_join_or ? 1 : 0);
        snprintf(s_ineq_status, sizeof(s_ineq_status), "System joins relations with %s",
                 s_ineq_join_or ? "OR" : "AND");
    } else if (graph) {
        inequality_sync_graph_boundaries();
        s_graphing_mode = 0;
        s_page = PAGE_GRAPH;
        s_current_app = APP_GRAPH;
        s_graph_trace = false;
        ui_draw_current();
        return true;
    } else if (trace) {
        inequality_store_intersections();
        return true;
    } else if (zoom) {
        inequality_optimize_from_calc();
        return true;
    } else if (y_equals && s_ineqs[s_ineq_selection].text[0]) {
        snprintf(s_calc_input, sizeof(s_calc_input), "%s", s_ineqs[s_ineq_selection].text);
        s_calc_cursor = strlen(s_calc_input);
        s_page = PAGE_CALCULATOR;
        s_current_app = APP_CALCULATOR;
        ui_draw_current();
        return true;
    } else if (back) {
        s_page = PAGE_APP;
        s_current_app = APP_INEQUALITY;
    }
    ui_draw_current();
    return true;
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

static void table_setup_adjust(int direction)
{
    if (direction == 0) return;
    switch (s_table_setup_selection) {
    case 0:
        s_table_x_start += direction * s_table_step;
        break;
    case 1: {
        double increment = s_graphing_mode == 3 ? 1.0 : 0.1;
        s_table_step += direction * increment;
        if (s_table_step < increment) s_table_step = increment;
        if (s_table_step > 1000.0) s_table_step = 1000.0;
        break;
    }
    case 2:
        s_table_rows += direction;
        if (s_table_rows < 4) s_table_rows = 4;
        if (s_table_rows > 10) s_table_rows = 10;
        break;
    case 3:
        s_table_precision += direction;
        if (s_table_precision < 0) s_table_precision = 0;
        if (s_table_precision > 6) s_table_precision = 6;
        break;
    default:
        break;
    }
}

static bool graph_symbolic_handle_key(int row, int col)
{
    if (s_page != PAGE_GRAPH_SYMBOLIC) return false;

    bool left = row == 1 && col == 3;
    bool up = row == 1 && col == 4;
    bool down = row == 2 && col == 3;
    bool right = row == 2 && col == 4;
    bool back = row == 2 && col == 2;
    bool enter = row == 9 && col == 4;
    bool y_equals = row == 0 && col == 0;
    bool window = row == 0 && col == 1;
    bool trace = row == 0 && col == 3;
    bool graph = row == 0 && col == 4;

    if (back) {
        key_back();
        return true;
    }
    if (enter || graph) {
        s_page = PAGE_GRAPH;
        s_current_app = APP_GRAPH;
        ui_draw_current();
        return true;
    }
    if (left || right || trace) {
        if (!s_graph_symbolic_pending && graph_select_relative(left ? -1 : 1)) {
            (void)submit_graph_symbolic_job();
        }
        ui_draw_current();
        return true;
    }
    if (up || down) {
        double step;
        if (s_graphing_mode == 1 || s_graphing_mode == 2) {
            step = (s_graph_tmax - s_graph_tmin) / 40.0;
        } else if (s_graphing_mode == 3) {
            step = 1.0;
        } else {
            step = (s_graph_xmax - s_graph_xmin) / 40.0;
        }
        s_graph_trace_x += up ? step : -step;
        if (s_graphing_mode == 3) s_graph_trace_x = floor(s_graph_trace_x + 0.5);
        if (s_graph_overlay_fn == s_graph_trace_fn) s_graph_overlay_x = s_graph_trace_x;
        ui_draw_current();
        return true;
    }
    if (y_equals || window) {
        if (s_graphing_mode == 0) {
            s_graph_overlay_fn = s_graph_trace_fn;
            s_graph_overlay_x = s_graph_trace_x;
            if (y_equals) s_graph_tangent_enabled = !s_graph_tangent_enabled;
            if (window) s_graph_integral_shade_enabled = !s_graph_integral_shade_enabled;
        }
        ui_draw_current();
        return true;
    }
    return true;
}

static void key_left(void)
{
    if (s_page == PAGE_PERIODIC_TABLE) {
        periodic_move_selection(-1, 0);
        ui_draw_current();
    } else if (s_page == PAGE_ELEMENT_DETAIL) {
        if (s_periodic_atomic_number > 1) s_periodic_atomic_number--;
        ui_draw_current();
    } else if (s_page == PAGE_REFERENCE_LIST) {
        reference_change_category(-1);
        ui_draw_current();
    } else if (s_page == PAGE_REFERENCE_DETAIL) {
        if (s_reference_selection > 0) s_reference_selection--;
        ui_draw_current();
    } else if (s_page == PAGE_GRAPH) {
        double span = s_graph_xmax - s_graph_xmin;
        if (s_graph_trace) {
            if (s_graphing_mode == 1 || s_graphing_mode == 2) {
                span = s_graph_tmax - s_graph_tmin;
            } else if (s_graphing_mode == 3) {
                span = s_graph_nmax - s_graph_nmin;
            }
            s_graph_trace_x -= span / 40.0;
            if (s_graphing_mode == 3) {
                s_graph_trace_x = floor(s_graph_trace_x + 0.5);
            }
        } else {
            s_graph_xmin -= span / 10.0;
            s_graph_xmax -= span / 10.0;
        }
        ui_draw_current();
    } else if (s_page == PAGE_GRAPH_WINDOW) {
        adjust_graph_window_value(-0.5);
        ui_draw_current();
    } else if (s_page == PAGE_GRAPH_FORMAT) {
        adjust_graph_format_value(-1);
        ui_draw_current();
    } else if (s_page == PAGE_MODE_MENU) {
        adjust_mode_value(-1);
        ui_draw_current();
    } else if (s_page == PAGE_TABLE) {
        int columns = s_graphing_mode == 1 ? 2 : 3;
        if (s_table_func_start >= columns) {
            s_table_func_start -= columns;
        } else {
            s_table_func_start = 0;
        }
        ui_draw_current();
    } else if (s_page == PAGE_TABLE_SETUP) {
        table_setup_adjust(-1);
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
    } else if (s_page == PAGE_SETTINGS && s_script_selection == 4) {
        adjust_audio_volume(-5);
    } else if (s_page == PAGE_LIST_EDITOR) {
        s_list_index = (s_list_index + LIST_COUNT - 1) % LIST_COUNT;
        if (s_list_cursor > s_list_counts[s_list_index]) s_list_cursor = s_list_counts[s_list_index];
        s_list_entry[0] = '\0';
        s_list_editing = false;
        ui_draw_current();
    } else if (s_page == PAGE_APP && s_current_app == APP_LISTS) {
        select_list_index(s_list_index - 1);
        snprintf(s_calc_output, sizeof(s_calc_output), "List L%d selected", s_list_index + 1);
        ui_draw_current();
    } else if (s_page == PAGE_APP && s_current_app == APP_MATRICES) {
        s_matrix_index = (s_matrix_index + MATRIX_COUNT - 1) % MATRIX_COUNT;
        matrix_reset_navigation();
        snprintf(s_calc_output, sizeof(s_calc_output), "Matrix %c selected", 'A' + s_matrix_index);
        ui_draw_current();
    } else if (s_page == PAGE_STATS_PLOT) {
        s_stats_plot_mode = (s_stats_plot_mode + 4) % 5;
        ui_draw_current();
    } else if (s_page == PAGE_CALCULATOR && opencalc_calc_history_selected() >= 0) {
        opencalc_calc_history_select_expression();
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
    if (s_page == PAGE_PERIODIC_TABLE) {
        periodic_move_selection(1, 0);
        ui_draw_current();
    } else if (s_page == PAGE_ELEMENT_DETAIL) {
        if (s_periodic_atomic_number < 118) s_periodic_atomic_number++;
        ui_draw_current();
    } else if (s_page == PAGE_REFERENCE_LIST) {
        reference_change_category(1);
        ui_draw_current();
    } else if (s_page == PAGE_REFERENCE_DETAIL) {
        int count = (int)opencalc_reference_count(s_reference_category);
        if (s_reference_selection + 1 < count) s_reference_selection++;
        ui_draw_current();
    } else if (s_page == PAGE_GRAPH) {
        double span = s_graph_xmax - s_graph_xmin;
        if (s_graph_trace) {
            if (s_graphing_mode == 1 || s_graphing_mode == 2) {
                span = s_graph_tmax - s_graph_tmin;
            } else if (s_graphing_mode == 3) {
                span = s_graph_nmax - s_graph_nmin;
            }
            s_graph_trace_x += span / 40.0;
            if (s_graphing_mode == 3) {
                s_graph_trace_x = floor(s_graph_trace_x + 0.5);
            }
        } else {
            s_graph_xmin += span / 10.0;
            s_graph_xmax += span / 10.0;
        }
        ui_draw_current();
    } else if (s_page == PAGE_GRAPH_WINDOW) {
        adjust_graph_window_value(0.5);
        ui_draw_current();
    } else if (s_page == PAGE_GRAPH_FORMAT) {
        adjust_graph_format_value(1);
        ui_draw_current();
    } else if (s_page == PAGE_MODE_MENU) {
        adjust_mode_value(1);
        ui_draw_current();
    } else if (s_page == PAGE_TABLE) {
        int columns = s_graphing_mode == 1 ? 2 : 3;
        int count = graph_table_series_count();
        if (s_table_func_start + columns < count) {
            s_table_func_start += columns;
            int max_start = count > columns ? count - columns : 0;
            if (s_table_func_start > max_start) {
                s_table_func_start = max_start;
            }
        }
        ui_draw_current();
    } else if (s_page == PAGE_TABLE_SETUP) {
        table_setup_adjust(1);
        ui_draw_current();
    } else if (s_page == PAGE_HOME && s_home_selection % UI_ICON_COLS < UI_ICON_COLS - 1 && s_home_selection + 1 < UI_APP_COUNT) {
        s_home_selection++;
        ui_draw_current();
    } else if (s_page == PAGE_MATH_MENU && s_math_tab + 1 < MATH_TAB_COUNT) {
        s_math_tab++;
        s_math_selection = 0;
        ui_draw_current();
    } else if (s_page == PAGE_SETTINGS && s_script_selection == 0) {
        adjust_brightness(10);
    } else if (s_page == PAGE_SETTINGS && s_script_selection == 4) {
        adjust_audio_volume(5);
    } else if (s_page == PAGE_LIST_EDITOR) {
        s_list_index = (s_list_index + 1) % LIST_COUNT;
        if (s_list_cursor > s_list_counts[s_list_index]) s_list_cursor = s_list_counts[s_list_index];
        s_list_entry[0] = '\0';
        s_list_editing = false;
        ui_draw_current();
    } else if (s_page == PAGE_APP && s_current_app == APP_LISTS) {
        select_list_index(s_list_index + 1);
        snprintf(s_calc_output, sizeof(s_calc_output), "List L%d selected", s_list_index + 1);
        ui_draw_current();
    } else if (s_page == PAGE_APP && s_current_app == APP_MATRICES) {
        s_matrix_index = (s_matrix_index + 1) % MATRIX_COUNT;
        matrix_reset_navigation();
        snprintf(s_calc_output, sizeof(s_calc_output), "Matrix %c selected", 'A' + s_matrix_index);
        ui_draw_current();
    } else if (s_page == PAGE_STATS_PLOT) {
        s_stats_plot_mode = (s_stats_plot_mode + 1) % 5;
        ui_draw_current();
    } else if (s_page == PAGE_CALCULATOR && opencalc_calc_history_selected() >= 0) {
        opencalc_calc_history_select_answer();
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
    if (s_page == PAGE_REFERENCE_HOME && s_reference_home_selection > 0) {
        s_reference_home_selection--;
        ui_draw_current();
    } else if (s_page == PAGE_PERIODIC_TABLE) {
        periodic_move_selection(0, -1);
        ui_draw_current();
    } else if (s_page == PAGE_REFERENCE_LIST && s_reference_selection > 0) {
        s_reference_selection--;
        ui_draw_current();
    } else if (s_page == PAGE_GRAPH) {
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
    } else if (s_page == PAGE_GRAPH_FORMAT && s_graph_format_selection > 0) {
        s_graph_format_selection--;
    } else if (s_page == PAGE_MODE_MENU && s_mode_selection > 0) {
        s_mode_selection--;
    } else if (s_page == PAGE_TABLE) {
        s_table_x_start -= s_table_step;
    } else if (s_page == PAGE_TABLE_SETUP && s_table_setup_selection > 0) {
        s_table_setup_selection--;
    } else if (s_page == PAGE_HOME && s_home_selection >= UI_ICON_COLS) {
        s_home_selection -= UI_ICON_COLS;
    } else if (s_page == PAGE_CALCULATOR && opencalc_calc_history_count() > 0) {
        opencalc_calc_history_move_up();
    } else if (s_page == PAGE_Y_EQUALS && s_graph_selection > 0) {
        s_graph_selection--;
    } else if (s_page == PAGE_MATH_MENU && s_math_selection > 0) {
        s_math_selection--;
    } else if (s_page == PAGE_GRAPH_CALC && s_graph_calc_selection > 0) {
        s_graph_calc_selection--;
    } else if (s_page == PAGE_SOLVER_ROOTS && s_solver_poly_root_selected > 0) {
        s_solver_poly_root_selected--;
    } else if (s_page == PAGE_APP && s_app_selection > 0) {
        s_app_selection--;
    } else if (s_page == PAGE_LIST_EDITOR && s_list_cursor > 0) {
        s_list_cursor--;
        s_list_entry[0] = '\0';
        s_list_editing = false;
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
    if (s_page == PAGE_REFERENCE_HOME && s_reference_home_selection < 3) {
        s_reference_home_selection++;
        ui_draw_current();
    } else if (s_page == PAGE_PERIODIC_TABLE) {
        periodic_move_selection(0, 1);
        ui_draw_current();
    } else if (s_page == PAGE_REFERENCE_LIST &&
               s_reference_selection + 1 < (int)opencalc_reference_count(s_reference_category)) {
        s_reference_selection++;
        ui_draw_current();
    } else if (s_page == PAGE_GRAPH) {
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
    } else if (s_page == PAGE_GRAPH_FORMAT && s_graph_format_selection + 1 < GRAPH_FORMAT_COUNT) {
        s_graph_format_selection++;
    } else if (s_page == PAGE_MODE_MENU && s_mode_selection < 4) {
        s_mode_selection++;
    } else if (s_page == PAGE_TABLE) {
        s_table_x_start += s_table_step;
    } else if (s_page == PAGE_TABLE_SETUP && s_table_setup_selection < 3) {
        s_table_setup_selection++;
    } else if (s_page == PAGE_HOME && s_home_selection + UI_ICON_COLS < UI_APP_COUNT) {
        s_home_selection += UI_ICON_COLS;
    } else if (s_page == PAGE_CALCULATOR && opencalc_calc_history_selected() >= 0) {
        opencalc_calc_history_move_down();
    } else if (s_page == PAGE_Y_EQUALS && s_graph_selection + 1 < graph_entry_count()) {
        s_graph_selection++;
    } else if (s_page == PAGE_MATH_MENU && s_math_selection + 1 < math_menu_count(s_math_tab)) {
        s_math_selection++;
    } else if (s_page == PAGE_GRAPH_CALC && s_graph_calc_selection + 1 < GRAPH_CALC_COUNT) {
        s_graph_calc_selection++;
    } else if (s_page == PAGE_SOLVER_ROOTS && s_solver_poly_root_selected + 1 < s_solver_poly_root_count) {
        s_solver_poly_root_selected++;
    } else if (s_page == PAGE_APP) {
        int count = 0;
        (void)app_tools_for(s_current_app, &count);
        if (s_app_selection + 1 < count) {
            s_app_selection++;
        }
    } else if (s_page == PAGE_LIST_EDITOR && s_list_cursor < s_list_counts[s_list_index] && s_list_cursor + 1 < LIST_MAX_VALUES) {
        s_list_cursor++;
        s_list_entry[0] = '\0';
        s_list_editing = false;
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

static bool list_editor_handle_key(int row, int col)
{
    static const char digit_rows[3][4] = {"789", "456", "123"};

    if (s_page != PAGE_LIST_EDITOR) return false;

    if (row >= 6 && row <= 8 && col >= 1 && col <= 3) {
        return list_editor_append_char(digit_rows[row - 6][col - 1]);
    }
    if (row == 9 && col == 1) return list_editor_append_char('0');
    if (row == 9 && col == 2) {
        if (strchr(s_list_entry, '.') == NULL && strchr(s_list_entry, 'e') == NULL &&
            strchr(s_list_entry, 'E') == NULL) {
            return list_editor_append_char('.');
        }
        return true;
    }
    if (row == 9 && col == 3) {
        size_t len = strlen(s_list_entry);
        if (!s_list_editing) {
            return list_editor_append_char('-');
        }
        if (len > 0 && (s_list_entry[len - 1] == 'e' || s_list_entry[len - 1] == 'E')) {
            return list_editor_append_char('-');
        }
        if (s_list_entry[0] == '-') {
            memmove(s_list_entry, s_list_entry + 1, len);
        } else if (len + 1 < sizeof(s_list_entry)) {
            memmove(s_list_entry + 1, s_list_entry, len + 1);
            s_list_entry[0] = '-';
        }
        ui_draw_current();
        return true;
    }
    if (s_second_active && row == 5 && col == 1) {
        if (strchr(s_list_entry, 'e') == NULL && strchr(s_list_entry, 'E') == NULL &&
            s_list_entry[0] != '\0') {
            (void)list_editor_append_char('E');
        }
        return true;
    }
    if (row == 1 && col == 3) {
        key_left();
        return true;
    }
    if (row == 1 && col == 4) {
        key_up();
        return true;
    }
    if (row == 2 && col == 3) {
        key_down();
        return true;
    }
    if (row == 2 && col == 4) {
        key_right();
        return true;
    }
    if (row == 9 && col == 4) {
        list_editor_save_entry();
        return true;
    }
    if (row == 2 && col == 2) {
        key_back();
        return true;
    }
    if (row == 3 && col == 4) {
        if (s_second_active) {
            memset(s_lists[s_list_index], 0, sizeof(s_lists[s_list_index]));
            s_list_counts[s_list_index] = 0;
            s_list_cursor = 0;
            s_list_entry[0] = '\0';
            s_list_editing = false;
            snprintf(s_calc_output, sizeof(s_calc_output), "L%d cleared", s_list_index + 1);
            ui_draw_current();
        } else {
            list_editor_delete();
        }
        return true;
    }

    return true;
}

static void dispatch_key(int row, int col)
{
    printf("key r%d c%d\n", row, col);
    worksheet_mark_dirty();
    bool had_second_active = s_second_active;

    if (s_page == PAGE_SCRIPT_IO && s_script_input_active) {
        bool submitted = false;
        bool cancelled = false;
        if (script_input_handle_key(row, col, &submitted, &cancelled)) {
            if (submitted || cancelled) {
                s_script_input_submitted = submitted;
                s_script_input_cancelled = cancelled;
                s_script_input_active = false;
                snprintf(s_script_status, sizeof(s_script_status),
                         cancelled ? "input cancelled" : "running...");
                if (s_script_task != NULL) xTaskNotifyGive(s_script_task);
            }
            ui_draw_current();
        }
        return;
    }
    if (script_io_handle_key(row, col)) return;

    if (s_page == PAGE_CALC_RESULT) {
        if (row == 1 && col == 4 && s_calc_result_scroll > 0) {
            s_calc_result_scroll--;
        } else if (row == 2 && col == 3) {
            int lines = ((int)strlen(s_calc_output) + 45) / 46;
            if (s_calc_result_scroll + 11 < lines) s_calc_result_scroll++;
        } else if (row == 9 && col == 4) {
            s_page = PAGE_CALCULATOR;
            s_current_app = APP_CALCULATOR;
            expression_append(s_calc_output);
        } else if (row == 2 && col == 2) {
            key_back();
        }
        ui_draw_current();
        return;
    }

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
        } else if (s_alpha_locked) {
            s_alpha_locked = false;
            s_alpha_active = false;
        } else {
            s_alpha_active = !s_alpha_active;
            s_second_active = false;
        }
        ui_draw_current();
        return;
    }

    if ((s_alpha_active || s_alpha_locked) && row == 0 && col == 2) {
        open_reference_center();
        return;
    }

    if ((s_alpha_active || s_alpha_locked) && row == 0 && col == 4) {
        s_alpha_active = false;
        open_graph_symbolic();
        return;
    }

    if (s_second_active && row == 1 && col == 1) {
        s_second_active = false;
        key_home();
        return;
    }

    if (alpha_case_key(row, col)) {
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

    if (list_editor_handle_key(row, col)) {
        goto finish_key;
    }

    if (variable_page_handle_key(row, col)) {
        goto finish_key;
    }

    if (s_page == PAGE_SCRIPT_EDITOR && script_editor_handle_key(row, col)) {
        goto finish_key;
    }

    if (stats_setup_handle_key(row, col)) {
        goto finish_key;
    }

    if (stats_page_handle_key(row, col)) {
        goto finish_key;
    }

    if (solver_page_handle_key(row, col)) {
        goto finish_key;
    }

    if (matrix_page_handle_key(row, col)) {
        goto finish_key;
    }

    if (finance_page_handle_key(row, col)) {
        goto finish_key;
    }

    if (conic_page_handle_key(row, col)) {
        goto finish_key;
    }

    if (inequality_page_handle_key(row, col)) {
        goto finish_key;
    }

    if (graph_symbolic_handle_key(row, col)) {
        goto finish_key;
    }

    if (handle_alpha_insert(row, col)) {
        goto finish_key;
    }

    if (handle_numbered_menu_key(row, col)) {
        goto finish_key;
    }

    if (row == 0 && col == 0) {
        if (s_second_active) {
            remember_app_transition(APP_STATS);
            s_current_app = APP_STATS;
            s_stats_return_page = PAGE_GRAPH;
            s_page = PAGE_STATS_PLOT;
            ui_draw_current();
            goto finish_key;
        }
        s_page = PAGE_Y_EQUALS;
        s_current_app = APP_GRAPH;
        ui_draw_current();
    } else if (row == 0 && col == 1) {
        if (s_second_active) {
            remember_app_transition(APP_TABLE);
            s_current_app = APP_TABLE;
            s_table_setup_selection = 0;
            s_page = PAGE_TABLE_SETUP;
            ui_draw_current();
            goto finish_key;
        }
        open_graph_window_menu();
    } else if (row == 0 && col == 2) {
        if (s_second_active) {
            s_second_active = false;
            open_graph_format_menu();
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
                variables_open(VARIABLE_ACTION_GET);
            } else {
                variables_open(VARIABLE_ACTION_STORE);
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
        else if (row == 3 && col == 3) {
            if (s_second_active) expression_append("convert(");
            else variables_open(VARIABLE_ACTION_INSERT);
        }
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

static void navigation_game_press(int row, int col, void *context)
{
    (void)context;
    int number = row * BOARD_KEYPAD_COLS + col + 1;
    debug_log_keypad_press(number, row, col);
    active_game_physical_press_button_number(number);
}

static void navigation_ui_press(int row, int col, void *context)
{
    (void)context;
    debug_log_keypad_press(row * BOARD_KEYPAD_COLS + col + 1, row, col);
    dispatch_key(row, col);
}

void opencalc_ui_handle_keypad_interrupt(void)
{
    const TickType_t now = xTaskGetTickCount();
    const TickType_t initial_repeat_delay = pdMS_TO_TICKS(350);
    const TickType_t repeat_interval = pdMS_TO_TICKS(85);

    debug_log_raw_keypad_levels();

    if (s_active_game != GAME_NONE) {
        opencalc_navigation_reset_ui();
        bool matrix[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS];
        if (!board_keypad_scan_matrix(matrix)) {
            opencalc_navigation_reset_game();
            vTaskDelay(pdMS_TO_TICKS(1));
            return;
        }
        opencalc_navigation_process_game(matrix, navigation_game_press, NULL);
        vTaskDelay(pdMS_TO_TICKS(1));
        return;
    }

#if OPENCALC_KEYPAD_POLL_WHEN_NO_INTERRUPT
    (void)board_keypad_take_interrupt();
#else
    bool had_interrupt = board_keypad_take_interrupt();
    bool had_pressed_key = opencalc_navigation_ui_key_held();
    if (!had_interrupt && !had_pressed_key) {
        return;
    }
#endif

    bool matrix[BOARD_KEYPAD_ROWS][BOARD_KEYPAD_COLS];
    if (!board_keypad_scan_matrix(matrix)) {
        opencalc_navigation_reset_ui();
        memset(s_script_key_state, 0, sizeof(s_script_key_state));
        return;
    }
    memcpy(s_script_key_state, matrix, sizeof(s_script_key_state));
    opencalc_navigation_process_ui(
        matrix, s_second_active || s_alpha_active, (uint32_t)now,
        (uint32_t)initial_repeat_delay, (uint32_t)repeat_interval,
        navigation_ui_press, NULL);
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
    if (opencalc_ui_work_ready()) return;
    if (!opencalc_ui_work_start(sizeof(ui_work_job_t), sizeof(ui_work_result_t),
                                OPENCALC_MATH_WORKER_TASK_STACK,
                                OPENCALC_WORKER_CORE, ui_work_execute)) {
        printf("ERROR: failed to start PSRAM math worker "
               "(internal=%u largest=%u psram=%u)\n",
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
               (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    } else {
        printf("Math worker ready: %u-byte PSRAM stack on core %d\n",
               (unsigned)OPENCALC_MATH_WORKER_TASK_STACK, OPENCALC_WORKER_CORE);
    }
}

void opencalc_ui_init(void)
{
    if (s_serial_button_queue == NULL) {
        s_serial_button_queue = xQueueCreate(8, sizeof(int));
    }
    opencalc_ui_start_worker();
    script_worker_start();
    s_light_mode = opencalc_persist_get_u32("light_mode", 0) != 0;
    s_sleep_enabled = opencalc_persist_get_u32("auto_sleep", 1) != 0;
    board_set_backlight_brightness((int)opencalc_persist_get_u32("brightness", 80));
    s_audio_volume_percent = (int)opencalc_persist_get_u32("audio_volume", OPENCALC_AUDIO_VOLUME_PERCENT);
    opencalc_audio_set_volume_percent(s_audio_volume_percent);
    s_power_save_saved_brightness = board_get_backlight_brightness();
    apply_power_save_mode(opencalc_persist_get_u32("power_save", 0) != 0);
    s_doom_high_score = opencalc_persist_get_u32("hs_doom", 0);
    s_doom_last_saved_high_score = s_doom_high_score;
    solver_saved_load_all();
    inequality_load_all();
    variables_load_all();
    opencalc_graph_model_reset();
    worksheet_persist_load();
    opencalc_tetris_init();
    opencalc_snake_init();
    opencalc_breakout_init();
    opencalc_mario_init();
}

void opencalc_ui_draw(void)
{
    ui_draw_current();
}

static void ui_memory_health_tick(void)
{
#if OPENCALC_DEBUG_MEMORY_HEALTH
    static int64_t last_log_us;
    int64_t now_us = esp_timer_get_time();
    if (now_us - last_log_us < 5000000) return;
    last_log_us = now_us;
    bool heap_ok = heap_caps_check_integrity_all(false);
    printf("memory health: heap=%s internal=%u largest=%u psram=%u largest=%u ui_stack=%u\n",
           heap_ok ? "ok" : "CORRUPT",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
           (unsigned)uxTaskGetStackHighWaterMark(NULL));
#endif
}

void opencalc_ui_tick(void)
{
    ui_memory_health_tick();
    ui_work_poll_results();
    script_worker_poll_result();
    worksheet_persist_poll();

    if (s_script_screen_dirty && s_page == PAGE_SCRIPT_IO) {
        s_script_screen_dirty = false;
        ui_draw_current();
    }

    s_cursor_blink_visible = ((esp_timer_get_time() / 450000) % 2) == 0;
    if (s_cursor_blink_visible == s_cursor_blink_last_visible) {
        return;
    }
    s_cursor_blink_last_visible = s_cursor_blink_visible;

    if (s_page == PAGE_CALCULATOR ||
        s_page == PAGE_Y_EQUALS ||
        s_page == PAGE_SCRIPT_EDITOR ||
        s_page == PAGE_MATRIX_EDITOR ||
        s_page == PAGE_FINANCE_TVM ||
        s_page == PAGE_FINANCE_CASHFLOW ||
        s_page == PAGE_VARIABLE_NAME ||
        (s_page == PAGE_SCRIPT_IO && s_script_input_active)) {
        ui_draw_current();
    }
}

bool opencalc_ui_doom_active(void)
{
    return s_active_game != GAME_NONE;
}

void opencalc_ui_tick_doom(void)
{
    ui_memory_health_tick();
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
