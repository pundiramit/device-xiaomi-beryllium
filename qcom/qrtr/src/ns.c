#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <linux/qrtr.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "addr.h"
#include "hash.h"
#include "list.h"
#include "map.h"
#include "ns.h"
#include "util.h"
#include "waiter.h"

#include "libqrtr.h"
#include "logging.h"

static const char *ctrl_pkt_strings[] = {
	[QRTR_TYPE_HELLO]	= "hello",
	[QRTR_TYPE_BYE]		= "bye",
	[QRTR_TYPE_NEW_SERVER]	= "new-server",
	[QRTR_TYPE_DEL_SERVER]	= "del-server",
	[QRTR_TYPE_DEL_CLIENT]	= "del-client",
	[QRTR_TYPE_RESUME_TX]	= "resume-tx",
	[QRTR_TYPE_EXIT]	= "exit",
	[QRTR_TYPE_PING]	= "ping",
	[QRTR_TYPE_NEW_LOOKUP]	= "new-lookup",
	[QRTR_TYPE_DEL_LOOKUP]	= "del-lookup",
};

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

struct context {
	int sock;

	int local_node;

	struct sockaddr_qrtr bcast_sq;

	struct list lookups;
};

struct server_filter {
	unsigned int service;
	unsigned int instance;
	unsigned int ifilter;
};

struct lookup {
	unsigned int service;
	unsigned int instance;

	struct sockaddr_qrtr sq;
	struct list_item li;
};

struct server {
	unsigned int service;
	unsigned int instance;

	unsigned int node;
	unsigned int port;
	struct map_item mi;
	struct list_item qli;
};

struct node {
	unsigned int id;

	struct map_item mi;
	struct map services;
};

static struct map nodes;

static void server_mi_free(struct map_item *mi);

static struct node *node_get(unsigned int node_id)
{
	struct map_item *mi;
	struct node *node;
	int rc;

	mi = map_get(&nodes, hash_u32(node_id));
	if (mi)
		return container_of(mi, struct node, mi);

	node = calloc(1, sizeof(*node));
	if (!node)
		return NULL;

	node->id = node_id;

	rc = map_create(&node->services);
	if (rc)
		LOGE_AND_EXIT("unable to create map");

	rc = map_put(&nodes, hash_u32(node_id), &node->mi);
	if (rc) {
		map_destroy(&node->services);
		free(node);
		return NULL;
	}

	return node;
}

static int server_match(const struct server *srv, const struct server_filter *f)
{
	unsigned int ifilter = f->ifilter;

	if (f->service != 0 && srv->service != f->service)
		return 0;
	if (!ifilter && f->instance)
		ifilter = ~0;
	return (srv->instance & ifilter) == f->instance;
}

static int server_query(const struct server_filter *f, struct list *list)
{
	struct map_entry *node_me;
	struct map_entry *me;
	struct node *node;
	int count = 0;

	list_init(list);
	map_for_each(&nodes, node_me) {
		node = map_iter_data(node_me, struct node, mi);

		map_for_each(&node->services, me) {
			struct server *srv;

			srv = map_iter_data(me, struct server, mi);
			if (!server_match(srv, f))
				continue;

			list_append(list, &srv->qli);
			++count;
		}
	}

	return count;
}

static int service_announce_new(struct context *ctx,
				struct sockaddr_qrtr *dest,
				struct server *srv)
{
	struct qrtr_ctrl_pkt cmsg;
	int rc;

	LOGD("advertising new server [%d:%x]@[%d:%d]\n",
		srv->service, srv->instance, srv->node, srv->port);

	cmsg.cmd = cpu_to_le32(QRTR_TYPE_NEW_SERVER);
	cmsg.server.service = cpu_to_le32(srv->service);
	cmsg.server.instance = cpu_to_le32(srv->instance);
	cmsg.server.node = cpu_to_le32(srv->node);
	cmsg.server.port = cpu_to_le32(srv->port);

	rc = sendto(ctx->sock, &cmsg, sizeof(cmsg), 0,
		    (struct sockaddr *)dest, sizeof(*dest));
	if (rc < 0)
		PLOGW("sendto()");

	return rc;
}

static int service_announce_del(struct context *ctx,
				struct sockaddr_qrtr *dest,
				struct server *srv)
{
	struct qrtr_ctrl_pkt cmsg;
	int rc;

	LOGD("advertising removal of server [%d:%x]@[%d:%d]\n",
		srv->service, srv->instance, srv->node, srv->port);

