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
#include <sys/stat.h>
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <libqrtr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "assoc.h"
#include "json.h"
#include "servreg_loc.h"

struct pd_map {
	const char *service;
	const char *domain;
	int instance;
};

static struct pd_map *pd_maps;

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

static int pd_load_map(const char *file)
{
	static int num_pd_maps;
	struct json_value *sr_service;
	struct json_value *sr_domain;
	struct json_value *root;
	struct json_value *it;
	const char *subdomain;
	const char *provider;
	const char *service;
	const char *domain;
	const char *soc;
	struct pd_map *newp;
	struct pd_map *map;
	double number;
	int count;
	int ret;

	root = json_parse_file(file);
	if (!root)
		return -1;

	sr_domain = json_get_child(root, "sr_domain");
	soc = json_get_string(sr_domain, "soc");
	domain = json_get_string(sr_domain, "domain");
	subdomain = json_get_string(sr_domain, "subdomain");
	ret = json_get_number(sr_domain, "qmi_instance_id", &number);
	if (ret)
		return ret;

	if (!soc || !domain || !subdomain) {
		fprintf(stderr, "failed to parse sr_domain\n");
		return -1;
	}

	sr_service = json_get_child(root, "sr_service");
	count = json_count_children(sr_service);
	if (count < 0)
		return count;

	newp = realloc(pd_maps, (num_pd_maps + count + 1) * sizeof(*newp));
	if (!newp)
		return -1;
	pd_maps = newp;

	for (it = sr_service->u.value; it; it = it->next) {
		provider = json_get_string(it, "provider");
		service = json_get_string(it, "service");

		if (!provider || !service) {
			fprintf(stderr,
				"failed to parse provdider or service from %s\n",
				file);
			return -1;
		}

		map = &pd_maps[num_pd_maps++];

		map->service = malloc(strlen(provider) + strlen(service) + 2);
		sprintf((char *)map->service, "%s/%s", provider, service);

		map->domain = malloc(strlen(soc) + strlen(domain) + strlen(subdomain) + 3);
		sprintf((char *)map->domain, "%s/%s/%s", soc, domain, subdomain);

		map->instance = number;
	}

	pd_maps[num_pd_maps].service = NULL;

	json_free(root);

	return 0;
}

#ifndef ANDROID
#define FIRMWARE_BASE	"/lib/firmware/"
#else
#define FIRMWARE_BASE	"/vendor/firmware/"
#endif

static int pd_enumerate_jsons(struct assoc *json_set)
{
	char firmware_value[PATH_MAX];
	char json_path[PATH_MAX];
	char firmware_attr[32];
	struct dirent *fw_de;
	char path[PATH_MAX];
	struct dirent *de;
	int firmware_fd;
	DIR *class_dir;
	int class_fd;
	DIR *fw_dir;
	size_t len;
	size_t n;

	class_fd = open("/sys/class/remoteproc", O_RDONLY | O_DIRECTORY);
	if (class_fd < 0) {
		warn("failed to open remoteproc class");
		return -1;
	}

	class_dir = fdopendir(class_fd);
	if (!class_dir) {
		warn("failed to opendir");
		goto close_class;
	}

	while ((de = readdir(class_dir)) != NULL) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;

		if (strlen(de->d_name) + sizeof("/firmware") > sizeof(firmware_attr))
			continue;

		strcpy(firmware_attr, de->d_name);
		strcat(firmware_attr, "/firmware");

		firmware_fd = openat(class_fd, firmware_attr, O_RDONLY);
		if (firmware_fd < 0)
			continue;

		n = read(firmware_fd, firmware_value, sizeof(firmware_value));
		close(firmware_fd);
		if (n < 0) {
			continue;
		}

		firmware_value[n] = '\0';

		if (strlen(FIRMWARE_BASE) + strlen(firmware_value) + 1 > sizeof(path))
			continue;

		strcpy(path, FIRMWARE_BASE);
		strcat(path, dirname(firmware_value));

		fw_dir = opendir(path);
		while ((fw_de = readdir(fw_dir)) != NULL) {
			if (!strcmp(fw_de->d_name, ".") || !strcmp(fw_de->d_name, ".."))
				continue;

			len = strlen(fw_de->d_name);
			if (len < 5 || strcmp(&fw_de->d_name[len - 4], ".jsn"))
				continue;

			if (strlen(FIRMWARE_BASE) + strlen(firmware_value) + 1 +
			    strlen(fw_de->d_name) + 1 > sizeof(path))
					continue;

			strcpy(json_path, path);
			strcat(json_path, "/");
			strcat(json_path, fw_de->d_name);

			assoc_set(json_set, json_path, NULL);
		}

		closedir(fw_dir);
	}

	closedir(class_dir);
close_class:
	close(class_fd);

	return 0;
}

static int pd_load_maps(void)
{
	struct assoc json_set;
	unsigned long it;
	const char *jsn;
	int ret = 0;

	assoc_init(&json_set, 20);

	pd_enumerate_jsons(&json_set);

	assoc_foreach(jsn, NULL, &json_set, it) {
		ret = pd_load_map(jsn);
		if (ret < 0)
			break;
	}

	assoc_destroy(&json_set);

	return ret;
}

int main(int argc __unused, char **argv __unused)
{
	struct sockaddr_qrtr sq;
	struct qrtr_packet pkt;
	unsigned int msg_id;
	socklen_t sl;
	char buf[4096];
	int ret;
	int fd;

	ret = pd_load_maps();
	if (ret)
		exit(1);

	if (!pd_maps) {
		fprintf(stderr, "no pd maps available\n");
		exit(1);
	}

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
