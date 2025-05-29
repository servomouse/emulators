#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>
// #include "utils.h"

#ifdef __unix__
    #define DLL_PREFIX 
#elif defined(_WIN32) || defined(WIN32)
    #define DLL_PREFIX __declspec(dllexport)
#endif

#define BUFFER_SIZE 256
#define LOG_FILE "cpu.log"

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

DLL_PREFIX
void tick(void) {
    static uint32_t counter = 0;
    static uint32_t mem_val = 0;
    static uint32_t io_val = 0;
    switch(counter) {
        case 0:
            mem_val = cpu_iface.mem_read(0x10);
            mylog(LOG_FILE, "Reading memory addr 0x10: %d", mem_val);
            break;
        case 1:
            cpu_iface.mem_write(0x20, mem_val);
            mylog(LOG_FILE, "Writing %d to memory address 0x20", mem_val);
            break;
        case 3:
            io_val = cpu_iface.io_read(0x30);
            mylog(LOG_FILE, "Reading io addr 0x30: %d", io_val);
            break;
        case 4:
            cpu_iface.io_write(0x40, io_val);
            mylog(LOG_FILE, "Writing %d to io address 0x40", io_val);
            break;
    }
    counter ++;
    if(counter == 5) {
        counter = 0;
    }
}

DLL_PREFIX
void init(void) {
    cpu_iface.log = NULL;
    cpu_iface.mem_read = NULL;
    cpu_iface.mem_write = NULL;
    cpu_iface.io_read = NULL;
    cpu_iface.io_write = NULL;
}

int main(void) {
    printf("Simple CPU main");
    return EXIT_SUCCESS;
}
