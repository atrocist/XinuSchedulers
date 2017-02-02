/* resched.c  -  resched */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <stdio.h>

int active_sched_class = DEFAULTSCHED; 
unsigned long currSP;	/* REAL sp of current process */
extern int ctxsw(int, int, int, int);


static void refill_rtrdyqueue();
static void refill_rdyqueue();
static int schedule_rtrdyqueue(struct pentry *optr);
static int schedule_rdyqueue(struct pentry *optr);
static void clean_queue(int head, int tail);
static int quantum_remaining(struct pentry *optr);

static int schedule_linux();
static int schedule_multiqueue();


void setschedclass(int sched_class){
	active_sched_class = sched_class;
}

int getschedclass()
{
	return active_sched_class;
}

/*-----------------------------------------------------------------------
 * resched  --  reschedule processor to highest priority ready process
 *
 * Notes:	Upon entry, currpid gives current process id.
 *		Proctab[currpid].pstate gives correct NEXT state for
 *			current process if other than PRREADY.
 *------------------------------------------------------------------------
 */

int resched()
{
	register struct	pentry	*optr;	/* pointer to old process entry */
	register struct	pentry	*nptr;	/* pointer to new process entry */


	optr = &proctab[currpid];

	if(active_sched_class == LINUXSCHED){
		schedule_linux();
		#ifdef	RTCLOCK
			preempt = QUANTUM;		/* reset preemption counter	*/
		#endif
	}else if(active_sched_class == MULTIQSCHED){
		schedule_multiqueue();
		#ifdef	RTCLOCK
			preempt = QUANTUM;		/* reset preemption counter	*/
		#endif
	}else{
		/* no switch needed if current process priority higher than next*/

		if((optr->pstate == PRCURR) && (lastkey(rdytail) < optr->pprio)) {
			return OK;
		}
	
		/* force context switch */

		if (optr->pstate == PRCURR) {
			optr->pstate = PRREADY;
			insert(currpid,rdyhead,optr->pprio);
		}

		/* remove highest priority process at end of ready list */

		nptr = &proctab[ (currpid = getlast(rdytail)) ];
		nptr->pstate = PRCURR;		/* mark it currently running	*/
	#ifdef	RTCLOCK
		preempt = QUANTUM;		/* reset preemption counter	*/
	#endif
	
		ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
	
		/* The OLD process returns here when resumed. */
		return OK;
	}
	return OK;
}


static int schedule_linux()
{	
	register struct pentry *nptr;
	register struct	pentry	*optr;	/* pointer to old process entry */
	optr = &proctab[currpid];
	
	/*reduce the quanta of current process*/
	/*check if current process has used up its quantum*/
	/*continue if quantum is left */
	quantum_remaining(optr);

	/*if quantum is over schedule the next process from rdyqueue*/
	if(schedule_rdyqueue(optr) == 1){
		return OK;
	}

	/*all processes have used their quantum so epoch is over*/
	/*start of next epoch*/
	refill_rdyqueue();
	
	/*schedule the next process from queue*/
	if(schedule_rdyqueue(optr) == 1){
		return OK;
	}

	/*schedule the null process if no processes are left*/
	currpid = 0;
	nptr = &proctab[currpid];
	nptr->quanta_left = 1;
 	nptr->eprio = 0;
	nptr->pstate = PRCURR;
	if(nptr != optr){
		ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
	}
	return OK;
}

static int schedule_multiqueue()
{
	register struct pentry *nptr;
	register struct	pentry	*optr;	/* pointer to old process entry */
	if(isbadpid(currpid)){
		kprintf("PANIC:badpid!!!!!\n");
	}	
	optr = &proctab[currpid];
	/*reduce the quanta of current process*/
	/*check if current process has used up its quantum*/
	/*continue if the quantum is left*/
	if(quantum_remaining(optr) == 1){
		if(optr->isRT == 1){
			return OK;
		}
	}
	
	/*if quantum is over schedule the next process from rtrdyqueue*/
	if(optr->isRT == 1){

		/*current epoch is used for RT queue*/
		if(schedule_rtrdyqueue(optr) == 1){
#if DEBUG_MODE
		kprintf("(RTQ_SEL)");
#endif
			return OK;
		}
	}else{
		/*current epoch is used for normal queue*/
		if(schedule_rdyqueue(optr) == 1){
#if DEBUG_MODE
		kprintf("(NQ_SEL)");
#endif
			return OK;
		}
	}
	/*randomly select between normal queue and real time queue but select realtime queue for 70% of the time*/
	if((rand()%100) < 30){
#if DEBUG_MODE
		kprintf("(randNQ_SEL)");
#endif
		refill_rdyqueue();	
		if(schedule_rdyqueue(optr) == 1){
			return OK;
		}
		/*if no normal processes availabe schedule rtrdyqueue*/
		refill_rtrdyqueue();	
		if(schedule_rtrdyqueue(optr) == 1){
			return OK;
		}
		/*neither rt process nor normal processes are available so schedule the null process*/
		currpid = 0;
		nptr = &proctab[currpid];
  		nptr->quanta_left = 1;
 	 	nptr->eprio = 0;
  		nptr->pstate = PRCURR;
  		if(nptr != optr){
    			ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
		}
  		return OK;
	}else{
#if DEBUG_MODE
		kprintf("(randRTQ_SEL)");
#endif
		refill_rtrdyqueue();	
		if(schedule_rtrdyqueue(optr) == 1){
			return OK;
		}
		/*if no real-time processes availabe schedule normal queue*/
		refill_rdyqueue();	
		if(schedule_rdyqueue(optr) == 1){
			return OK;
		}
		/*neither rt process nor normal processes are available so schedule the null process*/
		currpid = 0;
		nptr = &proctab[currpid];
  		nptr->quanta_left = 1;
 	 	nptr->eprio = 0;
  		nptr->pstate = PRCURR;
  		if(nptr != optr){
    			ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
		}
  		return OK;
	}
	return OK;
}


