#ifndef _WAITER_H_
#define _WAITER_H_

/** Waiter type. */
struct waiter;

/** Create a new waiter.
 * @return Newly created waiter on success, NULL on failure.
 */
struct waiter *waiter_create(void);

/** Destroy existing waiter.
 * @param w waiter to destroy.
 */
void waiter_destroy(struct waiter *w);

/** Wait for next ticket.
 * @param w waiter.
 */
void waiter_wait(struct waiter *w);

/** Wait for next ticket or timeout.
 * @param w waiter.
 * @param ms timeout in milliseconds.
 * @return 0 on ticket; !0 on timeout.
 */
int waiter_wait_timeout(struct waiter *w, unsigned int ms);

/** Synchronize timer-based tickets.
 * @param w waiter.
 */
void waiter_synchronize(struct waiter *w);

/** Waiter ticket type. */
struct waiter_ticket;

/** Add a null wait ticket to pool.
 * @param w waiter
 * @return wait ticket on success; NULL on failure.
 */
struct waiter_ticket *waiter_add_null(struct waiter *w);

/** Add a file descriptor to the pool.
 * @param w waiter.
 * @param fd file descriptor.
 * @return wait ticket on success; NULL on failure.
 */
struct waiter_ticket *waiter_add_fd(struct waiter *w, int fd);

/** Add a timeout to the pool.
 * @param w waiter.
 * @param ms duration of timeout in milliseconds.
 * @return wait ticket on success; NULL on failure.
 */
struct waiter_ticket *waiter_add_timeout(struct waiter *w, unsigned int ms);

/** Set ticket type to null.
 * @param tkt wait ticket.
 */
void waiter_ticket_set_null(struct waiter_ticket *tkt);

/** Set ticket type to file descriptor.
 * @param tkt wait ticket.
 * @param fd file descriptor.
 */
void waiter_ticket_set_fd(struct waiter_ticket *tkt, int fd);

/** Set ticket type to timeout.
 * @param tkt wait ticket.
 * @param ms timeout in milliseconds.
 */
void waiter_ticket_set_timeout(struct waiter_ticket *tkt, unsigned int ms);

/** Destroy ticket.
 * @param tkt wait ticket.
 */
void waiter_ticket_delete(struct waiter_ticket *tkt);

/** Check to see if ticket has triggered.
 * @param tkt wait ticket.
 * @return 0 if triggered, !0 otherwise.
 */
int waiter_ticket_check(const struct waiter_ticket *tkt);


/** Clear ticket trigger status.
 * @param tkt wait ticket.
 * @return 0 if triggered, !0 otherwise.
 */
int waiter_ticket_clear(struct waiter_ticket *tkt);

/** Wait ticket callback function type. */
typedef void (* waiter_ticket_cb_t)(void *, struct waiter_ticket *);

/** Register callback function for ticket trigger.
 * @param tkt wait ticket.
 * @param cb_fn callback function.
 * @param data private data to pass to callback function.
 */
void waiter_ticket_callback(struct waiter_ticket *tkt,
		waiter_ticket_cb_t cb_fn, void *data);

#endif
