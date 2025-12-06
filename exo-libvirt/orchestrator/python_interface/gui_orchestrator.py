import ctypes
import os
import sys
import subprocess
import tempfile
import tkinter as tk
from tkinter import ttk, messagebox, simpledialog, filedialog
import xml.etree.ElementTree as ET
from shutil import which as _which
import socket

# ========= Réglages =========
URI_DEFAULT = "qemu:///system"
BASE_DIR = os.path.dirname(__file__)
LIB_PATH = os.path.join(BASE_DIR, "libvirt_helper.so")
KNOWN_PATH = os.path.join(BASE_DIR, "known_vms.txt")

# ========= Chargement de la librairie .so =========
if not os.path.exists(LIB_PATH):
    messagebox.showerror("Erreur", f"Bibliothèque introuvable :\n{LIB_PATH}")
    sys.exit(1)

try:
    lib = ctypes.CDLL(LIB_PATH)
except OSError as e:
    messagebox.showerror("Erreur", f"Impossible de charger {LIB_PATH} :\n{e}")
    sys.exit(1)

# ========= Signatures C =========
lib.connect_hypervisor.argtypes = [ctypes.c_char_p]
lib.start_domain.argtypes       = [ctypes.c_char_p, ctypes.c_char_p]
lib.stop_domain.argtypes        = [ctypes.c_char_p, ctypes.c_char_p]
lib.save_domain.argtypes        = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
lib.restore_domain.argtypes     = [ctypes.c_char_p, ctypes.c_char_p]
lib.ensure_persistent.argtypes  = [ctypes.c_char_p, ctypes.c_char_p]
lib.reboot_domain.argtypes      = [ctypes.c_char_p, ctypes.c_char_p]

# ========= Helpers généraux =========
def capture_c_stdout(func, *args) -> str:
    """Capture stdout des fonctions C (printf) via redirection FD=1."""
    with tempfile.TemporaryFile() as tmp:
        saved = os.dup(1)
        try:
            os.dup2(tmp.fileno(), 1)
            func(*args)
        finally:
            os.dup2(saved, 1)
            os.close(saved)
        tmp.seek(0)
        return tmp.read().decode("utf-8", errors="ignore")


def run_cmd(cmd: list[str]) -> tuple[int, str]:
    """Exécute une commande et renvoie (rc, sortie)."""
    try:
        out = subprocess.check_output(cmd, text=True, stderr=subprocess.STDOUT)
        return 0, out
    except subprocess.CalledProcessError as e:
        return e.returncode, e.output
    except FileNotFoundError as e:
        return 127, str(e)


def which(name: str) -> str | None:
    return _which(name)


def normalize_uri(raw: str) -> str:
    """
    Accepte :
      - une URI complète (qemu+ssh://user@host/system, qemu:///system…)
      - ou juste une IP / hostname (192.168.1.50, machine1, machine2)
    et renvoie toujours une URI libvirt valide.

    Pour la machine locale (machine2 / localhost), on force qemu:///system
    pour éviter un SSH vers soi-même.
    """
    raw = (raw or "").strip()
    if not raw:
        return URI_DEFAULT

    if "://" in raw:
        return raw

    local_names = {
        "localhost",
        "127.0.0.1",
        socket.gethostname(),
        "machine2",
    }
    if raw in local_names:
        return "qemu:///system"

    user = os.getenv("USER", "user")
    return f"qemu+ssh://{user}@{raw}/system"


def virsh_cmd(uri: str, *args: str) -> list[str]:
    """Construit la commande virsh en prenant en compte l'URI."""
    uri = normalize_uri(uri)
    return ["virsh", "-c", uri, *args]


def virsh_list_all(uri: str):
    """Retourne [(name, 'Active'|'Inactive')] via virsh list --all sur l'URI donnée."""
    try:
        out = subprocess.check_output(
            virsh_cmd(uri, "list", "--all"),
            text=True,
            stderr=subprocess.STDOUT
        )
    except Exception:
        return []

    rows = []
    for line in out.splitlines():
        s = line.strip()
        if (not s) or s.startswith(("Id", "ID", "Nom", "Name", "État", "State", "-")):
            continue
        parts = s.split()
        if len(parts) < 2:
            continue
        id_or_dash, name = parts[0], parts[1]
        state_str = " ".join(parts[2:]).lower() if len(parts) >= 3 else ""
        state = "Active" if id_or_dash.isdigit() else "Inactive"
        if any(k in state_str for k in ("éteinte", "shut", "shutoff", "off")):
            state = "Inactive"
        elif any(k in state_str for k in ("en cours", "running")):
            state = "Active"
        rows.append((name, state))
    return rows


def load_known():
    if not os.path.exists(KNOWN_PATH):
        return set()
    with open(KNOWN_PATH, "r", encoding="utf-8") as f:
        return {line.strip() for line in f if line.strip()}


