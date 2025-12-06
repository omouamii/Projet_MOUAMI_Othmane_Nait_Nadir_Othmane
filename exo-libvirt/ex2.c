#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

int main(int argc, char *argv[]) {
    const char *uri = (argc > 1) ? argv[1] : "qemu+ssh://user@172.19.3.20/system";

    // Remplace 'user@remote-system' par ton nom d'utilisateur et ton hôte distant
    virConnectPtr conn = virConnectOpen(uri);
    if (conn == NULL) {
        fprintf(stderr, "Erreur : impossible de se connecter à %s\n", uri);
        return 1;
    }

    printf("Connection successful :)\n");

    // --- Hostname ---
    char *hostname = virConnectGetHostname(conn);
    if (hostname) {
        printf("Hostname: %s\n", hostname);
        free(hostname);
    }

    // --- Nombre de CPU ---
    int max_vcpu = virConnectGetMaxVcpus(conn, NULL);
    printf("Maximum support virtual CPUs: %d\n", max_vcpu);

    // --- Mémoire disponible ---
    unsigned long long freeMem = virNodeGetFreeMemory(conn);
    printf("Memory size: %llu kb\n", freeMem / 1024);

    virConnectClose(conn);
    return 0;
}
