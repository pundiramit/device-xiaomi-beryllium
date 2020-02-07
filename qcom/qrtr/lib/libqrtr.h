#ifndef _QRTR_LIB_H_
#define _QRTR_LIB_H_

#include <linux/qrtr.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef offsetof
#define offsetof(type, md) ((unsigned long)&((type *)0)->md)
#endif

#ifndef container_of
#define container_of(ptr, type, member) \
  ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

#ifndef AF_QIPCRTR
#define AF_QIPCRTR 42
#endif

struct sockaddr_qrtr;

struct qrtr_packet {
	int type;

	unsigned int node;
	unsigned int port;

	unsigned int service;
	unsigned int instance;
	unsigned int version;

	void *data;
	size_t data_len;
};

#define DEFINE_QRTR_PACKET(pkt, size) \
	char pkt ## _buf[size]; \
	struct qrtr_packet pkt = { .data = pkt ##_buf, \
				   .data_len = sizeof(pkt ##_buf), }

#define QMI_REQUEST     0
#define QMI_RESPONSE    2
#define QMI_INDICATION  4

#define QMI_COMMON_TLV_TYPE 0

enum qmi_elem_type {
	QMI_EOTI,
	QMI_OPT_FLAG,
	QMI_DATA_LEN,
	QMI_UNSIGNED_1_BYTE,
	QMI_UNSIGNED_2_BYTE,
	QMI_UNSIGNED_4_BYTE,
	QMI_UNSIGNED_8_BYTE,
	QMI_SIGNED_1_BYTE_ENUM,
	QMI_SIGNED_2_BYTE_ENUM,
	QMI_SIGNED_4_BYTE_ENUM,
	QMI_STRUCT,
	QMI_STRING,
};

enum qmi_array_type {
	NO_ARRAY,
	STATIC_ARRAY,
	VAR_LEN_ARRAY,
};

/**
 * struct qmi_elem_info - describes how to encode a single QMI element
 * @data_type:  Data type of this element.
 * @elem_len:   Array length of this element, if an array.
 * @elem_size:  Size of a single instance of this data type.
 * @array_type: Array type of this element.
 * @tlv_type:   QMI message specific type to identify which element
 *              is present in an incoming message.
 * @offset:     Specifies the offset of the first instance of this
 *              element in the data structure.
 * @ei_array:   Null-terminated array of @qmi_elem_info to describe nested
 *              structures.
 */
struct qmi_elem_info {
	enum qmi_elem_type data_type;
	uint32_t elem_len;
	uint32_t elem_size;
	enum qmi_array_type array_type;
	uint8_t tlv_type;
	uint32_t offset;
	struct qmi_elem_info *ei_array;
};

#define QMI_RESULT_SUCCESS_V01                  0
#define QMI_RESULT_FAILURE_V01                  1

#define QMI_ERR_NONE_V01                        0
#define QMI_ERR_MALFORMED_MSG_V01               1
#define QMI_ERR_NO_MEMORY_V01                   2
#define QMI_ERR_INTERNAL_V01                    3
#define QMI_ERR_CLIENT_IDS_EXHAUSTED_V01        5
#define QMI_ERR_INVALID_ID_V01                  41
#define QMI_ERR_ENCODING_V01                    58
#define QMI_ERR_INCOMPATIBLE_STATE_V01          90
#define QMI_ERR_NOT_SUPPORTED_V01               94

/**
 * qmi_response_type_v01 - common response header (decoded)
 * @result:     result of the transaction
 * @error:      error value, when @result is QMI_RESULT_FAILURE_V01
 */
struct qmi_response_type_v01 {
	uint16_t result;
	uint16_t error;
};

extern struct qmi_elem_info qmi_response_type_v01_ei[];

int qrtr_open(int rport);
void qrtr_close(int sock);

int qrtr_sendto(int sock, uint32_t node, uint32_t port, const void *data, unsigned int sz);
int qrtr_recvfrom(int sock, void *buf, unsigned int bsz, uint32_t *node, uint32_t *port);
int qrtr_recv(int sock, void *buf, unsigned int bsz);

int qrtr_new_server(int sock, uint32_t service, uint16_t version, uint16_t instance);
int qrtr_remove_server(int sock, uint32_t service, uint16_t version, uint16_t instance);

int qrtr_publish(int sock, uint32_t service, uint16_t version, uint16_t instance);
int qrtr_bye(int sock, uint32_t service, uint16_t version, uint16_t instance);

int qrtr_new_lookup(int sock, uint32_t service, uint16_t version, uint16_t instance);
int qrtr_remove_lookup(int sock, uint32_t service, uint16_t version, uint16_t instance);

int qrtr_poll(int sock, unsigned int ms);

int qrtr_decode(struct qrtr_packet *dest, void *buf, size_t len,
		const struct sockaddr_qrtr *sq);

int qmi_decode_header(const struct qrtr_packet *pkt, unsigned int *msg_id);
int qmi_decode_message(void *c_struct, unsigned int *txn,
		       const struct qrtr_packet *pkt,
		       int type, int id, struct qmi_elem_info *ei);
ssize_t qmi_encode_message(struct qrtr_packet *pkt, int type, int msg_id,
			   int txn_id, const void *c_struct,
			   struct qmi_elem_info *ei);

/* Initial kernel header didn't expose these */
#ifndef QRTR_NODE_BCAST

#define QRTR_NODE_BCAST 0xffffffffu
#define QRTR_PORT_CTRL  0xfffffffeu

enum qrtr_pkt_type {
        QRTR_TYPE_DATA          = 1,
        QRTR_TYPE_HELLO         = 2,
        QRTR_TYPE_BYE           = 3,
        QRTR_TYPE_NEW_SERVER    = 4,
        QRTR_TYPE_DEL_SERVER    = 5,
        QRTR_TYPE_DEL_CLIENT    = 6,
        QRTR_TYPE_RESUME_TX     = 7,
        QRTR_TYPE_EXIT          = 8,
        QRTR_TYPE_PING          = 9,
        QRTR_TYPE_NEW_LOOKUP    = 10,
        QRTR_TYPE_DEL_LOOKUP    = 11,
};

struct qrtr_ctrl_pkt {
        __le32 cmd;

        union {
                struct {
                        __le32 service;
                        __le32 instance;
                        __le32 node;
                        __le32 port;
                } server;

                struct {
                        __le32 node;
                        __le32 port;
                } client;
        };
} __attribute__((packed));

#endif

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif
