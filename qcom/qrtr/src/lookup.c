#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <linux/qrtr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "libqrtr.h"
#include "logging.h"
#include "ns.h"
#include "util.h"

#define DIAG_SERVICE 4097

static const struct {
	unsigned int service;
	unsigned int ifilter;
	const char *name;
} common_names[] = {
	{ 0, 0, "Control service" },
	{ 1, 0, "Wireless Data Service" },
	{ 2, 0, "Device Management Service" },
	{ 3, 0, "Network Access Service" },
	{ 4, 0, "Quality Of Service service" },
	{ 5, 0, "Wireless Messaging Service" },
	{ 6, 0, "Position Determination Service" },
	{ 7, 0, "Authentication service" },
	{ 8, 0, "AT service" },
	{ 9, 0, "Voice service" },
	{ 10, 0, "Card Application Toolkit service (v2)" },
	{ 11, 0, "User Identity Module service" },
	{ 12, 0, "Phonebook Management service" },
	{ 13, 0, "QCHAT service" },
	{ 14, 0, "Remote file system service" },
	{ 15, 0, "Test service" },
	{ 16, 0, "Location service (~ PDS v2)" },
	{ 17, 0, "Specific absorption rate service" },
	{ 18, 0, "IMS settings service" },
	{ 19, 0, "Analog to digital converter driver service" },
	{ 20, 0, "Core sound driver service" },
	{ 21, 0, "Modem embedded file system service" },
	{ 22, 0, "Time service" },
	{ 23, 0, "Thermal sensors service" },
	{ 24, 0, "Thermal mitigation device service" },
	{ 25, 0, "Service access proxy service" },
	{ 26, 0, "Wireless data administrative service" },
	{ 27, 0, "TSYNC control service" },
	{ 28, 0, "Remote file system access service" },
	{ 29, 0, "Circuit switched videotelephony service" },
	{ 30, 0, "Qualcomm mobile access point service" },
	{ 31, 0, "IMS presence service" },
	{ 32, 0, "IMS videotelephony service" },
	{ 33, 0, "IMS application service" },
	{ 34, 0, "Coexistence service" },
	{ 36, 0, "Persistent device configuration service" },
	{ 38, 0, "Simultaneous transmit service" },
	{ 39, 0, "Bearer independent transport service" },
	{ 40, 0, "IMS RTP service" },
	{ 41, 0, "RF radiated performance enhancement service" },
	{ 42, 0, "Data system determination service" },
	{ 43, 0, "Subsystem control service" },
	{ 49, 0, "IPA control service" },
	{ 51, 0, "CoreSight remote tracing service" },
	{ 52, 0, "Dynamic Heap Memory Sharing" },
	{ 64, 0, "Service registry locator service" },
	{ 66, 0, "Service registry notification service" },
	{ 69, 0, "ATH10k WLAN firmware service" },
	{ 224, 0, "Card Application Toolkit service (v1)" },
	{ 225, 0, "Remote Management Service" },
	{ 226, 0, "Open Mobile Alliance device management service" },
	{ 312, 0, "QBT1000 Ultrasonic Fingerprint Sensor service" },
	{ 769, 0, "SLIMbus control service" },
	{ 771, 0, "Peripheral Access Control Manager service" },
	{ DIAG_SERVICE, 0, "DIAG service" },
};

static const char *diag_instance_base_str(unsigned int instance_base)
{
	switch (instance_base) {
		case 0: return "MODEM";
		case 1: return "LPASS";
		case 2: return "WCNSS";
		case 3: return "SENSORS";
		case 4: return "CDSP";
		case 5: return "WDSP";
		default: return "<unk>";
	}
}

static const char *diag_instance_str(unsigned int instance)
{
	switch (instance) {
		case 0: return "CNTL";
		case 1: return "CMD";
		case 2: return "DATA";
		case 3: return "DCI_CMD";
		case 4: return "DCI";
		default: return "<unk>";
	}
}

