/*
 * thread.c
 *
 * Implementation of the threading library.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h> //SO WE HAVE INT_MAX.
#include "ut369.h"
#include "queue.h"
#include "thread.h"
#include "schedule.h"
#include "interrupt.h"

// put your global variables here
struct thread * thread_array[THREAD_MAX_THREADS];
fifo_queue_t * waiting_queues[THREAD_MAX_THREADS];
struct thread * current_thread;
int num_threads;
//scheduler is externally defined.

/**************************************************************************
 * Cooperative threads: Refer to ut369.h and this file for the detailed 
 *                      descriptions of the functions you need to implement. 
 **************************************************************************/
//MALLOC LIST: WE MALLOC THREADS, AS WELL AS STACKS FOR EACH THREAD.


/* Create Thread (TCB) Struct */
Tid initialize_TCB(Tid id)
//This function initializes the TCB, with all fields null except id, assuming the argument is an unused TID.
{
    thread_array[id] = malloc(sizeof(struct thread));
    if (thread_array[id] == NULL)
    {
        return THREAD_NOMEMORY;
    }
    thread_array[id]->prev = NULL;
    thread_array[id]->next = NULL;
    thread_array[id]->priority = 0;
    thread_array[id]->in_queue = false;
    thread_array[id]->killed = false;
    thread_array[id]->waiting_queue = NULL;
    thread_array[id]->id = id;
    thread_array[id]->stack = NULL;
//    thread_array[id]->thread_context;
    thread_array[id]->state = 0;
    thread_array[id]->reaped = false;

    return id;
}

/* set priority of the current thread and then schedule */
//We assume we only call this when using the priority scheduler.
void
set_priority(int priority)
{
    assert(priority >= 0);
    assert(priority <= INT_MAX);
    current_thread->priority = priority;

    //when changing the thread's priority, we might context switch, so we
    //should call yield here.

    if (scheduler->realtime) {
        int ret = thread_yield(THREAD_ANY); //we yield so that if the thread is no longer highest prio, we swap.
        assert(ret != THREAD_INVALID); //This ensures no errors ocurred.
        //It's ok if ret returns THREAD_NOMORE, this just means we continue running on this thread, as there are no others to swap to.
    };

    return;
}

/* Initialize the thread subsystem */
void
thread_init(void)
{
    //Todo: might need to consider priority later.
    for (int i = 0; i < THREAD_MAX_THREADS; i++){
        thread_array[i] = NULL; //null initialize.
        waiting_queues[i] = queue_create(THREAD_MAX_THREADS);
    }
	initialize_TCB(0);
	thread_array[0]->state = THREAD_RUNNING;
	num_threads = 1;
	current_thread = thread_array[0];
}

/* Returns the tid of the current running thread. */
Tid
thread_id(void)
{
	return current_thread->id;
}

/* Return the thread structure of the thread with identifier tid, or NULL if 
 * does not exist. Used by thread_yield and thread_wait's placeholder 
 * implementation.
 */
static struct thread * 
thread_get(Tid tid)
{
	(void)tid;
	if ((tid < 0) || (tid >= THREAD_MAX_THREADS) || thread_array[tid] == NULL){
	    return NULL;
	}

	return thread_array[tid];
}

/* Return whether the thread with identifier tid is runnable.
 * Used by thread_yield and thread_wait's placeholder implementation
 */
static bool
thread_runnable(Tid tid)
{
	if (thread_array[tid] == NULL)
	{
        return false;
	}

	if ((thread_array[tid]->state != THREAD_READY) && (thread_array[tid]->state != THREAD_RUNNING)){
	    return false;
	}

	return true;
}

/* Context switch to the next thread. Used by thread_yield. */
static void
thread_switch(struct thread * next)
{
    volatile int set_context_called = 0; 
    //First I update the context field in the current thread to keep it up to date.
    getcontext(&(current_thread->thread_context));

    if (set_context_called == 0){
        set_context_called++;
        //swap threads. scheduler enqueue/dequeue is handled in the yield function.
        current_thread = next;
        current_thread->state = THREAD_RUNNING;
        setcontext(&(next->thread_context));
    }

    if (current_thread->killed == true){
        thread_exit(THREAD_KILLED);
    } else {
        current_thread->state = THREAD_RUNNING; 
    }

    return;
}

