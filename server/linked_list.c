#include "linked_list.h"

thread_node* head = NULL;

void add_thread(pthread_t* id) {
    thread_node *new_node = (thread_node *)malloc(sizeof(thread_node));
    new_node->id = *id;
    new_node->is_active = true;
    new_node->next = head;
    head = new_node;
}
bool remove_thread(pthread_t* id) {
    thread_node *current = head;
    thread_node *previous = NULL;

    while (current != NULL)
    {
        if (pthread_equal(current->id, *id))
        {
            if (previous == NULL)
            {
                head = current->next;
            } else {
                previous->next = current->next;
            }
            free(current);
            return true;
        }
        previous = current;
        current = current->next;
    }
    return false;
}

thread_node* get_root_node() {
    return head;
}

void free_root_node() {
    head = NULL;
}