def save_known(names: set):
    try:
        with open(KNOWN_PATH, "w", encoding="utf-8") as f:
            for n in sorted(names):
                f.write(n + "\n")
    except Exception:
        pass


def detect_uefi_paths():
    """Détecte les chemins OVMF pour UEFI (paquet: ovmf)."""
    candidates_code = [
        "/usr/share/OVMF/OVMF_CODE.fd",
        "/usr/share/OVMF/OVMF_CODE_4M.fd",
        "/usr/share/qemu/OVMF_CODE.fd",
    ]
    candidates_vars = [
        "/usr/share/OVMF/OVMF_VARS.fd",
        "/usr/share/OVMF/OVMF_VARS_4M.fd",
        "/usr/share/qemu/OVMF_VARS.fd",
    ]
    code = next((p for p in candidates_code if os.path.exists(p)), None)
    vars_ = next((p for p in candidates_vars if os.path.exists(p)), None)
    return code, vars_


def list_networks(uri: str) -> list[str]:
    rc, out = run_cmd(virsh_cmd(uri, "net-list", "--all"))
    if rc != 0 or not out:
        return []
    nets = []
    for line in out.splitlines():
        s = line.strip()
        if not s or s.startswith(("Name", "Nom", "-")):
            continue
        name = s.split()[0]
        nets.append(name)
    return nets


def ensure_network_active(uri: str, name: str) -> tuple[bool, str]:
    """
    S'assure que le réseau libvirt 'name' existe et est actif.
    Traite 'déjà actif' et 'existe déjà' comme SUCCÈS.
    """
    logs = []

    rc, out = run_cmd(virsh_cmd(uri, "net-info", name))
    logs.append(out or "")
    if rc == 0:
        if ("Active: yes" in out) or ("actif: oui" in out.lower()):
            return True, "\n".join(logs)
        rc, out = run_cmd(virsh_cmd(uri, "net-start", name))
        logs.append(out or "")
        if rc == 0 or "already active" in (out or "").lower() or "est déjà actif" in (out or "").lower():
            run_cmd(virsh_cmd(uri, "net-autostart", name))
            return True, "\n".join(logs)

    default_xml = f"""
<network>
  <name>{name}</name>
  <forward mode='nat'/>
  <bridge name='virbr0' stp='on' delay='0'/>
  <ip address='192.168.122.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='192.168.122.2' end='192.168.122.254'/>
    </dhcp>
  </ip>
</network>
""".strip()

    with tempfile.NamedTemporaryFile("w", delete=False, suffix=".xml") as tf:
        tf.write(default_xml)
        xml_path = tf.name

    try:
        rc, out = run_cmd(virsh_cmd(uri, "net-define", xml_path))
        logs.append(out or "")
        if rc != 0 and "exists already" not in (out or "").lower() and "existe déjà" not in (out or "").lower():
            return False, "\n".join(logs)

        rc, out = run_cmd(virsh_cmd(uri, "net-start", name))
        logs.append(out or "")
        if rc != 0 and "already active" not in (out or "").lower() and "est déjà actif" not in (out or "").lower():
            return False, "\n".join(logs)

        run_cmd(virsh_cmd(uri, "net-autostart", name))
        return True, "\n".join(logs)
    finally:
        try:
            os.unlink(xml_path)
        except Exception:
            pass


def domain_exists(uri: str, name: str) -> bool:
    rc, out = run_cmd(virsh_cmd(uri, "dominfo", name))
    return rc == 0


def domain_is_active(uri: str, name: str) -> bool:
    rc, out = run_cmd(virsh_cmd(uri, "domstate", name))
    if rc != 0 or not out:
        return False
    return ("running" in out.lower()) or ("en cours" in out.lower())


def get_domain_xml(uri: str, name: str) -> str | None:
    rc, out = run_cmd(virsh_cmd(uri, "dumpxml", name))
    return out if rc == 0 else None


def disks_from_xml(xml: str) -> tuple[list[str], str | None]:
    """Extrait (liste_disques, nvram_path) depuis le XML (disques device='disk')."""
    disks = []
    nvram = None
    try:
        root = ET.fromstring(xml)
        os_el = root.find("os")
        if os_el is not None:
            nv = os_el.find("nvram")
            if nv is not None and nv.text:
                nvram = nv.text.strip()
        for d in root.findall(".//devices/disk"):
            if d.get("device") != "disk":
                continue
            src = d.find("source")
            if src is not None and "file" in src.attrib:
                disks.append(src.attrib["file"])
    except Exception:
        pass
    return disks, nvram


