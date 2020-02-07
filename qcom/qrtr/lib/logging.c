#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>

#define QLOG_BUF_SIZE 512

static const char default_tag[] = "libqrtr";
static const char *current_tag = default_tag;
static int min_priority = LOG_INFO;

static bool logging_to_syslog = false;

void qlog_setup(const char *tag, bool use_syslog)
{
	current_tag = tag;
	logging_to_syslog = use_syslog;

	openlog(tag, LOG_PID, LOG_USER);
}

void qlog_set_min_priority(int priority)
{
	if (priority < LOG_EMERG || priority > LOG_DEBUG)
		return;

	min_priority = priority;
}

static const char *get_priority_string(int priority)
{
	switch (priority) {
	case LOG_EMERG:
		return "EMERG";
	case LOG_ALERT:
		return "ALERT";
	case LOG_CRIT:
		return "CRIT";
	case LOG_ERR:
		return "ERROR";
	case LOG_WARNING:
		return "WARNING";
	case LOG_NOTICE:
		return "NOTICE";
	case LOG_INFO:
		return "INFO";
	case LOG_DEBUG:
		return "DEBUG";
	}
	return "";
}

void qlog(int priority, const char *format, ...)
{
	va_list ap;

	if (priority > min_priority)
		return;

	va_start(ap, format);

	if (logging_to_syslog) {
		vsyslog(priority, format, ap);
	} else {
		char buf[QLOG_BUF_SIZE];
		vsnprintf(buf, QLOG_BUF_SIZE, format, ap);

		fprintf(stderr, "%s %s: %s\n",
		        get_priority_string(priority), current_tag, buf);
	}

	va_end(ap);
}
