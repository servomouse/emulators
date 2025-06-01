import ctypes
# import log_manager
import copy
import os
import glob
import zipfile
from datetime import datetime
from dll_loader import DllLoader
from dll_devices import Memory, Cpu
from dll_loader import DllLoader
from logger import Logger


import platform
os_type = platform.system()
if os_type == 'Windows':
    import pefile   # pip install pefile
elif os_type == 'Linux':
    from elftools.elf.elffile import ELFFile


dll_loader = DllLoader()
logger = Logger()


io_buf = []


def io_write(addr, val):
    io_buf.append(val)


memory = Memory("Devices/bin/libmemory.dll", 0x8100, dll_loader, logger)
io_mem = Memory("Devices/bin/libmemory.dll", 10, dll_loader, logger)
memory.memory_map_file("bf_hello_world.txt", 0x8000)


cpu = Cpu("Devices/bin/libsimple_cpu.dll", dll_loader, logger)

# io_write_func_ptr = cpu.mem_write_func_t(io_write)

memory_iface = {
    "mem_read": memory.device.memory_read,
    "mem_write": memory.device.memory_write,
    "io_read": io_mem.device.memory_read,
    "io_write": io_write,
}
cpu.connect_memory(memory_iface)

while 0 == cpu.module_tick(1):
    pass

print(io_buf)