static int get_diag_instance_info(char *str, size_t size, unsigned int instance)
{
	return snprintf(str, size, "%s:%s",
			diag_instance_base_str(instance >> 6),
			diag_instance_str(instance & 0x3f));
}

static unsigned int read_num_le(const char *str, int *rcp)
{
	unsigned int ret;
	char *e;

	if (*rcp)
		return 0;

	errno = 0;
	ret = strtoul(str, &e, 0);
	*rcp = -(errno || *e);

	return cpu_to_le32(ret);
}

int main(int argc, char **argv)
{
	struct qrtr_ctrl_pkt pkt;
	struct sockaddr_qrtr sq;
	unsigned int instance;
	unsigned int service;
	unsigned int version;
	unsigned int node;
	unsigned int port;
	socklen_t sl = sizeof(sq);
	struct timeval tv;
	int sock;
	int len;
	int rc;
	const char *progname = basename(argv[0]);

	qlog_setup(progname, false);

	rc = 0;
	memset(&pkt, 0, sizeof(pkt));

	switch (argc) {
	default:
		rc = -1;
		break;
	case 3: pkt.server.instance = read_num_le(argv[2], &rc);
	case 2: pkt.server.service = read_num_le(argv[1], &rc);
	case 1: break;
	}
	if (rc) {
		fprintf(stderr, "Usage: %s [<service> [<instance> [<filter>]]]\n", progname);
		exit(1);
	}

	sock = socket(AF_QIPCRTR, SOCK_DGRAM, 0);
	if (sock < 0)
		PLOGE_AND_EXIT("sock(AF_QIPCRTR)");

	rc = getsockname(sock, (void *)&sq, &sl);
	if (rc || sq.sq_family != AF_QIPCRTR || sl != sizeof(sq))
		PLOGE_AND_EXIT("getsockname()");

	sq.sq_port = QRTR_PORT_CTRL;

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	pkt.cmd = cpu_to_le32(QRTR_TYPE_NEW_LOOKUP);

	rc = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	if (rc)
		PLOGE_AND_EXIT("setsockopt(SO_RCVTIMEO)");

	rc = sendto(sock, &pkt, sizeof(pkt), 0, (void *)&sq, sizeof(sq));
	if (rc < 0)
		PLOGE_AND_EXIT("sendto()");

	printf("  Service Version Instance Node  Port\n");

	while ((len = recv(sock, &pkt, sizeof(pkt), 0)) > 0) {
		unsigned int type = le32_to_cpu(pkt.cmd);
		const char *name = NULL;
		unsigned int i;

		if (len < sizeof(pkt) || type != QRTR_TYPE_NEW_SERVER) {
			PLOGW("invalid/short packet");
			continue;
		}

		if (!pkt.server.service && !pkt.server.instance &&
		    !pkt.server.node && !pkt.server.port)
			break;

		service = le32_to_cpu(pkt.server.service);
		version = le32_to_cpu(pkt.server.instance) & 0xff;
		instance = le32_to_cpu(pkt.server.instance) >> 8;
		node = le32_to_cpu(pkt.server.node);
		port = le32_to_cpu(pkt.server.port);

		for (i = 0; i < sizeof(common_names)/sizeof(common_names[0]); ++i) {
			if (service != common_names[i].service)
				continue;
			if (instance &&
			   (instance & common_names[i].ifilter) != common_names[i].ifilter)
				continue;
			name = common_names[i].name;
		}
		if (!name)
			name = "<unknown>";

		if (service == DIAG_SERVICE) {
			char buf[24];
			instance = le32_to_cpu(pkt.server.instance);
			get_diag_instance_info(buf, sizeof(buf), instance);
			printf("%9d %7s %8d %4d %5d %s (%s)\n",
				service, "N/A", instance, node, port, name, buf);
		} else {
			printf("%9d %7d %8d %4d %5d %s\n",
				service, version, instance, node, port, name);
		}
	}

	if (len < 0)
		PLOGE_AND_EXIT("recv()");

	close(sock);

	return 0;
}
