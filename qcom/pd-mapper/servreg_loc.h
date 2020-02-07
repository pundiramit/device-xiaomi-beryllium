#ifndef __QMI_SERVREG_LOC_H__
#define __QMI_SERVREG_LOC_H__

#include <stdint.h>
#include <stdbool.h>

#include "libqrtr.h"

#define SERVREG_QMI_SERVICE 64
#define SERVREG_QMI_VERSION 257
#define SERVREG_QMI_INSTANCE 0
#define QMI_RESULT_SUCCESS 0
#define QMI_RESULT_FAILURE 1
#define QMI_ERR_NONE 0
#define QMI_ERR_INTERNAL 1
#define QMI_ERR_MALFORMED_MSG 2
#define SERVREG_LOC_GET_DOMAIN_LIST 33
#define SERVREG_LOC_PFR 36

struct servreg_loc_qmi_result {
	uint16_t result;
	uint16_t error;
};

struct servreg_loc_domain_list_entry {
	uint32_t name_len;
	char name[256];
	uint32_t instance_id;
	uint8_t service_data_valid;
	uint32_t service_data;
};

struct servreg_loc_get_domain_list_req {
	uint32_t name_len;
	char name[256];
	bool offset_valid;
	uint32_t offset;
};

struct servreg_loc_get_domain_list_resp {
	struct servreg_loc_qmi_result result;
	bool total_domains_valid;
	uint16_t total_domains;
	bool db_revision_valid;
	uint16_t db_revision;
	bool domain_list_valid;
	uint32_t domain_list_len;
	struct servreg_loc_domain_list_entry domain_list[255];
};

struct servreg_loc_pfr_req {
	uint32_t service_len;
	char service[256];
	uint32_t reason_len;
	char reason[256];
};

struct servreg_loc_pfr_resp {
	struct servreg_loc_qmi_result result;
};

extern struct qmi_elem_info servreg_loc_get_domain_list_req_ei[];
extern struct qmi_elem_info servreg_loc_get_domain_list_resp_ei[];
extern struct qmi_elem_info servreg_loc_pfr_req_ei[];
extern struct qmi_elem_info servreg_loc_pfr_resp_ei[];

#endif