	cmsg.cmd = cpu_to_le32(QRTR_TYPE_DEL_SERVER);
	cmsg.server.service = cpu_to_le32(srv->service);
	cmsg.server.instance = cpu_to_le32(srv->instance);
	cmsg.server.node = cpu_to_le32(srv->node);
	cmsg.server.port = cpu_to_le32(srv->port);

	rc = sendto(ctx->sock, &cmsg, sizeof(cmsg), 0,
		    (struct sockaddr *)dest, sizeof(*dest));
	if (rc < 0)
		PLOGW("sendto()");

	return rc;
}

static int lookup_notify(struct context *ctx, struct sockaddr_qrtr *to,
			 struct server *srv, bool new)
{
	struct qrtr_ctrl_pkt pkt = {};
	int rc;

	pkt.cmd = new ? QRTR_TYPE_NEW_SERVER : QRTR_TYPE_DEL_SERVER;
	if (srv) {
		pkt.server.service = cpu_to_le32(srv->service);
		pkt.server.instance = cpu_to_le32(srv->instance);
		pkt.server.node = cpu_to_le32(srv->node);
		pkt.server.port = cpu_to_le32(srv->port);
	}

	rc = sendto(ctx->sock, &pkt, sizeof(pkt), 0,
		    (struct sockaddr *)to, sizeof(*to));
	if (rc < 0)
		PLOGW("send lookup result failed");
	return rc;
}

