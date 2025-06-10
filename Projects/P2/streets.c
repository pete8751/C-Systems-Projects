#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "streets.h"

struct node {
    int id;
    double lat;
    double lon;
    int num_ways;
    int *way_ids;
    int num_neighbors;
    struct way ** ways;
    struct node ** neighbors;
    struct node * prev_node; //only used to keep track of path in astar_algorithm
};

struct way {
    int id;
    char *name;
    float maxspeed;
    bool oneway;
    int num_nodes;
    int *node_ids; //ordered list of node ids
};

struct ssmap {
    struct node ** nodes; //ordered list of nodes
    struct way ** ways; //ordered list of ways
    int nr_nodes;
    int nr_ways;
};

struct ssmap *
ssmap_create(int nr_nodes, int nr_ways)
{
    //Allocating the memory to the new smap, and returning null if we can't
    struct ssmap *new_ssmap = malloc(sizeof(struct ssmap));
    if (new_ssmap == NULL) {
        return NULL;
    }

    //Allocating memory for nodes and ways array in smap struct, freeing all prior allocations
    //and returning null if we can't (free all prior allocations).
    new_ssmap->nodes = malloc(nr_nodes * sizeof(struct node*));
    if (new_ssmap->nodes == NULL) {
        free(new_ssmap);
        new_ssmap = NULL;
        return NULL;
    }

    //Initializing pointers in the array
    for (int i = 0; i < nr_nodes; i++) {
        new_ssmap->nodes[i] = malloc(sizeof(struct node));
    }

    new_ssmap->ways = malloc(nr_ways * sizeof(struct way*));
    if (new_ssmap->ways == NULL) {
        free(new_ssmap->nodes);
        free(new_ssmap);

        new_ssmap->nodes = NULL;
        new_ssmap = NULL;
        return NULL;
    }

    for (int i = 0; i < nr_ways; i++) {
        new_ssmap->ways[i]= malloc(sizeof(struct way));
    }

    new_ssmap->nr_nodes = nr_nodes;
    new_ssmap->nr_ways = nr_ways;
    //handle number ways/nodes = 0
    if (nr_nodes == 0 || nr_ways == 0)
    {
        return NULL;
    }

    return new_ssmap;
}

void add_neighbors(struct way * way, struct node ** nodes, bool oneway)
{
    int * ordered_node_ids = way->node_ids;
    int len = way->num_nodes;

    for(int j = 0; j < (len - 1); j++)
    {
        //finds the current node and the next node ids in the ordered list of node ids.
        int curr_node_id = ordered_node_ids[j];
        int neighbor_id = ordered_node_ids[j+1];

        //finds the current node and the next node in the list of nodes
        struct node * curr_node = nodes[curr_node_id];
        struct node * neighbor = nodes[neighbor_id];

        //adds the neighbor to the list of neighbors of the current node, and the way to the list of ways of the current node.
        int end = curr_node->num_neighbors;

        curr_node->neighbors[end] = neighbor;
        curr_node->ways[end] = way;

        //increments the number of neighbors of the current node.
        curr_node->num_neighbors += 1;
    }
    //Does the same thing but in reverse if the way is not oneway.
    if (!oneway)
    {
        for(int j = (len - 1); j > 0; j--)
        {
           int curr_node_id = ordered_node_ids[j];
           int neighbor_id = ordered_node_ids[j-1];

           struct node * curr_node = nodes[curr_node_id];
           struct node * neighbor = nodes[neighbor_id];

           int end = curr_node->num_neighbors;
           curr_node->neighbors[end] = neighbor;
           curr_node->ways[end] = way;

           //increments the number of neighbors of the current node.
           curr_node->num_neighbors += 1;
        }
    }
}

