/**
 * @file utente_op.h
 * @brief Header file for user operations and signal handling.
 *
 * This file contains declarations of functions and variables used by user processes
 * to interact with the system, handle signals, and manage message queues.
 *
 */

#ifndef utente_op_h
#define utente_op_h
#include <signal.h>
#include <stdbool.h>

/**
 * @brief Flag indicating if SIGUSR1 signal was received (end of simulation).
 */
extern volatile sig_atomic_t ricevuto_SIGUSR1;

/**
 * @brief Flag indicating if SIGUSR2 signal was received (end of day).
 */
extern volatile sig_atomic_t ricevuto_SIGUSR2;

/**
 * @brief Simulates a wait for a visit or service.
 * 
 * @param n_nano_secs Number of nanoseconds to wait.
 * @param ID Identifier for the process or entity waiting.
 */
void wait_visit(long n_nano_secs, int ID);

/**
 * @brief Signal handler function.
 * 
 * @param signum The signal number received.
 */
void handler(int signum);

/**
 * @brief Signal handler that ignores the signal.
 * 
 * @param signum The signal number to ignore.
 */
void ignore_hanler(int signum);

/**
 * @brief Initializes signal handlers.
 */
void handler_init();

/**
 * @brief Selects a random number of requests.
 *
 * @param n_req Total number of requests.
 * @param size Pointer to store the size of the selected requests.
 * @return Pointer to the array of selected requests.
 */
int* selectRequests(int n_req, int* size);

/**
 * @brief Decides the intention based on probability.
 * 
 * @param p_serv_min Minimum service probability.
 * @param p_serv_max Maximum service probability.
 * @return true If the intention is positive.
 * @return false If the intention is negative.
 */
bool intention_choose(int p_serv_min, int p_serv_max);

/**
 * @brief Reads a message from a queue.
 * 
 * @param quID Queue ID.
 * @param pid PID of the receiver.
 * @return The read message content or status.
 */
int msg_read(int quID, long pid);

/**
 * @brief Sends a message to a queue.
 * 
 * @param quID Queue ID.
 * @param type Message type.
 * @param val Message value.
 * @return true If the message was sent successfully.
 * @return false If the message sending failed.
 */
bool msg_send(int quID, long type, long val);

/**
 * @brief Prints an error message and closes the application or handles the error.
 * 
 * @param str Error description string.
 */
void error_close(const char* str);

/**
 * @brief Checks if SIGUSR2 has been received.
 * 
 * @return true If SIGUSR2 has been received.
 * @return false If SIGUSR2 has not been received.
 */
bool checkForSIG2();

#endif /* utente_op_h */
