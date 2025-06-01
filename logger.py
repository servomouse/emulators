import ctypes


class Logger:
    def __init__(self):
        self.callback_type = ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_char_p)

    def print(self, filename, logstring):
        print(logstring)
