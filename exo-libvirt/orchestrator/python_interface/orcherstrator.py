import ctypes
import os

# Charger la librairie C
lib_path = os.path.join(os.path.dirname(__file__), "../c_lib/libvirt_helper.so")
lib = ctypes.CDLL(lib_path)

# Définir les signatures des fonctions C
lib.connect_hypervisor.argtypes = [ctypes.c_char_p]
lib.list_active_domains.argtypes = [ctypes.c_char_p]
lib.list_inactive_domains.argtypes = [ctypes.c_char_p]
lib.start_domain.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
lib.stop_domain.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
lib.save_domain.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
lib.restore_domain.argtypes = [ctypes.c_char_p, ctypes.c_char_p]

def connect(uri):
    lib.connect_hypervisor(uri.encode())

def list_active(uri):
    lib.list_active_domains(uri.encode())

def list_inactive(uri):
    lib.list_inactive_domains(uri.encode())

def start(uri, domName):
    lib.start_domain(uri.encode(), domName.encode())

def stop(uri, domName):
    lib.stop_domain(uri.encode(), domName.encode())

def save(uri, domName, path):
    lib.save_domain(uri.encode(), domName.encode(), path.encode())

def restore(uri, path):
    lib.restore_domain(uri.encode(), path.encode())

def menu():
    uri = input("Enter hypervisor URI [default qemu:///system]: ") or "qemu:///system"
    while True:
        print("\n1) Connect\n2) List Active\n3) List Inactive\n4) Start VM\n5) Stop VM\n6) Save VM\n7) Restore VM\n0) Exit")
        choice = input("> ")
        if choice=="1": connect(uri)
        elif choice=="2": list_active(uri)
        elif choice=="3": list_inactive(uri)
        elif choice=="4": start(uri, input("VM Name: "))
        elif choice=="5": stop(uri, input("VM Name: "))
        elif choice=="6": save(uri, input("VM Name: "), input("Save Path: "))
        elif choice=="7": restore(uri, input("Restore Path: "))
        elif choice=="0": break
        else: print("Invalid choice!")

if __name__ == "__main__":
    menu()
