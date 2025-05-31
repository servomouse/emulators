import ctypes
# import log_manager
import copy
import os
import glob
import zipfile
from datetime import datetime

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
    # signature = signature.replace('(', ' ')
    # signature = signature.replace(')', ' ')
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


# memory = [0 for _ in range(0xFFFF)]

def mem_read(addr):
    # val = memory[addr]
    val = 82
    print(f"Reading memory {val = } from {addr = }")
    return val

def mem_write(addr, val):
    print(f"Writing memory {val = } to {addr = }")
    # memory[addr] = val


# io = [0 for _ in range(0xFFFF)]

def io_read(addr):
    # val = io[addr]
    val = 42
    print(f"Reading io {val = } from {addr = }")
    return val

def io_write(addr, val):
    print(f"Writing io {val = } to {addr = }")
    # io[addr] = val


def print_logs(filename, logstring):
    print(logstring)


print_callback_t = ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_char_p)
# print_callback = print_callback_t(print_logs)
mem_read_func_t = ctypes.CFUNCTYPE(get_type("uint8_t"), get_type("uint32_t"))
mem_write_func_t = ctypes.CFUNCTYPE(None, get_type("uint32_t"), get_type("uint8_t"))


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


class Cpu(CDevice):

    def __init__(self, filename):
        super().__init__(filename)
        # Log output function
        # self.device.set_log_func.argtypes = [print_callback_t]
        # self.device.set_log_func.restype = None
        # self.device.set_log_func(print_callback)
        self.set_func_pointer("set_log_func", print_callback_t, print_logs)
        # # Memory read function
        # self.device.set_mem_read_func.argtypes = [mem_read_func_t]
        # self.device.set_mem_read_func.restype = None
        # self.device.set_mem_read_func(mem_read_func_t(mem_read))
        # # Memory write function
        # self.device.set_mem_write_func.argtypes = [mem_write_func_t]
        # self.device.set_mem_write_func.restype = None
        # self.device.set_mem_write_func(mem_write_func_t(mem_write))
        # # IO read function
        # self.device.set_io_read_func.argtypes = [mem_read_func_t]
        # self.device.set_io_read_func.restype = None
        # self.device.set_io_read_func(mem_read_func_t(io_read))
        # # IO write function
        # self.device.set_io_write_func.argtypes = [mem_write_func_t]
        # self.device.set_io_write_func.restype = None
        # self.device.set_io_write_func(mem_write_func_t(io_write))

        # self.set_log_level = get_dll_function(self.device, "void set_log_level(uint8_t)")

        # self.module_reset = get_dll_function(self.device, "void module_reset(void)")
        # self.module_reset()

        # self.module_save = get_dll_function(self.device, "void module_save(void)")
        # self.module_restore = get_dll_function(self.device, "void module_restore(void)")
        self.module_tick = get_dll_function(self.device, "int module_tick(uint32_t)")


cpu = Cpu("Devices/bin/libsimple_cpu.dll")
for i in range(10):
    cpu.module_tick(i)