bool
ssmap_initialize(struct ssmap * m)
{
    struct way ** ways = m->ways;
    int num_ways = m->nr_ways;

    struct node ** nodes = m->nodes;
    int num_nodes = m->nr_nodes;

    //Allocating space for way and neighbors attributes in node.
    for (int i = 0; i < num_nodes; i++)
    {
        struct node * node = nodes[i];
        node->num_neighbors = 0;
        node->prev_node = NULL;

        int max_neighbors = 2 * node->num_ways;

        if (max_neighbors == 0)
        {
            node->ways = NULL;
            node->neighbors = NULL;
            continue;
        }

        //Allocating memory for ways and neighbors pointer lists.
        node->ways = malloc(max_neighbors * sizeof(struct way *));
        if(node->ways == NULL)
        {
            printf("error: out of memory.\n");
            return false;
        }

        node->neighbors = malloc(max_neighbors * sizeof(struct node *));
        if(node->neighbors == NULL)
        {
            free(node->ways);
            node->ways = NULL;

            printf("error: out of memory.\n");
            return false;
        }
    }

    //adding all ways and neighbors.
    for (int i = 0; i < num_ways; i++)
    {
        bool oneway = ways[i]->oneway;
        add_neighbors(ways[i], nodes, oneway);
    }
    return true;
}

void
ssmap_destroy(struct ssmap * m)
{
    //Start by freeing all memory that was allocated to each node and way, and then free the struct itself.
    //destroy all pointers in the nodes and ways arrays.
    for (int i = 0; i < (m->nr_nodes); i++) {
        struct node * node = m->nodes[i];
        free(node->way_ids);
        node->way_ids = NULL;

        if (node->neighbors != NULL)
        {
            free(node->ways);
            free(node->neighbors);

            node->ways = NULL;
            node->neighbors = NULL;
        }
        free(node);
        node = NULL;
    }

    free(m->nodes);
    m->nodes = NULL;

    for (int i = 0; i < (m->nr_ways); i++) {
        struct way * way = m->ways[i];
        free(way->name);
        way->name = NULL;
        free(way->node_ids);
        way->node_ids = NULL;
        free(way);
        way = NULL;
    }

    free(m->ways);
    m->ways = NULL;
    free(m);
    m = NULL;
    return;
}

bool no_duplicate_nodes(int size, const int node_ids[size])
{
    for(int i = 0; i < size; i++)
    {
        int x = node_ids[i];
        for(int j = i + 1; j < size; j++)
        {
            int y = node_ids[j];
            if(x == y)
            {
                printf("error: node %d appeared more than once.\n", y);
                return false;
            }
        }
    }
    return true;
}

struct
way * ssmap_add_way(struct ssmap * m, int id, const char * way_name, float maxspeed, bool oneway,
              int num_nodes, const int node_ids[num_nodes])
{
    //takes the address of the way with the given id in the ssmap, and assigns it to new_way
    struct way * new_way = m->ways[id];

    //First we allocate memory for the name of the way, then we allocate memory for the node_ids of the way
    //we return Null if the allocation fails, and free the memory that was allocated before.
    new_way->name = malloc((strlen(way_name) + 1) * sizeof(char));
    if (new_way->name == NULL) {
        return NULL;
    }

    new_way->node_ids = malloc(num_nodes * sizeof(int));
    if (new_way->node_ids == NULL) {
        free(new_way->name);
        new_way->name = NULL;
        return NULL;
    }
    for (int i = 0; i < num_nodes; i++) {
        new_way->node_ids[i] = node_ids[i];
    }
    //Assigning the values to the appropriate location in the way array in the struct.
    new_way->id = id;
    strcpy(new_way->name, way_name);
    new_way->maxspeed = maxspeed;
    new_way->oneway = oneway;
    new_way->num_nodes = num_nodes;

    return new_way;
}