/* Voluntarily pauses the execution of current thread and invokes scheduler
 * to switch to another thread.
 */
Tid
thread_yield(Tid want_tid)
{
    int prev_int_state = interrupt_off();
	struct thread * next_thread = NULL;

    if (want_tid == thread_id()) { //The case where we want to yield to the thread that is currently running.
        assert(thread_runnable(want_tid));
        interrupt_set(prev_int_state);
        return want_tid;
    }

	if (want_tid == THREAD_ANY){ //The case where we want to yield to any thread in the queue (the next one).
	    next_thread = scheduler->dequeue(); //Returns the thread, returns NULL if queue is empty.
	    if (next_thread == NULL) {
	        interrupt_set(prev_int_state);
	        return THREAD_NONE;
	    } //if queue is empty, returns THREAD_NONE (only possible for THREAD_ANY).

	    if (next_thread == current_thread){ //This will only happen in the priority queue, when the current thread is strictly highest priority.
	    //This will also happen when there are no threads left in the queue other than the current one.
            assert(thread_runnable(thread_id()));
            interrupt_set(prev_int_state);
            return thread_id();
	    }

	    want_tid = next_thread->id; //this should be returned when the yield returns.
	} else { //The case where we yield to a specific thread in the queue.
        next_thread = scheduler->remove(want_tid); //returns the thread, otherwise returns NULL if not in queue.
	}

    if (next_thread != NULL) {
		// if current thread is still runnable, enqueue it, and change corresponding attributes
		next_thread->in_queue = false;
		if (thread_runnable(thread_id())){
            current_thread->state = THREAD_READY; //currently this will only ever change running to ready.
            scheduler->enqueue(current_thread);
            current_thread->in_queue = true;
        }
        thread_switch(next_thread);
    }
	else {
		/* cannot find thread with that tid in the ready queue */
		want_tid = THREAD_INVALID;
	}

    interrupt_set(prev_int_state);
    return want_tid;
}

/* Fully clean up a thread structure and make its tid available for reuse.
 * Used by thread_wait's placeholder implementation
 */
static void
thread_destroy(struct thread * dead)
{
    Tid dead_id = dead->id;
    free(dead->stack);
    free(dead);
    thread_array[dead_id] = NULL;
    assert(waiting_queues[dead_id] != NULL); //ensuring waiting queue is not null.
    assert(queue_top(waiting_queues[dead_id]) == NULL); //ensuring queue is empty. (We won't destroy this until the end).
}

/* New thread starts by calling thread_stub. The arguments to thread_stub are
 * the thread_main() function, and one argument to the thread_main() function. 
 */
static void
thread_stub(int (*thread_main)(void *), void *arg)
{
    //WE SET INTERRUPTS TO ENABLED, ONCE THE THREAD CAN BEGIN EXECUTING.
    interrupt_on();


        thread_exit(THREAD_KILLED);
    }
	int ret = thread_main(arg); // call thread_main() function with arg

	thread_exit(ret);
}

