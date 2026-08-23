#pragma once

#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FILE_READ "rb"
#define FILE_WRITE "wb"

class File {
public:
    File() = default;
    explicit File(FILE *fp) : fp_(fp) {}
    ~File() { close(); }

    File(const File&) = delete;
    File& operator=(const File&) = delete;

    File(File&& other) noexcept : fp_(other.fp_)
    {
        other.fp_ = nullptr;
    }

    File& operator=(File&& other) noexcept
    {
        if (this != &other) {
            close();
            fp_ = other.fp_;
            other.fp_ = nullptr;
        }
        return *this;
    }

    explicit operator bool() const { return fp_ != nullptr; }

    size_t read(uint8_t *buf, size_t size)
    {
        return fp_ ? fread(buf, 1, size, fp_) : 0;
    }

    size_t write(const uint8_t *buf, size_t size)
    {
        return fp_ ? fwrite(buf, 1, size, fp_) : 0;
    }

    size_t write(const char *buf, size_t size)
    {
        return write((const uint8_t *)buf, size);
    }

    size_t print(const char *text)
    {
        return text ? write(text, strlen(text)) : 0;
    }

    bool seek(uint32_t pos)
    {
        return fp_ && fseek(fp_, (long)pos, SEEK_SET) == 0;
    }

    uint32_t position()
    {
        if (!fp_) {
            return 0;
        }
        long pos = ftell(fp_);
        return pos < 0 ? 0 : (uint32_t)pos;
    }

    void close()
    {
        if (fp_) {
            fclose(fp_);
            fp_ = nullptr;
        }
    }

private:
    FILE *fp_ = nullptr;
};

class SDClass {
public:
    File open(const char *path, const char *mode)
    {
        return File(fopen(path, mode));
    }

    bool exists(const char *path)
    {
        struct stat st;
        return stat(path, &st) == 0;
    }

    bool mkdir(const char *path)
    {
        return ::mkdir(path, 0775) == 0 || exists(path);
    }
};

extern SDClass SD;
