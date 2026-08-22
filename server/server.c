#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <malloc.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include "linked_list.h"

// ps aux | grep aesdsocket
// kill [pid]
// valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=<logfile_path> <your_program>

//#define DEBUG_LOG(msg,...) printf("sv.d.log: " msg "\n" , ##__VA_ARGS__)
//#define ERROR_LOG(msg,...) printf("sv.ERROR: " msg "\n" , ##__VA_ARGS__)

#define EXIT_FAILURE 1
#define MAX_BUFFER_SIZE 10485760
#define DEST_FILE "/var/tmp/aesdsocketdata"
#define MAX_CONNECTIONS 5

volatile sig_atomic_t shutdown_requested = 0;

int server_fd = -1;
int client_fd = -1;
bool daemon_mode = false;

struct thread_data {
    pthread_mutex_t *thread_mutex;
    pthread_rwlock_t *file_mutex;
    int client_fd;
};

void set_nonblocking(int socket_fd) {
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return;
    }
    
    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
    }
}

void logD(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    if (daemon_mode) {
        vsyslog(LOG_INFO, fmt, args);
    } else {
        printf("sv.DEBUG: ");
        vprintf(fmt, args);
        printf("\n");
    }

    va_end(args);
    
}

void logE(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    if (daemon_mode) {
        vsyslog(LOG_ERR, fmt, args);
    } else {
        printf("sv.ERROR: ");
        vprintf(fmt, args);
        printf("\n");
    }

    va_end(args);
}

void cleanupThenExit(int server_fd, int client_fd) {
    // printf("sv.ERROR: %s\n", msg);
    closelog();
    const char *file = DEST_FILE;
    remove(file);
    if (client_fd != -1) close(client_fd);
    if (server_fd != -1) close(server_fd);
    exit(0);
}

void onTerminate() {
    syslog(LOG_DEBUG, "Caught signal, exiting\n");
    cleanupThenExit(server_fd, client_fd);
}

void handle_signal(int sig)
{
    shutdown_requested = 1;
}

int writeToFile(const char* data, size_t size) {
    if (shutdown_requested == 1) return -1;

    logD("... Writing data of size %zu to file", size);
    mkdir("/var/tmp/", 0755);
    
    FILE *fp = fopen(DEST_FILE, "a");
    if (fp == NULL) {
        logE("Error opening file");
        return -1;
    }
    
    size_t written = fwrite(data, 1, size, fp);
    if (written != size) {
        logE("Error writing to file - expected: %zu, got: %zu", size, written);
        fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0) {
        logE("Error closing file");
        return -1;
    }
    return shutdown_requested == 0 ? 0 : -1;
}

int getFileLength(FILE* fp, long* file_size) {
    if (shutdown_requested == 1) return -1;
    // Move the file pointer to the end of the file
    if (fseek(fp, 0L, SEEK_END) != 0) {
        logE("Error seeking to end of file");
        fclose(fp);
        return -1;
    }
    // Get the size of the file
    *file_size = ftell(fp);
    if (*file_size < 0) {
        logE("Error getting file size");
        fclose(fp);
        return -1;
    }
    return 0;
}

// return -1 on error, 0 on success
int readChunk(FILE* fp, long offset, char** ptr_buffer, size_t* allocated, size_t* bytes_read) {
    if (shutdown_requested == 1) return -1;
    logD("... Reading data from file");

    // Move the file pointer to the designated offset of the file
    if (fseek(fp, offset, SEEK_SET) != 0) {
        logE("Error seeking to offset of file");
        return -1;
    }

    int c;
    size_t count = 0;
    char* buffer = *ptr_buffer;
    size_t allocated_inc = *allocated;

    while ((c = fgetc(fp)) != EOF)
	{
        if (shutdown_requested == 1) return -1;

        if (count >= allocated_inc) {
            allocated_inc = allocated_inc + 2048;
            // Reallocate memory for the buffer
            char* temp = realloc(*ptr_buffer, allocated_inc);
            if (temp == NULL) {
                logE("readChunk-Failed to reallocate memory for buffer");
                return -1;
            }
            *ptr_buffer = temp;
            buffer = temp;
        }

        buffer[count++] = c;
		if (c == '\n') {
            break;
        }
	}

    if (ferror(fp)) {
        logE("Error reading from file");
        return -1;
    }

    *allocated = allocated_inc;
    *bytes_read = count;

    
    return shutdown_requested == 0 ? 0 : -1;
}

static int send_buffer(int fd, const char *buffer, size_t length) {
    size_t total_sent = 0;

    while (total_sent < length) {
        ssize_t sent = send(fd,
                            buffer + total_sent,
                            length - total_sent,
                            0);

        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(10000); // 10ms, yield CPU before retrying
                continue;
            }

            logE("send failed: %s", strerror(errno));
            return -1;
        }

        if (sent == 0) {
            return -1;
        }

        total_sent += (size_t)sent;
    }

    return 0;
}

