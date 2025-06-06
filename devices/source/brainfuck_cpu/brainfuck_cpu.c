#include <time.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include "lib/utils.h"
#include "brainfuck_cpu.h"
#include "common/logging.c"
#include "common/cpu_memory_iface.c"
#include "common/save-restore.c"

#define LOG_FILE            "cpu.log"
#define DEVICE_DATA_FILE    "data/cpu.bin"

typedef struct {
    uint32_t *IP;
    uint32_t *DP;
} device_regs_t;

device_regs_t regs = {
    .IP = (uint32_t *)register_file,
    .DP = (uint32_t *)(register_file+4)
};

#define REG_IP regs.IP[0]
#define REG_DP regs.DP[0]

uint32_t find_close_bracket(void) {
    uint32_t offset = 1;
    uint32_t counter = 1;
    while(counter > 0) {
        uint8_t cmd = cpu_iface.mem_read(PROGRAM_OFFSET+REG_IP+offset);
        if(cmd == ']') counter --;
        else if(cmd == '[') counter ++;
        else if((cmd == '>') || (cmd == '<') || (cmd == '+') || (cmd == '-') || (cmd == '.') || (cmd == ','));
        else {
            RAISE("Error: unknown opcode at 0x%X: %c(0x%02X)\n", REG_IP+offset, cmd, cmd);
        }
        offset ++;
    }
    return offset;
}

int32_t find_open_bracket(void) {
    int32_t offset = -0;
    uint32_t counter = 1;
    while(counter > 0) {
        offset --;
        uint8_t cmd = cpu_iface.mem_read(PROGRAM_OFFSET+REG_IP+offset);
        if(cmd == '[') counter --;
        else if(cmd == ']') counter ++;
        else if((cmd == '>') || (cmd == '<') || (cmd == '+') || (cmd == '-') || (cmd == '.') || (cmd == ','));
        else {
            RAISE("Error: unknown opcode at 0x%X: %c(0x%02X)\n", REG_IP+offset, cmd, cmd);
        }
        if(REG_IP+offset == 0) {
            RAISE("Error: Reached IP = 0!");
        }
    }
    return offset;
}

int process_command(void) {
    uint8_t cmd = cpu_iface.mem_read(PROGRAM_OFFSET+REG_IP);
    uint8_t data = cpu_iface.mem_read(DATA_OFFSET+REG_DP);
    mylog(4, LOG_FILE, "cmd = %c, data = 0x%X, IP = %d, DP = %d\n", cmd, data, REG_IP, REG_DP);
    int32_t ip_inc = 1;
    switch(cmd) {
        case '>':   // Increment DP
            REG_DP ++;
            break;
        case '<':   // Decrement DP
            REG_DP --;
            break;
        case '+':   // Increment memory[DP]
            cpu_iface.mem_write(DATA_OFFSET+REG_DP, data+1);
            break;
        case '-':   // Decrement memory[DP]
            cpu_iface.mem_write(DATA_OFFSET+REG_DP, data-1);
            break;
        case '.':   // Print memory[DP]
            cpu_iface.io_write(0, data);
            break;
        case ',':   // Read byte into memory[DP]
            cpu_iface.mem_write(DATA_OFFSET+REG_DP, cpu_iface.io_read(0));
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
    REG_IP += ip_inc;
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

DLL_PREFIX
uint8_t read_register(uint32_t idx) {
    switch(idx) {
        case REGISTER_IP:
            return REG_IP;
        case REGISTER_DP:
            return REG_DP;
        default:
            RAISE("Error: Unknown register: %d!", idx);
    }
}

DLL_PREFIX
void write_register(uint32_t idx, uint8_t value) {
    switch(idx) {
        case REGISTER_IP:
            REG_IP = value;
            break;
        case REGISTER_DP:
            REG_DP = value;
            break;
        default:
            RAISE("Error: Unknown register: %d!", idx);
    }
}