struct node * 
ssmap_add_node(struct ssmap * m, int id, double lat, double lon, 
               int num_ways, const int way_ids[num_ways])
{
    struct node * new_node = m->nodes[id]; //takes the address of the node with the given id

    //Again start by allocating all the memory that is needed, and then copy the values into the allocated memory.
    //Returning NULL if the allocation fails, and freeing the memory that was allocated before.

    new_node->way_ids = malloc(num_ways * sizeof(int));
    if (new_node->way_ids == NULL) {
        return NULL;
    }
    for (int i = 0; i < num_ways; i++) {
        new_node->way_ids[i] = way_ids[i];
    }

    new_node->id = id;
    new_node->lat = lat;
    new_node->lon = lon;
    new_node->num_ways = num_ways;
    return new_node;
}

bool out_of_bounds(int id, int hard_max)
{
    if (id < 0 || id >= hard_max) {
        return true;
    }
    return false;
}

void
ssmap_print_way(const struct ssmap * m, int id)
{
    //Error handling for invalid node.
    if (out_of_bounds(id, m->nr_ways)) {
        printf("error: way %d does not exist.\n", id);
        return;
    }

    //Finds way objects, and prints values. Since we kept way objects in id
    //order where index == id in ways array in ssmap, we simply retrieve way at
    //index id from the array.

    struct way * target_way = m->ways[id];
    printf("Way %d: %s\n", id, target_way->name);
    return;
}

void
ssmap_print_node(const struct ssmap * m, int id)
{
    //Same as print_way
    if (out_of_bounds(id, m->nr_nodes)) {
        printf("error: node %d does not exist.\n", id);
        return;
    }

    struct node * target_node = m->nodes[id];
    printf("Node %d: (%.7f, %.7f)\n", id, target_node->lat, target_node->lon);
    return;
}

struct way ** find_way_by_name_array_helper(const struct ssmap * m, const char * name) {
    //Allocates memory for an array of way pointers of length nr_ways from ssmap m.
    int num_ways = m->nr_ways;
    struct way ** target_ways = malloc(num_ways * (sizeof(struct way *)));
    int count = 0;

    //Checks if each way in the ssmap has name as a substring of its own name.
    //If so, it adds the corresponding way pointer to the list, and moves on.
    for (int i = 0; i < num_ways; i++) {
       struct way * curr_way = m->ways[i];
       if(strstr(curr_way->name, name) != NULL) {
            target_ways[count] = curr_way;
            count++;
       }
    }
    //Sets the index after the last way id to be NULL, and returns the array of ways.
    target_ways[count] = NULL;
    return target_ways;
}

int * find_way_ids_helper(const struct ssmap * m, const char * name) {
    //Allocates memory for an array of ints of length nr_ways from ssmap m.
    int num_ways = m->nr_ways;
    int * target_way_ids = malloc(num_ways * sizeof(int));
    int count = 0;

    //Checks if each way in the ssmap has name as a substring of its own name.
    //If so, it adds the corresponding way id to the list, and moves on.
    for (int i = 0; i < num_ways; i++) {
       struct way * curr_way = m->ways[i];
       if(strstr(curr_way->name, name) != NULL) {
            target_way_ids[count] = curr_way->id;
            count++;
       }
    }
    //I could reallocate here to reduce size of target_way_ids.
    //Sets the index after the last way id to point to -1, and returns the array of ways.
    target_way_ids[count] = -1;
    return target_way_ids;
}

void 
ssmap_find_way_by_name(const struct ssmap * m, const char * name)
{
    //calls helper to get way ids for ways with target name.
    int *target_way_ids = find_way_ids_helper(m, name);

    int i = 0;
    //prints all way ids in the list.
    while(target_way_ids[i] != -1) {
        printf("%d ", target_way_ids[i]);
        i++;
    }

    printf("\n");
    free(target_way_ids);
    target_way_ids = NULL;
    return;
}

