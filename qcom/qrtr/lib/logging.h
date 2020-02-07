#ifndef _QRTR_LOGGING_H_
#define _QRTR_LOGGING_H_

#include <stdbool.h>
#include <stdlib.h>
#include <syslog.h>

#if defined(__GNUC__) || defined(__clang__)
#define __PRINTF__(fmt, args) __attribute__((format(__printf__, fmt, args)))
#else
#define __PRINTF__(fmt, args)
#endif

void qlog_setup(const char *tag, bool use_syslog);
void qlog_set_min_priority(int priority);

void qlog(int priority, const char *format, ...) __PRINTF__(2, 3);

#define LOGD(fmt, ...) qlog(LOG_DEBUG, fmt, ##__VA_ARGS__)

#define LOGW(fmt, ...) qlog(LOG_WARNING, fmt, ##__VA_ARGS__)
#define PLOGW(fmt, ...) \
	qlog(LOG_WARNING, fmt ": %s", ##__VA_ARGS__, strerror(errno))

#define LOGE(fmt, ...) qlog(LOG_ERR, fmt, ##__VA_ARGS__)
#define PLOGE(fmt, ...) qlog(LOG_ERR, fmt ": %s", ##__VA_ARGS__, strerror(errno))
#define LOGE_AND_EXIT(fmt, ...) do {			\
		qlog(LOG_ERR, fmt, ##__VA_ARGS__);	\
		exit(1);				\
	} while(0)
#define PLOGE_AND_EXIT(fmt, ...) do {						\
		qlog(LOG_ERR, fmt ": %s", ##__VA_ARGS__, strerror(errno));	\
		exit(1);							\
	} while(0)

#endif
