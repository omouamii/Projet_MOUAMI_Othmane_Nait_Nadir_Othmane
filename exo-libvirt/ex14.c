/* example ex14.c */
/* compile with: gcc -g -Wall ex14.c -o ex14 -lvirt */
#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

int main(int argc, char *argv[])
{
	virConnectPtr conn;

	conn = virConnectOpen("qemu:///system");
	if (conn == NULL) {
		fprintf(stderr, "Failed to open connection to qemu:///system\n");
		return 1;
	}

	fprintf(stdout, "Virtualization type: %s\n", virConnectGetType(conn));

	virConnectClose(conn);
	return 0;
}
