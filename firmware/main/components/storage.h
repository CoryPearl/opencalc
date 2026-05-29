#pragma once

#include "wear_levelling.h"

void init_storage(void);
void storage_set_label(void);
wl_handle_t storage_wl_handle(void);
