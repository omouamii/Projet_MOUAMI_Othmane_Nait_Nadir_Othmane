# Mini Orchestrator Libvirt (C + Python)

Ce projet est un mini orchestrateur permettant de gérer des machines virtuelles KVM/libvirt via une interface graphique Python/Tkinter. Une bibliothèque C est utilisée pour encapsuler les appels libvirt essentiels.

Fonctionnalités principales :
- Connexion à un hyperviseur local ou distant
- Affichage des VMs (actives / inactives)
- Démarrer, arrêter, redémarrer une VM
- Sauvegarde et restauration de l’état d’une VM
- Création d’une VM (ISO + QCOW2 + réseau + BIOS/UEFI)
- Clonage d’une VM
- Migration live vers un autre hyperviseur avec stockage partagé NFS
- Console graphique (virt-viewer / virt-manager) ou console texte (virsh console)
- Suppression complète d’une VM

---



````

---

## Prérequis

Système Linux avec un environnement KVM/libvirt opérationnel :

```bash
sudo apt install qemu-kvm libvirt-daemon-system libvirt-clients virtinst \
    virt-viewer virt-manager ovmf xterm python3 python3-tk
sudo systemctl enable --now libvirtd
````

Le stockage utilisé pour les disques QCOW2 est **partagé en NFS** entre les hyperviseurs afin de permettre les migrations live sans interruption.

---

## Compilation et préparation

Compiler la librairie C puis la copier côté interface Python :

```bash
cd orchestrator/c_lib
make
cp libvirt_helper.so ../python_interface/
```

---

## Lancement de l’application

```bash
cd orchestrator/python_interface
python3 gui_orchestrator.py
```

---

## Utilisation

Pour appliquer une action sur une VM, il faut d’abord **la sélectionner dans la liste** puis cliquer sur le bouton correspondant.

Actions disponibles :

* Start / Stop / Reboot
* Save / Restore (avec fichiers .save)
* Create VM (ISO + ressources)
* Clone VM
* Migrate VM (stockage NFS déjà disponible sur la destination)
* Remove VM (undefine + suppression disque)
* Open (GUI) via virt-viewer ou virt-manager
* Open Console via xterm (virsh console)

Un fichier `known_vms.txt` conserve localement les noms des VMs afin qu’elles restent visibles même si une machine distante est momentanément inaccessible.

---

## Remarques sur la migration live

La migration fonctionne à condition que :

* les disques QCOW2 soient situés sur un **partage NFS** monté au même chemin sur les deux hyperviseurs
* la connexion SSH entre les hyperviseurs soit fonctionnelle

Dans notre configuration, les disques sont stockés sur le NFS et libvirt peut donc migrer uniquement la mémoire et l’état de la VM.

---

## Limitations

* Création de VM simplifiée (1 ISO + 1 QCOW2)
* Pas de snapshots ou configuration CPU avancée
* Dépendances externes nécessaires : virsh, virt-clone, qemu-img

