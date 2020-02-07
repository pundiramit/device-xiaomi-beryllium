/*
 * Copyright (c) 2013-2014, Sony Mobile Communications Inc.
 * Copyright (c) 2014, Courtney Cavin
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *
 *  - Neither the name of the organization nor the names of its contributors
 *  may be used to endorse or promote products derived from this software
 *  without specific prior written permission.
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

#include "list.h"
#include "waiter.h"
#include "util.h"

struct pollset {
	int nfds;
	int cause;
};

static struct pollset *pollset_create(int count)
{
	struct pollset *ps;

	ps = calloc(1, sizeof(*ps) + sizeof(struct pollfd) * count);
	if (ps == NULL)
		return NULL;

	return ps;
}

static void pollset_destroy(struct pollset *ps)
{
	free(ps);
}

static void pollset_reset(struct pollset *ps)
{
	ps->nfds = 0;
}

static void pollset_add_fd(struct pollset *ps, int fd)
{
	struct pollfd *pfd = (struct pollfd *)(ps + 1);
	pfd[ps->nfds].fd = fd;
	pfd[ps->nfds].events = POLLERR | POLLIN;
	ps->nfds++;
}

static int pollset_wait(struct pollset *ps, int ms)
{
	struct pollfd *pfd = (struct pollfd *)(ps + 1);
	int rc;
	int i;

	rc = poll(pfd, ps->nfds, ms);
	if (rc <= 0)
		return rc;

	ps->cause = -1;
	for (i = 0; i < ps->nfds; ++i) {
		if (pfd[i].revents & (POLLERR | POLLIN)) {
			ps->cause = i;
			break;
		}
	}
	return rc;

}

static int pollset_cause_fd(struct pollset *ps, int fd)
{
	struct pollfd *pfd = (struct pollfd *)(ps + 1);
	return (ps->cause >= 0 && pfd[ps->cause].fd == fd);
}

enum waiter_type {
	WATCH_TYPE_NULL,
	WATCH_TYPE_FD,
	WATCH_TYPE_TIMEOUT,
};

struct waiter_ticket {
	enum waiter_type type;
	union {
		int filedes;
		unsigned int event;
		unsigned int interval;
	};
	struct {
		void (* fn)(void *data, struct waiter_ticket *);
		void *data;
	} callback;

	uint64_t start;
	int updated;
	struct waiter *waiter;
	struct list_item list_item;
};

struct waiter {
	struct list tickets;
	struct pollset *pollset;
	int count;
};

struct waiter *waiter_create(void)
{
	struct waiter *w;

	w = calloc(1, sizeof(*w));
	if (w == NULL)
		return NULL;

	list_init(&w->tickets);
	return w;
}

void waiter_destroy(struct waiter *w)
{
	struct waiter_ticket *ticket;
	struct list_item *safe;
	struct list_item *node;

	list_for_each_safe(&w->tickets, node, safe) {
		ticket = list_entry(node, struct waiter_ticket, list_item);
		free(ticket);
	}

	if (w->pollset)
		pollset_destroy(w->pollset);
	free(w);
}

void waiter_synchronize(struct waiter *w)
{
	struct waiter_ticket *oticket;
	struct waiter_ticket *ticket;
	struct list_item *node;

	list_for_each(&w->tickets, node) {
		struct list_item *onode;
		ticket = list_entry(node, struct waiter_ticket, list_item);

		if (ticket->type != WATCH_TYPE_TIMEOUT)
			continue;

		list_for_each_after(node, onode) {
			oticket = list_entry(onode, struct waiter_ticket, list_item);
			if (oticket->type != WATCH_TYPE_TIMEOUT)
				continue;

			if (oticket->interval == ticket->interval) {
				oticket->start = ticket->start;
				break;
			}
		}
	}
}

void waiter_wait(struct waiter *w)
{
	struct pollset *ps = w->pollset;
	struct waiter_ticket *ticket;
	struct list_item *node;
	uint64_t term_time;
	uint64_t now;
	int rc;

	pollset_reset(ps);

	term_time = (uint64_t)-1;
	list_for_each(&w->tickets, node) {
		ticket = list_entry(node, struct waiter_ticket, list_item);
		switch (ticket->type) {
		case WATCH_TYPE_TIMEOUT:
			if (ticket->start + ticket->interval < term_time)
				term_time = ticket->start + ticket->interval;
			break;
		case WATCH_TYPE_FD:
			pollset_add_fd(ps, ticket->filedes);
			break;
		case WATCH_TYPE_NULL:
			break;
		}
	}

	if (term_time == (uint64_t)-1) { /* wait forever */
		rc = pollset_wait(ps, -1);
	} else {
		now = time_ms();
		if (now >= term_time) { /* already past timeout, skip poll */
			rc = 0;
		} else {
			uint64_t delta;

			delta = term_time - now;
			if (delta > ((1u << 31) - 1))
				delta = ((1u << 31) - 1);
			rc = pollset_wait(ps, (int)delta);
		}
	}

	if (rc < 0)
		return;

	now = time_ms();
	list_for_each(&w->tickets, node) {
		int fresh = 0;

		ticket = list_entry(node, struct waiter_ticket, list_item);
		switch (ticket->type) {
		case WATCH_TYPE_TIMEOUT:
			if (now >= ticket->start + ticket->interval) {
				ticket->start = now;
				fresh = !ticket->updated;
			}
			break;
		case WATCH_TYPE_FD:
			if (rc == 0) /* timed-out */
				break;
			if (pollset_cause_fd(ps, ticket->filedes))
				fresh = !ticket->updated;
			break;
		case WATCH_TYPE_NULL:
			break;
		}
		if (fresh) {
			ticket->updated = 1;
			if (ticket->callback.fn)
				(* ticket->callback.fn)(
						ticket->callback.data,
						ticket
				);
		}
	}
}

