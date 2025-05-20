#include <stdlib.h>
#include <stdio.h>
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

typedef void(*log_func_t)(const char*, char*);
log_func_t print_log;

DLL_PREFIX
void set_log_func(log_func_t python_log_func) {
    print_log = python_log_func;
}

// The highter log_level the highter priority
void mylog(const char *log_file, const char *format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[BUFFER_SIZE] = {0};
    vsnprintf(buffer, sizeof(buffer), format, args);
    print_log(log_file, buffer);
    va_end(args);
}

DLL_PREFIX
void tick(void) {
    ;
}

int main(void) {
    printf("Simple CPU main");
    return EXIT_SUCCESS;
}
