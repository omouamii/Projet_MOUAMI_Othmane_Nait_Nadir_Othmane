/* example ex18.c */
/* compile with: gcc -g -Wall ex18.c -o ex18 -lvirt */
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

	fprintf(stdout, "Connection is encrypted: %d\n", virConnectIsEncrypted(conn));

	virConnectClose(conn);
	return 0;
}