int waiter_wait_timeout(struct waiter *w, unsigned int ms)
{
	struct waiter_ticket ticket;
	int rc;

	memset(&ticket, 0, sizeof(ticket));
	waiter_ticket_set_timeout(&ticket, ms);
	list_append(&w->tickets, &ticket.list_item);
	w->count++;

	waiter_wait(w);
	rc = waiter_ticket_check(&ticket);

	list_remove(&w->tickets, &ticket.list_item);
	w->count--;

	return -!rc;
}

void waiter_ticket_set_null(struct waiter_ticket *ticket)
{
	ticket->type = WATCH_TYPE_NULL;
}

void waiter_ticket_set_fd(struct waiter_ticket *ticket, int fd)
{
	ticket->type = WATCH_TYPE_FD;
	ticket->filedes = fd;
}

void waiter_ticket_set_timeout(struct waiter_ticket *ticket, unsigned int ms)
{
	ticket->type = WATCH_TYPE_TIMEOUT;
	ticket->interval = ms;
	ticket->start = time_ms();
}

struct waiter_ticket *waiter_add_null(struct waiter *w)
{
	struct waiter_ticket *ticket;

	ticket = calloc(1, sizeof(*ticket));
	if (ticket == NULL)
		return NULL;
	ticket->waiter = w;

	list_append(&w->tickets, &ticket->list_item);
	if ((w->count % 32) == 0) {
		if (w->pollset)
			pollset_destroy(w->pollset);
		w->pollset = pollset_create(w->count + 33);
		if (w->pollset == NULL)
			return NULL;
	}
	w->count++;

	waiter_ticket_set_null(ticket);

	return ticket;
}

struct waiter_ticket *waiter_add_fd(struct waiter *w, int fd)
{
	struct waiter_ticket *ticket;

	ticket = waiter_add_null(w);
	if (ticket == NULL)
		return NULL;

	waiter_ticket_set_fd(ticket, fd);

	return ticket;
}

struct waiter_ticket *waiter_add_timeout(struct waiter *w, unsigned int ms)
{
	struct waiter_ticket *ticket;

	ticket = waiter_add_null(w);
	if (ticket == NULL)
		return NULL;

	waiter_ticket_set_timeout(ticket, ms);

	return ticket;
}

void waiter_ticket_delete(struct waiter_ticket *ticket)
{
	struct waiter *w = ticket->waiter;
	list_remove(&w->tickets, &ticket->list_item);
	w->count--;
	free(ticket);
}

void waiter_ticket_callback(struct waiter_ticket *ticket, waiter_ticket_cb_t cb_fn, void *data)
{
	ticket->callback.fn = cb_fn;
	ticket->callback.data = data;
}

int waiter_ticket_check(const struct waiter_ticket *ticket)
{
	return -(ticket->updated == 0);
}

int waiter_ticket_clear(struct waiter_ticket *ticket)
{
	int ret;

	ret = waiter_ticket_check(ticket);
	ticket->updated = 0;

	return ret;
}