static int annouce_servers(struct context *ctx, struct sockaddr_qrtr *sq)
{
	struct map_entry *me;
	struct server *srv;
	struct node *node;
	int rc;

	node = node_get(ctx->local_node);
	if (!node)
		return 0;

	map_for_each(&node->services, me) {
		srv = map_iter_data(me, struct server, mi);

		rc = service_announce_new(ctx, sq, srv);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static struct server *server_add(unsigned int service, unsigned int instance,
	unsigned int node_id, unsigned int port)
{
	struct map_item *mi;
	struct server *srv;
	struct node *node;
	int rc;

	if (!service || !port)
		return NULL;

	srv = calloc(1, sizeof(*srv));
	if (srv == NULL)
		return NULL;

	srv->service = service;
	srv->instance = instance;
	srv->node = node_id;
	srv->port = port;

	node = node_get(node_id);
	if (!node)
		goto err;

	rc = map_reput(&node->services, hash_u32(port), &srv->mi, &mi);
	if (rc)
		goto err;

	LOGD("add server [%d:%x]@[%d:%d]\n", srv->service, srv->instance,
		srv->node, srv->port);

	if (mi) { /* we replaced someone */
		struct server *old = container_of(mi, struct server, mi);
		free(old);
	}

	return srv;

err:
	free(srv);
	return NULL;
}

static int server_del(struct context *ctx, struct node *node, unsigned int port)
{
	struct lookup *lookup;
	struct list_item *li;
	struct map_item *mi;
	struct server *srv;

	mi = map_get(&node->services, hash_u32(port));
	if (!mi)
		return -ENOENT;

	srv = container_of(mi, struct server, mi);
	map_remove(&node->services, srv->mi.key);

	/* Broadcast the removal of local services */
	if (srv->node == ctx->local_node)
		service_announce_del(ctx, &ctx->bcast_sq, srv);

	/* Announce the service's disappearance to observers */
	list_for_each(&ctx->lookups, li) {
		lookup = container_of(li, struct lookup, li);
		if (lookup->service && lookup->service != srv->service)
			continue;
		if (lookup->instance && lookup->instance != srv->instance)
			continue;

		lookup_notify(ctx, &lookup->sq, srv, false);
	}

	free(srv);

	return 0;
}

static int ctrl_cmd_hello(struct context *ctx, struct sockaddr_qrtr *sq,
			  const void *buf, size_t len)
{
	int rc;

	rc = sendto(ctx->sock, buf, len, 0, (void *)sq, sizeof(*sq));
	if (rc > 0)
		rc = annouce_servers(ctx, sq);

	return rc;
}

static int ctrl_cmd_bye(struct context *ctx, struct sockaddr_qrtr *from)
{
	struct qrtr_ctrl_pkt pkt;
	struct sockaddr_qrtr sq;
	struct node *local_node;
	struct map_entry *me;
	struct server *srv;
	struct node *node;
	int rc;

	node = node_get(from->sq_node);
	if (!node)
		return 0;

	map_for_each(&node->services, me) {
		srv = map_iter_data(me, struct server, mi);

		server_del(ctx, node, srv->port);
	}

	/* Advertise the removal of this client to all local services */
	local_node = node_get(ctx->local_node);
	if (!local_node)
		return 0;

	memset(&pkt, 0, sizeof(pkt));
	pkt.cmd = QRTR_TYPE_BYE;
	pkt.client.node = from->sq_node;

	map_for_each(&local_node->services, me) {
		srv = map_iter_data(me, struct server, mi);

		sq.sq_family = AF_QIPCRTR;
		sq.sq_node = srv->node;
		sq.sq_port = srv->port;

		rc = sendto(ctx->sock, &pkt, sizeof(pkt), 0,
				(struct sockaddr *)&sq, sizeof(sq));
		if (rc < 0)
			PLOGW("bye propagation failed");
	}

	return 0;
}

static int ctrl_cmd_del_client(struct context *ctx, struct sockaddr_qrtr *from,
			       unsigned node_id, unsigned port)
{
	struct qrtr_ctrl_pkt pkt;
	struct sockaddr_qrtr sq;
	struct node *local_node;
	struct list_item *tmp;
	struct lookup *lookup;
	struct list_item *li;
	struct map_entry *me;
	struct server *srv;
	struct node *node;
	int rc;

	/* Don't accept spoofed messages */
	if (from->sq_node != node_id)
		return -EINVAL;

	/* Local DEL_CLIENT messages comes from the port being closed */
	if (from->sq_node == ctx->local_node && from->sq_port != port)
		return -EINVAL;

	/* Remove any lookups by this client */
	list_for_each_safe(&ctx->lookups, li, tmp) {
		lookup = container_of(li, struct lookup, li);
		if (lookup->sq.sq_node != node_id)
			continue;
		if (lookup->sq.sq_port != port)
			continue;

		list_remove(&ctx->lookups, &lookup->li);
		free(lookup);
	}

	/* Remove the server belonging to this port*/
	node = node_get(node_id);
	if (node)
		server_del(ctx, node, port);

	/* Advertise the removal of this client to all local services */
	local_node = node_get(ctx->local_node);
	if (!local_node)
		return 0;

	pkt.cmd = QRTR_TYPE_DEL_CLIENT;
	pkt.client.node = node_id;
	pkt.client.port = port;

	map_for_each(&local_node->services, me) {
		srv = map_iter_data(me, struct server, mi);

		sq.sq_family = AF_QIPCRTR;
		sq.sq_node = srv->node;
		sq.sq_port = srv->port;

		rc = sendto(ctx->sock, &pkt, sizeof(pkt), 0,
				(struct sockaddr *)&sq, sizeof(sq));
		if (rc < 0)
			PLOGW("del_client propagation failed");
	}

	return 0;
}

static int ctrl_cmd_new_server(struct context *ctx, struct sockaddr_qrtr *from,
			       unsigned int service, unsigned int instance,
			       unsigned int node_id, unsigned int port)
{
	struct lookup *lookup;
	struct list_item *li;
	struct server *srv;
	int rc = 0;

	/* Ignore specified node and port for local servers*/
	if (from->sq_node == ctx->local_node) {
		node_id = from->sq_node;
		port = from->sq_port;
	}

	/* Don't accept spoofed messages */
	if (from->sq_node != node_id)
		return -EINVAL;

	srv = server_add(service, instance, node_id, port);
	if (!srv)
		return -EINVAL;

	if (srv->node == ctx->local_node)
		rc = service_announce_new(ctx, &ctx->bcast_sq, srv);

	list_for_each(&ctx->lookups, li) {
		lookup = container_of(li, struct lookup, li);
		if (lookup->service && lookup->service != service)
			continue;
		if (lookup->instance && lookup->instance != instance)
			continue;

		lookup_notify(ctx, &lookup->sq, srv, true);
	}

	return rc;
}

static int ctrl_cmd_del_server(struct context *ctx, struct sockaddr_qrtr *from,
			       unsigned int service, unsigned int instance,
			       unsigned int node_id, unsigned int port)
{
	struct node *node;

	/* Ignore specified node and port for local servers*/
	if (from->sq_node == ctx->local_node) {
		node_id = from->sq_node;
		port = from->sq_port;
	}

	/* Don't accept spoofed messages */
	if (from->sq_node != node_id)
		return -EINVAL;

	/* Local servers may only unregister themselves */
	if (from->sq_node == ctx->local_node && from->sq_port != port)
		return -EINVAL;

	node = node_get(node_id);
	if (!node)
		return -ENOENT;

	return server_del(ctx, node, port);
}

static int ctrl_cmd_new_lookup(struct context *ctx, struct sockaddr_qrtr *from,
			       unsigned int service, unsigned int instance)
{
	struct server_filter filter;
	struct list reply_list;
	struct lookup *lookup;
	struct list_item *li;
	struct server *srv;

	/* Accept only local observers */
	if (from->sq_node != ctx->local_node)
		return -EINVAL;

	lookup = calloc(1, sizeof(*lookup));
	if (!lookup)
		return -EINVAL;

	lookup->sq = *from;
	lookup->service = service;
	lookup->instance = instance;
	list_append(&ctx->lookups, &lookup->li);

	memset(&filter, 0, sizeof(filter));
	filter.service = service;
	filter.instance = instance;

	server_query(&filter, &reply_list);
	list_for_each(&reply_list, li) {
		srv = container_of(li, struct server, qli);

		lookup_notify(ctx, from, srv, true);
	}

	lookup_notify(ctx, from, NULL, true);

	return 0;
}

static int ctrl_cmd_del_lookup(struct context *ctx, struct sockaddr_qrtr *from,
			       unsigned int service, unsigned int instance)
{
	struct lookup *lookup;
	struct list_item *tmp;
	struct list_item *li;

	list_for_each_safe(&ctx->lookups, li, tmp) {
		lookup = container_of(li, struct lookup, li);
		if (lookup->sq.sq_node != from->sq_node)
			continue;
		if (lookup->sq.sq_port != from->sq_port)
			continue;
		if (lookup->service != service)
			continue;
		if (lookup->instance && lookup->instance != instance)
			continue;

		list_remove(&ctx->lookups, &lookup->li);
		free(lookup);
	}

	return 0;
}

static void ctrl_port_fn(void *vcontext, struct waiter_ticket *tkt)
{
	struct context *ctx = vcontext;
	struct sockaddr_qrtr sq;
	int sock = ctx->sock;
	struct qrtr_ctrl_pkt *msg;
	unsigned int cmd;
	char buf[4096];
	socklen_t sl;
	ssize_t len;
	int rc;

	sl = sizeof(sq);
	len = recvfrom(sock, buf, sizeof(buf), 0, (void *)&sq, &sl);
	if (len <= 0) {
		PLOGW("recvfrom()");
		close(sock);
		ctx->sock = -1;
		goto out;
	}
	msg = (void *)buf;

	if (len < 4) {
		LOGW("short packet from %d:%d", sq.sq_node, sq.sq_port);
		goto out;
	}

	cmd = le32_to_cpu(msg->cmd);
	if (cmd < ARRAY_SIZE(ctrl_pkt_strings) && ctrl_pkt_strings[cmd])
		LOGD("%s from %d:%d\n", ctrl_pkt_strings[cmd], sq.sq_node, sq.sq_port);
	else
		LOGD("UNK (%08x) from %d:%d\n", cmd, sq.sq_node, sq.sq_port);

	rc = 0;
	switch (cmd) {
	case QRTR_TYPE_HELLO:
		rc = ctrl_cmd_hello(ctx, &sq, buf, len);
		break;
	case QRTR_TYPE_BYE:
		rc = ctrl_cmd_bye(ctx, &sq);
		break;
	case QRTR_TYPE_DEL_CLIENT:
		rc = ctrl_cmd_del_client(ctx, &sq,
					 le32_to_cpu(msg->client.node),
					 le32_to_cpu(msg->client.port));
		break;
	case QRTR_TYPE_NEW_SERVER:
		rc = ctrl_cmd_new_server(ctx, &sq,
					 le32_to_cpu(msg->server.service),
					 le32_to_cpu(msg->server.instance),
					 le32_to_cpu(msg->server.node),
					 le32_to_cpu(msg->server.port));
		break;
	case QRTR_TYPE_DEL_SERVER:
		rc = ctrl_cmd_del_server(ctx, &sq,
					 le32_to_cpu(msg->server.service),
					 le32_to_cpu(msg->server.instance),
					 le32_to_cpu(msg->server.node),
					 le32_to_cpu(msg->server.port));
		break;
	case QRTR_TYPE_EXIT:
	case QRTR_TYPE_PING:
	case QRTR_TYPE_RESUME_TX:
		break;
	case QRTR_TYPE_NEW_LOOKUP:
		rc = ctrl_cmd_new_lookup(ctx, &sq,
					 le32_to_cpu(msg->server.service),
					 le32_to_cpu(msg->server.instance));
		break;
	case QRTR_TYPE_DEL_LOOKUP:
		rc = ctrl_cmd_del_lookup(ctx, &sq,
					 le32_to_cpu(msg->server.service),
					 le32_to_cpu(msg->server.instance));
		break;
	}

	if (rc < 0)
		LOGW("failed while handling packet from %d:%d",
		      sq.sq_node, sq.sq_port);
out:
	waiter_ticket_clear(tkt);
}

static int say_hello(struct context *ctx)
{
	struct qrtr_ctrl_pkt pkt;
	int rc;

	memset(&pkt, 0, sizeof(pkt));
	pkt.cmd = cpu_to_le32(QRTR_TYPE_HELLO);

	rc = sendto(ctx->sock, &pkt, sizeof(pkt), 0,
		    (struct sockaddr *)&ctx->bcast_sq, sizeof(ctx->bcast_sq));
	if (rc < 0)
		return rc;

	return 0;
}

static void server_mi_free(struct map_item *mi)
{
	free(container_of(mi, struct server, mi));
}

static void node_mi_free(struct map_item *mi)
{
	struct node *node = container_of(mi, struct node, mi);

	map_clear(&node->services, server_mi_free);
	map_destroy(&node->services);

	free(node);
}

static void usage(const char *progname)
{
	fprintf(stderr, "%s [-f] [-s] [<node-id>]\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	struct waiter_ticket *tkt;
	struct sockaddr_qrtr sq;
	struct context ctx;
	unsigned long addr = (unsigned long)-1;
	struct waiter *w;
	socklen_t sl = sizeof(sq);
	bool foreground = false;
	bool use_syslog = false;
	bool verbose_log = false;
	char *ep;
	int opt;
	int rc;
	const char *progname = basename(argv[0]);

	while ((opt = getopt(argc, argv, "fsv")) != -1) {
		switch (opt) {
		case 'f':
			foreground = true;
			break;
		case 's':
			use_syslog = true;
			break;
		case 'v':
			verbose_log = true;
			break;
		default:
			usage(progname);
		}
	}

	qlog_setup(progname, use_syslog);
	if (verbose_log)
		qlog_set_min_priority(LOG_DEBUG);

	if (optind < argc) {
		addr = strtoul(argv[optind], &ep, 10);
		if (argv[1][0] == '\0' || *ep != '\0' || addr >= UINT_MAX)
			usage(progname);

		qrtr_set_address(addr);
		optind++;
	}

	if (optind != argc)
		usage(progname);

	w = waiter_create();
	if (w == NULL)
		LOGE_AND_EXIT("unable to create waiter");

	list_init(&ctx.lookups);

	rc = map_create(&nodes);
	if (rc)
		LOGE_AND_EXIT("unable to create node map");

	ctx.sock = socket(AF_QIPCRTR, SOCK_DGRAM, 0);
	if (ctx.sock < 0)
		PLOGE_AND_EXIT("unable to create control socket");

	rc = getsockname(ctx.sock, (void*)&sq, &sl);
	if (rc < 0)
		PLOGE_AND_EXIT("getsockname()");
	sq.sq_port = QRTR_PORT_CTRL;
	ctx.local_node = sq.sq_node;

	rc = bind(ctx.sock, (void *)&sq, sizeof(sq));
	if (rc < 0)
		PLOGE_AND_EXIT("bind control socket");

	ctx.bcast_sq.sq_family = AF_QIPCRTR;
	ctx.bcast_sq.sq_node = QRTR_NODE_BCAST;
	ctx.bcast_sq.sq_port = QRTR_PORT_CTRL;

	rc = say_hello(&ctx);
	if (rc)
		PLOGE_AND_EXIT("unable to say hello");

	/* If we're going to background, fork and exit parent */
	if (!foreground && fork() != 0) {
		close(ctx.sock);
		exit(0);
	}

	tkt = waiter_add_fd(w, ctx.sock);
	waiter_ticket_callback(tkt, ctrl_port_fn, &ctx);

	while (ctx.sock >= 0)
		waiter_wait(w);

	puts("exiting cleanly");

	waiter_destroy(w);

	map_clear(&nodes, node_mi_free);
	map_destroy(&nodes);

	return 0;
}
