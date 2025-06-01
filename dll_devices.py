import ctypes


def get_type(item):
    c_types = {
        "void": None,
        "int": ctypes.c_int,
        "uint8_t": ctypes.c_uint8,
        "uint16_t": ctypes.c_uint16,
        "uint32_t": ctypes.c_uint32,
        "char*": ctypes.c_char_p,
    }
    if item in c_types:
        return c_types[item]
    print(f"ERROR: Unknown type: {item}")
    os.exit(1)


def get_dll_function(dll_object, signature: str):
    ''' Signature looks like this: "void foo (int, uint8_t)"
        where "foo" is the actual name of the function '''
    remove_symbols = [',', '(', ')']
    for s in remove_symbols:
        signature = signature.replace(s, ' ')
    items = signature.split()
    res_type = get_type(items[0])
    params = []
    for i in items[2:]:
        params.append(get_type(i))
    foo = getattr(dll_object, items[1])
    if params[0] is None:
        foo.argtypes = None
    else:
        foo.argtypes = params
    foo.restype = res_type
    return foo


class CDevice:
    def __init__(self, filename, dll_loader, logger):
        self.filename = filename
        # self.device = ctypes.CDLL(filename)
        self.device = dll_loader.upload(filename)
        # Log output function
        self.set_func_pointer("set_log_func", logger.callback_type, logger.print)
        self.set_log_level = get_dll_function(self.device, "void set_log_level(uint8_t)")
        self.get_log_level = get_dll_function(self.device, "uint8_t get_log_level(void)")

    def set_func_pointer(self, setter_name, func_type, func):
        callback = func_type(func)
        setattr(self, f"{setter_name}_type", callback)
        setter = getattr(self.device, setter_name)
        setattr(setter, "argtypes", [func_type])
        setattr(setter, "restype", None)
        setter(callback)


class Cpu(CDevice):
    def __init__(self, filename, dll_loader, logger):
        super().__init__(filename, dll_loader, logger)

        self.mem_read_func_t = ctypes.CFUNCTYPE(get_type("uint8_t"), get_type("uint32_t"))
        self.mem_write_func_t = ctypes.CFUNCTYPE(None, get_type("uint32_t"), get_type("uint8_t"))

        # self.set_log_level = get_dll_function(self.device, "void set_log_level(uint8_t)")

        # self.module_reset = get_dll_function(self.device, "void module_reset(void)")
        # self.module_reset()

        # self.module_save = get_dll_function(self.device, "void module_save(void)")
        # self.module_restore = get_dll_function(self.device, "void module_restore(void)")
        self.module_tick = get_dll_function(self.device, "int module_tick(uint32_t)")
    
    def connect_memory(self, memory_iface):
        self.set_func_pointer("set_mem_read_func",  self.mem_read_func_t,  memory_iface["mem_read"])
        self.set_func_pointer("set_mem_write_func", self.mem_write_func_t, memory_iface["mem_write"])
        self.set_func_pointer("set_io_read_func",   self.mem_read_func_t,  memory_iface["io_read"])
        self.set_func_pointer("set_io_write_func",  self.mem_write_func_t, memory_iface["io_write"])


class Memory(CDevice):
    def __init__(self, filename, memory_size, dll_loader, logger):
        self.mem_size = memory_size
        super().__init__(filename, dll_loader, logger)

        # self.set_log_level = get_dll_function(self.device, "void set_log_level(uint8_t)")
        # self.module_reset = get_dll_function(self.device, "void module_reset(void)")
        # self.module_reset()
        # self.module_save = get_dll_function(self.device, "void module_save(void)")
        # self.module_restore = get_dll_function(self.device, "void module_restore(void)")
        self.module_init = get_dll_function(self.device, "void module_init(uint32_t)")
        self.memory_read = get_dll_function(self.device, "uint8_t memory_read(uint32_t)")
        self.memory_write = get_dll_function(self.device, "void memory_write(uint32_t, uint8_t)")
        self.memory_map_array = get_dll_function(self.device, "void memory_map_array(uint32_t, uint32_t, char*)")
        self.module_init(self.mem_size)
    
    def memory_map_file(self, filename, offset):
        with open(filename, 'rb') as f:
            data = f.read()
        self.memory_map_array(offset, len(data), data)


class AddressDecoder(CDevice):
    def __init__(self, filename, dll_loader, logger):
        super().__init__(filename, dll_loader, logger)

        self.mem_read_funcs = []
        self.mem_write_funcs = []

        self.mem_read_func_t = ctypes.CFUNCTYPE(get_type("uint8_t"), get_type("uint32_t"))
        self.mem_write_func_t = ctypes.CFUNCTYPE(None, get_type("uint32_t"), get_type("uint8_t"))

        # self.set_log_level = get_dll_function(self.device, "void set_log_level(uint8_t)")
        # self.module_reset = get_dll_function(self.device, "void module_reset(void)")
        # self.module_reset()
        # self.module_save = get_dll_function(self.device, "void module_save(void)")
        # self.module_restore = get_dll_function(self.device, "void module_restore(void)")
        self.module_init = get_dll_function(self.device, "void module_init(void)")
        self.memory_read = get_dll_function(self.device, "uint8_t memory_read(uint32_t)")
        self.memory_write = get_dll_function(self.device, "void memory_write(uint32_t, uint8_t)")

        self.device.memory_map_device.argtypes = [
            get_type("uint32_t"),
            get_type("uint32_t"),
            self.mem_read_func_t,
            self.mem_write_func_t]
        self.device.memory_map_device.restype = get_type("uint32_t")
        # self.device.memory_map_device(log_manager.print_callback)
    
    def memory_map_device(self, device, addr_range):
        self.mem_read_funcs.append(self.mem_read_func_t(device.memory_read))
        self.mem_write_funcs.append(self.mem_write_func_t(device.memory_write))
        self.device.memory_map_device(addr_range[0], addr_range[1], self.mem_read_funcs[-1], self.mem_write_funcs[-1])
        # self.device.memory_map_device(addr_range[0], addr_range[1], device.memory_read, device.memory_write)
        # self.set_func_pointer("set_mem_read_func",  self.mem_read_func_t,  memory_iface["mem_read"])
