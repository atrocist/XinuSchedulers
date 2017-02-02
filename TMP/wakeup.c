/* wakeup.c - wakeup */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sleep.h>


/*------------------------------------------------------------------------
 * wakeup  --  called by clock interrupt dispatcher to awaken processes
 *------------------------------------------------------------------------
 */
INTPROC	wakeup()
{
	int pid;

        while (nonempty(clockq) && firstkey(clockq) <= 0){
		pid = getfirst(clockq);
#if DEBUG_MODE
		kprintf("(W%d)",pid);
#endif
                ready(pid,RESCHNO);
	}
	if ((slnempty = nonempty(clockq)))
		sltop = & q[q[clockq].qnext].qkey;
	resched();
        return(OK);
}
