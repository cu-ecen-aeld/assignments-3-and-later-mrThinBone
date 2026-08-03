#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

void msleep(long ms)
{
    struct timespec ts;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;

    nanosleep(&ts, NULL);
}

// Optional: use these functions to add debug or error prints to your application
// #define DEBUG_LOG(msg,...)
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    int wait_to_release_ms = thread_func_args->wait_to_release_ms;
    int wait_to_obtain_ms = thread_func_args->wait_to_obtain_ms;
    pthread_mutex_t *lock = thread_func_args->mutex;
    msleep(wait_to_obtain_ms); // sleep for wait_to_obtain_ms milliseconds

    DEBUG_LOG("Thread %lu is waiting to obtain the lock", pthread_self());
    // obtain the mutex lock
    int ret = pthread_mutex_lock(lock);
    if (ret != 0) return thread_param;
    DEBUG_LOG("Thread %lu has obtained the lock", pthread_self());
    msleep(wait_to_release_ms); // sleep for wait_to_release_ms milliseconds

    DEBUG_LOG("Thread %lu is releasing the lock", pthread_self());
    // release the mutex lock
    ret = pthread_mutex_unlock(lock);
    if (ret != 0) return thread_param;
    
    
    thread_func_args->thread_complete_success = true;
    
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    struct thread_data* thread_func_args = (struct thread_data *) malloc(sizeof(struct thread_data));
    if(thread_func_args == NULL)
        return false;
    thread_func_args->mutex = mutex;
    thread_func_args->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_func_args->wait_to_release_ms = wait_to_release_ms;
    thread_func_args->thread_complete_success = false;
    
    int rc = pthread_create(thread, NULL, threadfunc, (void *) thread_func_args);
    if(rc != 0) {
        ERROR_LOG("Error creating thread %d", rc);
        free(thread_func_args);
        thread_func_args = NULL;
        return false;
    }
    DEBUG_LOG("Thread %lu started successfully", (unsigned long)*thread);

    /*struct thread_data* thread_func_return;
    int rc2 = pthread_join(*thread, (void **)&thread_func_return);
    if (rc2 != 0) {
        ERROR_LOG("Error joining thread");
        free(thread_func_args);
        thread_func_args = NULL;
        return false;
    }

    bool success = thread_func_return->thread_complete_success;
    free(thread_func_return);
    thread_func_args = NULL;
    thread_func_return = NULL;

    DEBUG_LOG("Thread %lu completed with success: %d", (unsigned long)*thread, success);*/

    return true;
}
