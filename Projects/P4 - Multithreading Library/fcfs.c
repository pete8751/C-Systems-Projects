/*
 * fcfs.c
 *
 * Implementation of a first-come first-served scheduler.
 * Becomes round-robin once preemption is enabled.
 */

#include "ut369.h"
#include "queue.h"
#include "thread.h"
#include "schedule.h"
#include <stdlib.h>
#include <assert.h>

static fifo_queue_t* ready_queue;
static int count = 0; //the same as num_in_queue, might be unnecessary, but let's keep it incase.

int 
fcfs_init(void)
{
    ready_queue = queue_create(THREAD_MAX_THREADS);
    count = 0;

    if (ready_queue != NULL) {
        return 0;
    }
    else {
        return THREAD_NOMEMORY;
    }
}

int
fcfs_enqueue(struct thread * thread)
{
     if (queue_push(ready_queue, thread) == -1) {
         return THREAD_NOMORE;
     }

     count++;
     return 0;
}

struct thread *
fcfs_dequeue(void)
{
    struct thread * ret = queue_pop(ready_queue); //returns NULL if queue is empty.

    if (ret != NULL){count--;}

    return ret;
}

struct thread *
fcfs_remove(Tid tid)
{
    struct thread * ret = queue_remove(ready_queue, tid); //Returns NULL if id is not in the queue.
    if (ret != NULL){count--;}
    return ret;
}

void
fcfs_destroy(void)
{
    free(ready_queue);
    ready_queue = NULL;
    count = 0;
}