int sendResponse(int client_fd, char** buffer, size_t allocated) {
    FILE *fp = fopen(DEST_FILE, "r");
    if (fp == NULL) {
        logE("Error opening file");
        return -1;
    }

    long file_length;
    int ret = getFileLength(fp, &file_length);
    if (ret != 0 || shutdown_requested == 1) {
        fclose(fp);
        return -1;
    }

    long offset = 0;
    size_t bytes_read;

    while (offset < file_length) {    
        ret = readChunk(fp, offset, buffer, &allocated, &bytes_read);
        if (ret != 0) {
            logE("Error reading chunk");
            fclose(fp);
            return -1;
        }

        // size_t sent = send(client_fd, buffer, bytes_read, 0);
        ret = send_buffer(client_fd, *buffer, bytes_read);

        if (shutdown_requested == 1 || ret != 0) {
            logE("send failed");
            break;
        } else {
            logD("Sent %zd bytes", bytes_read);
        }
        offset += bytes_read;
    }

    if (fclose(fp) != 0) {
        logE("Error closing file");
        return -1;
    }

    return shutdown_requested == 0 ? 0 : -1;
}

void *handle_client(void *thread_param) 
{
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    int client_fd = thread_func_args->client_fd;
    pthread_mutex_t* thread_mutex = thread_func_args->thread_mutex;
    pthread_rwlock_t* file_mutex = thread_func_args->file_mutex;

    size_t total_bytes = 0;
    size_t allocated = 2048;
    char *buffer = malloc(2048);
    if (buffer == NULL)
    {
        logE("malloc failed");
        close(client_fd);
        free(thread_func_args);
        return NULL;
    }
    char buf[2048];
    ssize_t bytes;

    while (shutdown_requested == 0)
    {
        bytes = recv(client_fd, buf, sizeof(buf), 0);
        if (bytes < 0)
        { // error
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(10000); // 10ms, yield CPU before retrying
                continue;
            }
            logE("recv failed");
            free(buffer);
            buffer = NULL;
            /*if (errno == EINTR)
            {
                onTerminate(); 
            }*/
            break;
        }
        else
        {
            if (bytes == 0) {
                thread_func_args->client_fd = -1;
                break;
            }

            if (total_bytes + bytes > allocated)
            {
                allocated += 2048;

                if (allocated > MAX_BUFFER_SIZE)
                {
                    free(buffer);
                    buffer = NULL;
                    logE("recv - exceeded ma limit");
                    break;
                }

                char *temp = realloc(buffer, allocated);
                if (temp == NULL)
                {
                    logE("recv - failed to allocate mem");
                    free(buffer);
                    buffer = NULL;
                    break;
                }
                buffer = temp;
            }

            memcpy(buffer + total_bytes, buf, bytes);
            total_bytes += bytes;
        }
        if (bytes > 0 && buf[bytes - 1] == '\n')
            break;
    }

    if (shutdown_requested == 0 && buffer != NULL)
    {

        logD("Received %zd bytes", total_bytes);
        // Write to file and read back from it
        pthread_rwlock_wrlock(file_mutex);
        int rc = writeToFile(buffer, total_bytes);
        pthread_rwlock_unlock(file_mutex);

        if (rc < 0)
        {
            logE("writeToFile failed");
        }
        else
        {
            pthread_rwlock_rdlock(file_mutex);
            rc = sendResponse(client_fd, &buffer, total_bytes);
            pthread_rwlock_unlock(file_mutex);
            if (rc < 0)
            {
                logE("sendResponse failed");
            }
        }
    }
    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
    }

    pthread_t id = pthread_self();

    free(thread_func_args);
    close(client_fd);
    pthread_mutex_lock(thread_mutex);
    remove_thread(&id);
    pthread_mutex_unlock(thread_mutex);

    pthread_detach(id);

    /*if (shutdown_requested == 0)
    {
        close(client_fd);
        client_fd = -1;
        logD("Closed connection from %d", inet_ntoa(client_addr.sin_addr));
    }
    else
    {
        onTerminate();
    }*/

    return NULL;
}

