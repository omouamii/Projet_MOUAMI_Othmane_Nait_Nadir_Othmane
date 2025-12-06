/* example ex20.c */
/* compile with: gcc -g -Wall ex20.c -o ex20 -lvirt */
#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

int main(int argc, char *argv[])
{
	virConnectPtr conn;
	virSecurityModel secmodel;

	conn = virConnectOpen("qemu:///system");
	if (conn == NULL) {
		fprintf(stderr, "Failed to open connection to qemu:///system\n");
		return 1;
	}

	virNodeGetSecurityModel(conn, &secmodel);

	fprintf(stdout, "Security Model: %s\n", secmodel.model);
	fprintf(stdout, "Security DOI:   %s\n", secmodel.doi);

	virConnectClose(conn);
	return 0;
}
