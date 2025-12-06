/* example ex21.c */
/* compile with: gcc -g -Wall ex21.c -o ex21 -lvirt */
#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

static void customErrorFunc(void *userdata, virErrorPtr err)
{
	fprintf(stderr, "Failure of libvirt library call:\n");
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
	virConnectPtr conn;

	virSetErrorFunc(NULL, customErrorFunc);

	conn = virConnectOpen("qemu:///system");
	if (conn == NULL) {
		fprintf(stderr, "Failed to open connection to qemu:///system\n");
		return 1;
	}

	if (virConnectGetVersion(conn, NULL) < 0)
		fprintf(stderr, "virConnectGetVersion failed\n");

	virConnectClose(conn);
	return 0;
}