void *timestamp_thread(void *arg)
{
    pthread_rwlock_t *file_mutex = (pthread_rwlock_t *)arg;

    while (shutdown_requested == 0)
    {
        // Sleep for 10 seconds, but stay responsive to shutdown requests
        for (int i = 0; i < 10 && shutdown_requested == 0; i++) {
            sleep(1);
        }
        if (shutdown_requested == 1) {
            break;
        }

        // Build RFC 2822 compliant timestamp string
        char timestamp[128];
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);

        size_t len = strftime(timestamp, sizeof(timestamp),
                              "timestamp:%a, %d %b %Y %H:%M:%S %z\n", &tm_now);
        if (len == 0) {
            logE("strftime failed to format timestamp");
            continue;
        }

        pthread_rwlock_wrlock(file_mutex);
        int rc = writeToFile(timestamp, len);
        pthread_rwlock_unlock(file_mutex);

        if (rc < 0) {
            logE("timestamp writeToFile failed");
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {

    if (argc == 2 && strcmp(argv[1], "-d") == 0)
        daemon_mode = true;

    // Register the signal handler for SIGINT and SIGTERM
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // No SA_RESTART, we want accept() to be interrupted
    
    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction failed");
        closelog();
        exit(EXIT_FAILURE);
    }
    

    // Logs will go to /var/log/syslog or /var/log/messages
    openlog(NULL, 0, LOG_USER);

    // setting up the server
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket creation failed");
        closelog();
        exit(EXIT_FAILURE);
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt failed");
        closelog();
        exit(EXIT_FAILURE);
    }

    int port = 9000;
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        closelog();
        exit(EXIT_FAILURE);
    }

    if (daemon_mode) {
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("Fork failed");
            close(server_fd);
            closelog();
            exit(EXIT_FAILURE);
        }
        
        // Parent process exits successfully
        if (pid > 0) {
            exit(0); 
        }

        // --- Child process continues execution here ---
        
        // Create a new session
        if (setsid() < 0) {
            perror("setsid failed");
            close(server_fd);
            closelog();
            exit(EXIT_FAILURE);
        }

        // Change working directory to root
        if (chdir("/") < 0) {
            perror("chdir failed");
            close(server_fd);
            closelog();
            exit(EXIT_FAILURE);
        }

        // Redirect stdin, stdout, stderr to /dev/null
        int dev_null = open("/dev/null", O_RDWR);
        if (dev_null < 0) {
            perror("Failed to open /dev/null");
            close(server_fd);
            closelog();
            exit(EXIT_FAILURE);
        }

        // Redirect Standard I/O
        dup2(dev_null, STDIN_FILENO);
        dup2(dev_null, STDOUT_FILENO);
        dup2(dev_null, STDERR_FILENO);
        close(dev_null);
    }

    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        logE("listen failed");
        closelog();
        exit(EXIT_FAILURE);
    }

    logD("Server listening on port %d", port);

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    pthread_mutex_t thread_mutex;
    pthread_rwlock_t file_mutex;
    if (pthread_mutex_init(&thread_mutex, NULL) != 0) {
        logE("Failed to initialize mutex");
        closelog();
        exit(EXIT_FAILURE);
    }
    

    // select writer preference
    // pthread_rwlockattr_t attr;
    // pthread_rwlockattr_init(&attr);
    // pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
    // if (pthread_rwlock_init(&file_mutex, &attr) != 0) {
    if (pthread_rwlock_init(&file_mutex, NULL) != 0) {
        logE("Failed to initialize file mutex");
        pthread_mutex_destroy(&thread_mutex);
        // pthread_rwlockattr_destroy(&attr);
        closelog();
        exit(EXIT_FAILURE);
    }
    // pthread_rwlockattr_destroy(&attr);

    // Spawn a thread that periodically appends a timestamp to the data file
    pthread_t timer_thread;
    bool timer_thread_started = false;
    if (pthread_create(&timer_thread, NULL, timestamp_thread, &file_mutex) == 0) {
        timer_thread_started = true;
    } else {
        logE("Failed to create timestamp thread");
    }

    while (shutdown_requested == 0) {
        client_fd = accept(server_fd, (struct sockaddr*) &client_addr, &addr_len);

        if (client_fd < 0) {
            logE("accept failed");
            if (errno == EINTR) {
                // onTerminate();
                break;
            } else {
                continue;
            }
        }
        set_nonblocking(client_fd);

        logD("Accepted connection from %s:%d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        logD("create thread for connection %d", client_fd);

        struct thread_data *thread_func_args = (struct thread_data *)malloc(sizeof(struct thread_data));
        thread_func_args->client_fd = client_fd;
        thread_func_args->file_mutex = &file_mutex;
        thread_func_args->thread_mutex = &thread_mutex;

        pthread_t client_thread;

        if (pthread_create(&client_thread, NULL, handle_client, (void *)thread_func_args) == 0)
        {
            pthread_mutex_lock(&thread_mutex);
            add_thread(&client_thread);
            pthread_mutex_unlock(&thread_mutex);
        } else {
            logE("Failed to create thread for connection %d", client_fd);
            free(thread_func_args);
            close(client_fd);
        }
    }

    // join all threads
    pthread_mutex_lock(&thread_mutex);
    thread_node* root = get_root_node();
    while (root != NULL) {
        thread_node* next = root->next;
        pthread_join(root->id, NULL);
        free(root);
        root = next;
    }
    // Reset the head pointer after freeing all nodes
    root = NULL;
    free_root_node();
    pthread_mutex_unlock(&thread_mutex);

    // join the timestamp thread
    if (timer_thread_started) {
        pthread_join(timer_thread, NULL);
    }

    // free mutexes
    pthread_rwlock_destroy(&file_mutex);
    pthread_mutex_destroy(&thread_mutex);


    if (shutdown_requested == 0) {
        close(server_fd);
    } else {
        onTerminate();
    }

    return 0;
}