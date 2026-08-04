#pragma once

#include <stdbool.h>

void init_usb_msc(void);
bool usb_msc_mount_app(void);
bool usb_msc_mount_usb(void);
