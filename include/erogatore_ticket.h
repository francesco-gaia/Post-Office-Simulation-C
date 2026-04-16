/**
 * @file erogatore_ticket.h
 * @brief Header file for the ticket dispenser module.
 *
 * This file contains the definitions and function prototypes for the ticket dispenser
 * component of the system. It handles signal processing, semaphore operations, and
 * inter-process communication related to ticket generation.
 */

#ifndef EROGATORE_TICKET_H
#define EROGATORE_TICKET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <time.h>

#include "ufficio.h"

/**
 * @brief Handles system signals.
 * 
 * @param sig The signal number to handle.
 */
void handle_signal(int sig);


/**
 * @brief Locks a semaphore.
 * 
 * @param semid The semaphore set identifier.
 * @param sem_num The index of the semaphore within the set.
 * @return true if the lock was successful, false otherwise.
 */
bool sem_lock(int semid, int sem_num);

/**
 * @brief Unlocks a semaphore.
 * 
 * @param semid The semaphore set identifier.
 * @param sem_num The index of the semaphore within the set.
 * @return true if the unlock was successful, false otherwise.
 */
bool sem_unlock(int semid, int sem_num);

/**
 * @brief Checks if a specific service type is available.
 * 
 * @param tipo The type of service to check.
 * @return true if the service is available, false otherwise.
 */
bool servizio_disponibile(size_t tipo);

/**
 * @brief Updates daily statistics for the ticket dispenser.
 */
void aggiornaStatisticheGiornaliere();

/**
 * @brief Updates the final cumulative statistics.
 */
void aggiornaStatisticheFinali();

/**
 * @brief Resets the daily statistics counters.
 */
void resetStatisticheGiornaliere();

/**
 * @brief Updates the list of available daily services.
 */
void aggiorna_servizi_giornalieri();

/**
 * @brief Main loop to handle incoming ticket requests.
 */
void gestisci_richieste();

#endif
