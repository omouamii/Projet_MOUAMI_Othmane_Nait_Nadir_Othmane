// ex5_display_save.c — Arrêt / Démarrage / Sauvegarde / Rechargement d’une VM
// Compile: gcc ex5_display_save.c -o ex5 $(pkg-config --cflags --libs libvirt)
// Usage: ./ex5 [vm_name] [timeout_s]   (defaults: vm10, 60)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libvirt/libvirt.h>

static int wait_until_state(virDomainPtr dom, int targetState, int timeout_sec) {
    for (int i = 0; i < timeout_sec * 2; i++) {
        int state = -1, reason = 0;
        if (virDomainGetState(dom, &state, &reason, 0) == 0 && state == targetState)
            return 0;
        usleep(500 * 1000);
    }
    return -1;
}

/* ======= AFFICHAGE EXACT COMME LE SUJET ======= */
static void print_header(virConnectPtr conn) {
    printf("Connection successful :)\n");

    char *h = virConnectGetHostname(conn);
    if (h) { printf("Hostname:%s.\n", h); free(h); }
    else   { printf("Hostname:unknown.\n"); }

    printf("Connection is encrypted: %d\n", virConnectIsEncrypted(conn));

    virNodeInfo ni;
    if (virNodeGetInfo(conn, &ni) == 0) {
        printf("Maximum support virtual CPUs: %u\n", ni.cpus);
        printf("Memory size: %llukb\n", (unsigned long long)ni.memory);
    }
    printf(">>> Liste des domaines\n");
}

static void print_active_domains(virConnectPtr conn) {
    printf("Active domain IDs:\n");
    int n = virConnectNumOfDomains(conn);
    if (n <= 0) return;

    int *ids = malloc(sizeof(int) * n);
    int got = virConnectListDomains(conn, ids, n);
    for (int i = 0; i < got; i++) {
        virDomainPtr d = virDomainLookupByID(conn, ids[i]);
        if (!d) continue;

        virDomainInfo di;
        virDomainGetInfo(d, &di);

        printf(" ID : %d NOM : %s\n", ids[i], virDomainGetName(d));
        printf("   state : %d\n   maxMem : %llu\n   memory : %llu\n   nrVirtCpu : %u\n   cpuTime : %llu\n",
            di.state, (unsigned long long)di.maxMem, (unsigned long long)di.memory,
            di.nrVirtCpu, (unsigned long long)di.cpuTime);

        virDomainFree(d);
    }
    free(ids);
}

static void print_inactive_domains(virConnectPtr conn) {
    printf("Inactive domain names:\n");
    int n = virConnectNumOfDefinedDomains(conn);
    if (n <= 0) return;

    char **names = malloc(sizeof(char*) * n);
    int got = virConnectListDefinedDomains(conn, names, n);
    for (int i = 0; i < got; i++) {
        printf("    %s\n", names[i]);
        free(names[i]);
    }
    free(names);
}

/* ========= SAVE / RESTORE FUNCTIONS ========== */

static void build_save_path(const char *name, char *buf, size_t size) {
    snprintf(buf, size, "/var/lib/libvirt/qemu/save/%s.save", name);
}

static int save_domain(virDomainPtr dom, const char *path, int timeout) {
    if (virDomainSaveFlags(dom, path, NULL, VIR_DOMAIN_SAVE_BYPASS_CACHE) < 0) {
        if (virDomainSave(dom, path) < 0) return -1;
    }
    wait_until_state(dom, VIR_DOMAIN_SHUTOFF, timeout);
    return 0;
}

static int restore_domain(virConnectPtr conn, const char *domName, const char *path, int timeout) {
    if (virDomainRestore(conn, path) < 0) return -1;

    // Wait for domain to be Running again
    for (int i = 0; i < timeout * 2; i++) {
        virDomainPtr d = virDomainLookupByName(conn, domName);
        if (d) {
            int st, rs;
            if (virDomainGetState(d, &st, &rs, 0) == 0 && st == VIR_DOMAIN_RUNNING) {
                virDomainFree(d);
                return 0;
            }
            virDomainFree(d);
        }
        usleep(500 * 1000);
    }
    return -1;
}


/* =============== MAIN PROGRAM ================= */

int main(int argc, char **argv) {
    const char *domName = (argc >= 2 ? argv[1] : "vm10");
    int timeout = (argc >= 3 ? atoi(argv[2]) : 60);

    virConnectPtr conn = virConnectOpen("qemu:///system");
    if (!conn) return fprintf(stderr, "Unable to connect\n"), 1;

    print_header(conn);
    print_active_domains(conn);
    print_inactive_domains(conn);

    virDomainPtr dom = virDomainLookupByName(conn, domName);
    if (!dom) return printf("Domain not found!\n"), 1;


    /* ----------- SHUTDOWN ------------- */
    printf("\n>>> Arret de %s\n", domName);
    virDomainDestroy(dom);
    wait_until_state(dom, VIR_DOMAIN_SHUTOFF, timeout);
    print_active_domains(conn);
    print_inactive_domains(conn);


    /* ----------- START ---------------- */
    printf(">>> Demarage de %s\n", domName);
    virDomainCreate(dom);
    wait_until_state(dom, VIR_DOMAIN_RUNNING, timeout);
    print_active_domains(conn);
    print_inactive_domains(conn);


    /* -------- SAVE / RESTORE ---------- */
    char savepath[512];
    build_save_path(domName, savepath, sizeof(savepath));

    printf(">>> Sauvegarde de %s\n", domName);
    save_domain(dom, savepath, timeout);
    print_active_domains(conn);
    print_inactive_domains(conn);

    printf(">>> Rechargement de %s\n", domName);
    restore_domain(conn, domName, savepath, timeout);
    print_active_domains(conn);
    print_inactive_domains(conn);


    virDomainFree(dom);
    virConnectClose(conn);
    return 0;
}
