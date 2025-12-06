/* example ex13.c */
/* compile with: gcc -g -Wall ex13.c -o ex13 -lvirt */
#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

int main(int argc, char *argv[])
{
	virConnectPtr conn;
	virNodeInfo nodeinfo;
	unsigned long long *freemem;
	int i;
	int numnodes;

	conn = virConnectOpen("qemu:///system");
	if (conn == NULL) {
		fprintf(stderr, "Failed to open connection to qemu:///system\n");
		return 1;
	}

	/* first, get the node info.  This includes the number of nodes
	 *      * in the host in nodeinfo.nodes
	 *           */
	virNodeGetInfo(conn, &nodeinfo);

	/* allocate an array to hold all of the node free memory information */
	freemem = malloc(nodeinfo.nodes * sizeof(unsigned long long));

	/* fetch all the numa node free memory information from libvirt */
	numnodes = virNodeGetCellsFreeMemory(conn, freemem, 0, nodeinfo.nodes);

	for (i = 0; i < numnodes; i++)
		fprintf(stdout, "Node %d: %llukb free memory\n", i, freemem[i]);

	free(freemem);

	virConnectClose(conn);
	return 0;
}
