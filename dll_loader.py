import os
import shutil
import ctypes

# Allows to create multiple instances of the same dll

class DllLoader:
    def __init__(self):
        self.filename_counter = {}

    def upload(self, filename):
        if filename not in self.filename_counter:
            self.filename_counter[filename] = 0
            return ctypes.CDLL(filename)
        else:
            self.filename_counter[filename] += 1
            base, extension = os.path.splitext(filename)
            copy_filename = f"{base}_{self.filename_counter[filename]}{extension}"
            if not os.path.exists(copy_filename):
                shutil.copy(filename, copy_filename)
            return ctypes.CDLL(copy_filename)


# Usage: create a single instance like this:
# dll_loader = DllLoader()
# Then upload the necessary DLL with:
# object = dll_loader.upload(filename)
# If you try to create multiple instances of the same dll,
# DllLoader will simply create a new copy of the file
# for each consequitive upload call
