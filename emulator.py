import threading
import time

def emulator_thread(user_event):
    for _ in range(5):
        user_event.wait()   # user_event.wait(timeout = 10)
        print("Hello from the thread!", flush=True)
        time.sleep(1)


if __name__ == "__main__":
    print("Starting the thread")
    user_event = threading.Event()
    # user_event.set()
    # user_event.clear()
    t1 = threading.Thread(target = emulator_thread, args=(user_event,))
    t1.start()
    while True:
        user_command = input("Enter a command:")
        if user_command == 'run':
            if not user_event.is_set():
                user_event.set()
        elif user_command == 'stop':
            if user_event.is_set():
                user_event.clear()
        elif user_command == 'quit':
            if not user_event.is_set():
                user_event.set()
            break
    t1.join()
    print("Thread has finished")