int * find_nodes_ids_helper1(int nr_nodes, struct way ** target_ways) {
    //Setting all values in indicator list to zero.
    int *target_node_ids = malloc(nr_nodes * sizeof(int));
    for (int i = 0; i < nr_nodes; i++) {
        target_node_ids[i] = -2;
    }

    if (target_node_ids == NULL) {
        printf("error: out of memory.\n");
        return NULL;
    }

    //Looping over all node ids in the given array of ways. If a node id appears,
    //we set the value at the index corresponding to the node_id in the indicator
    //array to the way id. Thus if an index in array has nonzero value, we know that the node of id equal to the index
    //was included in one of the given paths.

    int index = 0;

    while(target_ways[index] != NULL) {
        int * node_ids = target_ways[index]->node_ids;
        int num_nodes = target_ways[index]->num_nodes;
        for(int i = 0; i < num_nodes; i++) {
            int node_id = node_ids[i];
            target_node_ids[node_id] = target_ways[index]->id;
        }
        index++;
    }

    return target_node_ids;
}

int * find_nodes_ids_helper2(int * node_indicator_array, struct way ** target_ways) {
    //mutates and returns the given node_indicator_array.
    int index = 0;
    //Similar idea as before, but this time, if we see that a value in the array is 1, we set
    //it to 2, to show that it also appears in one of the ways passed into this function.
    while(target_ways[index] != NULL) {
        int * node_ids = target_ways[index]->node_ids;
        int num_nodes = target_ways[index]->num_nodes;
        int way_id = target_ways[index]->id;
        for(int i = 0; i < num_nodes; i++) {
            int node_id = node_ids[i];
            if(node_indicator_array[node_id] == way_id || node_indicator_array[node_id] == -2)
            {
                continue;
            }
            else
            {
                node_indicator_array[node_id] = -1;
            }
        }
        index++;
    }
    return node_indicator_array;
}

void ssmap_find_node_by_names(const struct ssmap * m, const char * name1, const char * name2)
{
    struct way ** target_ways = find_way_by_name_array_helper(m, name1);
    int * node_indicator_array = find_nodes_ids_helper1(m->nr_nodes, target_ways);
    free(target_ways);
    target_ways = NULL;

    if(name2 == NULL) {
        for(int i = 0; i < m->nr_nodes; i++) {
            if(node_indicator_array[i] != -2) {
                printf("%d ", i);
            }
        }
    } else {

        struct way ** target_ways2 = find_way_by_name_array_helper(m, name2);
        int * node_indicator_array2 = find_nodes_ids_helper2(node_indicator_array, target_ways2);
        free(target_ways2);
        target_ways2 = NULL;

        for(int i = 0; i < m->nr_nodes; i++) {
            if(node_indicator_array2[i] == -1) {
                printf("%d ", i);
            }
        }
    }

    free(node_indicator_array);
    node_indicator_array = NULL;
    printf("\n");
    return;
}

struct node ** node_ids_to_node_ptr_list(const struct ssmap * m, int size, const int node_ids[size])
{
    struct node ** node_ptr_list = malloc(size * sizeof(struct node *));
    //Handling for memory issues
    if (node_ptr_list == NULL){
        printf("error: out of memory.\n");
        return NULL;
    }

    struct node ** nodes = m->nodes;
    int nr_nodes = m->nr_nodes;
    //Looping over each node id from the list to find the corresponding node in the map.
    for (int i = 0; i < size; i++) {
        int id = node_ids[i];

        //Checking if id is within given bounds
        if(out_of_bounds(id, nr_nodes))
        {
            printf("error: node %d does not exist.\n", id);
            free(node_ptr_list);
            node_ptr_list = NULL;
            return NULL;
        }

        //add pointer to the node to the list, and continue to next iteration. (nodes are ordered by id in the map array).
        node_ptr_list[i] = nodes[id];
    }
    return node_ptr_list;
}

