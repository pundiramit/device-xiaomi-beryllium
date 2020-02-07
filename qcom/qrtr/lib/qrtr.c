#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <linux/qrtr.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <poll.h>

#include "libqrtr.h"
#include "logging.h"
#include "ns.h"

static int qrtr_getname(int sock, struct sockaddr_qrtr *sq)
{
	socklen_t sl = sizeof(*sq);
	int rc;

	rc = getsockname(sock, (void *)sq, &sl);
	if (rc) {
		PLOGE("getsockname()");
		return -1;
	}

	if (sq->sq_family != AF_QIPCRTR || sl != sizeof(*sq))
		return -1;

	return 0;
}

int qrtr_open(int rport)
{
	struct timeval tv;
	int sock;
	int rc;

	sock = socket(AF_QIPCRTR, SOCK_DGRAM, 0);
	if (sock < 0) {
		PLOGE("socket(AF_QIPCRTR)");
		return -1;
	}

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	rc = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	if (rc) {
		PLOGE("setsockopt(SO_RCVTIMEO)");
		goto err;
	}

	if (rport != 0) {
		struct sockaddr_qrtr sq;

		sq.sq_family = AF_QIPCRTR;
		sq.sq_node = 1;
		sq.sq_port = rport;

		rc = bind(sock, (void *)&sq, sizeof(sq));
		if (rc < 0) {
			PLOGE("bind(%d)", rport);
			goto err;
		}
	}

	return sock;
err:
	close(sock);
	return -1;
}

void qrtr_close(int sock)
{
	close(sock);
}

int qrtr_sendto(int sock, uint32_t node, uint32_t port, const void *data, unsigned int sz)
{
	struct sockaddr_qrtr sq;
	int rc;

	sq.sq_family = AF_QIPCRTR;
	sq.sq_node = node;
	sq.sq_port = port;

	rc = sendto(sock, data, sz, 0, (void *)&sq, sizeof(sq));
	if (rc < 0) {
		PLOGE("sendto()");
		return -1;
	}

	return 0;
}

int qrtr_new_server(int sock, uint32_t service, uint16_t version, uint16_t instance)
{
	struct qrtr_ctrl_pkt pkt;
	struct sockaddr_qrtr sq;

	if (qrtr_getname(sock, &sq))
		return -1;

	memset(&pkt, 0, sizeof(pkt));

	pkt.cmd = cpu_to_le32(QRTR_TYPE_NEW_SERVER);
	pkt.server.service = cpu_to_le32(service);
	pkt.server.instance = cpu_to_le32(instance << 8 | version);

	return qrtr_sendto(sock, sq.sq_node, QRTR_PORT_CTRL, &pkt, sizeof(pkt));
}

int qrtr_remove_server(int sock, uint32_t service, uint16_t version, uint16_t instance)
{
	struct qrtr_ctrl_pkt pkt;
	struct sockaddr_qrtr sq;

	if (qrtr_getname(sock, &sq))
		return -1;

	memset(&pkt, 0, sizeof(pkt));

	pkt.cmd = cpu_to_le32(QRTR_TYPE_DEL_SERVER);
	pkt.server.service = cpu_to_le32(service);
	pkt.server.instance = cpu_to_le32(instance << 8 | version);
	pkt.server.node = cpu_to_le32(sq.sq_node);
	pkt.server.port = cpu_to_le32(sq.sq_port);

	return qrtr_sendto(sock, sq.sq_node, QRTR_PORT_CTRL, &pkt, sizeof(pkt));
}

int qrtr_publish(int sock, uint32_t service, uint16_t version, uint16_t instance)
{
	return qrtr_new_server(sock, service, version, instance);
}

int qrtr_bye(int sock, uint32_t service, uint16_t version, uint16_t instance)
{
	return qrtr_remove_server(sock, service, version, instance);
}

