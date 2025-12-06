/* example ex9.c */
/* compile with: gcc -g -Wall ex9.c -o ex9 -lvirt */
#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

int main(int argc, char *argv[])
{
	virConnectPtr conn;
	char *host;

	conn = virConnectOpen("qemu:///system");
	if (conn == NULL) {
		fprintf(stderr, "Failed to open connection to qemu:///system\n");
		return 1;
	}

	host = virConnectGetHostname(conn);
	fprintf(stdout, "Hostname:%s\n", host);
	free(host);

	virConnectClose(conn);
	return 0;
}