Tid
thread_create(int (*fn)(void *), void *parg, int priority)
{
    int prev_int_state = interrupt_off();
	Tid new_id = -1;

	if (num_threads >= THREAD_MAX_THREADS)
	{
        interrupt_set(prev_int_state);
	    return THREAD_NOMORE; //we have the maximum number of threads possible running.
	}
    //find a free TID, by iterating through thread array:
	for (int i = 0; i < THREAD_MAX_THREADS; i++)
	{
	    if (thread_array[i] == NULL)
	    {
	        new_id = i;
	        break;
	    }
	}
	if (new_id == -1){//all threads are either running, or yet to be reaped.
	    interrupt_set(prev_int_state);
	    return THREAD_NOMORE;
	}

	unsigned long stack_size = THREAD_MIN_STACK + 16; //ADD 16 BECAUSE WE NEED THREAD_MIN_STACK BYTES, BUT ALSO NEED TO START
	//ADDRESSING AT A 16BYTE ALIGNED ADDRESS.
	void* new_stack = malloc(stack_size);
	if (new_stack == NULL)
	{
	    interrupt_set(prev_int_state);
	    return THREAD_NOMEMORY;
	}

	new_id = initialize_TCB(new_id);	//initialize TCB:
	if (new_id == THREAD_NOMEMORY){
	    free(new_stack);
        interrupt_set(prev_int_state);
	    return THREAD_NOMEMORY;
	}

    struct thread* new_thread = thread_array[new_id];
    new_thread->priority = priority;
    new_thread->stack = new_stack;
    new_thread->state = THREAD_READY;

    getcontext(&(new_thread->thread_context));
    //Set parameters in the context of the new thread:
    //First I set the program counter to point to the thread_stub function.
    (new_thread->thread_context).uc_mcontext.gregs[REG_RIP] = (unsigned long) &thread_stub;
    //Setting stack pointer register:
    //creating stack_ptr
    unsigned long stack_ptr = ((unsigned long) new_stack) + stack_size - 1;
    while (((stack_ptr % 16) == 0) || ((stack_ptr % 8) != 0)){stack_ptr -= (unsigned long) 1;} //we need stack ptr to be divisible by 8 and not 16.
    (new_thread->thread_context).uc_mcontext.gregs[REG_RSP] = (unsigned long) stack_ptr;
    //Setting argument registers.
    (new_thread->thread_context).uc_mcontext.gregs[REG_RDI] = (unsigned long) fn;
    (new_thread->thread_context).uc_mcontext.gregs[REG_RSI] = (unsigned long) parg;

    assert(scheduler->enqueue(new_thread) != THREAD_NOMORE); //automatically changes in_queue attributes.
    new_thread->in_queue = true; //record that the thread is now in a queue.

    num_threads++; //increment the current number of threads.

    //NOW WE MUST CONSIDER PRIORITY SCHEDULING CASE.
    if (scheduler->realtime) {
        int ret = thread_yield(THREAD_ANY); //we yield so that if the new thread is higher prio, then we swap to it.
        assert(ret >= 0); //This ensures no errors ocurred.
    }

    interrupt_set(prev_int_state);
	return new_id;
}

Tid
thread_kill(Tid tid)
{
    int prev_int_state = interrupt_off();

	if (tid < 0 || tid >= THREAD_MAX_THREADS || thread_array[tid] == NULL || tid == thread_id()){
	    //first two conditions check if tid is within valid ranges.
	    //next two conditions check if the thread exists, and if it is the current thread.
        interrupt_set(prev_int_state);
        return THREAD_INVALID;
	}

	if (!(thread_array[tid]->state == THREAD_EXITED)){ //Ensure target thread has not already exited.
	    thread_array[tid]->killed = true; //now the thread knows it is killed.
	} //if target has already exited, then we should return successfully.


	//Now we check if the thread is asleep in some waiting queue, in which case we remove it, and add it to the ready queue.
	if (thread_array[tid]->waiting_queue != NULL){
	    queue_remove(thread_array[tid]->waiting_queue, tid); // remove the thread from the waiting queue.
	    thread_array[tid]->waiting_queue = NULL;
	    thread_array[tid]->state = THREAD_READY;
	    scheduler->enqueue(thread_array[tid]); //enqueue the thread in the ready queue.
	  

	    //HANDLE THE PRIO QUEUE CASE, NOW THAT WE HAVE ADDED THIS TO THE READY QUEUE.
        if (scheduler->realtime) {
            int ret = thread_yield(THREAD_ANY); //we yield so that if the new thread is higher prio, then we swap to it.
            assert(ret >= 0); //This ensures no errors ocurred.
        }
	}

    interrupt_set(prev_int_state);
	return tid;
}

void
thread_exit(int exit_code)
{
    
    interrupt_off();

    //Exiting the current_thread.
	current_thread->state = THREAD_EXITED;
	current_thread->exit_code = exit_code;
	current_thread->waiting_queue = NULL;
    num_threads--;

    //Now we must wake up all threads waiting on this thread to exit.
    int numwoken = thread_wakeup(waiting_queues[thread_id()], 0); //wakes a single reaper.

    if (numwoken == 1){current_thread->reaped = true;} //this implies that there is reaper ready to reap this now.

	if (num_threads > 0){
	    current_thread->priority = INT_MAX; //This sets the current thread to the lowest possible priority, so we must swap.
	    thread_yield(THREAD_ANY); //thread_yield will see that this thread is not runnable, and will not add it to the
	    //scheduler queue. Thus this thread will never run again.

	    
	}

    ut369_exit(exit_code);
}

