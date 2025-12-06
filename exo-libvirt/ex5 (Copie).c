#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

void afficher_domaines(virConnectPtr conn) {
    int i, numDomains;
    int *activeDomains;
    char **inactiveDomains;
    int numInactive;

    // Domaines actifs
    numDomains = virConnectNumOfDomains(conn);
    activeDomains = malloc(sizeof(int) * numDomains);
    numDomains = virConnectListDomains(conn, activeDomains, numDomains);

    printf(">>> Liste des domaines actifs\n");
    if (numDomains == 0) {
        printf("(aucun domaine actif)\n");
    } else {
        for (i = 0; i < numDomains; i++) {
            virDomainPtr dom = virDomainLookupByID(conn, activeDomains[i]);
            char *name = virDomainGetName(dom);
            int state;
            virDomainGetState(dom, &state, NULL, 0);
            printf("ID : %d NOM : %s\n", activeDomains[i], name);
            virDomainFree(dom);
        }
    }
    free(activeDomains);

    // Domaines inactifs
    numInactive = virConnectListDefinedDomains(conn, NULL, 0);
    inactiveDomains = malloc(sizeof(char*) * numInactive);
    virConnectListDefinedDomains(conn, inactiveDomains, numInactive);

    printf(">>> Liste des domaines inactifs\n");
    if (numInactive == 0) {
        printf("(aucun domaine inactif)\n");
    } else {
        for (i = 0; i < numInactive; i++) {
            printf("%s\n", inactiveDomains[i]);
        }
    }
    free(inactiveDomains);
}

int main() {
    virConnectPtr conn;
    virDomainPtr dom;
    const char *vm_name = "vm11";

    conn = virConnectOpen("qemu:///system");
    if (!conn) {
        fprintf(stderr, "Erreur de connexion à l'hyperviseur\n");
        return 1;
    }
    printf("Connexion libvirt OK (qemu:///system)\n");

    printf(">>> Liste des domaines avant opération\n");
    afficher_domaines(conn);

    dom = virDomainLookupByName(conn, vm_name);
    if (!dom) {
        fprintf(stderr, "VM %s non trouvée\n", vm_name);
        virConnectClose(conn);
        return 1;
    }

    printf(">>> Arret de %s\n", vm_name);
    if (virDomainDestroy(dom) == 0) {
        printf("%s arrêt immédiat demandé\n", vm_name);
    } else {
        printf("Erreur lors de l'arrêt de %s\n", vm_name);
    }

    printf(">>> Liste des domaines après arrêt\n");
    afficher_domaines(conn);

    printf(">>> Demarrage de %s\n", vm_name);
    if (virDomainCreate(dom) == 0) {
        printf("%s démarré\n", vm_name);
    } else {
        printf("Erreur lors du démarrage de %s\n", vm_name);
    }

    printf(">>> Liste des domaines après démarrage\n");
    afficher_domaines(conn);

    virDomainFree(dom);
    virConnectClose(conn);
    return 0;
}
