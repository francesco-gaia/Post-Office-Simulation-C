/**
 * @file utente_op.c
 * @brief Implementation of user operation functions.
 *
 * This file contains the implementation of functions related to user operations,
 * including signal handling flags and simulation wait times.
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/msg.h>
#include <string.h>
#include "../include/msg.h"
#include "../include/utente_op.h"

/**
 * @brief Flag indicating if SIGUSR1 (end of simulation) was received.
 */
volatile sig_atomic_t ricevuto_SIGUSR1 = 0; 

/**
 * @brief Flag indicating if SIGUSR2 (end of day) was received.
 */
volatile sig_atomic_t ricevuto_SIGUSR2 = 0;

/**
 * @brief Simulates the time taken for a user to visit the office.
 *
 * This function calculates a random wait time based on simulation minutes
 * and sleeps for that duration.
 *
 * @param n_nano_secs Nanoseconds per simulation unit.
 * @param ID User ID (used for debug purposes).
 */
void wait_visit(long n_nano_secs, int ID) {
    int minuti_sim;
    (void)ID; // Used for debug
    if (rand() % 2 == 0) {
        minuti_sim = (rand() % 59) + 1;
    } else {
        minuti_sim = (rand() % 121) + 60;
    }
    long attesa_ns = (long) minuti_sim * n_nano_secs;
    struct timespec req;
    req.tv_sec = attesa_ns / 1000000000L;
    req.tv_nsec = attesa_ns % 1000000000L;
    nanosleep(&req, NULL);
    //printf("User ID:%d pid:%ld arrives after %d simulated minutes\n", ID, getpid(), minuti_sim);
}

/**
 * @brief Signal handler for the user process.
 *
 * Handles SIGTERM, SIGUSR1, SIGUSR2, and SIGINT.
 *
 * @param signum The signal number received.
 */
void handler(int signum){
    if(signum == SIGTERM){
        _exit(1);
    }
    if(signum == SIGUSR1)
        ricevuto_SIGUSR1 = 1;
    if(signum == SIGUSR2)
        ricevuto_SIGUSR2 = 1;
    if(signum == SIGINT){
        kill(getppid(), SIGUSR2);
        _exit(1);
    }
}
/**
 * @brief Signal handler that ignores the signal.
 *
 * @param signum The signal number to ignore.
 */
void ignore_handler(int signum){
    (void)signum;
}
/**
 * @brief Initializes signal handlers for the user process.
 *
 * Sets up handlers for SIGUSR1, SIGUSR2, SIGINT, SIGTERM (custom handler)
 * and ignores SIGTSTP, SIGHUP.
 */
void handler_init(){
    struct sigaction sa, s1;

    sigemptyset(&sa.sa_mask);
    sa.sa_handler = handler;
    sa.sa_flags = 0;

    sigemptyset(&s1.sa_mask);
    s1.sa_handler = ignore_handler;
    s1.sa_flags = 0;

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Errore nella sigaction per SIGUSR1");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror("Errore nella sigaction per SIGUSR2");
        exit(EXIT_FAILURE);
    }
    if(sigaction(SIGINT, &sa, NULL) == -1){
        perror("Errore nella sigaction per SIGINT");
        exit(EXIT_FAILURE);
    }
    if(sigaction(SIGTERM, &sa, NULL) == -1){
        perror("Errore nella sigaction per SIGTERM");
        exit(EXIT_FAILURE);
    }

    if(sigaction(SIGTSTP, &s1, NULL) == -1){
        perror("Errore nella sigaction per SIGTSTP");
        exit(EXIT_FAILURE);
    }
    if(sigaction(SIGHUP, &s1, NULL) == -1){
        perror("Errore nella sigaction per SIGHUP");
        exit(EXIT_FAILURE);
    }
}
/**
 * @brief Selects a random number of service requests.
 *
 * @param n_req Maximum number of requests.
 * @param size Pointer to an integer where the size of the generated array will be stored.
 * @return Pointer to an allocated array containing the selected service IDs, or NULL on error.
 */
int* selectRequests(int n_req, int* size){
    
    int* arr = NULL;
    if(n_req > 0){
        *size = (rand() % n_req) + 1;
        arr = malloc(sizeof(int) * (*size));
        if(arr){
            int num;
            for (int i = 0; i < *size; i++) {
                num = (rand() % 6) + 1;
                arr[i] = num;
            }
        }
    }else
        printf("n_req not valid");
    return arr;
}
/**
 * @brief Decides whether the user will take a break or make requests.
 *
 * @param p_serv_min Minimum probability threshold.
 * @param p_serv_max Maximum probability threshold.
 * @return true if the user decides to take a break, false otherwise.
 */
bool intention_choose(int p_serv_min, int p_serv_max) {
    int probability = p_serv_min + rand() % (p_serv_max - p_serv_min + 1);
    int r = rand() % 100;
    return r < probability;
}
/**
 * @brief Sends a message to a message queue.
 *
 * @param quID The message queue ID.
 * @param type The message type.
 * @param val The value to send in the message body.
 * @return true on success, false on failure.
 */
bool msg_send(int quID, long type, long val){
    bool ret = true;
    struct msgbuf msg;
    msg.mtype = type;
    snprintf(msg.mtext, MSG_SIZE, "%ld", val);
    if (msgsnd(quID, &msg, strlen(msg.mtext) + 1, 0) == -1) {
        ret = false;
    }
    return ret;
}
/**
 * @brief Reads a message from a message queue.
 *
 * @param quID The message queue ID.
 * @param pid The message type to read (usually the PID).
 * @return The integer value read from the message, or -2 on error.
 */
int msg_read(int quID, long pid){
    struct msgbuf msg;
    int ret;
    ssize_t s = msgrcv(quID, &msg, sizeof(msg.mtext), pid, 0);
    if (s == -1) {
        ret = -2;
    }else
        ret = atoi(msg.mtext);

    return ret;
}
/**
 * @brief Handles fatal errors by printing an error message and exiting.
 *
 * Sends SIGUSR2 to the parent process before exiting.
 *
 * @param str The error message to print.
 */
void error_close(const char* str){
    perror(str);
    kill(getppid(), SIGUSR2);
    _exit(1);
}
/**
 * @brief Checks if SIGUSR2 has been received.
 *
 * @return true if SIGUSR2 was received, false otherwise.
 */
bool checkForSIG2(){
    return ricevuto_SIGUSR2 == 1;
}
