import threading
import queue
import time
import sys
import msvcrt


command_queue = queue.Queue()
response_queue = queue.Queue()


current_input_lock = threading.Lock()
current_input = ''
prompt_string = "Enter a command (sleep <seconds>, hello, exit): "


def worker():
    while True:
        # Check if there's a command in the queue non-blockingly
        try:
            command = command_queue.get_nowait()
            if command == "exit":
                response_queue.put("Exiting worker thread...")
                break
            elif command.startswith("sleep"):
                duration = int(command.split()[1])
                time.sleep(duration)
                response_queue.put(f"Slept for {duration} seconds.")
            elif command == "hello":
                response_queue.put("Hello from the worker thread!")
            else:
                response_queue.put("Unknown command.")
            command_queue.task_done()
        except queue.Empty:
            # No commands in the queue, continue working
            time.sleep(1)  # Simulate doing some work


def get_user_input():
    global current_input  # Use global variable to store the input
    current_input = ''
    while True:
        if msvcrt.kbhit():
            char = msvcrt.getch()
            if char in [b'\r', b'\n']:  # Enter key
                print()
                return current_input
            elif char == b'\x08':  # Backspace key
                current_input = current_input[:-1]
                to_print = f"{prompt_string}{current_input}"
                sys.stdout.write('\r' + '\033[K')  # Clear the line
                sys.stdout.write(f"{to_print}")
                sys.stdout.flush()
            else:
                current_input += char.decode()
                sys.stdout.write(char.decode())
                sys.stdout.flush()


def response_handler():
    while True:
        response = response_queue.get()
        if response == "Exiting worker thread...":
            print(response)
            break

        sys.stdout.write("\r" + " " * 80 + "\r")  # Clear the line
        print(f"Worker response: {response}")

        with current_input_lock:
            sys.stdout.write(f"{prompt_string}{current_input}")  
        sys.stdout.flush()
        response_queue.task_done()

def main():
    worker_thread = threading.Thread(target=worker)
    worker_thread.start()

    response_thread = threading.Thread(target=response_handler)
    response_thread.start()

    try:
        while True:
            sys.stdout.write(prompt_string)
            sys.stdout.flush()
            user_input = get_user_input()
            command_queue.put(user_input)

            if user_input == "exit":
                break
    finally:
        worker_thread.join()
        response_thread.join()


if __name__ == "__main__":
    main()
