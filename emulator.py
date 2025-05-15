import threading
import time
import sys
from queue import Queue
from pynput import keyboard # pip install pynput


class UserInputGetter:
    def __init__(self):
        self.input_queue = Queue()
        self.part_input = ""    # Contains user input before they hit Enter
        self.command = None     # Contains user input after user hit Enter, become None after read
        self.event = threading.Event()  # Set when user hit Enter, can be used for smart wait
        self.listener = keyboard.Listener(on_press=self.key_press)
        self.listener.start()

    def get_part_input(self):
        return self.part_input

    def get_command(self):
        cmd = self.command
        self.command = None
        self.event.clear()
        return cmd

    def key_press(self, key):
        if key == keyboard.Key.enter:
            self.command = self.part_input
            self.part_input = ""
            self.event.set()
        elif key == keyboard.Key.backspace:
            self.part_input = self.part_input[:-1]
        elif key == keyboard.Key.space:
            self.part_input += " "
        elif hasattr(key, 'char'):
            self.part_input += key.char


def print_to_file(string):
    with open("log.txt", 'a') as f:
        f.write(string + "\n")


def print_line(line, part_cmd):
    print(f"\033[2K\r{line}")
    print(f"Enter command: {part_cmd}", end = "")
    sys.stdout.flush()


def emulator_thread(command_queue, ui_getter):
    inner_state = 'stopped'
    counter = 0
    last_update = time.time()
    while True:
        part_cmd = ui_getter.get_part_input()
        command = ui_getter.get_command()

        if inner_state == 'running':
            if time.time()-last_update > 1:
                print_line(f"Counter = {counter}", part_input)
                counter += 1
                last_update = time.time()
        elif inner_state == 'stopped':
            print_line("The thread is stopped", part_cmd)
            ui_getter.event.wait()

        if command is not None:
            print_line(f"User command from thread: {command}", part_cmd)
            if command == 'run':
                inner_state = 'running'
            elif command == 'print value':
                print_line("Here is the value!", part_cmd)
            elif command == 'stop':
                inner_state = 'stopped'
            elif command == 'quit':
                return

        # if not command_queue.empty():
        #     cmd = command_queue.get()
        #     print_line(f"User cmd from thread: {cmd}", part_cmd)
        #     if cmd == 'run':
        #         inner_state = 'running'
        #     elif cmd == 'print value':
        #         print_line("Here is the value!", part_cmd)
        #     elif cmd == 'stop':
        #         inner_state = 'stopped'
        #         print_line("The thread is stopped", part_cmd)
        #     elif cmd == 'quit':
        #         return
        #     command_queue.task_done()
        # elif inner_state == 'running':
        #     print_line("Hello from the thread!", part_cmd)
        #     time.sleep(1)


def main():
    ui_getter = UserInputGetter()

    counter = 0
    last_update = time.time()
    inner_state = 'stopped'
    while True:
        part_input = ui_getter.get_part_input()
        command = ui_getter.get_command()

        if command is not None:
            print_line(f"User command from thread: {command}", part_input)
            if command == 'run':
                inner_state = 'running'
            elif command == 'print value':
                print_line("Here is the value!", part_input)
            elif command == 'stop':
                inner_state = 'stopped'
            elif command == 'quit':
                return

        if inner_state == 'running':
            if time.time()-last_update > 1:
                print_line(f"Counter = {counter}", part_input)
                counter += 1
                last_update = time.time()
        elif inner_state == 'stopped':
            print_line("The thread is stopped", part_input)
            ui_getter.event.wait()


if __name__ == "__main__":
    # print("Starting the thread")
    # # print(f"{dir(keyboard.Key) = }")
    # # sys.exit(0)

    # command_queue = Queue()
    # ui_getter = UserInputGetter()

    # t1 = threading.Thread(target=emulator_thread, args=(command_queue, ui_getter))
    # t1.setDaemon(True)
    # t1.start()

    # last_update = time.time()
    # last_input = ""
    # while True:
    #     part_input = ui_getter.get_part_input()
    #     command = ui_getter.get_command()

    #     # if time.time()-last_update > 1:
    #     #     print_line(f"Counter = {counter}", part_input)
    #     #     counter += 1
    #     #     last_update = time.time()

    #     if command is not None:
    #         print_line(f"User input: {command}", part_input)
    #         command_queue.put(command)
    #         if command == "quit":
    #             break
    main()
