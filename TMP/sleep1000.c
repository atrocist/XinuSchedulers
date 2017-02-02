/* sleep1000.c - sleep1000 */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sleep.h>
#include <stdio.h>

extern int active_sched_class;

/*------------------------------------------------------------------------
 * sleep1000 --  delay the caller for a time specified in 1/100 of seconds
 *------------------------------------------------------------------------
 */
SYSCALL sleep1000(int n)
{
	STATWORD ps;    

	if (n < 0  || clkruns==0)
	         return(SYSERR);
	disable(ps);
	if (n == 0) {		/* sleep1000(0) -> end time slice */
	        ;
	} else {
//		if(active_sched_class != DEFAULTSCHED){
//			dequeue(currpid);
//		}
		insertd(currpid,clockq,n);
		proctab[currpid].pstate = PRSLEEP;
		slnempty = TRUE;
		sltop = &q[q[clockq].qnext].qkey;
	}
	resched();
        restore(ps);
	return(OK);
}
