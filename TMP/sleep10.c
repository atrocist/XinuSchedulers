/* sleep10.c - sleep10 */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sleep.h>
#include <stdio.h>

extern int active_sched_class;

/*------------------------------------------------------------------------
 * sleep10  --  delay the caller for a time specified in tenths of seconds
 *------------------------------------------------------------------------
 */
SYSCALL	sleep10(int n)
{
	STATWORD ps;    
	if (n < 0  || clkruns==0)
	         return(SYSERR);
	disable(ps);
	if (n == 0) {		/* sleep10(0) -> end time slice */
	        ;
	} else {
//		if(active_sched_class != DEFAULTSCHED){
//			dequeue(currpid);
//		}
		insertd(currpid,clockq,n*100);
		proctab[currpid].pstate = PRSLEEP;
		slnempty = TRUE;
		sltop = &q[q[clockq].qnext].qkey;
#if DEBUG_MODE
		kprintf("(Sl%d)",currpid);
#endif
	}
	resched();
        restore(ps);
	return(OK);
}
