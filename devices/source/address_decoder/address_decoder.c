#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include "common/dll_header.h"
#include "common/logging.c"

#define BUFFER_SIZE         1024
#define LOG_FILE            "address_decoder.log"
#define DEVICE_DATA_FILE    "data/address_decoder.bin"

typedef uint8_t(mem_read_func_t) (uint32_t);            // param: address, ret_val: read value
typedef void   (mem_write_func_t)(uint32_t, uint8_t);   // params: address, value to write

typedef struct {
    uint32_t MEM_SIZE;
} device_regs_t;

device_regs_t regs;
uint8_t *memory;

DLL_PREFIX
uint8_t memory_read(uint32_t address) {
    if(address < regs.MEM_SIZE) {
        uint8_t val = memory[address];
        // mylog("MEMORY", "Memory read: %d => %d (%c)\n", address, val, val);
        return val;
    } else {
        RAISE("Error: Attempting to read from outside of memory! Addr: %d\n", address);
    }
}

DLL_PREFIX
void memory_write(uint32_t address, uint8_t val) {
    // mylog("MEMORY", "Memory write: %d <= %d (%c)\n", address, val, val);
    if(address < regs.MEM_SIZE) {
        memory[address] = val;
    } else {
        RAISE("Error: Attempting to write outside of memory! Addr: %d\n", address);
    }
}

DLL_PREFIX
void memory_map_array(uint32_t offset, uint32_t size, const uint8_t *data) {
    if((offset+size) > regs.MEM_SIZE) {
        RAISE("Error: Attempting to write data outside of the memory!\n");
    }
    for(uint32_t i=0; i<size; i++) {
        memory[offset+i] = data[i];
    }
}

DLL_PREFIX
void module_init(uint32_t mem_size) {
    regs.MEM_SIZE = mem_size;
    if(memory != NULL) free(memory);
    mylog("MEMORY", "Memory: Allocating %d bytes\n", regs.MEM_SIZE);
    fflush(stdout);
    memory = (uint8_t*)calloc(regs.MEM_SIZE, 1);
}

DLL_PREFIX
void module_reset(void) {
    memset(&regs, 0, sizeof(device_regs_t));
}
