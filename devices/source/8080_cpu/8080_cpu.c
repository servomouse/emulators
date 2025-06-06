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
#include "common/cpu_memory_iface.c"

#define STARTUP_ADDR 0x0066

#define LOG_FILE "cpu.log"
#define DEVICE_DATA_FILE    "data/z80_cpu.bin"

uint8_t register_file[16];

typedef struct {
    uint32_t IP;
    uint32_t DP;
} device_regs_t;

device_regs_t regs;

#include "common/save-restore.c"

typedef enum {
    REGA = 0b111,
    REGB = 0b000,
    REGC = 0b001,
    REGD = 0b010,
    REGE = 0b011,
    REGH = 0b100,
    REGL = 0b101
} cpu_reg_t;

uint32_t LD(uint8_t opcode) {
    switch(opcode & 0xC0) {
        case 0x00:
            break;
        case 0x40:  // LD R, R'
            break;
        case 0x80:
            break;
        case 0xC0:
            break;
    }
}

int process_command(void) {
    uint8_t cmd = cpu_iface.mem_read(regs.IP);
    mylog(4, "CPU", "cmd = %c, data = 0x%X, IP = %d, DP = %d\n", cmd, data, regs.IP, regs.DP);
    int32_t ip_inc = 1;
    switch(cmd) {
        case '>':   // Increment DP
            regs.DP ++;
            break;
        case '<':   // Decrement DP
            regs.DP --;
            break;
        case '+':   // Increment memory[DP]
            cpu_iface.mem_write(DATA_OFFSET+regs.DP, data+1);
            break;
        case '-':   // Decrement memory[DP]
            cpu_iface.mem_write(DATA_OFFSET+regs.DP, data-1);
            break;
        case '.':   // Print memory[DP]
            cpu_iface.io_write(0, data);
            break;
        case ',':   // Read byte into memory[DP]
            cpu_iface.mem_write(DATA_OFFSET+regs.DP, cpu_iface.io_read(0));
            break;
        case '[':   // If memory[DP] == 0 => jump to the matching ']', otherwise increment IP
            if(data == 0) {
                ip_inc = find_close_bracket();
            }
            break;
        case ']':   // If memory[DP] != 0 => jump to the matching '[', otherwise increment IP
            if(data != 0) {
                ip_inc = find_open_bracket();
            }
            break;
        default:
            return EXIT_FAILURE;
    }
    regs.IP += ip_inc;
    return EXIT_SUCCESS;
}

DLL_PREFIX
int module_tick(uint32_t ticks) {
    mylog(4, "CPU", "counter = %d\n", ticks);
    return process_command();
}

DLL_PREFIX
void init(void) {
    cpu_iface.mem_read = NULL;
    cpu_iface.mem_write = NULL;
    cpu_iface.io_read = NULL;
    cpu_iface.io_write = NULL;
}

DLL_PREFIX
void module_reset(void) {
    memset(&regs, 0, sizeof(device_regs_t));
}

int main(void) {
    printf("Simple CPU main");
    return EXIT_SUCCESS;
}

// DLL_PREFIX
// void module_save(void) {
//     store_data(&regs, sizeof(device_regs_t), DEVICE_DATA_FILE);
// }

// DLL_PREFIX
// void module_restore(void) {
//     device_regs_t data;
//     if(EXIT_SUCCESS == restore_data(&data, sizeof(device_regs_t), DEVICE_DATA_FILE)) {
//         memcpy(&regs, &data, sizeof(device_regs_t));
//     }
// }
