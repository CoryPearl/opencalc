/*
 * OpenCalc C++ allocations prefer PSRAM when the Giac backend is enabled.
 * Giac uses standard containers heavily; leaving those allocations in DRAM
 * exhausts the ESP32-S3 internal heap before useful CAS work can complete.
 */

#include "opencalc_config.h"

#if OPENCALC_ENABLE_GIAC_CAS

#include <cstddef>
#include <cstdlib>
#include <new>

#include "esp_heap_caps.h"

namespace {

void *allocate(size_t size)
{
    if (size == 0) size = 1;
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    void *pointer = size <= psram_free &&
                            psram_free - size >= OPENCALC_PSRAM_RESERVE_BYTES
                        ? heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
                        : nullptr;
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (pointer == nullptr && size <= internal_free &&
        internal_free - size >= OPENCALC_INTERNAL_HEAP_RESERVE_BYTES) {
        pointer = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return pointer;
}

void *allocate_aligned(size_t alignment, size_t size)
{
    if (size == 0) size = 1;
    if (alignment < sizeof(void *)) alignment = sizeof(void *);
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    void *pointer = size <= psram_free &&
                            psram_free - size >= OPENCALC_PSRAM_RESERVE_BYTES
                        ? heap_caps_aligned_alloc(
                              alignment, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
                        : nullptr;
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (pointer == nullptr && size <= internal_free &&
        internal_free - size >= OPENCALC_INTERNAL_HEAP_RESERVE_BYTES) {
        pointer = heap_caps_aligned_alloc(
            alignment, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return pointer;
}

} // namespace

void *operator new(size_t size)
{
    void *pointer = allocate(size);
    if (pointer == nullptr) throw std::bad_alloc();
    return pointer;
}

void *operator new[](size_t size)
{
    void *pointer = allocate(size);
    if (pointer == nullptr) throw std::bad_alloc();
    return pointer;
}

void operator delete(void *pointer) noexcept { std::free(pointer); }
void operator delete[](void *pointer) noexcept { std::free(pointer); }
void operator delete(void *pointer, size_t) noexcept { std::free(pointer); }
void operator delete[](void *pointer, size_t) noexcept { std::free(pointer); }

void *operator new(size_t size, std::align_val_t alignment)
{
    void *pointer = allocate_aligned(static_cast<size_t>(alignment), size);
    if (pointer == nullptr) throw std::bad_alloc();
    return pointer;
}

void *operator new[](size_t size, std::align_val_t alignment)
{
    void *pointer = allocate_aligned(static_cast<size_t>(alignment), size);
    if (pointer == nullptr) throw std::bad_alloc();
    return pointer;
}

void operator delete(void *pointer, std::align_val_t) noexcept { std::free(pointer); }
void operator delete[](void *pointer, std::align_val_t) noexcept { std::free(pointer); }
void operator delete(void *pointer, size_t, std::align_val_t) noexcept { std::free(pointer); }
void operator delete[](void *pointer, size_t, std::align_val_t) noexcept { std::free(pointer); }

void *operator new(size_t size, const std::nothrow_t &) noexcept
{
    return allocate(size);
}

void *operator new[](size_t size, const std::nothrow_t &) noexcept
{
    return allocate(size);
}

void *operator new(size_t size, std::align_val_t alignment,
                   const std::nothrow_t &) noexcept
{
    return allocate_aligned(static_cast<size_t>(alignment), size);
}

void *operator new[](size_t size, std::align_val_t alignment,
                     const std::nothrow_t &) noexcept
{
    return allocate_aligned(static_cast<size_t>(alignment), size);
}

void operator delete(void *pointer, const std::nothrow_t &) noexcept { std::free(pointer); }
void operator delete[](void *pointer, const std::nothrow_t &) noexcept { std::free(pointer); }
void operator delete(void *pointer, std::align_val_t,
                     const std::nothrow_t &) noexcept { std::free(pointer); }
void operator delete[](void *pointer, std::align_val_t,
                       const std::nothrow_t &) noexcept { std::free(pointer); }

#endif
