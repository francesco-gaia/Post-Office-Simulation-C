/**
 * @file ufficio.c
 *
 * @brief Shared implementation used by all "workers" processes.
 *
 * This file provides functions for ticket management, shared memory access,
 * and sportello reservation logic. All "workers" processes in the simulation—
 * i.e., the ticket issuer, operators, and the Director (as in employees of the office)—
 * invoke these functions to interact with the shared data structures.
 * The user process does not use this interface.
 *
 * @see ufficio.h
 */

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>
#include <stdio.h>
#include "../include/ufficio.h"

/**
 * @brief Global counter for assigning unique ticket IDs.
 *
 * Each new ticket receives an incremented ID based on this variable.
 */
size_t ticket_id = 1;

/**
 * @brief Inserts a new ticket into the ticket queue in shared memory.
 *
 * This function finds the next free slot in the ticket queue, populates it with
 * the ticket information (ID, service type, user PID, insertion time), and updates
 * the queue pointers (first, last, next_free). It also signals the semaphore
 * corresponding to the service type to notify waiting operators.
 *
 * @param local_shm Pointer to the local shared memory management structure.
 * @param service_type The type of service requested.
 * @param user_pid The PID of the user requesting the ticket.
 * @param orario_inserimento The timestamp when the ticket is inserted.
 * @return The assigned ticket ID on success, or 0 on error.
 */
size_t insertTicket(ManageSHM* local_shm, int service_type, pid_t user_pid, struct timespec orario_inserimento){
    
    size_t ret = 0;
    SharedMemory* shm = local_shm->shm;
    SHMoffset* offsets = local_shm->offsets;
    if(shm->coda_ticket.n_ticket != shm->coda_ticket.max_ticket){
        offsets->lista_ticket[shm->coda_ticket.next_free].ticket_id = ticket_id++;
        offsets->lista_ticket[shm->coda_ticket.next_free].service_type = service_type;
        offsets->lista_ticket[shm->coda_ticket.next_free].user_pid = user_pid;
        offsets->lista_ticket[shm->coda_ticket.next_free].orario_inserimento.tv_sec = orario_inserimento.tv_sec;
        offsets->lista_ticket[shm->coda_ticket.next_free].orario_inserimento.tv_nsec = orario_inserimento.tv_nsec;
        offsets->lista_ticket[shm->coda_ticket.next_free].next_ticket = -1;
        ret = offsets->lista_ticket[shm->coda_ticket.next_free].ticket_id;
        if(shm->coda_ticket.n_ticket == 0){
            shm->coda_ticket.first = shm->coda_ticket.next_free;
            offsets->lista_ticket[shm->coda_ticket.next_free].prev = -1;
        }else{
            offsets->lista_ticket[shm->coda_ticket.last].next_ticket = shm->coda_ticket.next_free;
            offsets->lista_ticket[shm->coda_ticket.next_free].prev = shm->coda_ticket.last;
        }
        shm->coda_ticket.last = shm->coda_ticket.next_free;
        shm->coda_ticket.n_ticket++;
        if(shm->coda_ticket.n_ticket != shm->coda_ticket.max_ticket){
            bool found = false;
            int i = (shm->coda_ticket.last + 1) % shm->coda_ticket.max_ticket;
            while(!found){
                if(offsets->lista_ticket[i].ticket_id == 0){
                    shm->coda_ticket.next_free = i;
                    found = true;
                }
                i = (i + 1) % shm->coda_ticket.max_ticket;
            }
        }
    }
    if(ret != 0){
        int tSem = local_shm->shm->coda_ticket.semaforo;
        struct sembuf op;
        op.sem_num = service_type-1;
        op.sem_op = 1;
        semop(tSem, &op, 1);
        
    }
    return ret;
}

/**
 * @brief Retrieves and removes a ticket of a specific service type from the queue.
 *
 * This function searches the ticket queue for the first ticket matching the
 * specified service type. If found, it retrieves the ticket details (ID, user PID,
 * insertion time), removes the ticket from the queue, and updates the queue pointers.
 *
 * @param local_shm Pointer to the local shared memory management structure.
 * @param service_type The service type to search for.
 * @param user_pid Pointer to store the PID of the user who owns the ticket.
 * @param insert_time Pointer to a timespec structure to store the ticket's insertion time.
 * @return The ID of the served ticket, or 0 if no ticket of the specified type is found.
 */
