#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include "Python.h" // Needed to generate python exceptions
// #include "utils.h"

#ifdef __unix__
    #define DLL_PREFIX 
#elif defined(_WIN32) || defined(WIN32)
    #define DLL_PREFIX __declspec(dllexport)
#endif

#define PROGRAM_OFFSET  0x8000
#define DATA_OFFSET     0x0000

#define BUFFER_SIZE 256
#define LOG_FILE "cpu.log"
#define DEVICE_DATA_FILE    "data/cga.bin"

// #define RAISE(msg, ...) PyErr_Format(PyExc_RuntimeError, msg " (line %d in file %s)\n", ##__VA_ARGS__, __LINE__, __FILE__)
#define RAISE(msg, ...) printf(msg " (line %d in file %s)\n", ##__VA_ARGS__, __LINE__, __FILE__); exit(EXIT_FAILURE)

typedef void   (log_func_t)      (const char*, char*);
typedef uint8_t(mem_read_func_t) (uint32_t);            // param: address, ret_val: read value
typedef void   (mem_write_func_t)(uint32_t, uint8_t);   // params: address, value to write

typedef struct {
    log_func_t       *log;
    mem_read_func_t  *mem_read;
    mem_write_func_t *mem_write;
    mem_read_func_t  *io_read;
    mem_write_func_t *io_write;
} cpu_iface_t;

typedef struct {
    uint32_t IP;
    uint32_t DP;
} device_regs_t;

device_regs_t regs;

cpu_iface_t cpu_iface;

DLL_PREFIX
void set_log_func(log_func_t *foo) {
    cpu_iface.log = foo;
}

DLL_PREFIX
void set_mem_read_func(mem_read_func_t *foo) {
    cpu_iface.mem_read = foo;
}

DLL_PREFIX
void set_mem_write_func(mem_write_func_t *foo) {
    cpu_iface.mem_write = foo;
}

DLL_PREFIX
void set_io_read_func(mem_read_func_t *foo) {
    cpu_iface.io_read = foo;
}

DLL_PREFIX
void set_io_write_func(mem_write_func_t *foo) {
    cpu_iface.io_write = foo;
}

// The highter log_level the highter priority
void mylog(const char *log_file, const char *format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[BUFFER_SIZE] = {0};
    vsnprintf(buffer, sizeof(buffer), format, args);
    cpu_iface.log(log_file, buffer);
    va_end(args);
}

uint32_t find_close_bracket(void) {
    uint32_t offset = 1;
    uint32_t counter = 1;
    while(counter > 0) {
        uint8_t cmd = cpu_iface.mem_read(PROGRAM_OFFSET+regs.IP+offset);
        if(cmd == ']') counter --;
        else if(cmd == '[') counter ++;
        else if((cmd == '>') || (cmd == '<') || (cmd == '+') || (cmd == '-') || (cmd == '.') || (cmd == ','));
        else {
            RAISE("Error: unknown opcode at 0x%X: %c(0x%02X)\n", regs.IP+offset, cmd, cmd);
        }
        offset ++;
    }
    return offset;
}

int32_t find_open_bracket(void) {
    int32_t offset = -1;
    uint32_t counter = 1;
    while((counter > 0) && (regs.IP > 0)) {
        uint8_t cmd = cpu_iface.mem_read(PROGRAM_OFFSET+regs.IP+offset);
        if(cmd == '[') counter --;
        else if(cmd == ']') counter ++;
        else if((cmd == '>') || (cmd == '<') || (cmd == '+') || (cmd == '-') || (cmd == '.') || (cmd == ','));
        else {
            // printf("Error: unknown opcode at 0x%X: %c(0x%02X)\n", regs.IP+offset, cmd, cmd);
            RAISE("Error: unknown opcode at 0x%X: %c(0x%02X)\n", regs.IP+offset, cmd, cmd);
        }
        offset --;
    }
    return offset;
}

void write_memory(uint32_t addr, uint8_t data) {
    mylog("CPU", "Memory write: addr = %d, data = %d\n", addr, data);
    // fflush(stdout);
    cpu_iface.mem_write(DATA_OFFSET, data);
    // printf("Memory write completed\n");
    // fflush(stdout);
}

int process_command(void) {
    uint8_t cmd = cpu_iface.mem_read(PROGRAM_OFFSET+regs.IP);
    uint8_t data = cpu_iface.mem_read(DATA_OFFSET+regs.DP);
    // write_memory(DATA_OFFSET+10, data+1);
    // return;
    uint32_t ip_inc = 1;
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
            if(data == 0) {
                ip_inc = find_open_bracket();
            }
            break;
        default:
            // RAISE("Error: unknown opcode at 0x%X: %c(0x%02X)\n", regs.IP, cmd, cmd);
            return EXIT_FAILURE;
    }
    regs.IP += ip_inc;
    return EXIT_SUCCESS;
}

DLL_PREFIX
int module_tick(uint32_t ticks) {
    // process_command();
    // fflush(stdout);
    // uint8_t data = cpu_iface.mem_read(DATA_OFFSET+regs.DP);
    // cpu_iface.mem_write(DATA_OFFSET+10, data+1);
    return process_command();
}

DLL_PREFIX
void init(void) {
    cpu_iface.log = NULL;
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
