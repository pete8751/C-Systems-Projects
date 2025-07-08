/*
 * thread.h
 *
 * Definition of the thread structure and internal helper functions.
 * 
 */

#ifndef _THREAD_H_
#define _THREAD_H_

#include "ut369.h"
#include <stdbool.h>
#include <ucontext.h>

/*
 * Valid thread states=
 */

enum {
    THREAD_EXITED = -2,
    THREAD_BLOCKED = -1,
	THREAD_READY = 0,
	THREAD_RUNNING = 1,
};

struct thread {
    Tid id;
    int state;
    int priority;

    bool in_queue;
    bool killed;

    struct thread *next;
    struct thread *prev;

    ucontext_t thread_context;
    void *stack;

    fifo_queue_t *waiting_queue;
    int exit_code;
    bool reaped;


};

// functions defined in thread.c
void thread_init(void);
void thread_end(void);

// functions defined in ut369.c
void ut369_exit(int exit_code);


#endif /* _THREAD_H_ */
