#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <malloc.h>

typedef struct thread_node {
    pthread_t id;
    bool is_active;
    struct thread_node *next;
} thread_node;

// void init_thread_list(void);
void add_thread(pthread_t* id);
bool remove_thread(pthread_t* id);
// thread_node* find_thread(pthread_t id);
thread_node* get_root_node();
void free_root_node();

