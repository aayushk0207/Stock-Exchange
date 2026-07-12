import gdb
import time
import threading

def interrupt_gdb():
    time.sleep(5)
    print("Interrupting...")
    gdb.post_event(lambda: gdb.execute("interrupt"))

# Start a background thread to interrupt gdb after 5 seconds
threading.Thread(target=interrupt_gdb).start()

# Run the program
gdb.execute("run")

# Once interrupted, print backtrace of all threads
print("=== BACKTRACE OF ALL THREADS ===")
gdb.execute("thread apply all bt")
gdb.execute("quit")