int way_id_from_node_pair(const struct node * x, const struct node * y) {
    int *ways1 = x->way_ids;
    int nr_ways1 = x-> num_ways;
    int *ways2 = y->way_ids;
    int nr_ways2 = y-> num_ways;

    //Iterates through way ids associated in both nodes to find a common way id.
    for(int i = 0; i < nr_ways1; i++){
        int way_id = ways1[i];
        for(int j = 0; j < nr_ways2; j++){
            if (way_id == ways2[j]){
                return way_id;
            }
        }
    }
    //Prints an error, when no ways are found in common between input nodes, and returns -1.
    printf("error: there are no roads between node %d and node %d.\n", x->id, y->id);
    return -1;
}

struct way * way_from_node_pair(const struct node * x, const struct node * y) {
    struct node ** neighbors = x->neighbors;
    struct way ** ways = x->ways;
    int num_neighbors = x->num_neighbors;
    int target_id = y->id;

    for (int i = 0; i < num_neighbors; i++)
    {
        struct node * neighbor = neighbors[i];
        if(neighbor->id == target_id)
        {
            return ways[i];
        }
    }
    //Prints an error, when no ways are found in common between input nodes, and returns -1.
    printf("error: there are no roads between node %d and node %d.\n", x->id, y->id);
    return NULL;
}

struct way * wayptr_from_wayid(const struct ssmap * m, int way_id)
{
    struct way ** ways = m->ways;
    int nr_ways = m->nr_ways;

    //Looping through all ways in the list of ways from the map to find the one corresponding to the given id.
    for(int i = 0; i < nr_ways; i++){
        if (ways[i]->id == way_id){
            return ways[i];
        }
    }

    //If we don't find it, we return Null, and print an error.
    printf("error: way %d does not exist.\n", way_id);
    return NULL;
}

bool way_node_pair_compatibility(const struct node * x, const struct node * y)
{
    int x_id = x->id;
    int y_id = y->id;

    struct node ** x_neighbors = x->neighbors;
    int num_x_neighbors = x->num_neighbors;

    struct node ** y_neighbors = y->neighbors;
    int num_y_neighbors = y->num_neighbors;


    //Checks if there is a way from x to y.
    for(int j = 0; j < num_x_neighbors; j++)
    {
        if (x_neighbors[j]->id == y->id)
        {
            return true;
        }
    }

    //Checks if there is a way from y to x:
    for(int j = 0; j < num_y_neighbors; j++)
        {
            if (y_neighbors[j]->id == x->id)
            {
                printf("error: cannot go in reverse from node %d to node %d.\n", x_id, y_id);
                return false;
            }
        }

    //otherwise there are no ways between the nodes, and return appropriate message:
    printf("error: cannot go directly from node %d to node %d.\n", x_id, y_id);
    return false;
}

/**
 * Converts from degree to radian
 *
 * @param deg The angle in degrees.
 * @return the equivalent value in radian
 */
#define d2r(deg) ((deg) * M_PI/180.)

/**
 * Calculates the distance between two nodes using the Haversine formula.
 *
 * @param x The first node.
 * @param y the second node.
 * @return the distance between two nodes, in kilometre.
 */
static double
distance_between_nodes(const struct node * x, const struct node * y) {
    double R = 6371.;       
    double lat1 = x->lat;
    double lon1 = x->lon;
    double lat2 = y->lat;
    double lon2 = y->lon;
    double dlat = d2r(lat2-lat1); 
    double dlon = d2r(lon2-lon1); 
    double a = pow(sin(dlat/2), 2) + cos(d2r(lat1)) * cos(d2r(lat2)) * pow(sin(dlon/2), 2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a)); 
    return R * c; 
}

