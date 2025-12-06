#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

int main() {

    const char *uri_src = "qemu:///system";
    const char *uri_dst = "qemu+ssh://user@machine1/system";

    virConnectPtr src = virConnectOpen(uri_src);
    if (!src) return 1;

    virConnectPtr dst = virConnectOpen(uri_dst);
    if (!dst) {
        virConnectClose(src);
        return 1;
    }

    virDomainPtr dom = virDomainLookupByName(src, "vm10");
    if (!dom) {
        virConnectClose(src);
        virConnectClose(dst);
        return 1;
    }

    unsigned long flags =
        VIR_MIGRATE_LIVE |
        VIR_MIGRATE_UNSAFE |
        VIR_MIGRATE_TUNNELLED |
        VIR_MIGRATE_PEER2PEER;

    virDomainPtr res = virDomainMigrate(
        dom, dst, flags, NULL, NULL, 0
    );

    if (!res) {
        printf("Migration echouee.\n");
    } else {
        printf("Migration reussie.\n");
        virDomainFree(res);
    }

    virDomainFree(dom);
    virConnectClose(src);
    virConnectClose(dst);
    return 0;
}
