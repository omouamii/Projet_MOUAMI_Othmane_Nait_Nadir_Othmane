/* example ex28.c */
/* compile with: gcc -g -Wall ex28.c -o ex28 -lvirt */
#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

static void customConnErrorFunc(void *userdata, virErrorPtr err)
{
	fprintf(stderr, "Connection handler, failure of libvirt library call:\n");
	fprintf(stderr, " Code: %d\n", err->code);
	fprintf(stderr, " Domain: %d\n", err->domain);
	fprintf(stderr, " Message: %s\n", err->message);
	fprintf(stderr, " Level: %d\n", err->level);
	fprintf(stderr, " str1: %s\n", err->str1);
	fprintf(stderr, " str2: %s\n", err->str2);
	fprintf(stderr, " str3: %s\n", err->str3);
	fprintf(stderr, " int1: %d\n", err->int1);
	fprintf(stderr, " int2: %d\n", err->int2);
}

static void customGlobalErrorFunc(void *userdata, virErrorPtr err)
{
	fprintf(stderr, "Global handler, failure of libvirt library call:\n");
	fprintf(stderr, " Code: %d\n", err->code);
	fprintf(stderr, " Domain: %d\n", err->domain);
	fprintf(stderr, " Message: %s\n", err->message);
	fprintf(stderr, " Level: %d\n", err->level);
	fprintf(stderr, " str1: %s\n", err->str1);
	fprintf(stderr, " str2: %s\n", err->str2);
	fprintf(stderr, " str3: %s\n", err->str3);
	fprintf(stderr, " int1: %d\n", err->int1);
	fprintf(stderr, " int2: %d\n", err->int2);
}