double time_between_linked_nodes(const struct way * way, const struct node * start_node, const struct node * end_node)
{
    float maxspeed = way->maxspeed;
    double distance = distance_between_nodes(start_node, end_node);

    return 60 * (distance / maxspeed);
}
/**DOCSTRING BY CHATGPT:
 * Calculates the total travel time for a given path in a street map.
 *
 *
 * Constraints:
 * - You always drive at the speed limit of the road that you are currently on.
 * - There are no acceleration or deceleration; you instantly drive at the speed limit.
 * - Instantaneous turning is assumed; there are no turn restrictions.
 * - All way objects have a non-zero speed limit.
 * - The street map (m) is never NULL.
 *
 * @param m - A pointer to the street map.
 * @param size - The number of nodes in the given path.
 * @param node_ids - An array of node IDs representing the path.
 *
 * @return The total travel time for the given path. If any error occurs, -1.0 is returned.
 */
double 
ssmap_path_travel_time(const struct ssmap * m, int size, int node_ids[size])
{
    double total_time = 0;

    //Check for duplicates in the given path, appropriate error is printed in helper.
    bool no_duplicates = no_duplicate_nodes(size, node_ids);
    if(!no_duplicates){return -1.0;}

    //Getting list of node pointers based on nodes in path.
    //NULL value indicates errors occured in the helper. Node related error messages are printed appropriately in helper.
    struct node ** node_ptr_list = node_ids_to_node_ptr_list(m, size, node_ids);
    if(node_ptr_list == NULL){return -1.0;}

    for(int i = 0; i < (size - 1); i++){
        //Checks if there is a way associated with given ids: (returns appropriate message if not)
        int way_id = way_id_from_node_pair(node_ptr_list[i], node_ptr_list[i + 1]);
        if(way_id == -1)
        {
            free(node_ptr_list);
            node_ptr_list = NULL;
            return -1.0;
        }

        //Checks final adjacency conditions, and if all conditions are satisfied, compatible is true, else it is false.
        bool compatible = way_node_pair_compatibility(node_ptr_list[i], node_ptr_list[i + 1]);
        if (!compatible)
        {
            free(node_ptr_list);
            node_ptr_list = NULL;
            return -1.0;
        }

        struct way * way_ptr = way_from_node_pair(node_ptr_list[i], node_ptr_list[i + 1]);
        double time = time_between_linked_nodes(way_ptr, node_ptr_list[i], node_ptr_list[i + 1]);
        total_time += time;
    }

    free(node_ptr_list);
    node_ptr_list = NULL;
    return total_time;
}

//ChatGPT gave the pseudocode for the following minheap that I implemented:

typedef struct min_heap_node
{
    double value; //The lower this is, the higher the priority.
    double cost;
    struct node * node;
} min_heap_node;

typedef struct min_heap
{
    min_heap_node ** node_list;
    int capacity;
    int num_nodes;
} min_heap;

min_heap * new_min_heap(int size)
{
    min_heap* heap = malloc(sizeof(min_heap));
    heap->capacity = size;
    heap->node_list = malloc(size * (sizeof(min_heap_node *)));
    for(int i = 0; i < size; i++)
    {
        heap->node_list[i] = malloc(sizeof(min_heap_node));

    }
    heap->num_nodes = 0;
    return heap;
}

void destroy_min_heap(min_heap * heap)
{
    for(int i = 0; i < heap->capacity; i++)
    {
        free(heap->node_list[i]);
        heap->node_list[i] = NULL;
    }
    free(heap->node_list);
    heap->node_list = NULL;
    free(heap);
    heap = NULL;
    return;
}

void swap_nodes(struct min_heap_node ** node_list, int index1, int index2)
{
    min_heap_node * node_2 = node_list[index2];
    node_list[index2] = node_list[index1];
    node_list[index1] = node_2;
    return;
}

void heapify_up(struct min_heap * min_heap, int curr_index)
{
    //returns immediately if element is at root. (we don't need to heapify).
    if (curr_index == 0)
    {
        return;
    }

    min_heap_node ** node_list = min_heap->node_list;

    //Calculate parent index using structure of min-heap.
    int parent_index = (curr_index - 1) / 2;
    //If parent value is greater than current node value, swap the two nodes, and heapify up from parent index.
    int parent_value = node_list[parent_index]->value;
    int curr_value = node_list[curr_index]->value;

    if (parent_value > curr_value)
    {
        swap_nodes(node_list, parent_index, curr_index);
        heapify_up(min_heap, parent_index);
    }

    return;
}

