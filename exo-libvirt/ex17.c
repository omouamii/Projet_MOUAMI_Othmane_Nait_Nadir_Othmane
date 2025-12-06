/* example ex17.c */
/* compile with: gcc -g -Wall ex17.c -o ex17 -lvirt */
#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

int main(int argc, char *argv[])
{
	virConnectPtr conn;
	char *uri;

	conn = virConnectOpen("qemu:///system");
	if (conn == NULL) {
		fprintf(stderr, "Failed to open connection to qemu:///system\n");
		return 1;
	}

	uri = virConnectGetURI(conn);

	fprintf(stdout, "Canonical URI: %s\n", uri);

	free(uri);

	virConnectClose(conn);
	return 0;
}