# ========= Fenêtre de création =========
class CreateVMDialog(tk.Toplevel):
    def __init__(self, master, on_created_callback, uri: str):
        super().__init__(master)
        self.title("Create a new VM")
        self.resizable(False, False)
        self.on_created = on_created_callback
        self.uri = uri

        self.name = tk.StringVar()
        self.iso_path = tk.StringVar()
        self.memory = tk.IntVar(value=2048)
        self.vcpus = tk.IntVar(value=2)
        self.disk_gb = tk.IntVar(value=20)
        self.disk_dir = tk.StringVar(value="/srv/libvirt-images")
        nets = list_networks(self.uri)
        self.network = tk.StringVar(value=nets[0] if nets else "default")
        self.graphics = tk.StringVar(value="vnc")
        self.firmware = tk.StringVar(value="bios")
        self.autostart = tk.BooleanVar(value=True)
        self.start_now = tk.BooleanVar(value=False)

        self._build_ui(nets)

    def _build_ui(self, nets: list[str]):
        pad = {'padx': 8, 'pady': 6}

        row = 0
        ttk.Label(self, text="VM Name:").grid(row=row, column=0, sticky="e", **pad)
        ttk.Entry(self, textvariable=self.name, width=32).grid(row=row, column=1, sticky="w", **pad)

        row += 1
        ttk.Label(self, text="ISO file:").grid(row=row, column=0, sticky="e", **pad)
        iso_frame = ttk.Frame(self)
        iso_frame.grid(row=row, column=1, sticky="w", **pad)
        ttk.Entry(iso_frame, textvariable=self.iso_path, width=40).pack(side="left")
        ttk.Button(iso_frame, text="Browse…", command=self._browse_iso).pack(side="left", padx=6)

        row += 1
        ttk.Label(self, text="Memory (MiB):").grid(row=row, column=0, sticky="e", **pad)
        ttk.Spinbox(self, from_=256, to=262144, increment=256, textvariable=self.memory, width=10).grid(row=row, column=1, sticky="w", **pad)

        row += 1
        ttk.Label(self, text="vCPUs:").grid(row=row, column=0, sticky="e", **pad)
        ttk.Spinbox(self, from_=1, to=64, textvariable=self.vcpus, width=10).grid(row=row, column=1, sticky="w", **pad)

        row += 1
        ttk.Label(self, text="Disk size (GB):").grid(row=row, column=0, sticky="e", **pad)
        ttk.Spinbox(self, from_=5, to=1024, textvariable=self.disk_gb, width=10).grid(row=row, column=1, sticky="w", **pad)

        row += 1
        ttk.Label(self, text="Disk folder:").grid(row=row, column=0, sticky="e", **pad)
        dsk_frame = ttk.Frame(self)
        dsk_frame.grid(row=row, column=1, sticky="w", **pad)
        ttk.Entry(dsk_frame, textvariable=self.disk_dir, width=40).pack(side="left")
        ttk.Button(dsk_frame, text="Browse…", command=self._browse_diskdir).pack(side="left", padx=6)

        row += 1
        ttk.Label(self, text="Network:").grid(row=row, column=0, sticky="e", **pad)
        if nets:
            cb = ttk.Combobox(self, values=nets, textvariable=self.network, width=18, state="readonly")
            cb.grid(row=row, column=1, sticky="w", **pad)
        else:
            ttk.Entry(self, textvariable=self.network, width=18).grid(row=row, column=1, sticky="w", **pad)

        row += 1
        ttk.Label(self, text="Graphics:").grid(row=row, column=0, sticky="e", **pad)
        g_frame = ttk.Frame(self)
        g_frame.grid(row=row, column=1, sticky="w", **pad)
        ttk.Radiobutton(g_frame, text="VNC", variable=self.graphics, value="vnc").pack(side="left")
        ttk.Radiobutton(g_frame, text="SPICE", variable=self.graphics, value="spice").pack(side="left", padx=10)

        row += 1
        ttk.Label(self, text="Firmware:").grid(row=row, column=0, sticky="e", **pad)
        f_frame = ttk.Frame(self)
        f_frame.grid(row=row, column=1, sticky="w", **pad)
        ttk.Radiobutton(f_frame, text="BIOS", variable=self.firmware, value="bios").pack(side="left")
        ttk.Radiobutton(f_frame, text="UEFI (OVMF)", variable=self.firmware, value="uefi").pack(side="left", padx=10)

        row += 1
        ttk.Checkbutton(self, text="Autostart", variable=self.autostart).grid(row=row, column=1, sticky="w", **pad)

        row += 1
        ttk.Checkbutton(self, text="Start immediately", variable=self.start_now).grid(row=row, column=1, sticky="w", **pad)

        row += 1
        btns = ttk.Frame(self)
        btns.grid(row=row, column=0, columnspan=2, pady=(10, 12))
        ttk.Button(btns, text="Cancel", command=self.destroy).pack(side="right", padx=6)
        ttk.Button(btns, text="Create", command=self._on_create).pack(side="right")

    def _browse_iso(self):
        path = filedialog.askopenfilename(
            title="Select ISO",
            filetypes=[("ISO images", "*.iso *.ISO"), ("All files", "*.*")]
        )
        if path:
            self.iso_path.set(path)

    def _browse_diskdir(self):
        path = filedialog.askdirectory(title="Select disk folder", mustexist=True)
        if path:
            self.disk_dir.set(path)

    def _on_create(self):
        name = self.name.get().strip()
        iso = self.iso_path.get().strip()
        mem = int(self.memory.get())
        vcpus = int(self.vcpus.get())
        disk_gb = int(self.disk_gb.get())
        disk_dir = self.disk_dir.get().strip()
        network = self.network.get().strip()
        graphics = self.graphics.get()
        firmware = self.firmware.get()
        autostart = self.autostart.get()
        start_now = self.start_now.get()

        if not name:
            messagebox.showerror("Erreur", "Nom de VM requis.")
            return
        if not iso or not os.path.exists(iso):
            messagebox.showerror("Erreur", f"ISO introuvable : {iso}")
            return
        if not os.path.isdir(disk_dir):
            messagebox.showerror("Erreur", f"Dossier disque introuvable : {disk_dir}")
            return

        ok, _ = ensure_network_active(self.uri, network)
        if not ok:
            messagebox.showerror("Réseau", f"Echec d'activation/création du réseau '{network}'.")
            return

        disk_path = os.path.join(disk_dir, f"{name}.qcow2")
        rc, out = run_cmd(["qemu-img", "create", "-f", "qcow2", disk_path, f"{disk_gb}G"])
        if rc != 0:
            messagebox.showerror("Erreur", f"qemu-img a échoué:\n{out}")
            return

        graphics_xml = f"<graphics type='{graphics}' port='-1' autoport='yes' listen='127.0.0.1'/>"
        os_xml = "<os><type arch='x86_64'>hvm</type><boot dev='cdrom'/></os>"
        if firmware == "uefi":
            code, vars_ = detect_uefi_paths()
            if not code or not vars_:
                messagebox.showerror("UEFI non disponible", "Installe le paquet 'ovmf' puis réessaie.")
                return
            os_xml = (
                "<os>"
                "  <type arch='x86_64'>hvm</type>"
                f"  <loader readonly='yes' type='pflash'>{code}</loader>"
                f"  <nvram>{vars_}</nvram>"
                "  <boot dev='cdrom'/>"
                "</os>"
            )

        domain_xml = (
            "<domain type='kvm'>"
            f"<name>{name}</name>"
            f"<memory unit='MiB'>{mem}</memory>"
            f"<vcpu>{vcpus}</vcpu>"
            f"{os_xml}"
            "<features><acpi/><apic/><vmport state='off'/></features>"
            "<cpu mode='host-model'/>"
            "<devices>"
            "  <disk type='file' device='cdrom'>"
            "    <driver name='qemu' type='raw'/>"
            f"    <source file='{iso}'/>"
            "    <target dev='hdc' bus='ide'/>"
            "    <readonly/>"
            "  </disk>"
            "  <disk type='file' device='disk'>"
            "    <driver name='qemu' type='qcow2'/>"
            f"    <source file='{disk_path}'/>"
            "    <target dev='vda' bus='virtio'/>"
            "  </disk>"
            "  <interface type='network'>"
            f"    <source network='{network}'/>"
            "    <model type='virtio'/>"
            "  </interface>"
            f"  {graphics_xml}"
            "  <serial type='pty'>"
            "    <target port='0'/>"
            "  </serial>"
            "  <console type='pty'>"
            "    <target type='serial' port='0'/>"
            "  </console>"
            "</devices>"
            "</domain>"
        )

        with tempfile.NamedTemporaryFile("w", delete=False, suffix=".xml") as tf:
            tf.write(domain_xml)
            xml_path = tf.name

        try:
            rc, out = run_cmd(virsh_cmd(self.uri, "define", xml_path))
        finally:
            try:
                os.unlink(xml_path)
            except Exception:
                pass

        if rc != 0:
            messagebox.showerror("Erreur", f"virsh define a échoué:\n{out}")
            try:
                if os.path.exists(disk_path):
                    os.remove(disk_path)
            except Exception:
                pass
            return

        if autostart:
            run_cmd(virsh_cmd(self.uri, "autostart", name))

        if start_now:
            run_cmd(virsh_cmd(self.uri, "start", name))

        messagebox.showinfo("Création", f"VM '{name}' créée avec succès.")
        if callable(self.on_created):
            self.on_created(name)
        self.destroy()