min_heap_node * heap_pop(struct min_heap * min_heap)
{
    int *num_nodes = &min_heap->num_nodes;
    min_heap_node ** node_list = min_heap->node_list;

    //Swapping root and last element, and setting last element to NULL, adjusting num_nodes accordingly.
    swap_nodes(node_list, 0, (*num_nodes) - 1);
    min_heap_node * node = node_list[(*num_nodes) - 1];
    *num_nodes += -1;

    //returning the removed node
    return node;
}

void heapify_down(struct min_heap * min_heap, int index)
{
    min_heap_node ** node_list = min_heap->node_list;
    //get index of current node, left child, and right child as per min-heap structure:
    int current = node_list[index]->value;
    int left_ind = 2 * index + 1;
    int right_ind = 2 * index + 2;

    // Check if left child exists and determine the smallest child
     if (left_ind < min_heap->num_nodes) {
            int left_val = node_list[left_ind]->value;
            int smallest_child_ind = left_ind;

            // Check if right child exists and update smallest child accordingly
            if (right_ind < min_heap->num_nodes) {
                int right_val = node_list[right_ind]->value;
                if (right_val < left_val) {
                    smallest_child_ind = right_ind;
                }
            }

    //If current value is greater than smallest child, swap these elements in the heap, and continue the process
        if(current > node_list[smallest_child_ind]->value)
        {
            swap_nodes(node_list, index, smallest_child_ind);
            heapify_down(min_heap, smallest_child_ind);
        }
    }
    return;
}

//Priority queue using min_heap implementation:

typedef struct priority_queue
{
    min_heap * queue;
} priority_queue;

priority_queue * new_priority_queue(int capacity)
{
    //Initializing priority queue, using new_min_heap.
    priority_queue * new_prio_queue = malloc(sizeof(priority_queue));
    if (new_prio_queue == NULL)
    {
        return NULL;
    }

    min_heap *new_heap = new_min_heap(capacity);
    if (new_heap == NULL)
    {
        return NULL;
    }

    new_prio_queue->queue = new_heap;

    return new_prio_queue;
}

void destroy_priority_queue(priority_queue * priority_queue)
{
    destroy_min_heap(priority_queue->queue);
    free(priority_queue);
    priority_queue = NULL;
    return;
}

void enqueue (priority_queue * priority_queue, double value, double cost, struct node * node, struct node * prev_node)
{
    //Get node min heap list pointer, and pointer to node after final node in the list.
    min_heap * queue = priority_queue->queue;
    int len = queue->num_nodes;

    int j = len;

    //check if node being added is already in the queue, and if so, if the new cost is lower than the old cost (update node if both are true).
    for (int i = 0; i < len; i++)
    {
        min_heap_node * curr_node = queue->node_list[i];
        if(curr_node->node == node)
        {
            if(value < curr_node->value)
            {
                j = i;
                break;
            }
            return;
        }
    }
    min_heap_node * node_ptr = queue->node_list[j];

    if (j == len)
    {
        //if we are adding a new node, we need to increment the number of nodes in the queue.
        queue->num_nodes += 1;

    }

    //Set new node at end of list with given values, id, path, add 1 to num_nodes counter.
    node_ptr->value = value;
    node_ptr->cost = cost;
    node_ptr->node = node;
    node->prev_node = prev_node;

    //re-heapify starting from new added element.
    heapify_up(queue, j);

    return;
}

min_heap_node * dequeue(priority_queue * priority_queue)
{
    min_heap * queue = priority_queue->queue;
    min_heap_node * node = heap_pop(queue);
    heapify_down(queue, 0);
    return node;
}

