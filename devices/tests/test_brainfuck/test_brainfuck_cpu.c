#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "brainfuck_cpu/brainfuck_cpu.h"

#define MEMORY_SIZE 128

char p_memory[] = "++++[->,++++.----<]";

uint8_t d_memory[128] = {0};

uint8_t mem_read(uint32_t addr) {
    uint8_t is_p_mem = 0, val;
    if(addr >= PROGRAM_OFFSET) {
        addr -= PROGRAM_OFFSET;
        if(addr >= sizeof(p_memory)) {
            printf("Error: Attempting to read from outside the p_memory! Address = %d\n", addr);
            exit(EXIT_FAILURE);
        }
        val = p_memory[addr];
    } else {
        if(addr >= MEMORY_SIZE) {
            printf("Error: Attempting to read from outside the d_memory! Address = %d\n", addr);
            exit(EXIT_FAILURE);
        }
        val = d_memory[addr];
    }
    // printf("Reading memory addr = %d, val = %d\n", addr, val);
    return val;
}

void mem_write(uint32_t addr, uint8_t val) {
    // printf("Writing memory addr = %d, val = %d\n", addr, val);
    if(addr >= MEMORY_SIZE) {
        printf("Error: Attempting to write to outside the d_memory! Address = %d\n", addr);
        exit(EXIT_FAILURE);
    }
    d_memory[addr] = val;
}

uint8_t io_inputs[] = {5, 7, 123, 0};
uint8_t io_in_ptr = 0;
uint8_t io_outputs[4] = {0};
uint8_t io_out_ptr = 0;

uint8_t io_read(uint32_t addr) {
    uint8_t val = io_inputs[io_in_ptr];
    printf("Reading io addr = %d, val = %d\n", io_in_ptr, val);
    io_in_ptr += 1;
    return val;
}

void io_write(uint32_t addr, uint8_t val) {
    printf("Writing io addr = %d, val = %d (%d)\n", io_out_ptr, val, &io_out_ptr);
    io_outputs[io_out_ptr] = val;
    io_out_ptr +=1;
}

int main(void) {
    init(); // Init CPU
    set_mem_read_func(mem_read);
    set_mem_write_func(mem_write);
    set_io_read_func(io_read);
    set_io_write_func(io_write);
    set_log_level(4);

    uint32_t counter = 0;
    while(counter++ < 1000) {
        if( module_tick(counter)) break;
    }
    if(counter >= 1000) {
        printf("Error: counter overflow");
        return EXIT_FAILURE;
    }
    for(uint32_t i=0; i<sizeof(io_inputs); i++) {
        printf("i = %d, input = %d, output = %d\n", i, io_inputs[i], io_outputs[i]);
    }
    return EXIT_SUCCESS;
}
