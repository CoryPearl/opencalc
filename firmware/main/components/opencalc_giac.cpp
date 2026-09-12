#include "opencalc_giac.h"

#include "opencalc_config.h"
#include "opencalc_symbols.h"

#if OPENCALC_ENABLE_GIAC_CAS

/* Keep Giac's public types ABI-compatible with the vendored engine build. */
#ifndef HAVE_CONFIG_H
#define HAVE_CONFIG_H 1
#endif

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <new>
#include <string>
#include <strings.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "gen.h"
#include "global.h"
#include "input_lexer.h"
#include "prog.h"
#include "subst.h"
#include "usual.h"

namespace {

constexpr size_t kInputMax = OPENCALC_SYMBOL_TEXT_MAX + 64;
constexpr size_t kResultMax = 1024;
constexpr char kTag[] = "giac";

enum class RequestType : uint8_t {
    Evaluate,
    Reset,
};

struct GiacRequest {
    unsigned references{1};
    bool abandoned{false};
    RequestType type;
    bool degrees;
    opencalc_cas_domain_t domain;
    char assumptions[OPENCALC_CAS_ASSUMPTIONS_MAX];
    char expression[kInputMax];
    char result[kResultMax];
    bool ok;
    SemaphoreHandle_t complete;
    StaticSemaphore_t complete_storage;
};

QueueHandle_t s_queue;
TaskHandle_t s_task;
SemaphoreHandle_t s_start_lock;
giac::context *s_context;
uint32_t s_synced_symbol_generation;
char s_synced_symbol_names[OPENCALC_SYMBOL_MAX][OPENCALC_SYMBOL_NAME_MAX];
size_t s_synced_symbol_count;
portMUX_TYPE s_request_lock = portMUX_INITIALIZER_UNLOCKED;
bool s_quarantined;
bool s_recovery_required;

GiacRequest *allocate_request()
{
    constexpr size_t request_size = sizeof(GiacRequest);
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    void *memory = request_size <= psram_free &&
                           psram_free - request_size >= OPENCALC_PSRAM_RESERVE_BYTES
                       ? heap_caps_malloc(request_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
                       : nullptr;
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (memory == nullptr && request_size <= internal_free &&
        internal_free - request_size >= OPENCALC_INTERNAL_HEAP_RESERVE_BYTES) {
        memory = heap_caps_malloc(request_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (memory == nullptr) return nullptr;
    return new (memory) GiacRequest{};
}

void release_request(GiacRequest *request)
{
    if (request == nullptr) return;
    bool destroy = false;
    taskENTER_CRITICAL(&s_request_lock);
    if (--request->references == 0) destroy = true;
    taskEXIT_CRITICAL(&s_request_lock);
    if (destroy) {
        request->~GiacRequest();
        heap_caps_free(request);
    }
}

void retain_request(GiacRequest *request)
{
    taskENTER_CRITICAL(&s_request_lock);
    request->references++;
    taskEXIT_CRITICAL(&s_request_lock);
}

bool request_abandoned(GiacRequest *request)
{
    taskENTER_CRITICAL(&s_request_lock);
    bool abandoned = request->abandoned;
    taskEXIT_CRITICAL(&s_request_lock);
    return abandoned;
}

void abandon_request(GiacRequest *request)
{
    taskENTER_CRITICAL(&s_request_lock);
    request->abandoned = true;
    taskEXIT_CRITICAL(&s_request_lock);
}

void clear_interrupt_request()
{
    giac::ctrl_c = false;
    giac::interrupted = false;
    giac::kbd_interrupted = false;
}

void interrupt_request(GiacRequest *request)
{
    abandon_request(request);
    /* Giac's long-running algebra loops call control_c(), which throws when
       ctrl_c is set. The worker catches that exception and recycles the
       context; never delete a C++ task while it may own allocator state. */
    giac::kbd_interrupted = true;
    giac::interrupted = true;
    giac::ctrl_c = true;
}

bool quarantined()
{
    taskENTER_CRITICAL(&s_request_lock);
    bool value = s_quarantined;
    taskEXIT_CRITICAL(&s_request_lock);
    return value;
}

void set_quarantined(bool value)
{
    taskENTER_CRITICAL(&s_request_lock);
    s_quarantined = value;
    taskEXIT_CRITICAL(&s_request_lock);
}

void set_recovery_required(bool value)
{
    taskENTER_CRITICAL(&s_request_lock);
    s_recovery_required = value;
    taskEXIT_CRITICAL(&s_request_lock);
}

void configure_context(giac::context *context)
{
    /* Keep setup context-local. Lexer localization mutates shared parser tables
       and is unnecessary for the English command set used by OpenCalc. */
    giac::xcas_mode(0, context);
    giac::approx_mode(false, context);
    giac::complex_mode(true, context);
    giac::complex_variables(true, context);
    giac::i_sqrt_minus1(1, context);
    giac::withsqrt(true, context);
    giac::eval_level(context) = 1;
    giac::step_infolevel(context) = 0;
}

bool ensure_context()
{
    if (s_context != nullptr) return true;
    ESP_LOGI(kTag, "Creating context (internal=%u, psram=%u)",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    try {
        s_context = new giac::context;
        ESP_LOGI(kTag, "Context allocated; applying calculator settings");
        configure_context(s_context);
        ESP_LOGI(kTag, "Context ready");
        return true;
    } catch (...) {
        ESP_LOGE(kTag, "Context initialization failed");
        delete s_context;
        s_context = nullptr;
        return false;
    }
}

bool evaluate_sync_command(const char *command)
{
    giac::gen parsed(command, s_context);
    if (giac::is_undef(parsed)) return false;
    giac::gen result = giac::eval(parsed, giac::eval_level(s_context), s_context);
    return !giac::is_undef(result);
}

bool sync_real_payload(const opencalc_symbol_t *symbol, const double *values,
                       size_t count, void *)
{
    if (symbol == nullptr || values == nullptr || count != symbol->element_count) return false;
    try {
        giac::gen value;
        if (symbol->type == OPENCALC_SYMBOL_MATRIX) {
            giac::vecteur rows;
            rows.reserve(symbol->rows);
            for (size_t row = 0; row < symbol->rows; row++) {
                giac::vecteur columns;
                columns.reserve(symbol->cols);
                for (size_t col = 0; col < symbol->cols; col++) {
                    columns.push_back(giac::gen(values[row * symbol->cols + col]));
                }
                rows.push_back(giac::gen(columns));
            }
            value = giac::gen(rows, giac::_MATRIX__VECT);
        } else {
            giac::vecteur list;
            list.reserve(count);
            for (size_t i = 0; i < count; i++) list.push_back(giac::gen(values[i]));
            value = giac::gen(list);
        }
        giac::gen destination{giac::identificateur(symbol->name)};
        return !giac::is_undef(giac::sto(value, destination, s_context));
    } catch (...) {
        return false;
    }
}

void sync_symbol_table()
{
    uint32_t generation = opencalc_symbols_generation();
    if (generation == 0 || generation == s_synced_symbol_generation) return;

    char command[kInputMax];
    for (size_t i = 0; i < s_synced_symbol_count; i++) {
        opencalc_symbol_t current;
        if (opencalc_symbol_get(s_synced_symbol_names[i], &current) &&
            (current.flags & OPENCALC_SYMBOL_GIAC_SYNC) &&
            std::strcmp(current.name, s_synced_symbol_names[i]) == 0) continue;
        int written = std::snprintf(command, sizeof(command), "purge(%s)",
                                    s_synced_symbol_names[i]);
        if (written > 0 && static_cast<size_t>(written) < sizeof(command)) {
            (void)evaluate_sync_command(command);
        }
    }

    size_t synced_count = 0;
    opencalc_symbol_t symbol;
    for (size_t i = 0;
         i < OPENCALC_SYMBOL_MAX &&
         opencalc_symbol_at(i, OPENCALC_SYMBOL_GIAC_SYNC, &symbol);
         i++) {
        if (symbol.structured) {
            if (!opencalc_symbol_visit_real_values(symbol.name, sync_real_payload, nullptr)) {
                ESP_LOGW(kTag, "Could not synchronize structured symbol %s", symbol.name);
                continue;
            }
            std::snprintf(s_synced_symbol_names[synced_count],
                          sizeof(s_synced_symbol_names[synced_count]), "%s", symbol.name);
            synced_count++;
            continue;
        }
        if (symbol.text[0] == '\0') continue;
        int written = symbol.type == OPENCALC_SYMBOL_FUNCTION
            ? std::snprintf(command, sizeof(command), "%s:=unapply((%s),x)",
                            symbol.name, symbol.text)
            : std::snprintf(command, sizeof(command), "%s:=(%s)",
                            symbol.name, symbol.text);
        if (written <= 0 || static_cast<size_t>(written) >= sizeof(command)) {
            ESP_LOGW(kTag, "Symbol %s is too large for the CAS context", symbol.name);
            continue;
        }
        if (!evaluate_sync_command(command)) {
            ESP_LOGW(kTag, "Could not synchronize %s with the CAS context", symbol.name);
            continue;
        }
        std::snprintf(s_synced_symbol_names[synced_count],
                      sizeof(s_synced_symbol_names[synced_count]), "%s", symbol.name);
        synced_count++;
    }
    s_synced_symbol_count = synced_count;
    s_synced_symbol_generation = generation;
}

bool expression_mutates_context(const char *expression)
{
    if (expression == nullptr) return false;
    while (std::isspace(static_cast<unsigned char>(*expression))) expression++;
    if (strncasecmp(expression, "sto(", 4) == 0 ||
        strncasecmp(expression, "purge(", 6) == 0) return true;

    const char *cursor = expression;
    if (!(std::isalpha(static_cast<unsigned char>(*cursor)) || *cursor == '_')) return false;
    while (std::isalnum(static_cast<unsigned char>(*cursor)) || *cursor == '_') cursor++;
    while (std::isspace(static_cast<unsigned char>(*cursor))) cursor++;
    return (cursor[0] == ':' && cursor[1] == '=') ||
        (cursor[0] == '=' && cursor[1] != '=');
}

std::string normalize_expression(const char *expression)
{
    std::string normalized(expression != nullptr ? expression : "");
    struct Alias {
        const char *from;
        const char *to;
    };
    static constexpr Alias aliases[] = {
        {"deriv(", "diff("},
        {"int(", "integrate("},
        {"defint(", "integrate("},
        {"log(", "log10("},
        {"numerator(", "numer("},
        {"denominator(", "denom("},
        {"transpose(", "tran("},
        {"eigenvec(", "eigenvects("},
    };

    for (const Alias &alias : aliases) {
        size_t position = 0;
        while ((position = normalized.find(alias.from, position)) != std::string::npos) {
            bool identifier_start = position == 0 ||
                !(std::isalnum(static_cast<unsigned char>(normalized[position - 1])) ||
                  normalized[position - 1] == '_');
            if (identifier_start) {
                normalized.replace(position, std::strlen(alias.from), alias.to);
                position += std::strlen(alias.to);
            } else {
                position += std::strlen(alias.from);
            }
        }
    }
    return normalized;
}

bool evaluate_request(GiacRequest *request)
{
    if (!ensure_context()) {
        std::snprintf(request->result, sizeof(request->result), "CAS out of memory");
        return false;
    }

    try {
        giac::angle_radian(!request->degrees, s_context);
        bool complex_domain = request->domain == OPENCALC_CAS_DOMAIN_AUTO ||
                              request->domain == OPENCALC_CAS_DOMAIN_COMPLEX;
        giac::complex_mode(complex_domain, s_context);
        giac::complex_variables(complex_domain, s_context);
        sync_symbol_table();
        if (request->assumptions[0] != '\0') {
            char assume_command[OPENCALC_CAS_ASSUMPTIONS_MAX + 16];
            int written = std::snprintf(assume_command, sizeof(assume_command),
                                        "assume(%s)", request->assumptions);
            if (written <= 0 || static_cast<size_t>(written) >= sizeof(assume_command) ||
                !evaluate_sync_command(assume_command)) {
                std::snprintf(request->result, sizeof(request->result), "invalid assumptions");
                return false;
            }
        }
        std::string input = normalize_expression(request->expression);
        ESP_LOGI(kTag, "Parsing: %.96s", input.c_str());
        giac::gen parsed(input, s_context);
        if (giac::is_undef(parsed)) return false;

        ESP_LOGI(kTag, "Evaluating expression");
        giac::gen result = giac::eval(parsed, giac::eval_level(s_context), s_context);
        if (giac::is_undef(result)) return false;

        ESP_LOGI(kTag, "Formatting result");
        std::string text = result.print(s_context);
        if (text.empty() || text.size() >= sizeof(request->result)) return false;
        std::memcpy(request->result, text.c_str(), text.size() + 1);
        return true;
    } catch (const std::bad_alloc &) {
        std::snprintf(request->result, sizeof(request->result), "CAS out of memory");
    } catch (const std::exception &error) {
        std::snprintf(request->result, sizeof(request->result), "CAS: %.240s", error.what());
    } catch (...) {
        std::snprintf(request->result, sizeof(request->result), "CAS evaluation error");
    }
    return false;
}

void giac_task(void *)
{
    GiacRequest *request = nullptr;
    while (true) {
        if (xQueueReceive(s_queue, &request, portMAX_DELAY) != pdTRUE || request == nullptr) {
            continue;
        }

        clear_interrupt_request();
        bool recycle_context = request_abandoned(request);
        if (recycle_context) {
            request->ok = false;
            std::snprintf(request->result, sizeof(request->result), "cancelled");
        } else if (request->type == RequestType::Reset) {
            delete s_context;
            s_context = nullptr;
            s_synced_symbol_generation = 0;
            s_synced_symbol_count = 0;
            request->ok = true;
            request->result[0] = '\0';
        } else {
            request->ok = evaluate_request(request);
            recycle_context = request_abandoned(request);
        }

        bool rebuild_context = !recycle_context &&
            (request->assumptions[0] != '\0' ||
             (request->type == RequestType::Evaluate &&
              expression_mutates_context(request->expression)));
        if (rebuild_context) {
            /* Assumptions and assignments are request-local to Giac. The native
               typed symbol table is the authority and will populate a fresh
               context for the next request. */
            delete s_context;
            s_context = nullptr;
            s_synced_symbol_generation = 0;
            s_synced_symbol_count = 0;
        }

        bool restart_worker = recycle_context;
        if (recycle_context) {
            ESP_LOGW(kTag, "Discarding CAS context after interrupted request");
            delete s_context;
            s_context = nullptr;
            s_synced_symbol_generation = 0;
            s_synced_symbol_count = 0;
            clear_interrupt_request();
            taskENTER_CRITICAL(&s_request_lock);
            s_task = nullptr;
            taskEXIT_CRITICAL(&s_request_lock);
            if (quarantined()) {
                set_quarantined(false);
            }
        }
        xSemaphoreGive(request->complete);
        if (restart_worker) set_recovery_required(false);
        release_request(request);
        if (restart_worker) {
            ESP_LOGI(kTag, "CAS context cleared; retiring worker generation");
            vTaskDelete(nullptr);
        }
    }
}

bool ensure_task()
{
    if (quarantined()) return false;
    if (s_task != nullptr && s_queue != nullptr) return true;

    if (s_start_lock == nullptr) s_start_lock = xSemaphoreCreateMutex();
    if (s_start_lock == nullptr || xSemaphoreTake(s_start_lock, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    if (s_queue == nullptr) s_queue = xQueueCreate(1, sizeof(GiacRequest *));
    if (s_task == nullptr && s_queue != nullptr) {
        ESP_LOGI(kTag, "Starting CAS task with %u-byte PSRAM stack on core %d",
                 static_cast<unsigned>(OPENCALC_GIAC_TASK_STACK), OPENCALC_WORKER_CORE);
        BaseType_t created = xTaskCreatePinnedToCoreWithCaps(
            giac_task,
            "opencalc_giac",
            OPENCALC_GIAC_TASK_STACK,
            nullptr,
            5,
            &s_task,
            OPENCALC_WORKER_CORE,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (created != pdPASS) {
            s_task = nullptr;
            ESP_LOGE(kTag, "Failed to create PSRAM-backed CAS task");
        }
    }

    bool ready = s_task != nullptr && s_queue != nullptr;
    xSemaphoreGive(s_start_lock);
    return ready;
}

opencalc_giac_status_t submit_request(GiacRequest *request,
                                      unsigned timeout_ms,
                                      opencalc_giac_cancel_fn should_cancel,
                                      void *cancel_context)
{
    if (!ensure_task()) return OPENCALC_GIAC_UNAVAILABLE;
    request->complete = xSemaphoreCreateBinaryStatic(&request->complete_storage);
    if (request->complete == nullptr) return OPENCALC_GIAC_UNAVAILABLE;

    retain_request(request); // Giac task ownership.
    if (xQueueSend(s_queue, &request, pdMS_TO_TICKS(100)) != pdTRUE) {
        release_request(request);
        return OPENCALC_GIAC_UNAVAILABLE;
    }

    TickType_t elapsed = 0;
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    if (timeout_ticks == 0) timeout_ticks = 1;
    TickType_t poll_ticks = pdMS_TO_TICKS(OPENCALC_CAS_CANCEL_POLL_MS);
    if (poll_ticks == 0) poll_ticks = 1;

    opencalc_giac_status_t interrupted_status = OPENCALC_GIAC_OK;
    while (elapsed < timeout_ticks) {
        if (should_cancel != nullptr && should_cancel(cancel_context)) {
            interrupted_status = OPENCALC_GIAC_CANCELLED;
            break;
        }
        TickType_t remaining = timeout_ticks - elapsed;
        TickType_t wait = remaining < poll_ticks ? remaining : poll_ticks;
        if (xSemaphoreTake(request->complete, wait) == pdTRUE) {
            return request->ok ? OPENCALC_GIAC_OK : OPENCALC_GIAC_ERROR;
        }
        elapsed += wait;
    }
    if (interrupted_status == OPENCALC_GIAC_OK) {
        interrupted_status = OPENCALC_GIAC_TIMEOUT;
    }

    interrupt_request(request);
    /* Reject new work while the current context is unwinding. The worker clears
       this only after deleting the interrupted context. */
    set_quarantined(true);
    TickType_t grace_ticks = pdMS_TO_TICKS(OPENCALC_CAS_RECOVERY_GRACE_MS);
    if (grace_ticks == 0) grace_ticks = 1;
    if (xSemaphoreTake(request->complete, grace_ticks) != pdTRUE) {
        set_recovery_required(true);
        if (xSemaphoreTake(request->complete, 0) == pdTRUE) {
            set_recovery_required(false);
            ESP_LOGI(kTag, "CAS interruption completed at recovery deadline");
        } else {
            ESP_LOGE(kTag,
                     "CAS did not acknowledge interruption within %u ms; controlled recovery required",
                     static_cast<unsigned>(OPENCALC_CAS_RECOVERY_GRACE_MS));
        }
    } else {
        set_quarantined(false);
        set_recovery_required(false);
        ESP_LOGI(kTag, "CAS interruption acknowledged; worker will restart cleanly");
    }
    return interrupted_status;
}

} // namespace

extern "C" opencalc_giac_status_t opencalc_giac_eval_timed(
    const char *expression, bool degrees, char *out, size_t out_size,
    unsigned timeout_ms, opencalc_giac_cancel_fn should_cancel, void *cancel_context)
{
    opencalc_giac_options_t options = {
        .degrees = degrees,
        .domain = OPENCALC_CAS_DOMAIN_AUTO,
        .assumptions = {},
    };
    return opencalc_giac_eval_timed_options(expression, &options, out, out_size,
                                            timeout_ms, should_cancel, cancel_context);
}

extern "C" opencalc_giac_status_t opencalc_giac_eval_timed_options(
    const char *expression, const opencalc_giac_options_t *options,
    char *out, size_t out_size, unsigned timeout_ms,
    opencalc_giac_cancel_fn should_cancel, void *cancel_context)
{
    return opencalc_giac_eval_timed_structured(
        expression, options, out, out_size, nullptr, timeout_ms,
        should_cancel, cancel_context);
}

extern "C" opencalc_giac_status_t opencalc_giac_eval_timed_structured(
    const char *expression, const opencalc_giac_options_t *options,
    char *out, size_t out_size, opencalc_cas_result_t *structured,
    unsigned timeout_ms, opencalc_giac_cancel_fn should_cancel, void *cancel_context)
{
    if (expression == nullptr || expression[0] == '\0' || out == nullptr || out_size == 0) {
        return OPENCALC_GIAC_ERROR;
    }

    GiacRequest *request = allocate_request();
    if (request == nullptr) return OPENCALC_GIAC_UNAVAILABLE;

    request->type = RequestType::Evaluate;
    request->degrees = options != nullptr ? options->degrees : false;
    request->domain = options != nullptr ? options->domain : OPENCALC_CAS_DOMAIN_AUTO;
    if (options != nullptr) {
        std::snprintf(request->assumptions, sizeof(request->assumptions), "%s",
                      options->assumptions);
    }
    if (std::strlen(expression) >= sizeof(request->expression)) {
        release_request(request);
        return OPENCALC_GIAC_ERROR;
    }
    std::memcpy(request->expression, expression, std::strlen(expression) + 1);

    opencalc_giac_status_t status = submit_request(request, timeout_ms,
                                                   should_cancel, cancel_context);
    if (status == OPENCALC_GIAC_OK) {
        size_t result_length = std::strlen(request->result);
        if (result_length < out_size) {
            std::memcpy(out, request->result, result_length + 1);
        } else if (out_size >= 4) {
            std::memcpy(out, request->result, out_size - 4);
            std::memcpy(out + out_size - 4, "...", 4);
        } else {
            status = OPENCALC_GIAC_ERROR;
        }
    } else if (status == OPENCALC_GIAC_ERROR && request->result[0] != '\0') {
        std::snprintf(out, out_size, "%s", request->result);
    } else if (status == OPENCALC_GIAC_TIMEOUT) {
        std::snprintf(out, out_size, "CAS timed out");
    } else if (status == OPENCALC_GIAC_CANCELLED) {
        std::snprintf(out, out_size, "cancelled");
    } else {
        std::snprintf(out, out_size, "CAS unavailable");
    }
    release_request(request);
    if (status == OPENCALC_GIAC_OK && structured != nullptr) {
        opencalc_cas_analyze_result(out, structured);
    }
    return status;
}

extern "C" bool opencalc_giac_eval(const char *expression,
                                     bool degrees,
                                     char *out,
                                     size_t out_size)
{
    return opencalc_giac_eval_timed(expression, degrees, out, out_size,
                                    OPENCALC_CAS_TIMEOUT_MS, nullptr, nullptr) == OPENCALC_GIAC_OK;
}

extern "C" int opencalc_giac_self_test(void)
{
    struct TestCase {
        const char *expression;
        /* Null means evaluation success is sufficient. */
        const char *expected_fragment;
    };
    static constexpr TestCase tests[] = {
        {"2+2", "4"},
        {"1/2+1/3", "5/6"},
        {"2^100", "1267650600228229401496703205376"},
        {"simplify((x^2-1)/(x-1))", "x+1"},
        {"factor(x^3-6*x^2+11*x-6)", "(x-1)*(x-2)*(x-3)"},
        {"solve(x^2-2=0,x)", "sqrt(2)"},
        {"diff(sin(x),x)", "cos(x)"},
        {"integrate(x^2,x)", "x^3/3"},
        {"sin(pi/6)", "1/2"},
        {"det([[1,2],[3,4]])", "-2"},
        {"sqrt(-1)", "i"},
        {"opencalc_test_var:=5", "5"},
        {"opencalc_test_var", "5"},
        {"purge(opencalc_test_var)", nullptr},
        {"opencalc_test_var", "opencalc_test_var"},
    };

    opencalc_giac_reset();
    int failures = 0;
    char output[192];
    size_t internal_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t psram_before = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_LOGI(kTag, "Starting %u-case CAS smoke test", (unsigned)(sizeof(tests) / sizeof(tests[0])));
    for (const TestCase &test : tests) {
        bool ok = opencalc_giac_eval(test.expression, false, output, sizeof(output));
        if (!ok || (test.expected_fragment != nullptr &&
                    std::strstr(output, test.expected_fragment) == nullptr)) {
            ESP_LOGE(kTag, "FAIL %s -> %s (expected text containing %s)",
                     test.expression, ok ? output : "<evaluation failed>",
                     test.expected_fragment != nullptr ? test.expected_fragment : "<any result>");
            failures++;
        } else {
            ESP_LOGI(kTag, "PASS %s -> %s", test.expression, output);
        }
    }
    UBaseType_t stack_remaining =
        s_task != nullptr ? uxTaskGetStackHighWaterMark(s_task) : 0;
    opencalc_giac_reset();
    size_t internal_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t psram_after = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_LOGI(kTag,
             "CAS smoke test complete: %d failure(s), stack free low-water=%u, internal=%u->%u, psram=%u->%u",
             failures, (unsigned)stack_remaining,
             (unsigned)internal_before, (unsigned)internal_after,
             (unsigned)psram_before, (unsigned)psram_after);
    return failures;
}

extern "C" void opencalc_giac_reset(void)
{
    GiacRequest *request = allocate_request();
    if (request == nullptr) return;
    request->type = RequestType::Reset;
    (void)submit_request(request, OPENCALC_CAS_TIMEOUT_MS, nullptr, nullptr);
    release_request(request);
}

extern "C" bool opencalc_giac_recovery_required(void)
{
    taskENTER_CRITICAL(&s_request_lock);
    bool required = s_recovery_required;
    taskEXIT_CRITICAL(&s_request_lock);
    return required;
}

#else

extern "C" bool opencalc_giac_eval(const char *, bool, char *, size_t)
{
    return false;
}

extern "C" opencalc_giac_status_t opencalc_giac_eval_timed(
    const char *, bool, char *, size_t, unsigned, opencalc_giac_cancel_fn, void *)
{
    return OPENCALC_GIAC_UNAVAILABLE;
}

extern "C" opencalc_giac_status_t opencalc_giac_eval_timed_options(
    const char *, const opencalc_giac_options_t *, char *, size_t, unsigned,
    opencalc_giac_cancel_fn, void *)
{
    return OPENCALC_GIAC_UNAVAILABLE;
}

extern "C" opencalc_giac_status_t opencalc_giac_eval_timed_structured(
    const char *, const opencalc_giac_options_t *, char *, size_t,
    opencalc_cas_result_t *, unsigned, opencalc_giac_cancel_fn, void *)
{
    return OPENCALC_GIAC_UNAVAILABLE;
}

extern "C" void opencalc_giac_reset(void)
{
}

extern "C" bool opencalc_giac_recovery_required(void)
{
    return false;
}

extern "C" int opencalc_giac_self_test(void)
{
    return -1;
}

#endif
