/* example ex11.c */
/* compile with: gcc -g -Wall ex11.c -o ex11 -lvirt */
#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

int main(int argc, char *argv[])
{
	virConnectPtr conn;
	unsigned long long node_free_memory;

	conn = virConnectOpen("qemu:///system");
	if (conn == NULL) {
		fprintf(stderr, "Failed to open connection to qemu:///system\n");
		return 1;
	}

	node_free_memory = virNodeGetFreeMemory(conn);
	fprintf(stdout, "Node free memory: %llu\n", node_free_memory);

	virConnectClose(conn);
	return 0;
}