static void clean_queue(int head, int tail)
{
	while(nonempty(head)){
		if(lastkey(tail)<=0){
			getlast(tail);
		}else{
			break;
		}
	}
}

static void refill_rdyqueue()
{
	struct pentry *pptr;
	int pid;
#if DEBUG_MODE
	kprintf("(ENEW)");
#endif
	clean_queue(rdyhead,rdytail);
	for(pid=1;pid<NPROC;pid++){
		pptr = &proctab[pid];
		if(pptr->isRT == 0){
			if(pptr->pstate != PRFREE){
				pptr->eprio = pptr->pprio;
				if(pptr->quanta_left ==0){
					pptr->quanta_left =  pptr->eprio;
					pptr->goodness = pptr->quanta_left + pptr->eprio;
				}else{
					pptr->quanta_left = pptr->quanta_left/2 + pptr->eprio;
					pptr->goodness = pptr->quanta_left + pptr->eprio;
				}
			}
			if(pptr->pstate == PRREADY){
#if DEBUG_MODE
				kprintf("(%d:%d:%d)",pptr->pid,pptr->quanta_left,pptr->goodness);
#endif
				insert(pid,rdyhead,pptr->goodness);
			}
		}
	}
#if DEBUG_MODE
	kprintf("\n");
#endif
}

static void refill_rtrdyqueue()
{
	struct pentry *pptr;
	int pid;
	clean_queue(rtrdyhead,rtrdytail);
	for(pid=1;pid<NPROC;pid++){
		pptr = &proctab[pid];
		if(pptr->isRT == 1){
			if(pptr->pstate !=PRFREE){
				pptr->quanta_left = RT_QUANTUM; //quantum for rtp
			}
			if(pptr->pstate == PRREADY){
				insert(pid,rtrdyhead,pptr->quanta_left);
			}
		}
	}
	
}

static int quantum_remaining(struct pentry *optr)
{

	optr->quanta_left = optr->quanta_left-(QUANTUM - preempt);	
	if(optr->pstate == PRCURR){
#if DEBUG_MODE
			kprintf("(Q%d:%d:%d)",optr->pid,optr->quanta_left,optr->goodness);
#endif
		if(optr->quanta_left > 0){
			if(optr->isRT == 1){
				return 1;
			}else{
				if(nonempty(rdyhead)){
#if DEBUG_MODE
kprintf("(Dq%d)",optr->pid);
#endif
					dequeue(optr->pid);
				}
				optr->goodness = optr->quanta_left + optr->eprio;
				insert(optr->pid,rdyhead,optr->goodness);
				optr->pstate = PRREADY;
			}
			return 1;
		}else{
			optr->quanta_left = 0;
			if(optr->isRT ==1){
				optr->pstate = PRREADY;
			}else{
				if(nonempty(rdyhead)){
#if DEBUG_MODE
kprintf("(Dq%d)",optr->pid);
#endif
					dequeue(optr->pid);
				}
				optr->goodness = 0;
				insert(optr->pid,rdyhead,optr->goodness);
				optr->pstate = PRREADY;
			}
#if DEBUG_MODE
			kprintf("(Q%d:%d:%d)",optr->pid,optr->quanta_left,optr->goodness);
#endif
			return 0;
		}
	}
	return 0;
}

static int schedule_rdyqueue(struct pentry *optr)
{
	int pid;
	register struct pentry *nptr;
	clean_queue(rdyhead,rdytail);
	if(nonempty(rdyhead)){
		pid = getlast(rdytail);
		if(!(isbadpid(pid))){
			nptr = &proctab[pid];
			if(nptr->pstate == PRREADY){
#if DEBUG_MODE
				kprintf("(S%d:%d:%d)",nptr->pid,nptr->quanta_left,nptr->goodness);
#endif
    				nptr->pstate = PRCURR;
				currpid = pid;
    				if(nptr != optr){
      					ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
				}
				return 1;
			}
		}
	}	
#if DEBUG_MODE
				kprintf("(qempty)");
#endif
	return 0;
}

static int schedule_rtrdyqueue(struct pentry *optr)
{
	int pid;
	register struct pentry *nptr;
	clean_queue(rtrdyhead,rtrdytail);
	if(nonempty(rtrdyhead)){
		pid = getlast(rtrdytail);
		if(!(isbadpid(pid))){
			nptr = &proctab[pid];
			if(nptr->pstate == PRREADY){
#if DEBUG_MODE
				kprintf("(SRT%d:%d)",nptr->pid,nptr->quanta_left);
#endif
		    		nptr->pstate = PRCURR;
				currpid = pid;
    				if(nptr != optr){
      					ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
				}
				return 1;
			}
		}
	}	
#if DEBUG_MODE
				kprintf("(RTqempty)");
#endif
	return 0;
}
