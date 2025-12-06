/* example ex10.c */
/* compile with: gcc -g -Wall ex10.c -o ex10 -lvirt */
#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

int main(int argc, char *argv[])
{
	virConnectPtr conn;
	int vcpus;

	conn = virConnectOpen("qemu:///system");
	if (conn == NULL) {
		fprintf(stderr, "Failed to open connection to qemu:///system\n");
		return 1;
	}

	vcpus = virConnectGetMaxVcpus(conn, NULL);
	fprintf(stdout, "Maximum support virtual CPUs: %d\n", vcpus);

	virConnectClose(conn);
	return 0;
}