/* Clean-up logic to unload the threading system. Used by ut369.c. You may 
 * assume all threads are either freed or in the zombie state when this is 
 * called.
 */
void
thread_end(void)
{
	for (int i = 0; i < THREAD_MAX_THREADS; i++){
	    if (thread_array[i] != NULL){ //free nonnull arrays.
	        free(thread_array[i]->stack);
	        free(thread_array[i]);
	        thread_array[i] = NULL;
	    }
	    queue_destroy(waiting_queues[i]);
	}
	num_threads = 0;
	current_thread = NULL;
}

/**************************************************************************
 * Preemptive threads: Refer to ut369.h for the detailed descriptions of 
 *                     the functions you need to implement. 
 **************************************************************************/

Tid
thread_wait(Tid tid, int *exit_code)
{
    int prev_int_state = interrupt_off();

	if (tid == thread_id()) {
		return THREAD_INVALID; //the target is itself.
	}

	// If thread does not exist, return error
	struct thread * target = thread_get(tid);
	if (target == NULL) {
		return THREAD_INVALID;
	}

	if (target->state == THREAD_EXITED){//in this case the target thread has already exited.
	    if(target->reaped == true){
            interrupt_set(prev_int_state);
	        return THREAD_INVALID; //There is already at least one reaper, so we fail.
	    }

	    if (exit_code != NULL){
	        *exit_code = target->exit_code;
	    }
	    thread_destroy(target); //this destroys the target, so its TID may be reused.

    } else {//In this case, we must suspend the running thread until the target exits.
        int ret = thread_sleep(waiting_queues[tid]); //we send this thread to the gulag.
        assert(ret != THREAD_INVALID); //This will happen when the waiting queue for the target is null.
        assert(ret != THREAD_NONE); //This will happen when there were no other processes in the ready queue allowing the current process to sleep.

        //If we got to this point, then we have woken up because our target thread has exited.
        if (exit_code != NULL){
            *exit_code = target->exit_code;
        }

        //We check to see if this is the last reaper.
        if (queue_top(waiting_queues[tid]) == NULL){//In this case, we are the last reaper.
            thread_destroy(target); //this destroys the target, so its TID may be reused.
        } else {//This is not the last reaper, wake another one.
            thread_wakeup(waiting_queues[tid], 0);
        }
	}

    interrupt_set(prev_int_state);
	return 0;
}

Tid
thread_sleep(fifo_queue_t *queue)
{

	/* TBD */
    int prev_int_state = interrupt_off();

    //first we handle a prerequisite check.
    if (queue == NULL){
        interrupt_set(prev_int_state);
        return THREAD_INVALID;
    }

    //Since the current thread is running, it is not in the ready queue (it is not in any queue).
    current_thread->state = THREAD_BLOCKED; //This changes current thread state from running to blocked.

    int ret = queue_push(queue, current_thread); //Returns -1 if queue is full.
    assert(ret != 1); //THIS ASSERTATION ENSURES THE QUEUE WE ARE PUSHING ONTO HAS ENOUGH SPACE.

    //AFTER THIS IS DONE, WE ALSO NEED TO RECORD THAT THE NODE IS NOW ENQUEUED.
    current_thread->in_queue = true;
    current_thread->waiting_queue = queue; //Track which waiting queue the current thread is in.

    //NOW WE YIELD TO ANY. IF THERE ARE NO THREADS LEFT IN THE READY QUEUE, THEN YIELD WILL RETURN WITH THE
    //APPROPRIATE ERROR (THREAD_NONE). OTHERWISE, WHEN YIELD RETURNS, IT WILL HAVE THE PROCESS ID WE YIELDED TO.

    ret = thread_yield(THREAD_ANY);
    if(ret == THREAD_NONE){//UNDO EVERYTHING IF THERE ARE NO THREADS TO SWAP TO.
        //There were no threads to swap to, so we must undo everything we have just done.
        current_thread->state = THREAD_RUNNING; //reset thread state to running
        queue_remove(queue, current_thread->id); //remove the thread from the given queue.
        current_thread->in_queue = false; //Note that the thread is not in any queue once again.
        current_thread->waiting_queue = NULL;
        ret = THREAD_NONE;
    }

    //Once we reach here, either yield failed, or we are awaken again. Regardless, the thread no longer waits.


    interrupt_set(prev_int_state);
	return ret;
}