int path_length(const struct node *end_node)
{
    const struct node *current = end_node;
    int length = 0;

    // Count the length of the path
    while (current != NULL)
    {
        length++;
        current = current->prev_node;
    }

    return length;
}

int * reconstructPath(struct node * end_node)
{
    //I add one so that I can set the last element of the array to NULL, so we know where to stop when printing.
    int length = path_length(end_node) + 1;

    // Allocate memory for the path
    int *path = malloc(length * sizeof(int));

    if (path == NULL)
    {
        printf("error: out of memory.\n");
        return NULL;
    }

    // Reconstruct the path
    struct node * current = end_node;

    // Set the last element of the array to -1
    int index = length - 1;
    path[index] = -1;
    index--;

    // Add the nodes to the path
    while (current != NULL)
    {
        path[index] = current->id;
        index--;

        struct node * temp = current;
        current = current->prev_node;
        //Remove the link to the previous node to avoid issues when running the algorithm multiple times.
        temp->prev_node = NULL;
    }

    return path;
}

int * astar_algorithm(int num_nodes, struct node * start_node, struct node * end_node)
{
    //initialize a priority queue to store nodes to explore and their priorities.
    priority_queue * Queue = new_priority_queue(num_nodes);
    //Initialize a list of length num_nodes of all zeros, so we can track which nodes have been visited.
    int * visited = calloc(num_nodes, sizeof(int));
    enqueue(Queue, 0, 0, start_node, NULL);
    while(Queue->queue->num_nodes > 0)
    {
        min_heap_node * node = dequeue(Queue);
        struct node * curr_node = node->node;
        if (curr_node->id == end_node->id)
        {
            free(visited);
            visited = NULL;
            destroy_priority_queue(Queue);
            return reconstructPath(curr_node);
        }

        //Marking the index corresponding to node id as 1 to show it has been visited.
        visited[curr_node->id] = 1;

        struct node ** neighbors = curr_node->neighbors;
        struct way ** paths = curr_node->ways;
        int num_neighbors = curr_node->num_neighbors;
        int index = 0;

        while(index < num_neighbors && neighbors[index] != NULL)
        {
            int neighbor_id = neighbors[index]->id;
            //Check if neighbor has been visited already:
            if(visited[neighbor_id] == 1)
            {
                index++;
                continue;
            }

            double heuristic_cost = distance_between_nodes(neighbors[index], end_node);
            double time_to_neighbor = time_between_linked_nodes(paths[index], curr_node, neighbors[index]);

            double current_cumulative_cost = node->cost + time_to_neighbor;

            double total_priority = current_cumulative_cost + heuristic_cost;
            enqueue(Queue, total_priority, current_cumulative_cost, neighbors[index], curr_node);
            //points to min_heap_node in the queue, and sets the previous node to the current node.

            index++;
        }
    }

    free(visited);
    visited = NULL;
    destroy_priority_queue(Queue);
    return NULL;
}


void ssmap_path_create(const struct ssmap * m, int start_id, int end_id)
{
    struct node ** node_list = m->nodes;
    int num_nodes = m->nr_nodes;

    //checks if ids exist.
    if(out_of_bounds(start_id, num_nodes))
    {
        printf("error: node %d does not exist.\n", start_id);
        return;
    }
    if(out_of_bounds(end_id, num_nodes))
    {
        printf("error: node %d does not exist.\n", start_id);
        return;
    }

    if (start_id == end_id)
    {
        printf("%d\n", start_id);
        return;
    }

    struct node * start_node = node_list[start_id];
    struct node * end_node = node_list[end_id];

    int * path = astar_algorithm(num_nodes, start_node, end_node);
    if (path == NULL)
    {
        printf("error: no path found between node %d and node %d.\n", start_id, end_id);
        return;
    }

    //print the path
    int i = 0;
    while(path[i] != -1)
    {
        printf("%d ", path[i]);
        i++;
    }
    printf("\n");

    free(path);
    path = NULL;
    return;
}