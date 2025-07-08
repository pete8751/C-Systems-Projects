/*
 * prio.c
 *
 * Implementation of a priority scheduler (A3)
 */

#include "ut369.h"
#include "queue.h"
#include "thread.h"
#include "schedule.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static fifo_queue_t* prio_queue;
extern int queue_push_sorted(fifo_queue_t *, node_item_t *); // declaration for a function defined elsewhere
extern struct thread * current_thread;

int 
prio_init(void)
{
    prio_queue = queue_create(THREAD_MAX_THREADS);

    if (prio_queue != NULL) {
        return 0;
    }
    else {
        return THREAD_NOMEMORY;
    }
}

int
prio_enqueue(struct thread * thread)
{
    if (queue_push_sorted(prio_queue, thread) == -1) {
        return THREAD_NOMORE;
    }

    return 0;
}

struct thread *
prio_dequeue(void)
{
    if (queue_top(prio_queue) == NULL){ //This detects if there are no other nodes in the queue, and then returns NULL.
        return NULL;
    }

    if (current_thread->priority < queue_top(prio_queue)->priority){
        if ((current_thread->state == THREAD_READY) || (current_thread->state == THREAD_RUNNING)) {
            return current_thread;
        } //This is the case where the current thread is the highest priority, and it is runnable.
        //we just return back the current_thread.
    }

    return queue_pop(prio_queue);
}

struct thread *
prio_remove(Tid tid)
{
    struct thread * ret = queue_remove(prio_queue, tid); //Returns NULL if id is not in the queue.
    return ret;
}

void
prio_destroy(void)
{
    free(prio_queue);
    prio_queue = NULL;
}