int qrtr_new_lookup(int sock, uint32_t service, uint16_t version, uint16_t instance)
{
	struct qrtr_ctrl_pkt pkt;
	struct sockaddr_qrtr sq;

	if (qrtr_getname(sock, &sq))
		return -1;

	memset(&pkt, 0, sizeof(pkt));

	pkt.cmd = cpu_to_le32(QRTR_TYPE_NEW_LOOKUP);
	pkt.server.service = cpu_to_le32(service);
	pkt.server.instance = cpu_to_le32(instance << 8 | version);

	return qrtr_sendto(sock, sq.sq_node, QRTR_PORT_CTRL, &pkt, sizeof(pkt));
}

int qrtr_remove_lookup(int sock, uint32_t service, uint16_t version, uint16_t instance)
{
	struct qrtr_ctrl_pkt pkt;
	struct sockaddr_qrtr sq;

	if (qrtr_getname(sock, &sq))
		return -1;

	memset(&pkt, 0, sizeof(pkt));

	pkt.cmd = cpu_to_le32(QRTR_TYPE_DEL_LOOKUP);
	pkt.server.service = cpu_to_le32(service);
	pkt.server.instance = cpu_to_le32(instance << 8 | version);
	pkt.server.node = cpu_to_le32(sq.sq_node);
	pkt.server.port = cpu_to_le32(sq.sq_port);

	return qrtr_sendto(sock, sq.sq_node, QRTR_PORT_CTRL, &pkt, sizeof(pkt));
}

int qrtr_poll(int sock, unsigned int ms)
{
	struct pollfd fds;

	fds.fd = sock;
	fds.revents = 0;
	fds.events = POLLIN | POLLERR;

	return poll(&fds, 1, ms);
}

int qrtr_recv(int sock, void *buf, unsigned int bsz)
{
	int rc;

	rc = recv(sock, buf, bsz, 0);
	if (rc < 0)
		PLOGE("recv()");
	return rc;
}

int qrtr_recvfrom(int sock, void *buf, unsigned int bsz, uint32_t *node, uint32_t *port)
{
	struct sockaddr_qrtr sq;
	socklen_t sl;
	int rc;

	sl = sizeof(sq);
	rc = recvfrom(sock, buf, bsz, 0, (void *)&sq, &sl);
	if (rc < 0) {
		PLOGE("recvfrom()");
		return rc;
	}
	if (node)
		*node = sq.sq_node;
	if (port)
		*port = sq.sq_port;
	return rc;
}

int qrtr_decode(struct qrtr_packet *dest, void *buf, size_t len,
		const struct sockaddr_qrtr *sq)
{
	const struct qrtr_ctrl_pkt *ctrl = buf;

	if (sq->sq_port == QRTR_PORT_CTRL){
		if (len < sizeof(*ctrl))
			return -EMSGSIZE;

		dest->type = le32_to_cpu(ctrl->cmd);
		switch (dest->type) {
		case QRTR_TYPE_BYE:
			dest->node = le32_to_cpu(ctrl->client.node);
			break;
		case QRTR_TYPE_DEL_CLIENT:
			dest->node = le32_to_cpu(ctrl->client.node);
			dest->port = le32_to_cpu(ctrl->client.port);
			break;
		case QRTR_TYPE_NEW_SERVER:
		case QRTR_TYPE_DEL_SERVER:
			dest->node = le32_to_cpu(ctrl->server.node);
			dest->port = le32_to_cpu(ctrl->server.port);
			dest->service = le32_to_cpu(ctrl->server.service);
			dest->version = le32_to_cpu(ctrl->server.instance) & 0xff;
			dest->instance = le32_to_cpu(ctrl->server.instance) >> 8;
			break;
		default:
			dest->type = 0;
		}
	} else {
		dest->type = QRTR_TYPE_DATA;
		dest->node = sq->sq_node;
		dest->port = sq->sq_port;

		dest->data = buf;
		dest->data_len = len;
	}

	return 0;
}