# ========= GUI PRINCIPALE =========
class OrchestratorGUI(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Mini Orchestrator Libvirt")
        self.geometry("1080x680")

        self.configure(bg="#0f172a")

        self.uri = tk.StringVar(value=URI_DEFAULT)
        self.known_vms = load_known()

        self._setup_style()
        self._build_ui()
        self.refresh_domains()

    def _setup_style(self):
        style = ttk.Style(self)
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass

        style.configure("Top.TFrame", background="#0f172a")
        style.configure("Main.TFrame", background="#0f172a")
        style.configure("Top.TLabel", background="#0f172a", foreground="#e5e7eb",
                        font=("Segoe UI", 10, "bold"))

        style.configure(
            "Primary.TButton",
            padding=(10, 5),
            font=("Segoe UI", 9, "bold")
        )
        style.map(
            "Primary.TButton",
            foreground=[("!disabled", "white")],
            background=[("!disabled", "#2563eb"), ("active", "#1d4ed8")]
        )

        style.configure(
            "Danger.TButton",
            padding=(10, 5),
            font=("Segoe UI", 9, "bold")
        )
        style.map(
            "Danger.TButton",
            foreground=[("!disabled", "white")],
            background=[("!disabled", "#dc2626"), ("active", "#b91c1c")]
        )

        style.configure(
            "Neutral.TButton",
            padding=(8, 4),
            font=("Segoe UI", 9)
        )

        style.configure(
            "Treeview",
            background="#020617",
            fieldbackground="#020617",
            foreground="#e5e7eb",
            rowheight=24,
            borderwidth=0
        )
        style.configure(
            "Treeview.Heading",
            background="#111827",
            foreground="#e5e7eb",
            font=("Segoe UI", 10, "bold")
        )

        style.configure("Log.TLabelframe", background="#0f172a", foreground="#e5e7eb")
        style.configure("Log.TLabelframe.Label", background="#0f172a", foreground="#9ca3af")

        style.configure("Status.TLabel", background="#111827", foreground="#e5e7eb",
                        font=("Segoe UI", 9))

    def _build_ui(self):
        top = ttk.Frame(self, style="Top.TFrame")
        top.pack(fill="x", padx=8, pady=6)

        ttk.Label(top, text="Hypervisor URI:", style="Top.TLabel").pack(side="left")
        ttk.Entry(top, textvariable=self.uri, width=60).pack(side="left", padx=6)

        ttk.Button(top, text="Connect", style="Primary.TButton",
                   command=self.connect).pack(side="left", padx=3)
        ttk.Button(top, text="Refresh", style="Neutral.TButton",
                   command=self.refresh_domains).pack(side="left", padx=3)

        actions = ttk.Frame(self, style="Main.TFrame")
        actions.pack(fill="x", padx=8, pady=(0, 6))

        ttk.Button(actions, text="Start VM", style="Primary.TButton",
                   command=self.start_vm).pack(side="left", padx=3)
        ttk.Button(actions, text="Stop VM", style="Neutral.TButton",
                   command=self.stop_vm).pack(side="left", padx=3)
        ttk.Button(actions, text="Save VM", style="Neutral.TButton",
                   command=self.save_vm).pack(side="left", padx=3)
        ttk.Button(actions, text="Restore VM", style="Neutral.TButton",
                   command=self.restore_vm).pack(side="left", padx=3)
        ttk.Button(actions, text="Reboot VM", style="Primary.TButton",
                   command=self.reboot_vm).pack(side="left", padx=3)
        ttk.Button(actions, text="Create VM", style="Primary.TButton",
                   command=self.create_vm).pack(side="left", padx=3)

        ttk.Button(actions, text="Clone VM", style="Primary.TButton",
                   command=self.clone_vm).pack(side="left", padx=3)

        ttk.Button(actions, text="Migrate VM", style="Primary.TButton",
                   command=self.migrate_vm).pack(side="left", padx=3)

        ttk.Button(actions, text="Remove VM", style="Danger.TButton",
                   command=self.remove_vm).pack(side="left", padx=3)
        ttk.Button(actions, text="Open (GUI)", style="Primary.TButton",
                   command=self.open_vm_gui).pack(side="left", padx=8)
        ttk.Button(actions, text="Open Console", style="Primary.TButton",
                   command=self.open_vm_console).pack(side="left", padx=3)

        self.tree = ttk.Treeview(self, columns=("Name", "State"), show="headings",
                                 height=18)
        self.tree.heading("Name", text="VM Name")
        self.tree.heading("State", text="State")
        self.tree.column("Name", width=680)
        self.tree.column("State", width=160, anchor="center")
        self.tree.pack(fill="both", expand=True, padx=8, pady=6)

        self.tree.tag_configure("Active", background="#064e3b", foreground="#ecfdf5")
        self.tree.tag_configure("Inactive", background="#020617", foreground="#9ca3af")

        log_frame = ttk.LabelFrame(self, text="Libvirt output", style="Log.TLabelframe")
        log_frame.pack(fill="both", expand=False, padx=8, pady=(0, 6))
        self.log = tk.Text(
            log_frame,
            height=8,
            bg="#020617",
            fg="#e5e7eb",
            insertbackground="#e5e7eb",
            borderwidth=0,
            highlightthickness=0,
        )
        self.log.pack(fill="both", expand=True, padx=6, pady=6)

        self.status = tk.StringVar(value="Ready")
        status_lbl = ttk.Label(
            self,
            textvariable=self.status,
            anchor="w",
            style="Status.TLabel",
            relief="sunken",
        )
        status_lbl.pack(fill="x", side="bottom")

    def _append_log(self, txt):
        if txt:
            self.log.insert("end", txt + ("\n" if not txt.endswith("\n") else ""))
            self.log.see("end")

    def _selected_vm(self):
        sel = self.tree.selection()
        if not sel:
            messagebox.showwarning("Sélection", "Choisis une VM.")
            return None
        return self.tree.item(sel[0])["values"][0]

    def _update_known(self, names):
        changed = False
        for n in names:
            if n not in self.known_vms:
                self.known_vms.add(n)
                changed = True
        if changed:
            save_known(self.known_vms)

    def _remove_from_cache(self, name: str):
        if name in self.known_vms:
            self.known_vms.remove(name)
            save_known(self.known_vms)

    def connect(self):
        uri_norm = normalize_uri(self.uri.get())
        self.uri.set(uri_norm)
        out = capture_c_stdout(lib.connect_hypervisor, uri_norm.encode())
        self._append_log(out.strip() or f"connect_hypervisor({uri_norm}) called")
        messagebox.showinfo("Connexion", f"Connecté à {uri_norm}")
        self.refresh_domains()

    def refresh_domains(self):
        self.tree.delete(*self.tree.get_children())
        uri = self.uri.get()
        rows = virsh_list_all(uri)
        names_from_virsh = [name for name, _ in rows]
        self._update_known(names_from_virsh)
        state_map = {name: state for name, state in rows}
        union_names = sorted(set(names_from_virsh) | self.known_vms, key=str.lower)
        shown = 0
        for name in union_names:
            state = state_map.get(name, "Inactive")
            self.tree.insert("", "end", values=(name, state), tags=(state,))
            shown += 1
        active = sum(1 for n in union_names if state_map.get(n) == "Active")
        inactive = shown - active
        self.status.set(f"Total: {shown} | Actives: {active} | Inactives: {inactive}")

    def start_vm(self):
        name = self._selected_vm()
        if not name:
            return
        uri = self.uri.get()
        if not domain_exists(uri, name):
            messagebox.showwarning("Introuvable", f"La VM '{name}' n'existe plus. Elle sera retirée de la liste.")
            self._remove_from_cache(name)
            self.refresh_domains()
            return
        uri_n = normalize_uri(uri)
        out = capture_c_stdout(lib.start_domain, uri_n.encode(), name.encode())
        self._append_log(out.strip() or f"start_domain({name}) called")
        self._update_known([name])
        self.refresh_domains()

    def stop_vm(self):
        name = self._selected_vm()
        if not name:
            return
        uri = self.uri.get()
        if not domain_exists(uri, name):
            messagebox.showwarning("Introuvable", f"La VM '{name}' n'existe plus. Elle sera retirée de la liste.")
            self._remove_from_cache(name)
            self.refresh_domains()
            return
        uri_n = normalize_uri(uri)
        _ = capture_c_stdout(lib.ensure_persistent, uri_n.encode(), name.encode())
        out = capture_c_stdout(lib.stop_domain, uri_n.encode(), name.encode())
        self._append_log(out.strip() or f"stop_domain({name}) called")
        self._update_known([name])
        self.refresh_domains()

    def save_vm(self):
        name = self._selected_vm()
        if not name:
            return
        uri = self.uri.get()
        if not domain_exists(uri, name):
            messagebox.showwarning("Introuvable", f"La VM '{name}' n'existe plus. Elle sera retirée de la liste.")
            self._remove_from_cache(name)
            self.refresh_domains()
            return
        path = simpledialog.askstring("Sauvegarde", "Chemin du fichier .save :")
        if not path:
            return
        uri_n = normalize_uri(uri)
        out = capture_c_stdout(lib.save_domain, uri_n.encode(), name.encode(), path.encode())
        self._append_log(out.strip())
        self._update_known([name])
        self.refresh_domains()

    def restore_vm(self):
        uri = self.uri.get()
        path = simpledialog.askstring("Restauration", "Chemin du fichier .save à restaurer :")
        if not path:
            return
        uri_n = normalize_uri(uri)
        out = capture_c_stdout(lib.restore_domain, uri_n.encode(), path.encode())
        self._append_log(out.strip())
        self.refresh_domains()

    def reboot_vm(self):
        name = self._selected_vm()
        if not name:
            return
        uri = self.uri.get()
        if not domain_exists(uri, name):
            messagebox.showwarning("Introuvable", f"La VM '{name}' n'existe plus. Elle sera retirée de la liste.")
            self._remove_from_cache(name)
            self.refresh_domains()
            return
        uri_n = normalize_uri(uri)
        out = capture_c_stdout(lib.reboot_domain, uri_n.encode(), name.encode())
        self._append_log(out.strip())
        self.refresh_domains()

    def create_vm(self):
        def after_created(new_name: str):
            self._update_known([new_name])
            self.refresh_domains()
        CreateVMDialog(self, after_created, self.uri.get())

    def clone_vm(self):
        """Clone une VM via virt-clone (--auto-clone)."""
        name = self._selected_vm()
        if not name:
            return

        uri = self.uri.get()

        if not domain_exists(uri, name):
            messagebox.showwarning(
                "Introuvable",
                f"La VM '{name}' n'existe plus. Elle sera retirée de la liste."
            )
            self._remove_from_cache(name)
            self.refresh_domains()
            return

        new_name = simpledialog.askstring(
            "Clone VM",
            f"Nom pour le clone de '{name}' :"
        )
        if not new_name:
            return

        uri_n = normalize_uri(uri)
        rc, out = run_cmd([
            "virt-clone",
            "--connect", uri_n,
            "--original", name,
            "--name", new_name,
            "--auto-clone"
        ])

        self._append_log(out)

        if rc != 0:
            messagebox.showerror(
                "Erreur",
                f"Le clonage a échoué :\n\n{out}"
            )
            return

        self._update_known([new_name])

        messagebox.showinfo(
            "Clone réussi",
            f"La VM '{new_name}' a été clonée avec succès."
        )

        self.refresh_domains()

    def migrate_vm(self):
        """
        Migration à chaud (live) avec stockage déjà présent sur la destination.

        IMPORTANT : avant de lancer la migration, il faut que le disque
        (ex: /var/lib/libvirt/images/VM.qcow2) soit déjà copié sur
        la machine de destination, au même chemin, avec les bons droits.
        """
        name = self._selected_vm()
        if not name:
            return

        src_uri = self.uri.get()

        if not domain_exists(src_uri, name):
            messagebox.showwarning(
                "Introuvable",
                f"La VM '{name}' n'existe plus sur {src_uri}. Elle sera retirée de la liste."
            )
            self._remove_from_cache(name)
            self.refresh_domains()
            return

        if not self._ensure_running_or_ask(name):
            return

        dest_host = simpledialog.askstring(
            "Migration",
            "Hôte de destination (ex: machine1, 192.168.1.50) :"
        )
        if not dest_host:
            return
        dest_host = dest_host.strip()

        new_name = simpledialog.askstring(
            "Nom sur la destination",
            "Nom de la VM sur la machine de destination "
            "(laisser vide pour garder le même) :",
            initialvalue=name,
        )
        if new_name is None:
            return
        new_name = new_name.strip() or name

        src_uri_norm = normalize_uri(src_uri)

        user = os.getenv("USER", "user")
        dest_uri = f"qemu+ssh://{user}@{dest_host}/system"

        cmd = [
            "virsh",
            "-c", src_uri_norm,
            "migrate",
            "--live",
            "--p2p",
            "--tunnelled",
            "--persistent",
            "--undefinesource",
            "--unsafe",
            "--dname", new_name,
            name,
            dest_uri,
        ]

        rc, out = run_cmd(cmd)
        self._append_log(f"$ {' '.join(cmd)}\n{out}")

        if rc != 0:
            messagebox.showerror(
                "Migration échouée",
                f"La migration de '{name}' vers {dest_host} a échoué :\n\n{out}"
            )
            return

        self._remove_from_cache(name)
        self.refresh_domains()

        messagebox.showinfo(
            "Migration terminée",
            f"Migration de '{name}' vers {dest_host} terminée.\n"
            f"Sur la destination, connecte-toi à {dest_host} dans l’URI pour voir la VM."
        )

    def remove_vm(self):
        name = self._selected_vm()
        if not name:
            return

        uri = self.uri.get()

        if not domain_exists(uri, name):
            if messagebox.askyesno(
                "Domaine introuvable",
                f"'{name}' n'existe plus dans libvirt.\n"
                f"Retirer de la liste et supprimer le disque {name}.qcow2 si présent ?"
            ):
                orph = os.path.join("/var/lib/libvirt/images", f"{name}.qcow2")
                if os.path.exists(orph):
                    try:
                        os.remove(orph)
                        self._append_log(f"[INFO] Deleted orphan disk: {orph}")
                    except Exception as e:
                        self._append_log(f"[WARN] Unable to delete {orph}: {e}")
                self._remove_from_cache(name)
                self.refresh_domains()
            return

        if not messagebox.askyesno("Confirmation", f"Supprimer définitivement la VM '{name}' ?"):
            return

        if domain_is_active(uri, name):
            choice = messagebox.askyesnocancel(
                "VM en cours d'exécution",
                f"La VM '{name}' tourne. Voulez-vous l'arrêter proprement ?\n"
                f"Oui = Shutdown, Non = Force (Destroy), Annuler = Stop."
            )
            if choice is None:
                return
            if choice:
                run_cmd(virsh_cmd(uri, "shutdown", name))
            else:
                run_cmd(virsh_cmd(uri, "destroy", name))

        xml = get_domain_xml(uri, name)
        disks, nvram_path = disks_from_xml(xml) if xml else ([], None)

        if nvram_path and os.path.exists(nvram_path):
            rc, out = run_cmd(virsh_cmd(uri, "undefine", "--nvram", name))
        else:
            rc, out = run_cmd(virsh_cmd(uri, "undefine", name))

        if rc != 0 and (
            "failed to get domain" in (out or "").lower()
            or "domain not found" in (out or "").lower()
            or "impossible de récupérer le domaine" in (out or "").lower()
        ):
            self._append_log("[WARN] Domaine déjà absent, purge du cache/disques.")
        elif rc != 0:
            messagebox.showerror("Erreur", f"virsh undefine a échoué:\n{out}")
            return

        if disks:
            if messagebox.askyesno(
                "Supprimer les disques ?",
                f"Supprimer {len(disks)} fichier(s) disque associé(s) ?"
            ):
                for p in disks:
                    try:
                        if os.path.exists(p):
                            os.remove(p)
                            self._append_log(f"[INFO] Deleted disk: {p}")
                    except Exception as e:
                        self._append_log(f"[WARN] Unable to delete {p}: {e}")

        if nvram_path and os.path.exists(nvram_path):
            try:
                os.remove(nvram_path)
                self._append_log(f"[INFO] Deleted NVRAM: {nvram_path}")
            except Exception as e:
                self._append_log(f"[WARN] Unable to delete NVRAM {nvram_path}: {e}")

        self._remove_from_cache(name)
        messagebox.showinfo("Remove", f"VM '{name}' supprimée.")
        self.refresh_domains()

    def _ensure_running_or_ask(self, name: str) -> bool:
        uri = self.uri.get()
        if domain_is_active(uri, name):
            return True
        if messagebox.askyesno("VM éteinte", f"'{name}' est éteinte. La démarrer ?"):
            uri_n = normalize_uri(uri)
            out = capture_c_stdout(lib.start_domain, uri_n.encode(), name.encode())
            self._append_log(out.strip() or f"start_domain({name}) called")
            self.refresh_domains()
            return domain_is_active(uri, name)
        return False

    def open_vm_gui(self):
        """Ouvre la VM en mode graphique (virt-viewer ou virt-manager)."""
        name = self._selected_vm()
        if not name:
            return
        uri = self.uri.get()
        if not domain_exists(uri, name):
            messagebox.showwarning("Introuvable", f"La VM '{name}' n'existe plus. Elle sera retirée de la liste.")
            self._remove_from_cache(name)
            self.refresh_domains()
            return
        if not self._ensure_running_or_ask(name):
            return

        uri_n = normalize_uri(uri)

        viewer = which("virt-viewer")
        if viewer:
            try:
                subprocess.Popen([viewer, "-c", uri_n, name])
                self._append_log(f"[INFO] virt-viewer -c {uri_n} {name}")
            except Exception as e:
                messagebox.showerror("Erreur", f"virt-viewer a échoué :\n{e}")
            return

        vman = which("virt-manager")
        if vman:
            try:
                subprocess.Popen(
                    [vman, "--connect", uri_n, "--show-domain-console", name]
                )
                self._append_log(
                    f"[INFO] virt-manager --connect {uri_n} --show-domain-console {name}"
                )
            except Exception as e:
                messagebox.showerror("Erreur", f"virt-manager a échoué :\n{e}")
            return

        messagebox.showerror(
            "Programme manquant",
            "Installe l’un des paquets :\n  sudo apt install virt-viewer\n"
            "ou  sudo apt install virt-manager"
        )

    def open_vm_console(self):
        """Ouvre une console texte (virsh console) dans xterm."""
        name = self._selected_vm()
        if not name:
            return
        uri = self.uri.get()
        if not domain_exists(uri, name):
            messagebox.showwarning("Introuvable", f"La VM '{name}' n'existe plus. Elle sera retirée de la liste.")
            self._remove_from_cache(name)
            self.refresh_domains()
            return
        if not self._ensure_running_or_ask(name):
            return

        term = which("xterm")
        if not term:
            messagebox.showerror(
                "xterm manquant",
                "Pour la console texte, installe xterm :\n  sudo apt install xterm"
            )
            return

        uri_n = normalize_uri(uri)
        cmd = (
            f"virsh -c {uri_n} console '{name}'; "
            "echo; echo 'Quitter: Ctrl+] dans la console, puis ferme la fenêtre.'; "
            "read -n1 -r -p 'Appuie sur une touche pour fermer...' _"
        )

        try:
            subprocess.Popen([term, "-e", "bash", "-lc", cmd])
            self._append_log(f"[INFO] xterm console for {name} (URI={uri_n})")
        except Exception as e:
            messagebox.showerror("Erreur", f"Impossible de lancer xterm :\n{e}")


# ========= Lancer l'interface =========
if __name__ == "__main__":
    app = OrchestratorGUI()
    app.mainloop()
