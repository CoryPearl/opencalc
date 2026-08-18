#pragma once

#include <stdio.h>

#define LOG(msg) printf("%s\n", msg)
#define LOGF(...) printf(__VA_ARGS__)

