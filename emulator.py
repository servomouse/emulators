import threading
import time
from queue import Queue


def print_to_file(string):
    with open("log.txt", 'a') as f:
        f.write(string + "\n")


def emulator_thread(command_queue):
    inner_state = 'running'
    while True:
        if not command_queue.empty():
            cmd = command_queue.get()
            print_to_file(f"User cmd from thread: {cmd}")
            if cmd == 'run':
                inner_state = 'running'
            elif cmd == 'print value':
                print_to_file("Here is the value!")
            elif cmd == 'stop':
                inner_state = 'stopped'
                print_to_file("The thread is stopped")
            elif cmd == 'quit':
                return
            command_queue.task_done()
        elif inner_state == 'running':
            print_to_file("Hello from the thread!")
            time.sleep(1)

if __name__ == "__main__":
    print("Starting the thread")

    command_queue = Queue()

    t1 = threading.Thread(target=emulator_thread, args=(command_queue,))
    t1.setDaemon(True)
    t1.start()

    while True:
        user_command = input("Enter a command: ")
        print(f"User command: \"{user_command}\"")
        command_queue.put(user_command)
        if user_command == 'quit':
            break
        print(f"Waiting for user command...")
