/*
 * Copyright (c) 2018, Linaro Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <errno.h>
#include <libqrtr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "servreg_loc.h"

struct pd_map {
	const char *service;
	const char *domain;
	int instance;
};

static const struct pd_map pd_maps[] = {
	{ "kernel/elf_loader", "msm/modem/wlan_pd", 1 },
	{}
};

static void handle_get_domain_list(int sock, const struct qrtr_packet *pkt)
{
	struct servreg_loc_get_domain_list_resp resp = {};
	struct servreg_loc_get_domain_list_req req = {};
	struct servreg_loc_domain_list_entry *entry;
	DEFINE_QRTR_PACKET(resp_buf, 256);
	const struct pd_map *pd_map = pd_maps;
	unsigned int txn;
	ssize_t len;
	int ret;

	ret = qmi_decode_message(&req, &txn, pkt, QMI_REQUEST,
				 SERVREG_LOC_GET_DOMAIN_LIST,
				 servreg_loc_get_domain_list_req_ei);
	if (ret < 0) {
		resp.result.result = QMI_RESULT_FAILURE;
		resp.result.error = QMI_ERR_MALFORMED_MSG;
		goto respond;
	}

	req.name[sizeof(req.name)-1] = '\0';

	resp.result.result = QMI_RESULT_SUCCESS;
	resp.db_revision_valid = 1;
	resp.db_revision = 1;

	while (pd_map->service) {
		if (!strcmp(pd_map->service, req.name)) {
			entry = &resp.domain_list[resp.domain_list_len++];

			strcpy(entry->name, pd_map->domain);
			entry->name_len = strlen(pd_map->domain);
			entry->instance_id = pd_map->instance;
		}

		pd_map++;
	}

	if (resp.domain_list_len)
		resp.domain_list_valid = 1;

	resp.total_domains_valid = 1;
	resp.total_domains = resp.domain_list_len;

respond:
	len = qmi_encode_message(&resp_buf,
				 QMI_RESPONSE, SERVREG_LOC_GET_DOMAIN_LIST,
				 txn, &resp,
				 servreg_loc_get_domain_list_resp_ei);
	if (len < 0) {
		fprintf(stderr,
			"[PD-MAPPER] failed to encode get_domain_list response: %s\n",
			strerror(-len));
		return;
	}

	ret = qrtr_sendto(sock, pkt->node, pkt->port,
			  resp_buf.data, resp_buf.data_len);
	if (ret < 0) {
		fprintf(stderr,
			"[PD-MAPPER] failed to send get_domain_list response: %s\n",
			strerror(-ret));
	}
}

int main(int argc, char **argv)
{
	struct sockaddr_qrtr sq;
	struct qrtr_packet pkt;
	unsigned int msg_id;
	socklen_t sl;
	char buf[4096];
	int ret;
	int fd;

	fd = qrtr_open(0);
	if (fd < 0) {
		fprintf(stderr, "failed to open qrtr socket\n");
		exit(1);
	}

	ret = qrtr_publish(fd, SERVREG_QMI_SERVICE,
			   SERVREG_QMI_VERSION, SERVREG_QMI_INSTANCE);
	if (ret < 0) {
		fprintf(stderr, "failed to publish service registry service\n");
		exit(1);
	}

	for (;;) {
		ret = qrtr_poll(fd, -1);
		if (ret < 0) {
			if (errno == EINTR) {
				continue;
			} else {
				fprintf(stderr, "qrtr_poll failed\n");
				break;
			}
		}

		sl = sizeof(sq);
		ret = recvfrom(fd, buf, sizeof(buf), 0, (void *)&sq, &sl);
		if (ret < 0) {
			ret = -errno;
			if (ret != -ENETRESET)
				fprintf(stderr, "[PD-MAPPER] recvfrom failed: %d\n", ret);
			return ret;
		}

		ret = qrtr_decode(&pkt, buf, ret, &sq);
		if (ret < 0) {
			fprintf(stderr, "[PD-MAPPER] unable to decode qrtr packet\n");
			return ret;
		}

		switch (pkt.type) {
		case QRTR_TYPE_DATA:
			ret = qmi_decode_header(&pkt, &msg_id);
			if (ret < 0)
				continue;

			switch (msg_id) {
			case SERVREG_LOC_GET_DOMAIN_LIST:
				handle_get_domain_list(fd, &pkt);
				break;
			case SERVREG_LOC_PFR:
				printf("[PD-MAPPER] pfr\n");
				break;
			};
			break;
		};
	}

	close(fd);

	return 0;
}
