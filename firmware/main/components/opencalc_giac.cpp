#include "opencalc_giac.h"

#include "opencalc_config.h"

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

namespace {

constexpr size_t kInputMax = 768;
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
portMUX_TYPE s_request_lock = portMUX_INITIALIZER_UNLOCKED;
bool s_quarantined;

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
            request->ok = true;
            request->result[0] = '\0';
        } else {
            request->ok = evaluate_request(request);
            recycle_context = request_abandoned(request);
        }

        if (recycle_context) {
            ESP_LOGW(kTag, "Discarding CAS context after interrupted request");
            delete s_context;
            s_context = nullptr;
            clear_interrupt_request();
            if (quarantined()) {
                set_quarantined(false);
                ESP_LOGI(kTag, "CAS worker recovered; accepting requests again");
            }
        }
        xSemaphoreGive(request->complete);
        release_request(request);
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
        ESP_LOGE(kTag,
                 "CAS did not acknowledge interruption within %u ms; backend quarantined",
                 static_cast<unsigned>(OPENCALC_CAS_RECOVERY_GRACE_MS));
    } else {
        set_quarantined(false);
        ESP_LOGI(kTag, "CAS interruption acknowledged; context recycled");
    }
    return interrupted_status;
}

} // namespace

extern "C" opencalc_giac_status_t opencalc_giac_eval_timed(
    const char *expression, bool degrees, char *out, size_t out_size,
    unsigned timeout_ms, opencalc_giac_cancel_fn should_cancel, void *cancel_context)
{
    if (expression == nullptr || expression[0] == '\0' || out == nullptr || out_size == 0) {
        return OPENCALC_GIAC_ERROR;
    }

    GiacRequest *request = allocate_request();
    if (request == nullptr) return OPENCALC_GIAC_UNAVAILABLE;

    request->type = RequestType::Evaluate;
    request->degrees = degrees;
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

extern "C" void opencalc_giac_reset(void)
{
}

extern "C" int opencalc_giac_self_test(void)
{
    return -1;
}

#endif
