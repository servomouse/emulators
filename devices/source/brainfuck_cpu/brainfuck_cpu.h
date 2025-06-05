#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "common/memory_header.h"
#include "common/dll_header.h"

#define PROGRAM_OFFSET  0x8000
#define DATA_OFFSET     0x0000

#define REGISTER_IP 0
#define REGISTER_DP 1

DLL_PREFIX void set_mem_read_func(mem_read_func_t *foo);
DLL_PREFIX void set_mem_write_func(mem_write_func_t *foo);
DLL_PREFIX void set_io_read_func(mem_read_func_t *foo);
DLL_PREFIX void set_io_write_func(mem_write_func_t *foo);
DLL_PREFIX int module_tick(uint32_t ticks);
DLL_PREFIX void init(void);

DLL_PREFIX void set_log_level(uint8_t new_val);

DLL_PREFIX uint8_t read_register(uint32_t idx);
DLL_PREFIX void write_register(uint32_t idx, uint8_t value);

DLL_PREFIX void module_save(char * PATH);
DLL_PREFIX void module_restore(char * PATH);
