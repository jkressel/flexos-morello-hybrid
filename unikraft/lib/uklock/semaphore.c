#include <uk/semaphore.h>
#include <flexos/isolation.h>

void uk_semaphore_init(struct uk_semaphore *s, long count)
{
	s->count = count;
	/* Volatile to make sure that the compiler doesn't reorganize
	 * the code in such a way that the dereference happens in the
	 * other domain... */
	volatile struct uk_waitq *wq = &s->wait;
	flexos_nop_gate(0, 0, uk_waitq_init, wq);

#ifdef UK_SEMAPHORE_DEBUG
	uk_pr_debug("Initialized semaphore %p with %ld\n",
		    s, s->count);
#endif
}