int main(int argc, char *argv[])
{
	virConnectPtr conn1;
	virConnectPtr conn2;
	virConnectPtr conn3;
	virConnectPtr conn4;
	char *caps;
	virError err;
	char *hostname;
	virErrorPtr err2;
	int vcpus;
	unsigned long long node_free_memory;
	virNodeInfo nodeinfo;
	unsigned long long *node_cells_freemem;
	int numnodes;
	int i;
	const char *type;
	unsigned long virtVersion;
	unsigned long libvirtVersion;
	char *uri;
	int is_encrypted;
	int is_secure;
	virSecurityModel secmodel;

	/* set a global error function for all connections */
	virSetErrorFunc(NULL, customGlobalErrorFunc);

	/* open a connection using the old-style virConnectOpen */
	conn1 = virConnectOpen("qemu:///system");
	if (conn1 == NULL) {
		fprintf(stderr, "Failed to open connection to qemu:///system\n");
		return 1;
	}

	/* open a read-only connection using the old-style virConnectOpenReadOnly */
	conn2 = virConnectOpenReadOnly("qemu:///system");
	if (conn2 == NULL) {
		fprintf(stderr, "Failed to open connection to qemu:///system\n");
		virConnectClose(conn1);
		return 2;
	}

	/* open a connection using the new-style virConnectOpenAuth */
	conn3 = virConnectOpenAuth("qemu:///system", virConnectAuthPtrDefault, 0);
	if (conn3 == NULL) {
		fprintf(stderr, "Failed to open connection to qemu:///system\n");
		virConnectClose(conn2);
		virConnectClose(conn1);
		return 3;
	}

	/* open a read-only connection using the new-style virConnectOpenAuth */
	conn4 = virConnectOpenAuth("qemu:///system", virConnectAuthPtrDefault,
			VIR_CONNECT_RO);
	if (conn4 == NULL) {
		fprintf(stderr, "Failed to open connection to qemu:///system\n");
		virConnectClose(conn3);
		virConnectClose(conn2);
		virConnectClose(conn1);
		return 3;
	}

	/* conn1 will use a different error function */
	virConnSetErrorFunc(conn1, NULL, customConnErrorFunc);

	/* test out error handling */
	/* this failure will use customConnErrorFunc */
	if (virConnectGetVersion(conn1, NULL) < 0)
		fprintf(stderr, "virConnectGetVersion failed\n");
	/* this failure will use customGlobalErrorFunc */
	if (virConnectGetVersion(conn2, NULL) < 0)
		fprintf(stderr, "virConnectGetVersion failed\n");

	/* clear out the per-connection error function; we will report errors with
	 *      * the vir*Error() functions below*/
	virConnSetErrorFunc(conn1, NULL, NULL);

	/* clear out the global error function; we will report errors with
	 *      * the vir*Error() functions below*/
	virSetErrorFunc(NULL, NULL);

	/* get the capabilities of conn1 */
	caps = virConnectGetCapabilities(conn1);
	if (caps == NULL) {
		virCopyLastError(&err);
		fprintf(stderr, "virConnectGetCapabilities failed: %s\n", err.message);
		virResetError(&err);
	}
	fprintf(stdout, "Capabilities of connection 1:\n%s\n", caps);
	free(caps);

	/* get the hostname reported from conn2 */
	hostname = virConnectGetHostname(conn2);
	if (hostname == NULL) {
		err2 = virSaveLastError();
		fprintf(stderr, "virConnectGetVersion failed: %s\n", err2->message);
		virFreeError(err2);
	}
	fprintf(stdout, "Connection 2 hostname: %s\n", hostname);
	free(hostname);

	/* get the maximum number of vcpus supported by conn3 */
	vcpus = virConnectGetMaxVcpus(conn3, NULL);
	if (vcpus < 0) {
		err2 = virSaveLastError();
		fprintf(stderr, "virConnectGetMaxVcpus failed: %s\n", err2->message);
		virFreeError(err2);
	}
	fprintf(stdout, "Maximum number of cpus supported on connection 3: %d\n",
			vcpus);

	/* get the amount of free memory available on the node from conn4 */
	node_free_memory = virNodeGetFreeMemory(conn4);
	if (node_free_memory == 0) {
		err2 = virSaveLastError();
		fprintf(stderr, "virNodeGetFreeMemory failed: %s\n", err2->message);
		virFreeError(err2);
	}

	/* get the node information from conn1 */
	if (virNodeGetInfo(conn1, &nodeinfo) < 0) {
		err2 = virSaveLastError();
		fprintf(stderr, "virNodeGetInfo failed: %s\n", err2->message);
		virFreeError(err2);
	}
	fprintf(stdout, "Node information from connection 1:\n");
	fprintf(stdout, " Model: %s\n", nodeinfo.model);
	fprintf(stdout, " Memory size: %lukb\n", nodeinfo.memory);
	fprintf(stdout, " Number of CPUs: %u\n", nodeinfo.cpus);
	fprintf(stdout, " MHz of CPUs: %u\n", nodeinfo.mhz);
	fprintf(stdout, " Number of NUMA nodes: %u\n", nodeinfo.nodes);
	fprintf(stdout, " Number of CPU sockets: %u\n", nodeinfo.sockets);
	fprintf(stdout, " Number of CPU cores per socket: %u\n", nodeinfo.cores);
	fprintf(stdout, " Number of CPU threads per core: %u\n", nodeinfo.threads);

	/* get the amount of memory in each of the NUMA nodes from connection 1 */
	/* we already know the number of nodes from virNodeGetInfo above */
	node_cells_freemem = malloc(nodeinfo.nodes * sizeof(unsigned long long));
	numnodes = virNodeGetCellsFreeMemory(conn1, node_cells_freemem, 0,
			nodeinfo.nodes);
	if (numnodes < 0) {
		err2 = virSaveLastError();
		fprintf(stderr, "virNodeGetCellsFreeMemory failed: %s\n",
				err2->message);
		virFreeError(err2);
	}
	fprintf(stdout, "Node Cells Free Memory from connection 1:\n");
	for (i = 0; i < numnodes; i++)
		fprintf(stdout, " Cell %d: %llukb free memory\n", i,
				node_cells_freemem[i]);
	free(node_cells_freemem);

	/* get the virtualization type from conn2 */
	type = virConnectGetType(conn2);
	if (type == NULL) {
		err2 = virSaveLastError();
		fprintf(stderr, "virConnectGetType failed: %s\n", err2->message);
		virFreeError(err2);
	}
	fprintf(stdout, "Virtualization type from connection 2: %s\n", type);

	/* get the virtualization version from conn3 */
	if (virConnectGetVersion(conn3, &virtVersion) < 0) {
		err2 = virSaveLastError();
		fprintf(stderr, "virConnectGetVersion failed: %s\n", err2->message);
		virFreeError(err2);
	}
	fprintf(stdout, "Virtualization version from connection 3: %lu\n",
			virtVersion);

	/* get the libvirt version from conn4 */
	if (virConnectGetLibVersion(conn4, &libvirtVersion) < 0) {
		err2 = virSaveLastError();
		fprintf(stderr, "virConnectGetLibVersion failed: %s\n", err2->message);
		virFreeError(err2);
	}
	fprintf(stdout, "Libvirt version from connection 4: %lu\n", libvirtVersion);

	/* get the URI from conn1 */
	uri = virConnectGetURI(conn1);
	if (uri == NULL) {
		err2 = virSaveLastError();
		fprintf(stderr, "virConnectGetURI failed: %s\n", err2->message);
		virFreeError(err2);
	}
	fprintf(stdout, "Canonical URI from connection 1: %s\n", uri);
	free(uri);

	/* is the connection encrypted? from conn2 */
	is_encrypted = virConnectIsEncrypted(conn2);
	if (is_encrypted < 0) {
		err2 = virSaveLastError();
		fprintf(stderr, "virConnectIsEncrypted failed: %s\n", err2->message);
		virFreeError(err2);
	}
	fprintf(stdout, "Connection 2 %s encrypted\n",
			(is_encrypted == 0) ? "is not" : "is");

	/* is the connection secure? from conn3 */
	is_secure = virConnectIsSecure(conn3);
	if (is_secure < 0) {
		err2 = virSaveLastError();
		fprintf(stderr, "virConnectIsSecure failed: %s\n", err2->message);
		virFreeError(err2);
	}
	fprintf(stdout, "Connection 3 %s secure\n",
			(is_secure == 0) ? "is not" : "is");

	/* get the security model from conn4 */
	if (virNodeGetSecurityModel(conn4, &secmodel) < 0) {
		err2 = virSaveLastError();
		fprintf(stderr, "virNodeGetSecurityModel failed: %s\n", err2->message);
		virFreeError(err2);
	}
	fprintf(stdout, "Connection 4 Security Model = %s, DOI = %s\n",
			secmodel.model, secmodel.doi);

	virConnectClose(conn4);
	virConnectClose(conn3);
	virConnectClose(conn2);
	virConnectClose(conn1);
	return 0;
}

