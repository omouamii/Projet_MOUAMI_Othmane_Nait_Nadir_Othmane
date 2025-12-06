/* example ex4.c */
/* compile with: gcc -g -Wall ex4.c -o ex4 -lvirt */
#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <string.h>

static int authCreds[] = {
	VIR_CRED_AUTHNAME,
	VIR_CRED_PASSPHRASE,
};

static int authCb(virConnectCredentialPtr cred, unsigned int ncred, void *cbdata)
{
	int i;
	char buf[1024];

	for (i = 0; i < ncred; i++) {
		if (cred[i].type == VIR_CRED_AUTHNAME) {
			printf("%s: ", cred[i].prompt);
			fflush(stdout);
			fgets(buf, sizeof(buf), stdin);
			buf[strlen(buf) - 1] = '\0';
			cred[i].result = strdup(buf);
			if (cred[i].result == NULL)
				return -1;
			cred[i].resultlen = strlen(cred[i].result);
		}
		else if (cred[i].type == VIR_CRED_PASSPHRASE) {
			printf("%s: ", cred[i].prompt);
			fflush(stdout);
			fgets(buf, sizeof(buf), stdin);
			buf[strlen(buf) - 1] = '\0';
			cred[i].result = strdup(buf);
			if (cred[i].result == NULL)
				return -1;
			cred[i].resultlen = strlen(cred[i].result);
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
	virConnectPtr conn;
	virConnectAuth auth;

	auth.credtype = authCreds;
	auth.ncredtype = sizeof(authCreds)/sizeof(int);
	auth.cb = authCb;
	auth.cbdata = NULL;

	conn = virConnectOpenAuth("qemu+tcp://localhost/system", &auth, 0);
	if (conn == NULL) {
		fprintf(stderr, "Failed to open connection to qemu+tcp://localhost/system\n");
		return 1;
	}
	virConnectClose(conn);
	return 0;
}
