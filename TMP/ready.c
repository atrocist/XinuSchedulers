/* ready.c - ready */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>


extern int active_sched_class;



/*------------------------------------------------------------------------
 * ready  --  make a process eligible for CPU service
 *------------------------------------------------------------------------
 */
int ready(int pid, int resch)
{
	register struct	pentry	*pptr;

	if (isbadpid(pid))
		return(SYSERR);
	pptr = &proctab[pid];
	if(active_sched_class == LINUXSCHED){

#if DEBUG_MODE
		kprintf("(Rdy%d:%d)",pid,pptr->goodness);
#endif
		insert(pid,rdyhead, pptr->goodness);
		pptr->pstate = PRREADY;
		resch = RESCHNO;
	}
	else if(active_sched_class == MULTIQSCHED){
		if(pptr->isRT == 1){
 			insert(pid,rtrdyhead,pptr->quanta_left);
			pptr->pstate = PRREADY;
			resch = RESCHYES;
		}
		else{
#if DEBUG_MODE
		kprintf("(Rdy%d:%d)",pid,pptr->goodness);
#endif
 			insert(pid,rdyhead, pptr->goodness);
			pptr->pstate = PRREADY;
			resch = RESCHNO;
		}
	}
	else{	
		insert(pid,rdyhead,pptr->pprio);
		pptr->pstate = PRREADY;
	}
	if (resch)
		resched();
	return(OK);
}
