import ctypes
import sys


class Logger:
    def __init__(self, ui_getter, print_to_file=False):
        self.ui_getter = ui_getter
        self.print_to_file = print_to_file
        self.callback_type = ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_char_p)

    def print(self, filename, logstring):
        if self.print_to_file:
            with open(filename, 'a') as f:
                f.write(logstring + "\n")
        else:
            self.print_line(logstring)

    def print_line(self, line):
        part_cmd = self.ui_getter.get_part_input()
        print(f"\033[2K\r{line}")
        print(f"Enter command: {part_cmd}", end = "")
        sys.stdout.flush()
