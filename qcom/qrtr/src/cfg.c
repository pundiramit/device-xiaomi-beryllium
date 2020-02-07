#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/qrtr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "addr.h"
#include "libqrtr.h"
#include "logging.h"

static void usage(const char *progname)
{
	fprintf(stderr, "%s <node-id>\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	unsigned long addrul;
	uint32_t addr;
	char *ep;
	const char *progname = basename(argv[0]);

	qlog_setup(progname, false);

	if (argc != 2)
		usage(progname);

	addrul = strtoul(argv[1], &ep, 10);
	if (argv[1][0] == '\0' || *ep != '\0' || addrul >= UINT_MAX)
		usage(progname);
	addr = addrul;
	qrtr_set_address(addr);

	return 0;
}
