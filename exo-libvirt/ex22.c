/* example ex22.c */
/* compile with: gcc -g -Wall ex22.c -o ex22 -lvirt */
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

	/* set a global error function for all connections */
	virSetErrorFunc(NULL, customGlobalErrorFunc);

	conn1 = virConnectOpen("qemu:///system");
	if (conn1 == NULL) {
		fprintf(stderr, "Failed to open connection to qemu:///system\n");
		return 1;
	}

	conn2 = virConnectOpen("qemu:///system");
	if (conn2 == NULL) {
		fprintf(stderr, "Failed to open connection to qemu:///system\n");
		virConnectClose(conn1);
		return 2;
	}

	/* conn1 will use a different error function */
	virConnSetErrorFunc(conn1, NULL, customConnErrorFunc);

	/* this failure will use customConnErrorFunc */
	if (virConnectGetVersion(conn1, NULL) < 0)
		fprintf(stderr, "virConnectGetVersion failed\n");

	/* this failure will use customGlobalErrorFunc */
	if (virConnectGetVersion(conn2, NULL) < 0)
		fprintf(stderr, "virConnectGetVersion failed\n");

	virConnectClose(conn2);
	virConnectClose(conn1);
	return 0;
}
