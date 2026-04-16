
/**
 * @file add_users.c
 * @brief Program to add users to the simulation by communicating with the director process.
 *
 * This program takes the number of users to create as a command-line argument,
 * finds the FIFO pipe to communicate with the director, and sends a signal
 * to initiate user creation.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>

/**
 * @brief Path to the FIFO used for communication.
 */
#define FIFO_PATH "tmp/fifo"

char* get_directory_path(char* path);

/**
 * @brief Main function of the add_users program.
 *
 * Parses command line arguments to get the number of users to create.
 * Retrieves the directory path, constructs the FIFO path, and writes
 * the number of users to the FIFO. Then sends a SIGUSR1 signal to the
 * director process to trigger the user creation.
 *
 * @param argc Argument count.
 * @param argv Argument vector. argv[1] should contain the number of users.
 * @return 0 on success, 1 on error (invalid arguments, FIFO errors, memory allocation failure).
 */
int main(int argc, char* argv[]){
    
    if(argc < 2){
        printf("Usage: add_users <n_users>");
        exit(1);
    }
    int n_usr = atoi(argv[1]);
    if(n_usr > 0){
        char* dir_path = get_directory_path(argv[0]);
        size_t lenght = strlen(dir_path) + strlen(FIFO_PATH) + 1;
        char* f_path = malloc(lenght+1);
        if(f_path){
            snprintf(f_path, lenght+1, "%s/%s", dir_path, FIFO_PATH);
            struct stat st;
            if(stat(f_path, &st) == -1) {
                perror("FIFO non esistente");
                free(f_path);
                exit(1);
            }
            if(!S_ISFIFO(st.st_mode)) {
                fprintf(stderr, "Errore: %s non è una FIFO\n", f_path);
                free(f_path);
                exit(1);
            }
            int fd = open(f_path, O_RDWR | O_NONBLOCK);
            if(fd == -1) {
                perror("Errore apertura FIFO");
                free(f_path);
                exit(1);
            }
            char buf[32];
            ssize_t nread = read(fd, buf, sizeof(buf) - 1);
            if(nread <= 0) {
                perror("Errore lettura PID dal FIFO");
                close(fd);
                free(f_path);
                exit(1);
            }
            buf[nread] = '\0';
            pid_t dir_pid = (pid_t)atoi(buf);

            char buf_nusr[32];
            snprintf(buf_nusr, sizeof(buf_nusr), "%d", n_usr);
            if (write(fd, buf_nusr, strlen(buf_nusr) + 1) != (ssize_t)strlen(buf_nusr) + 1) {
                perror("Errore scrittura n_usr");
                close(fd);
                free(f_path);
                exit(1);
            }
            kill(dir_pid, SIGUSR1);
            close(fd);
            free(f_path);
        }else{
            perror("Malloc error");
            exit(1);
        }
        
    }else{
        printf("Inserire un numero positivo di processi utente da creare");
        exit(1);
    }
    return 0;
}

/**
 * @brief Finds the directory path of the process
 *
 * The function finds the directory of the executable, and returns the absolute path
 * to the project directory structure.
 *
 * @param path pointer to argv[0], the path of the executable
 * @return pointer to string containing the absolute path to the project structure
 */
char* get_directory_path(char* path){
    char resolved_path[PATH_MAX];
    realpath(path, resolved_path);
    char* dir = dirname(resolved_path);
    size_t len = strlen(dir) + strlen("/..")+ 1;
    char* dir_path = (char*)malloc(len);
    snprintf(dir_path, len, "%s/..", dir);
    return dir_path;
}
