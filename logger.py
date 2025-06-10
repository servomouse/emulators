import ctypes
import sys


class Logger:
    def __init__(self, ui_getter):
        self.ui_getter = ui_getter
        self.callback_type = ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_char_p)

    def print(self, filename, logstring):
        print(logstring)

    def print_line(self, line):
        part_cmd = self.ui_getter.get_part_input()
        print(f"\033[2K\r{line}")
        print(f"Enter command: {part_cmd}", end = "")
        sys.stdout.flush()
