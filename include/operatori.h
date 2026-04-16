/**
 * @file operatori.h
 * @brief Header file for operator management and simulation.
 *
 * This file contains declarations of external variables and functions used
 * to manage operator processes, their interactions with shared memory,
 * and signal handling.
 *
 */

#ifndef OPERATORI_H
#define OPERATORI_H

#include "ufficio.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h> 

/**
 * @brief Flag indicating if SIGUSR1 signal was received.
 * 
 * Volatile is used to avoid unsafe optimizations for signals.
 */
extern volatile sig_atomic_t ricevuto_SIGUSR1;

/**
 * @brief Flag indicating if SIGUSR2 signal was received.
 */
extern volatile sig_atomic_t ricevuto_SIGUSR2;

/**
 * @brief Flag indicating if SIGINT signal was received.
 */
extern volatile sig_atomic_t ricevuto_SIGINT;

/**
 * @brief Shared memory ID.
 */
extern int shm_id;

/**
 * @brief Semaphore ID for memory access.
 */
extern int sem_id_mem;

/**
 * @brief Semaphore ID for simulation start.
 */
extern int sem_id_avvio;

/**
 * @brief Message queue ID for tickets.
 */
extern int msg_id_ticket;

/**
 * @brief Message queue ID for communication.
 */
extern int msg_id_comunicazione;

/**
 * @brief Maximum number of pauses allowed.
 */
extern int MAX_PAUSE;

/**
 * @brief Pointer to the array of counters (sportelli).
 */
extern Sportello *sportelli;

/**
 * @brief Pointer to the shared memory management structure.
 */
extern ManageSHM* l_shm;

/**
 * @brief PID of the director process.
 */
extern pid_t direttore_pid;

/**
 * @brief Signal handler function.
 * 
 * @param signum The signal number received.
 */
void handler(int signum);

/**
 * @brief Starts the operator simulation loop.
 * 
 * This function manages the operator's lifecycle, including waiting for start signal,
 * serving tickets, and handling pauses.
 * 
 * @param sem_id_avvio Semaphore ID used to wait for the start of the simulation.
 */
void avvia_simulazione_operatore(int sem_id_avvio);

/**
 * @brief Attempts to occupy a counter for a specific service type.
 * 
 * @param service_type The type of service the operator wants to provide.
 * @return int The ID of the occupied counter, or -1 if no counter is available.
 */
int occupa_sportello(int service_type);

/**
 * @brief Updates the statistics after a service is completed.
 * 
 * @param service_type The type of service provided.
 * @param service_time The duration of the service in nanoseconds.
 * @param start_time The timestamp when the service started.
 * @param ticket_time The timestamp when the ticket was issued.
 * @param n_nano_secs The duration of the service in nanoseconds (redundant?).
 */
void aggiorna_statistiche(int service_type, long service_time, struct timespec start_time, struct timespec ticket_time, long n_nano_secs);

/**
 * @brief Manages the operator's pause.
 * 
 * @param sportello_id The ID of the counter currently occupied by the operator.
 */
void gestisci_pausa(int sportello_id);

#endif //OPERATORI_H 
