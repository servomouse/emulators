typedef struct {
    mem_read_func_t  *mem_read;
    mem_write_func_t *mem_write;
    mem_read_func_t  *io_read;
    mem_write_func_t *io_write;
} cpu_iface_t;

cpu_iface_t cpu_iface;

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
