#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include "z80_cpu.h"
#include "lib/utils.h"
#include "common/dll_header.h"
#include "common/logging.c"
#include "common/memory_header.h"
#include "common/cpu_memory_iface.c"
#include "common/save-restore.c"

#define STARTUP_ADDR 0x0066

#define LOG_FILE            "cpu.log"
#define DEVICE_DATA_FILE    "data/z80_cpu.bin"

typedef struct {
    // Main register set:
    uint8_t *A;
    uint8_t *B;
    uint8_t *C;
    uint8_t *D;
    uint8_t *E;
    uint8_t *F;
    uint8_t *H;
    uint8_t *L;
    // Alternative register set:
    uint8_t *A_alt;
    uint8_t *B_alt;
    uint8_t *C_alt;
    uint8_t *D_alt;
    uint8_t *E_alt;
    uint8_t *F_alt;
    uint8_t *H_alt;
    uint8_t *L_alt;
    // Special purpose registers:
    uint8_t *I;     // Interrupt
    uint8_t *R;     // Memory refresh
    uint16_t *IX;   // Index register
    uint16_t *IY;   // Index register
    uint16_t *SP;   // Stack pointer
    uint16_t *PC;   // Program counter
} device_regs_t;

device_regs_t regs = {
    .A=register_file,
    .B=register_file+1,
    .C=register_file+2,
    .D=register_file+3,
    .E=register_file+4,
    .F=register_file+5,
    .H=register_file+6,
    .L=register_file+7,
    .A_alt=register_file+8,
    .B_alt=register_file+9,
    .C_alt=register_file+10,
    .D_alt=register_file+11,
    .E_alt=register_file+12,
    .F_alt=register_file+13,
    .H_alt=register_file+14,
    .L_alt=register_file+15,
    .I=register_file+16,
    .R=register_file+17,

    .IX=(uint16_t *)(register_file+18),
    .IY=(uint16_t *)(register_file+20),
    .SP=(uint16_t *)(register_file+22),
    .PC=(uint16_t *)(register_file+24),
};

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
    return 0;
}

int process_command(void) {
    // uint8_t cmd = cpu_iface.mem_read(regs.IP);
    // mylog(4, LOG_FILE, "cmd = %c, data = 0x%X, IP = %d, DP = %d\n", cmd, data, regs.IP, regs.DP);
    // int32_t ip_inc = 1;
    // switch(cmd) {
    //     default:
    //         return EXIT_FAILURE;
    // }
    // regs.IP += ip_inc;
    return EXIT_SUCCESS;
}

DLL_PREFIX
int module_tick(uint32_t ticks) {
    mylog(4, LOG_FILE, "counter = %d\n", ticks);
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
