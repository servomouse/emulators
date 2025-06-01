#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include "common/dll_header.h"
#include "common/logging.c"
#include "common/memory_header.h"

#define LOG_FILE            "address_decoder.log"
#define DEVICE_DATA_FILE    "data/address_decoder.bin"
#define DEVICE_MAP_SIZE 128

typedef struct {
    mem_read_func_t *read;
    mem_write_func_t *write;
    uint32_t range[2];
} device_map_t;

device_map_t device_map[DEVICE_MAP_SIZE];

uint32_t find_device_idx(uint32_t addr) {
    for(uint32_t i=0; i<DEVICE_MAP_SIZE; i++) {
        if(device_map[i].range[0] <= addr && device_map[i].range[1] >= addr) {
            return i;
        }
    }
    RAISE("Error: Couldn't find device for a given address (0x%X)!\n", addr);
}

DLL_PREFIX
uint8_t memory_read(uint32_t address) {
    uint32_t idx = find_device_idx(address);
    uint32_t offset = device_map[idx].range[0];
    return device_map[idx].read(address-offset);
}

DLL_PREFIX
void memory_write(uint32_t address, uint8_t val) {
    uint32_t idx = find_device_idx(address);
    uint32_t offset = device_map[idx].range[0];
    device_map[idx].write(address-offset, val);
}

DLL_PREFIX
uint32_t memory_map_device(uint32_t addr_start, uint32_t addr_end, mem_read_func_t *read_func, mem_write_func_t *write_func) {
    for(uint32_t i=0; i<DEVICE_MAP_SIZE; i++) {
        if(device_map[i].range[0] == 0 && device_map[i].range[1] == 0) {
            device_map[i].read = read_func;
            device_map[i].write = write_func;
            device_map[i].range[0] = addr_start;
            device_map[i].range[1] = addr_end;
            return i;
        }
    }
    RAISE("Error: Couldn't map device: table is full\n");
}

DLL_PREFIX
void module_init(void) {
    for(uint32_t i=0; i<DEVICE_MAP_SIZE; i++) {
        device_map[i].read = NULL;
        device_map[i].write = NULL;
        device_map[i].range[0] = 0;
        device_map[i].range[1] = 0;
    }
}

DLL_PREFIX
void module_reset(void) {
    ;
}
