/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright (C) 2021 Intel Corporation
 *                    Paweł Marczewski <pawel@invisiblethingslab.com>
 */

#include "init.h"

/*
 * Helper symbols for accessing the `.init_array` section at run time. These are part of default
 * linker script (you can see it by running `ld --verbose`) as well as our custom linker scripts.
 *
 * The `visibility("hidden")` annotation makes the compiler use RIP-relative addressing with known
 * offset (not a reference to GOT) when accessing these variables
 *
 * NOTE: We rely on the fact that each ELF object (PAL, LibOS) contains its own copy of this module.
 */
extern void (*__init_array_start)(void) __attribute__((visibility("hidden")));
extern void (*__init_array_end)(void) __attribute__((visibility("hidden")));

void call_init_array(void) {
    void (**func)(void);
    for (func = &__init_array_start; func < &__init_array_end; func++) {
        (*func)();
    }
}
