/*
 *
 *
 * To test the above program, the following configuration must be present:
 * /etc/libvirt/libvirtd.conf
 * listen_tls = 0
 * listen_tcp = 1
 * auth_tcp = "sasl"
 * /etc/sasl2/libvirt.conf
 * mech_list: digest-md5
 * A virt user has been added to the SASL database:
 * # saslpasswd2 -a libvirt virt # this will prompt for a password
 * libvirtd has been started with --listen
 */

/* example ex3.c */
/* compile with: gcc -g -Wall ex3.c -o ex3 -lvirt */
#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

int main(int argc, char *argv[]) {
    const char *uri = (argc > 1) ? argv[1] : "qemu+ssh://user@172.19.3.20/system";

    virConnectPtr conn = virConnectOpen(uri);
    if (conn == NULL) {
        fprintf(stderr, "Erreur : impossible de se connecter à %s\n", uri);
        return 1;
    }

    printf("Connection successful :)\n");

    // --- VMs actives ---
    int activeIDs[100];
    int numActive = virConnectListDomains(conn, activeIDs, 100);
    printf("Active domain IDs:\n");
    for (int i = 0; i < numActive; i++) {
        virDomainPtr dom = virDomainLookupByID(conn, activeIDs[i]);
        if (dom == NULL) continue;

        virDomainInfo info;
        virDomainGetInfo(dom, &info);

        printf("id : %d, nom : %s\n", activeIDs[i], virDomainGetName(dom));
        printf("state : %d\nmaxMem : %lu\nmemory : %lu\nnrVirtCpu : %d\ncpuTime : %llu\n",
               info.state, info.maxMem, info.memory, info.nrVirtCpu, info.cpuTime);

        virDomainFree(dom);
    }

    // --- VMs inactives ---
    printf("Inactive domain names:\n");
    int numInactive = virConnectNumOfDefinedDomains(conn);
    if (numInactive > 0) {
        char **inactiveNames = malloc(sizeof(char*) * numInactive);
        virConnectListDefinedDomains(conn, inactiveNames, numInactive);
        for (int i = 0; i < numInactive; i++) {
            printf("%s\n", inactiveNames[i]);
            free(inactiveNames[i]);
        }
        free(inactiveNames);
    }

    virConnectClose(conn);
    return 0;
}
