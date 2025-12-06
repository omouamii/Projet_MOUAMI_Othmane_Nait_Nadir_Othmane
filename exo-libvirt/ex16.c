/* example ex16.c */
/* compile with: gcc -g -Wall ex16.c -o ex16 -lvirt */
#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

int main(int argc, char *argv[])
{
	virConnectPtr conn;
	unsigned long ver;

	conn = virConnectOpen("qemu:///system");
	if (conn == NULL) {
		fprintf(stderr, "Failed to open connection to qemu:///system\n");
		return 1;
	}

	virConnectGetLibVersion(conn, &ver);

	fprintf(stdout, "Libvirt Version: %lu\n", ver);

	virConnectClose(conn);
	return 0;
}
