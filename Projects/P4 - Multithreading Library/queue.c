/*
 * queue.c
 *
 * Definition of the queue structure and implemenation of its API functions.
 *
 */

#include "thread.h"
#include "queue.h"
#include <stdlib.h>
#include <assert.h>

struct _fifo_queue {
    node_item_t *head;
    node_item_t *tail;
    unsigned num_in_queue;
    unsigned capacity;
};

int queue_count(fifo_queue_t * queue)
{
    assert (queue != NULL);
    return queue->num_in_queue;
}

bool node_in_queue(node_item_t * node)
{
    // Return the in_queue bool attribute of the node.
    assert (node != NULL); //ensuring pointer is not Null.
    return node->in_queue;
}

fifo_queue_t * queue_create(unsigned capacity)
{
    if (capacity <= 0)
    {
        return NULL;
    }

    fifo_queue_t * new_queue = malloc(sizeof(fifo_queue_t));
    if (new_queue != NULL)
    {
        new_queue->head = NULL;
        new_queue->tail = NULL;
        new_queue->num_in_queue = 0;
        new_queue->capacity = capacity;
    }

    //if malloc fails, new_queue will be NULL.
    return new_queue;
}

void queue_destroy(fifo_queue_t * queue)
{
     assert (queue != NULL); //ensuring pointer argument is non null.
     assert (queue->num_in_queue == 0); //ensuring queue is empty before destroying.
     free(queue);
}

node_item_t * queue_pop(fifo_queue_t * queue)
{
    assert (queue != NULL); //ensuring pointer argument is non null.
    //Caller's responsibility to free nodes.
    if (queue->num_in_queue == 0)
    {
        return NULL;
    }

    node_item_t *ret = queue->head;
    queue->head = queue->head->prev;
    if (queue->head != NULL)
    {
        queue->head->next = NULL;
    } else
    {
        queue->tail = NULL;
    }
    queue->num_in_queue--;

    ret->prev = NULL; //removing information about the queue from the node.
    ret->in_queue = false;
    return ret;
}

node_item_t * queue_top(fifo_queue_t * queue)
{
    assert (queue != NULL); //ensuring pointer argument is non null.
    if (queue->num_in_queue == 0)
    {
        return NULL;
    }

    return queue->head;
}

int queue_push(fifo_queue_t * queue, node_item_t * node)
{
    //IT IS CALLERS RESPONSIBILITY TO ALLOCATE AND INITIALIZE NODE OBJECT.
    assert (queue != NULL); //ensuring pointer argument is non null.
    assert (node != NULL); //ensuring pointer argument is non null.
    assert (!node_in_queue(node)); //ensuring node is not already in another queue.
    if (queue->num_in_queue == queue->capacity)
    {
        return -1;
    }

    node->next = queue->tail;
    if (queue->tail != NULL)
    {
        queue->tail->prev = node;
    } else
    {
        queue->head = node;
    }

    queue->tail = node;
    queue->num_in_queue++;

    node->in_queue = true; //editing in_queue attribute to show it is now in a queue.
    return 0;
}

node_item_t * queue_remove(fifo_queue_t * queue, int id)
{
    assert (queue != NULL); //ensuring pointer argument is non null.

    node_item_t *curr = queue->tail;
    while ((curr != NULL) && (curr->id != id))
    {
        curr = curr->next;
    }

    if (curr != NULL)
    {
        node_item_t *next = curr->next;
        node_item_t *prev = curr->prev;

        if (prev == NULL) //curr is tail
        {
            queue->tail = curr->next;
        } else
        {
            prev->next = curr->next;
        }

        if (next == NULL) //curr is head
        {
            queue->head = curr->prev;
        } else
        {
            next->prev = curr->prev;
        }

        curr->next = NULL;
        curr->prev = NULL;
        queue->num_in_queue--;
        curr->in_queue = false;
    }

    return curr;
}

int queue_push_sorted(fifo_queue_t * queue, node_item_t * node) {
    assert (queue != NULL); //ensuring pointer argument is non null.
    assert (node != NULL); //ensuring pointer argument is non null.
    assert (!node_in_queue(node)); //ensuring node is not already in another queue.

    if (queue->num_in_queue == queue->capacity)
    {
        return -1; //queue is full.
    }

    node->next = NULL;
    node->prev = NULL;

    node_item_t * curr = queue->tail;
    if (curr == NULL){//queue was empty.
        queue->tail = node;
        queue->head = node;

        queue->num_in_queue++;

        node->in_queue = true; //editing in_queue attribute to show it is now in a queue.
        return 0;
    }

    if (queue->num_in_queue == 1){//only one node in the queue.
        if (curr->priority <= node->priority){
            node->next = queue->tail;
            queue->tail = node; //no need to set the head, at it was already equal to the other node.
            queue->head->prev = node;
        } else {
            queue->head = node;
            queue->tail->next = node;
            node->prev = queue->tail;
        }

        queue->num_in_queue++;

        node->in_queue = true; //editing in_queue attribute to show it is now in a queue.
        return 0;
    }

    if (curr->priority <= node->priority){ //the node has lowest priority in the queue.
        curr->prev = node;
        node->next = curr;
        queue->tail = node;
    } else {
        while (curr->next != NULL){
            if (curr->next->priority <= node->priority){ //this means the node is less important.
                curr->next->prev = node;
                node->next = curr->next;
                node->prev = curr;
                curr->next = node;
                break;
            }
            curr = curr->next;
        }
        //At this point, either we inserted the node, or curr->next is NULL
        if (curr->next == NULL){ //we are at the top of the queue, and our node is highest prio.
            curr->next = node;
            queue->head = node;
            node->prev = curr;
        }
    }

    queue->num_in_queue++;
    node->in_queue = true; //editing in_queue attribute to show it is now in a queue.
    return 0;
}
