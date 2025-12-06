/* example ex15.c */
/* compile with: gcc -g -Wall ex15.c -o ex15 -lvirt */
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

	virConnectGetVersion(conn, &ver);

	fprintf(stdout, "Version: %lu\n", ver);

	virConnectClose(conn);
	return 0;
}
