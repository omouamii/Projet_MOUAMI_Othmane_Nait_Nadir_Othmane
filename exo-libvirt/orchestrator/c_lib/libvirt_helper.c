#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libvirt/libvirt.h>

#include "libvirt_helper.h"

/* Petit wrapper pour ouvrir une connexion libvirt */
static virConnectPtr open_conn(const char *uri)
{
    virConnectPtr conn = NULL;

    if (!uri || !uri[0]) {
        fprintf(stderr, "[ERROR] open_conn: URI invalide\n");
        return NULL;
    }

    conn = virConnectOpen(uri);
    if (!conn) {
        fprintf(stderr, "[ERROR] Impossible d'ouvrir la connexion libvirt vers '%s'\n", uri);
        return NULL;
    }

    return conn;
}

/* -------- Connexion simple (pour test) -------- */

int connect_hypervisor(const char *uri)
{
    virConnectPtr conn = open_conn(uri);
    if (!conn) {
        printf("[ERROR] Unable to open connection to %s\n", uri);
        return -1;
    }

    printf("Connection successful to %s\n", uri);
    virConnectClose(conn);
    return 0;
}

/* -------- Listing de domaines (facultatif) -------- */

int list_active_domains(const char *uri)
{
    virConnectPtr conn = open_conn(uri);
    if (!conn)
        return -1;

    int num = virConnectNumOfDomains(conn);
    if (num < 0) {
        fprintf(stderr, "[ERROR] virConnectNumOfDomains\n");
        virConnectClose(conn);
        return -1;
    }

    int *ids = calloc(num, sizeof(int));
    if (!ids) {
        fprintf(stderr, "[ERROR] malloc ids\n");
        virConnectClose(conn);
        return -1;
    }

    num = virConnectListDomains(conn, ids, num);
    if (num < 0) {
        fprintf(stderr, "[ERROR] virConnectListDomains\n");
        free(ids);
        virConnectClose(conn);
        return -1;
    }

    printf("Active domains on %s:\n", uri);
    for (int i = 0; i < num; i++) {
        virDomainPtr dom = virDomainLookupByID(conn, ids[i]);
        if (dom) {
            const char *name = virDomainGetName(dom);
            printf("  ID %d : %s\n", ids[i], name ? name : "(null)");
            virDomainFree(dom);
        }
    }

    free(ids);
    virConnectClose(conn);
    return 0;
}

int list_inactive_domains(const char *uri)
{
    virConnectPtr conn = open_conn(uri);
    if (!conn)
        return -1;

    int num = virConnectNumOfDefinedDomains(conn);
    if (num < 0) {
        fprintf(stderr, "[ERROR] virConnectNumOfDefinedDomains\n");
        virConnectClose(conn);
        return -1;
    }

    char **names = calloc(num, sizeof(char*));
    if (!names) {
        fprintf(stderr, "[ERROR] malloc names\n");
        virConnectClose(conn);
        return -1;
    }

    num = virConnectListDefinedDomains(conn, names, num);
    if (num < 0) {
        fprintf(stderr, "[ERROR] virConnectListDefinedDomains\n");
        free(names);
        virConnectClose(conn);
        return -1;
    }

    printf("Inactive domains on %s:\n", uri);
    for (int i = 0; i < num; i++) {
        printf("  %s\n", names[i]);
        free(names[i]);
    }

    free(names);
    virConnectClose(conn);
    return 0;
}

/* -------- Start / Stop -------- */

int start_domain(const char *uri, const char *domName)
{
    if (!domName || !domName[0]) {
        fprintf(stderr, "[ERROR] start_domain: nom de domaine vide\n");
        return -1;
    }

    virConnectPtr conn = open_conn(uri);
    if (!conn)
        return -1;

    virDomainPtr dom = virDomainLookupByName(conn, domName);
    if (!dom) {
        fprintf(stderr, "[ERROR] Domain %s not found\n", domName);
        virConnectClose(conn);
        return -1;
    }

    if (virDomainCreate(dom) < 0) {
        fprintf(stderr, "[ERROR] Failed to start domain %s\n", domName);
        virDomainFree(dom);
        virConnectClose(conn);
        return -1;
    }

    printf("[INFO] Domain %s started\n", domName);

    virDomainFree(dom);
    virConnectClose(conn);
    return 0;
}

int stop_domain(const char *uri, const char *domName)
{
    if (!domName || !domName[0]) {
        fprintf(stderr, "[ERROR] stop_domain: nom de domaine vide\n");
        return -1;
    }

    virConnectPtr conn = open_conn(uri);
    if (!conn)
        return -1;

    virDomainPtr dom = virDomainLookupByName(conn, domName);
    if (!dom) {
        fprintf(stderr, "[ERROR] Domain %s not found\n", domName);
        virConnectClose(conn);
        return -1;
    }

    if (virDomainShutdown(dom) < 0) {
        fprintf(stderr, "[ERROR] Failed to shutdown domain %s\n", domName);
        virDomainFree(dom);
        virConnectClose(conn);
        return -1;
    }

    printf("[INFO] Domain %s is shutting down\n", domName);

    virDomainFree(dom);
    virConnectClose(conn);
    return 0;
}

/* -------- Save / Restore -------- */

