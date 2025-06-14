#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "brainfuck_cpu/brainfuck_cpu.h"
#include "lib/utils.h"

#define MEMORY_SIZE 128

char p_memory[] = "++++[->,++++.----<]";

uint8_t d_memory[128] = {0};

uint8_t mem_read(uint32_t addr) {
    uint8_t val;
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
    return val;
}

void mem_write(uint32_t addr, uint8_t val) {
    if(addr >= MEMORY_SIZE) {
        printf("Error: Attempting to write to outside the d_memory! Address = %d\n", addr);
        exit(EXIT_FAILURE);
    }
    d_memory[addr] = val;
}

#define NUM_INPUTS 4

uint8_t io_inputs[NUM_INPUTS] = {5, 7, 123, 0};
uint8_t io_in_ptr = 0;
uint8_t io_outputs[NUM_INPUTS] = {0};
uint8_t io_out_ptr = 0;

uint8_t io_read(uint32_t addr) {
    uint8_t val = io_inputs[io_in_ptr];
    printf("Reading io addr = %d (%d), val = %d\n", addr, io_in_ptr, val);
    io_in_ptr += 1;
    return val;
}

void io_write(uint32_t addr, uint8_t val) {
    printf("Writing io addr = %d (%d), val = %d\n", io_out_ptr, addr, val);
    io_outputs[io_out_ptr] = val;
    io_out_ptr +=1;
}

int main(void) {
    init();         // Init CPU
    set_mem_read_func(mem_read);
    set_mem_write_func(mem_write);
    set_io_read_func(io_read);
    set_io_write_func(io_write);
    set_log_level(4);

    uint32_t counter = 0;
    while(counter++ < 100) {
        if(module_tick(counter)) break;
    }
    if(counter >= 66) {
        printf("Error: counter overflow");
        return EXIT_FAILURE;
    }
    for(uint32_t i=0; i<NUM_INPUTS; i++) {
        if(io_outputs[i] != (4 + io_inputs[i])) {
            printf("Error: output value is not correct!\n");
            printf("i = %d, input = %d, output = %d\n", i, io_inputs[i], io_outputs[i]);
            return EXIT_FAILURE;
        }
    }
    // Save original values:
    uint8_t reg_ip_orig = read_register(REGISTER_IP);
    uint8_t reg_dp_orig = read_register(REGISTER_DP);
    module_save(BCKP_DIR_PATH "/brainfuck_cpu_bckp.bin");
    // Modify registers:
    uint8_t regs_test_values[2] = {17, 128};
    write_register(REGISTER_IP, regs_test_values[0]);
    write_register(REGISTER_DP, regs_test_values[1]);
    uint8_t reg_ip_new = read_register(REGISTER_IP);
    uint8_t reg_dp_new = read_register(REGISTER_DP);
    if((reg_ip_new != regs_test_values[0]) || (reg_dp_new != regs_test_values[1])) {
        RAISE("ERROR: write register function doesn't work!");
    }
    // Restore original values
    module_restore(BCKP_DIR_PATH "/brainfuck_cpu_bckp.bin");
    uint8_t reg_ip_last = read_register(REGISTER_IP);
    uint8_t reg_dp_last = read_register(REGISTER_DP);
    if((reg_ip_last != reg_ip_orig) || (reg_dp_last != reg_dp_orig)) {
        RAISE("ERROR: restore function doesn't work!");
    }
    return EXIT_SUCCESS;
}
