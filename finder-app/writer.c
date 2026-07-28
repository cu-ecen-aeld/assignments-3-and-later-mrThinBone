#include <stdio.h>
#include <syslog.h>

int main(int argc, char** argv) {
    openlog(NULL, 0, LOG_USER);
    syslog(LOG_INFO, "Starting %s\n", argv[0]);

    if (argc != 3) {
        printf("Usage: %s <target_path> <write_content>\n", argv[0]);
        syslog(LOG_ERR, "Invalid arguments\n");
        closelog();
        return 1;
    }

    char* target_path = argv[1];
    char* write_content = argv[2];

    syslog(LOG_DEBUG, "Writing %s to %s\n", write_content, target_path);

    FILE* file = NULL;
    file = fopen(target_path, "w");
    if (file == NULL) {
        perror("Error opening file");
        syslog(LOG_ERR, "Failed to open file\n");
        closelog();
        return 1;
    }

    int write_ret = fprintf(file, "%s", write_content);
    if (write_ret < 0) {
        perror("Error writing to file");
        fclose(file);
        syslog(LOG_ERR, "Failed to write to file\n");
        closelog();
        return 1;
    }

    int close_ret = fclose(file);
    if (close_ret != 0) {
        perror("Error closing file");
        syslog(LOG_ERR, "Failed to close file\n");
        closelog();
        return 1;
    }

    syslog(LOG_INFO, "Success!\n");

    return 0;
}