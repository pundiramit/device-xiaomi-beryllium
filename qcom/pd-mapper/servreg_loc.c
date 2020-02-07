#include <errno.h>
#include <string.h>
#include "servreg_loc.h"

struct qmi_elem_info servreg_loc_qmi_result_ei[] = {
	{
		.data_type = QMI_UNSIGNED_2_BYTE,
		.elem_len = 1,
		.elem_size = sizeof(uint16_t),
		.offset = offsetof(struct servreg_loc_qmi_result, result),
	},
	{
		.data_type = QMI_UNSIGNED_2_BYTE,
		.elem_len = 1,
		.elem_size = sizeof(uint16_t),
		.offset = offsetof(struct servreg_loc_qmi_result, error),
	},
	{}
};

struct qmi_elem_info servreg_loc_domain_list_entry_ei[] = {
	{
		.data_type = QMI_STRING,
		.elem_len = 256,
		.elem_size = sizeof(char),
		.offset = offsetof(struct servreg_loc_domain_list_entry, name)
	},
	{
		.data_type = QMI_UNSIGNED_4_BYTE,
		.elem_len = 1,
		.elem_size = sizeof(uint32_t),
		.offset = offsetof(struct servreg_loc_domain_list_entry, instance_id),
	},
	{
		.data_type = QMI_UNSIGNED_1_BYTE,
		.elem_len = 1,
		.elem_size = sizeof(uint8_t),
		.offset = offsetof(struct servreg_loc_domain_list_entry, service_data_valid),
	},
	{
		.data_type = QMI_UNSIGNED_4_BYTE,
		.elem_len = 1,
		.elem_size = sizeof(uint32_t),
		.offset = offsetof(struct servreg_loc_domain_list_entry, service_data),
	},
	{}
};

struct qmi_elem_info servreg_loc_get_domain_list_req_ei[] = {
	{
		.data_type = QMI_STRING,
		.elem_len = 256,
		.elem_size = sizeof(char),
		.array_type = VAR_LEN_ARRAY,
		.tlv_type = 1,
		.offset = offsetof(struct servreg_loc_get_domain_list_req, name)
	},
	{
		.data_type = QMI_OPT_FLAG,
		.elem_len = 1,
		.elem_size = sizeof(bool),
		.tlv_type = 16,
		.offset = offsetof(struct servreg_loc_get_domain_list_req, offset_valid),
	},
	{
		.data_type = QMI_UNSIGNED_4_BYTE,
		.elem_len = 1,
		.elem_size = sizeof(uint32_t),
		.tlv_type = 16,
		.offset = offsetof(struct servreg_loc_get_domain_list_req, offset),
	},
	{}
};

struct qmi_elem_info servreg_loc_get_domain_list_resp_ei[] = {
	{
		.data_type = QMI_STRUCT,
		.elem_len = 1,
		.elem_size = sizeof(struct servreg_loc_qmi_result),
		.tlv_type = 2,
		.offset = offsetof(struct servreg_loc_get_domain_list_resp, result),
		.ei_array = servreg_loc_qmi_result_ei,
	},
	{
		.data_type = QMI_OPT_FLAG,
		.elem_len = 1,
		.elem_size = sizeof(bool),
		.tlv_type = 16,
		.offset = offsetof(struct servreg_loc_get_domain_list_resp, total_domains_valid),
	},
	{
		.data_type = QMI_UNSIGNED_2_BYTE,
		.elem_len = 1,
		.elem_size = sizeof(uint16_t),
		.tlv_type = 16,
		.offset = offsetof(struct servreg_loc_get_domain_list_resp, total_domains),
	},
	{
		.data_type = QMI_OPT_FLAG,
		.elem_len = 1,
		.elem_size = sizeof(bool),
		.tlv_type = 17,
		.offset = offsetof(struct servreg_loc_get_domain_list_resp, db_revision_valid),
	},
	{
		.data_type = QMI_UNSIGNED_2_BYTE,
		.elem_len = 1,
		.elem_size = sizeof(uint16_t),
		.tlv_type = 17,
		.offset = offsetof(struct servreg_loc_get_domain_list_resp, db_revision),
	},
	{
		.data_type = QMI_OPT_FLAG,
		.elem_len = 1,
		.elem_size = sizeof(bool),
		.tlv_type = 18,
		.offset = offsetof(struct servreg_loc_get_domain_list_resp, domain_list_valid),
	},
	{
		.data_type = QMI_DATA_LEN,
		.elem_len = 1,
		.elem_size = sizeof(uint8_t),
		.tlv_type = 18,
		.offset = offsetof(struct servreg_loc_get_domain_list_resp, domain_list_len),
	},
	{
		.data_type = QMI_STRUCT,
		.elem_len = 255,
		.elem_size = sizeof(struct servreg_loc_domain_list_entry),
		.array_type = VAR_LEN_ARRAY,
		.tlv_type = 18,
		.offset = offsetof(struct servreg_loc_get_domain_list_resp, domain_list),
		.ei_array = servreg_loc_domain_list_entry_ei,
	},
	{}
};

struct qmi_elem_info servreg_loc_pfr_req_ei[] = {
	{
		.data_type = QMI_STRING,
		.elem_len = 256,
		.elem_size = sizeof(char),
		.array_type = VAR_LEN_ARRAY,
		.tlv_type = 1,
		.offset = offsetof(struct servreg_loc_pfr_req, service)
	},
	{
		.data_type = QMI_STRING,
		.elem_len = 256,
		.elem_size = sizeof(char),
		.array_type = VAR_LEN_ARRAY,
		.tlv_type = 2,
		.offset = offsetof(struct servreg_loc_pfr_req, reason)
	},
	{}
};

struct qmi_elem_info servreg_loc_pfr_resp_ei[] = {
	{
		.data_type = QMI_STRUCT,
		.elem_len = 1,
		.elem_size = sizeof(struct servreg_loc_qmi_result),
		.tlv_type = 2,
		.offset = offsetof(struct servreg_loc_pfr_resp, result),
		.ei_array = servreg_loc_qmi_result_ei,
	},
	{}
};