int save_domain(const char *uri, const char *domName, const char *path)
{
    if (!domName || !path) {
        fprintf(stderr, "[ERROR] save_domain: argument(s) invalide(s)\n");
        return -1;
    }

    virConnectPtr conn = open_conn(uri);
    if (!conn)
        return -1;

    virDomainPtr dom = virDomainLookupByName(conn, domName);
    if (!dom) {
        fprintf(stderr, "[ERROR] Domain %s not found\n", domName);
        virConnectClose(conn);
        return -1;
    }

    if (virDomainSave(dom, path) < 0) {
        fprintf(stderr, "[ERROR] Failed to save domain %s to %s\n", domName, path);
        virDomainFree(dom);
        virConnectClose(conn);
        return -1;
    }

    printf("[INFO] Domain %s saved to %s\n", domName, path);

    virDomainFree(dom);
    virConnectClose(conn);
    return 0;
}

int restore_domain(const char *uri, const char *path)
{
    if (!path || !path[0]) {
        fprintf(stderr, "[ERROR] restore_domain: chemin invalide\n");
        return -1;
    }

    virConnectPtr conn = open_conn(uri);
    if (!conn)
        return -1;

    if (virDomainRestore(conn, path) < 0) {
        fprintf(stderr, "[ERROR] Failed to restore domain from %s\n", path);
        virConnectClose(conn);
        return -1;
    }

    printf("[INFO] Domain restored from %s\n", path);

    virConnectClose(conn);
    return 0;
}

/* -------- Persistence / Reboot -------- */

int ensure_persistent(const char *uri, const char *domName)
{
    if (!domName || !domName[0]) {
        fprintf(stderr, "[ERROR] ensure_persistent: nom vide\n");
        return -1;
    }

    virConnectPtr conn = open_conn(uri);
    if (!conn)
        return -1;

    virDomainPtr dom = virDomainLookupByName(conn, domName);
    if (!dom) {
        fprintf(stderr, "[ERROR] Domain %s not found\n", domName);
        virConnectClose(conn);
        return -1;
    }

    char *xml = virDomainGetXMLDesc(dom, 0);
    if (!xml) {
        fprintf(stderr, "[ERROR] virDomainGetXMLDesc failed for %s\n", domName);
        virDomainFree(dom);
        virConnectClose(conn);
        return -1;
    }

    /* Si un domaine est purement transient, le définir via defineXML le rend persistant */
    if (!virDomainIsPersistent(dom)) {
        if (!virDomainDefineXML(conn, xml)) {
            printf("[INFO] Domain %s is now persistent\n", domName);
        } else {
            fprintf(stderr, "[ERROR] Failed to make domain %s persistent\n", domName);
            free(xml);
            virDomainFree(dom);
            virConnectClose(conn);
            return -1;
        }
    } else {
        printf("[INFO] Domain %s is already persistent\n", domName);
    }

    free(xml);
    virDomainFree(dom);
    virConnectClose(conn);
    return 0;
}

int reboot_domain(const char *uri, const char *domName)
{
    if (!domName || !domName[0]) {
        fprintf(stderr, "[ERROR] reboot_domain: nom vide\n");
        return -1;
    }

    virConnectPtr conn = open_conn(uri);
    if (!conn)
        return -1;

    virDomainPtr dom = virDomainLookupByName(conn, domName);
    if (!dom) {
        fprintf(stderr, "[ERROR] Domain %s not found\n", domName);
        virConnectClose(conn);
        return -1;
    }

    if (virDomainReboot(dom, 0) < 0) {
        fprintf(stderr, "[ERROR] Failed to reboot domain %s\n", domName);
        virDomainFree(dom);
        virConnectClose(conn);
        return -1;
    }

    printf("[INFO] Domain %s reboot requested\n", domName);

    virDomainFree(dom);
    virConnectClose(conn);
    return 0;
}

/* -------- Migration à chaud -------- */
/*
 * src_uri   : URI de l’hyperviseur source (ex: qemu:///system ou qemu+ssh://user@machine1/system)
 * domName   : nom de la VM à migrer sur la source
 * dest_host : host ou IP de destination (ex: "machine1", "192.168.1.50")
 * dest_name : nom de la VM sur la destination (NULL ou "" => même nom)
 */
int migrate_domain(const char *src_uri,
                   const char *domName,
                   const char *dest_host,
                   const char *dest_name)
{
    if (!src_uri || !domName || !dest_host) {
        fprintf(stderr, "[ERROR] migrate_domain: invalid argument(s)\n");
        return -1;
    }

    virConnectPtr conn = open_conn(src_uri);
    if (!conn)
        return -1;

    virDomainPtr dom = virDomainLookupByName(conn, domName);
    if (!dom) {
        fprintf(stderr, "[ERROR] Domain %s not found on %s\n", domName, src_uri);
        virConnectClose(conn);
        return -1;
    }

    char dest_uri[512];
    snprintf(dest_uri, sizeof(dest_uri), "qemu+ssh://%s/system", dest_host);

    unsigned long flags =
        VIR_MIGRATE_LIVE |          /* VM allumée */
        VIR_MIGRATE_TUNNELLED |     /* tunnel SSH */
        VIR_MIGRATE_PERSIST_DEST |  /* définie de façon persistante sur la dest */
        VIR_MIGRATE_UNDEFINE_SOURCE;/* supprimée sur la source */

    const char *final_name = (dest_name && dest_name[0]) ? dest_name : domName;

    if (virDomainMigrateToURI2(dom,
                               dest_uri,
                               NULL,      /* params */
                               NULL,      /* params_values */
                               flags,
                               final_name,
                               0) < 0) {  /* bandwidth = 0 (par défaut) */
        fprintf(stderr, "[ERROR] Migration of %s to %s failed\n", domName, dest_uri);
        virDomainFree(dom);
        virConnectClose(conn);
        return -1;
    }

    printf("[INFO] Migration of %s to %s (%s) succeeded\n",
           domName, dest_uri, final_name);

    virDomainFree(dom);
    virConnectClose(conn);
    return 0;
}
