#include <stdio.h>
#include <errno.h>
#include <uk/essentials.h>
#include <uk/sched.h>

/* Internal main */
int __weak main(int argc __unused, char *argv[] __unused)
{
	printf("weak main() called. This won't exit. TODO: don't waste this thread.\n");
	while (1) {
		uk_sched_yield();
	}
	return -EINVAL;
}
