#include <stdlib.h>

#define BUFFER_SIZE 1024

// #define RAISE(msg, ...) PyErr_Format(PyExc_RuntimeError, msg " (line %d in file %s)\n", ##__VA_ARGS__, __LINE__, __FILE__)
#define RAISE(msg, ...) printf(msg " (line %d in file %s)\n", ##__VA_ARGS__, __LINE__, __FILE__); fflush(stdout); exit(EXIT_FAILURE)

typedef void (log_func_t) (const char*, char*);
log_func_t *log_func;
uint8_t device_log_level = 1;

DLL_PREFIX
void set_log_func(log_func_t *foo) {
    log_func = foo;
}

DLL_PREFIX
void set_log_level(uint8_t new_val) {
    device_log_level = new_val;
}

DLL_PREFIX
uint8_t get_log_level(void) {
    return device_log_level;
}

// The higher log level the lower priority
void mylog(uint8_t log_level, const char *log_file, const char *format, ...) {
    if(log_level > device_log_level) return;
    va_list args;
    va_start(args, format);
    char buffer[BUFFER_SIZE] = {0};
    vsnprintf(buffer, sizeof(buffer), format, args);
    log_func(log_file, buffer);
    va_end(args);
}
