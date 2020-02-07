#include <err.h>
#include <errno.h>
#include <linux/qrtr.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "libqrtr.h"
#include "logging.h"

void qrtr_set_address(uint32_t addr)
{
	struct {
		struct nlmsghdr nh;
		struct ifaddrmsg ifa;
		char attrbuf[32];
	} req;
	struct {
		struct nlmsghdr nh;
		struct nlmsgerr err;
	} resp;
	struct sockaddr_qrtr sq;
	struct rtattr *rta;
	socklen_t sl = sizeof(sq);
	int sock;
	int ret;

	/* Trigger loading of the qrtr kernel module */
	sock = socket(AF_QIPCRTR, SOCK_DGRAM, 0);
	if (sock < 0)
		PLOGE_AND_EXIT("failed to create AF_QIPCRTR socket");

	ret = getsockname(sock, (void*)&sq, &sl);
	if (ret < 0)
		PLOGE_AND_EXIT("getsockname()");
	close(sock);

	/* Skip configuring the address, if it's same as current */
	if (sl == sizeof(sq) && sq.sq_node == addr)
		return;

	sock = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	if (sock < 0)
		PLOGE_AND_EXIT("failed to create netlink socket");

	memset(&req, 0, sizeof(req));
	req.nh.nlmsg_len = NLMSG_SPACE(sizeof(struct ifaddrmsg));
	req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	req.nh.nlmsg_type = RTM_NEWADDR;
	req.ifa.ifa_family = AF_QIPCRTR;

	rta = (struct rtattr *)(((char *) &req) + req.nh.nlmsg_len);
	rta->rta_type = IFA_LOCAL;
	rta->rta_len = RTA_LENGTH(sizeof(addr));
	memcpy(RTA_DATA(rta), &addr, sizeof(addr));

	req.nh.nlmsg_len += rta->rta_len;

	ret = send(sock, &req, req.nh.nlmsg_len, 0);
	if (ret < 0)
		PLOGE_AND_EXIT("failed to send netlink request");

	ret = recv(sock, &resp, sizeof(resp), 0);
	if (ret < 0)
		PLOGE_AND_EXIT("failed to receive netlink response");

	if (resp.nh.nlmsg_type == NLMSG_ERROR && resp.err.error != 0) {
		errno = -resp.err.error;
		PLOGE_AND_EXIT("failed to configure node id");
	}

	close(sock);
}
