#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

#include "util.h"

uint64_t time_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec*1000 + tv.tv_usec/1000;
}

void util_sleep(int ms)
{
	usleep(ms * 1000);
}