size_t serveTicket(ManageSHM* local_shm, int service_type, pid_t* user_pid, struct timespec* insert_time){
    
    size_t ret = 0;
    SharedMemory* shm = local_shm->shm;
    SHMoffset* offsets = local_shm->offsets;
    int count = 0;
    int pos = shm->coda_ticket.first;
    while(ret == 0 && count < shm->coda_ticket.n_ticket){
        if(offsets->lista_ticket[pos].service_type == service_type){
            ret = offsets->lista_ticket[pos].ticket_id;
            *user_pid = offsets->lista_ticket[pos].user_pid;
            insert_time->tv_nsec = offsets->lista_ticket[pos].orario_inserimento.tv_nsec;
            insert_time->tv_sec = offsets->lista_ticket[pos].orario_inserimento.tv_sec;
            //printf("Ticket %zu %d found\n", ret, service_type);
            int pred = offsets->lista_ticket[pos].prev;
            int next = offsets->lista_ticket[pos].next_ticket;
            if (pred != -1) {
                offsets->lista_ticket[pred].next_ticket = next;
            } else {
                shm->coda_ticket.first = next;
            }
            if (next != -1) {
                offsets->lista_ticket[next].prev = pred;
            } else {
                shm->coda_ticket.last = pred;
            }
            offsets->lista_ticket[pos].ticket_id = 0;
            offsets->lista_ticket[pos].user_pid = 0;
            offsets->lista_ticket[pos].orario_inserimento.tv_sec = 0;
            offsets->lista_ticket[pos].orario_inserimento.tv_nsec = 0;
            offsets->lista_ticket[pos].service_type = 0;
            offsets->lista_ticket[pos].prev = -1;
            offsets->lista_ticket[pos].next_ticket = -1;
            
            shm->coda_ticket.next_free = pos;
            shm->coda_ticket.n_ticket--;
            break;
        }
        
        count++;
        pos = offsets->lista_ticket[pos].next_ticket;
        if (pos == -1) {
            break;
        }
    }
    
    return ret;
}

/**
 * @brief Searches and reserves a sportello with the specified service type.
 *
 * This function scans the array of sportelli in shared memory to find the first available one
 * that matches the given service type. If found, the sportello is reserved by writing the
 * operator's PID into the corresponding field.
 *
 * @param local_shm Pointer to the local shared memory management structure.
 * @param pid The PID of the operator attempting to reserve the sportello.
 * @param service_type The service type handled by the sportello to be reserved.
 * @param mem_sem_id The semaphore ID for protecting shared memory access.
 * @return The ID of the reserved sportello if successful; 0 if no suitable sportello is available.
 */
size_t reserveSportello(ManageSHM* local_shm, pid_t pid, int service_type, int mem_sem_id){
    
    size_t ret = 0;
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = -1;
    SharedMemory* shm = local_shm->shm;
    SHMoffset* offsets = local_shm->offsets;
    bool found = false;
    if(shm != NULL){
        if(semop(mem_sem_id, &op, 1)!=-1){
            for(int i=0; i<shm->lista_sportelli.n_sportelli && !found; i++){
                if(offsets->sportelli[i].operator_pid == 0 && offsets->sportelli[i].service_type == service_type){
                    offsets->sportelli[i].operator_pid = pid;
                    offsets->sportelli[i].occupied_daily++;
                    ret = offsets->sportelli[i].id_s;
                    found = true;
                }
            }
            op.sem_num = 0;
            op.sem_op = 1;
            semop(mem_sem_id, &op, 1);
        }
    }
    return ret;
}

/**
 * @brief Frees an occupied sportello if owned by the calling operator.
 *
 * This function releases a reserved sportello by setting its operator PID to 0.
 * The release is allowed only if the specified sportello is currently reserved by the calling operator.
 *
 * @param local_shm Pointer to the local shared memory management structure.
 * @param id_s ID of the sportello to be released.
 * @param pid PID of the operator attempting to release the sportello.
 * @return true if the sportello was successfully released; false if the sportello is not owned by the calling operator or if the ID is invalid.
 */
bool freeSportello(ManageSHM* local_shm, size_t id_s, pid_t pid){
    
    bool ret = false;
    SharedMemory* shm = local_shm->shm;
    SHMoffset* offsets = local_shm->offsets;
    struct sembuf op;
    if(shm != NULL){
        if(id_s-1 < shm->lista_sportelli.n_sportelli && pid == offsets->sportelli[id_s-1].operator_pid){
            op.sem_num = offsets->sportelli[id_s-1].service_type-1;
            op.sem_op = 1;
            offsets->sportelli[id_s-1].operator_pid = 0;
            if(semop(shm->lista_sportelli.semaforo, &op, 1)!=-1)
                ret = true;
        }
    }
    return ret;
}

/**
 * @brief Attaches the shared memory segment and sets internal structure pointers.
 *
 * This function attaches to the shared memory segment identified by the given ID,
 * and initializes all internal pointers to substructures (e.g., sportelli array, ticket queue,
 * statistics) by computing their correct offsets from the base shared memory address.
 *
 * @param shmid The identifier of the shared memory segment returned by shmget().
 * @param offsets Pointer to the structure where the calculated offsets will be stored.
 * @return A pointer to the initialized SharedMemory structure on success; NULL on failure.
 */
SharedMemory* attachSHM(int shmid, SHMoffset* offsets){
    
    SharedMemory* shm = NULL;
    void* shared_mem = shmat(shmid, NULL, 0);
    if(shared_mem != (void*) -1){
        shm = (SharedMemory*) shared_mem;
        offsets->sportelli = (Sportello*)((char*)shm+shm->lista_sportelli.offset_sportelli);
        offsets->lista_ticket = (Ticket*)((char*)shm+shm->coda_ticket.offset_lista_ticket);
        offsets->operator_sportelli_ratio = (float*)((char*)shm+shm->statistics.offset_operator_sportelli_ratio);
        offsets->service_stats = (ServiceStats*)((char*)shm+shm->statistics.offset_service_stats);
    }
    return shm;
}
