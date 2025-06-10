import ctypes
import copy
import os
import glob
import zipfile
from datetime import datetime
from dll_loader import DllLoader
from dll_devices import Memory, Cpu, AddressDecoder
from clock import Clock
# from logger import Logger
from io_devices import IODevice


import platform
os_type = platform.system()
if os_type == 'Windows':
    import pefile   # pip install pefile
elif os_type == 'Linux':
    from elftools.elf.elffile import ELFFile


class BrainfuckPC:
    def __init__(self, binary="binaries/bf_hello_world.txt", logger=None):
        self.binary = binary
        self.dll_loader = DllLoader()
        # self.logger = Logger(None)
        self.logger = logger

        self.address_router = AddressDecoder("devices/bin/libaddress_decoder.dll", self.dll_loader, self.logger)

        self.d_memory = Memory("devices/bin/libmemory.dll", 128, self.dll_loader, self.logger)
        self.p_memory = Memory("devices/bin/libmemory.dll", 128, self.dll_loader, self.logger)
        self.p_memory.memory_map_file(self.binary, 0)
        self.address_router.memory_map_device(self.d_memory, [0, 128])
        self.address_router.memory_map_device(self.p_memory, [0x8000, 0x8080])

        self.io = IODevice()

        self.cpu = Cpu("Devices/bin/libbrainfuck_cpu.dll", self.dll_loader, self.logger)

        memory_iface = {
            "mem_read": self.address_router.device.memory_read,
            "mem_write": self.address_router.device.memory_write,
            "io_read": self.io.io_input,
            "io_write": self.io.io_output,
        }
        self.cpu.connect_memory(memory_iface)

        self.clock = Clock(100)
        self.clock.add_device(self.cpu)