/* When the 'all' parameter is 1, wake up all threads waiting in the queue.
 * returns whether a thread was woken up on not. 
 */
int
thread_wakeup(fifo_queue_t *queue, int all)
{
	/* TBD */
    int prev_int_state = interrupt_off();

    int num_woken = 0;
    if (queue == NULL){
        interrupt_set(prev_int_state);
        return num_woken;
    } //When queue is invalid we return 0.

    //when queue has no suspended threads, we also return 0.

    struct thread * awoken;

    if (all == 0){ //this is the case where we only wake one thread,
        //Now we pop a thread from the queue.
        awoken = queue_pop(queue); //The method automatically changes the in queue attribute of the awoken thread for us.

        if (awoken != NULL){ //There is a suspended thread.
            awoken->state = THREAD_READY; //sets the state of the thread to ready from blocked/waiting.
            scheduler->enqueue(awoken);
            awoken->in_queue = true;
            awoken->waiting_queue = NULL;
            num_woken++;

            //NOW WE MUST CONSIDER PRIORITY SCHEDULING CASE AFTER ADDING THE THREAD TO THE READY QUEUE.
            if (scheduler->realtime) {
                int ret = thread_yield(THREAD_ANY); //we yield so that if the new thread is higher prio, then we swap to it.
                assert(ret >= 0); //This ensures no errors ocurred.
            }
        }
    } else {
        //IN THIS CASE, WE POP EVERYTHING IN THE QUEUE, UNTIL IT IS EMPTY, AND ENQUEUE EVERYTHING.
        awoken = queue_pop(queue); //The method automatically changes the in queue attribute of the awoken thread for us.
        while (awoken != NULL){
            awoken->state = THREAD_READY; //sets the state of the thread to ready from blocked/waiting.
            scheduler->enqueue(awoken);
            awoken->in_queue = true;
            awoken->waiting_queue = NULL;
            num_woken++;

            //NOW WE MUST CONSIDER PRIORITY SCHEDULING CASE AFTER ADDING THE THREAD TO THE READY QUEUE.
            if (scheduler->realtime) {
                int ret = thread_yield(THREAD_ANY); //we yield so that if the new thread is higher prio, then we swap to it.
                assert(ret >= 0); //This ensures no errors ocurred.
            }

            awoken = queue_pop(queue);
        }
    }

    interrupt_set(prev_int_state);
	return num_woken;
}

struct lock {
    fifo_queue_t *waiting_queue;
    bool acquired;
    Tid owner;

};

struct lock *
lock_create()
{
	struct lock *lock = malloc(sizeof(struct lock));
	/* TBD */
	lock->waiting_queue = queue_create(THREAD_MAX_THREADS);
	lock->acquired = false;
	lock->owner = -1;
	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);
	/* TBD */

	if (lock->acquired == true){
	}
	queue_destroy(lock->waiting_queue);
	lock->waiting_queue = NULL;
	free(lock);
	lock = NULL;

	return;
}

void
lock_acquire(struct lock *lock)
{
    int prev_int_state = interrupt_off();

	assert(lock != NULL);

	/* TBD */

	while (lock->acquired){
	    int ret = thread_sleep(lock->waiting_queue); //Make this thread sleep until the lock is no longer acquired.
	    assert(ret != THREAD_NONE); //when queue is null.
	    assert(ret != THREAD_INVALID); //when there is nothing to yield to while sleeping.
	}

	//if we get here, then we assume the thread is awakened, and the lock is not aquired.
	lock->acquired = true; //acquire the lock.
	lock->owner = thread_id(); //set the lock owner to the current thread.

    interrupt_set(prev_int_state);
    return; //return
}

void
lock_release(struct lock *lock)
{
    int prev_int_state = interrupt_off();

	assert(lock != NULL);
	/* TBD */
	assert(lock->acquired); //ensures that the lock has been acquired.
	assert(lock->owner == thread_id()); //ensures the lock owner is the calling thread.

	lock->acquired = false;
	lock->owner = -1;
	thread_wakeup(lock->waiting_queue, 0); //wakeup a thread to acquire the lock.

    interrupt_set(prev_int_state);
}
