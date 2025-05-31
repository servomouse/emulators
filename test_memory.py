import ctypes
# import log_manager
import copy
import os
import glob
import zipfile
from datetime import datetime
import random

import platform
os_type = platform.system()
if os_type == 'Windows':
    import pefile   # pip install pefile
elif os_type == 'Linux':
    from elftools.elf.elffile import ELFFile


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


def print_logs(filename, logstring):
    print(logstring)


print_callback_t = ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_char_p)


class CDevice:
    def __init__(self, filename):
        self.filename = filename
        self.device = ctypes.CDLL(filename)

    def set_func_pointer(self, setter_name, func_type, func):
        callback = func_type(func)
        setattr(self, f"{setter_name}_type", callback)
        setter = getattr(self.device, setter_name)
        setattr(setter, "argtypes", [func_type])
        setattr(setter, "restype", None)
        setter(callback)


class Memory(CDevice):

    def __init__(self, filename, memory_size):
        self.mem_size = memory_size
        super().__init__(filename)
        # Log output function
        self.set_func_pointer("set_log_func", print_callback_t, print_logs)



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
    
    def memory_map_file(self, filename):
        with open("bf_hello_world.txt") as f:
            data = f.read()
        self.memory_map_array(0, len(data), data.encode())


memory = Memory("Devices/bin/libmemory.dll", 512)
# for i in range(100):
#     addr = random.randint(0, 128)
#     val = random.randint(0, 128)
#     memory.memory_write(addr, val)
#     rval = memory.memory_read(addr)
#     print(f"Addr {addr}: {val = }, {rval = }")

memory.memory_map_file("bf_hello_world.txt")
for i in range(106):
    rval = memory.memory_read(i)
    print(chr(rval))
