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

namespace giac {
void check_browser_functions();
void lexer_localization(int lang, const context *contextptr);
}

namespace {

constexpr size_t kInputMax = 768;
constexpr size_t kResultMax = 1024;
constexpr char kTag[] = "giac";

enum class RequestType : uint8_t {
    Evaluate,
    Reset,
};

struct GiacRequest {
    RequestType type;
    bool degrees;
    char expression[kInputMax];
    char result[kResultMax];
    bool ok;
    SemaphoreHandle_t complete;
};

QueueHandle_t s_queue;
TaskHandle_t s_task;
SemaphoreHandle_t s_start_lock;
giac::context *s_context;

void configure_context(giac::context *context)
{
    giac::xcas_mode(0, context);
    giac::approx_mode(false, context);
    giac::complex_mode(true, context);
    giac::complex_variables(true, context);
    giac::i_sqrt_minus1(1, context);
    giac::withsqrt(true, context);
    giac::eval_level(context) = 1;
    giac::step_infolevel(context) = 0;
    giac::language(0, context);
    giac::check_browser_functions();
    giac::lexer_localization(0, context);
    giac::cas_setup(giac::makevecteur(0, 0, 0, 1, 0), context);
}

bool ensure_context()
{
    if (s_context != nullptr) return true;
    try {
        s_context = new giac::context;
        configure_context(s_context);
        return true;
    } catch (...) {
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
        giac::gen parsed(input, s_context);
        if (giac::is_undef(parsed)) return false;

        giac::gen result = giac::eval(parsed, giac::eval_level(s_context), s_context);
        if (giac::is_undef(result)) return false;

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

        if (request->type == RequestType::Reset) {
            delete s_context;
            s_context = nullptr;
            request->ok = true;
            request->result[0] = '\0';
        } else {
            request->ok = evaluate_request(request);
        }
        xSemaphoreGive(request->complete);
    }
}

bool ensure_task()
{
    if (s_task != nullptr && s_queue != nullptr) return true;

    if (s_start_lock == nullptr) s_start_lock = xSemaphoreCreateMutex();
    if (s_start_lock == nullptr || xSemaphoreTake(s_start_lock, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    if (s_queue == nullptr) s_queue = xQueueCreate(1, sizeof(GiacRequest *));
    if (s_task == nullptr && s_queue != nullptr) {
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

bool submit_request(GiacRequest *request)
{
    if (!ensure_task()) return false;
    StaticSemaphore_t complete_storage{};
    request->complete = xSemaphoreCreateBinaryStatic(&complete_storage);
    if (request->complete == nullptr) return false;

    bool queued = xQueueSend(s_queue, &request, pdMS_TO_TICKS(100)) == pdTRUE;
    /* Once queued, the request lives on this caller's stack until completion. */
    bool completed = queued && xSemaphoreTake(request->complete, portMAX_DELAY) == pdTRUE;
    request->complete = nullptr;
    return completed && request->ok;
}

} // namespace

extern "C" bool opencalc_giac_eval(const char *expression,
                                     bool degrees,
                                     char *out,
                                     size_t out_size)
{
    if (expression == nullptr || expression[0] == '\0' || out == nullptr || out_size == 0) {
        return false;
    }

    GiacRequest *request = static_cast<GiacRequest *>(
        heap_caps_calloc(1, sizeof(GiacRequest), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (request == nullptr) {
        request = static_cast<GiacRequest *>(
            heap_caps_calloc(1, sizeof(GiacRequest), MALLOC_CAP_8BIT));
    }
    if (request == nullptr) return false;

    request->type = RequestType::Evaluate;
    request->degrees = degrees;
    if (std::strlen(expression) >= sizeof(request->expression)) {
        heap_caps_free(request);
        return false;
    }
    std::memcpy(request->expression, expression, std::strlen(expression) + 1);

    if (!submit_request(request)) {
        heap_caps_free(request);
        return false;
    }
    size_t result_length = std::strlen(request->result);
    if (result_length >= out_size) {
        if (out_size < 4) {
            heap_caps_free(request);
            return false;
        }
        std::memcpy(out, request->result, out_size - 4);
        std::memcpy(out + out_size - 4, "...", 4);
        heap_caps_free(request);
        return true;
    }
    std::memcpy(out, request->result, result_length + 1);
    heap_caps_free(request);
    return true;
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
    GiacRequest *request = static_cast<GiacRequest *>(
        heap_caps_calloc(1, sizeof(GiacRequest), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (request == nullptr) {
        request = static_cast<GiacRequest *>(
            heap_caps_calloc(1, sizeof(GiacRequest), MALLOC_CAP_8BIT));
    }
    if (request == nullptr) return;
    request->type = RequestType::Reset;
    (void)submit_request(request);
    heap_caps_free(request);
}

#else

extern "C" bool opencalc_giac_eval(const char *, bool, char *, size_t)
{
    return false;
}

extern "C" void opencalc_giac_reset(void)
{
}

extern "C" int opencalc_giac_self_test(void)
{
    return -1;
}

#endif
