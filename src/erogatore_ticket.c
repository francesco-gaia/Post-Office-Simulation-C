/**
 * @file erogatore_ticket.c
 * @brief Implementation of the ticket dispenser module.
 *
 * This file contains the logic for the ticket dispenser process, which handles
 * ticket requests from users and communicates with the central office system.
 */
#include "../include/ufficio.h"
#include "../include/erogatore_ticket.h"
#include "../include/msg.h"
#include <time.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_BUFFER 128

/**
 * @brief Shared memory ID.
 */
int shmid;
/**
 * @brief Semaphore ID for simulation start/day management.
 */
int sem_avvio_id;
/**
 * @brief Message queue ID for user requests and responses.
 */
int msg_queue_id;
/**
 * @brief Semaphore ID for shared memory access (e.g., ticket queue, statistics).
 */
int mem_sem_id;
/**
 * @brief File descriptor for the pipe used to receive daily services from the director.
 */
int fd_pipe;         
/**
 * @brief Total number of counters/tellers in the office.
 */
int n_sportelli;

/**
 * @brief Pointer to the shared memory management structure (queue, statistics, counters).
 */
ManageSHM* m_shm;   

/**
 * @brief Array storing the services enabled for the current day.
 *        Each element represents a service type assigned to a counter.
 */
size_t* servizi_abilitati;
/**
 * @brief Total number of distinct services available in the system.
 */
int services_number = 6;

/**
 * @brief PID of the director process (used to send signals if necessary).
 */
pid_t direttore_pid;

/**
 * @brief Flag to indicate the end of the simulation. Set by SIGUSR1.
 */
volatile sig_atomic_t fine_simulazione = 0;
/**
 * @brief Flag to indicate the end of the current simulated day. Set by SIGUSR2.
 */
volatile sig_atomic_t fine_giornata = 0;
/**
 * @brief Flag to indicate if SIGTERM was received (not currently used for specific logic, but good practice).
 */
volatile sig_atomic_t ricevuto_sigterm = 0;

/**
 * @brief Signal handler for the ticket dispenser process.
 * 
 * Handles:
 * - SIGUSR1: Sets the simulation end flag.
 * - SIGUSR2: Sets the day end flag.
 * - SIGTERM/SIGINT: Terminates the process immediately.
 * 
 * @param sig The received signal.
 */
void handle_signal(int sig) {
    if (sig == SIGUSR1) {
        fine_simulazione = 1;
    } else if (sig == SIGUSR2) {
        fine_giornata = 1;
    } else if (sig == SIGTERM || sig == SIGINT) {
        exit(1);
    }
}

/**
 * @brief Performs a lock operation on a semaphore.
 * 
 * Decrements the semaphore value.
 * 
 * @param semid ID of the semaphore set.
 * @param sem_num Index of the semaphore within the set.
 * @return true If the operation is successful.
 * @return false If the operation fails.
 */
bool sem_lock(int semid, int sem_num) {
    struct sembuf sop = {sem_num, -1, 0};
    if (semop(semid, &sop, 1) == -1) {
        //printf("sem_lock for semid %d and sem_num %d",semid, sem_num);
        return false;
    }
    return true;
}

/**
 * @brief Performs an unlock operation on a semaphore.
 * 
 * Increments the semaphore value.
 * 
 * @param semid ID of the semaphore set.
 * @param sem_num Index of the semaphore within the set.
 * @return true If the operation is successful.
 * @return false If the operation fails.
 */
bool sem_unlock(int semid, int sem_num) {
    struct sembuf sop = {sem_num, 1, 0};
    if (semop(semid, &sop, 1) == -1) {
        //perror("sem_unlock failed erog");
        return false;
    }
    return true;
}

/**
 * @brief Checks if a specific service type is available for the current day.
 * 
 * Scans the `servizi_abilitati` array to see if the requested service type
 * is among those active at the counters.
 * 
 * @param tipo The service type to check.
 * @return true If the service is available.
 * @return false If the service is not available.
 */
bool servizio_disponibile(size_t tipo) {
    for (int i = 0; i < n_sportelli; i++) {
        if (servizi_abilitati[i] == tipo){
            return true;
        }
    }
    //printf("Service %zu not found\n", tipo);
    return false;
}

/**
 * @brief Updates daily statistics at the end of a simulated day.
 * 
 * Calculates daily averages for served users, waiting times, service times,
 * and pauses. It also updates global simulation totals by accumulating daily data.
 * Additionally, it calculates the operator/counter ratio and counts unserved
 * services (users remaining in the queue).
 */
void aggiornaStatisticheGiornaliere() {
    SharedMemory* shm = m_shm->shm;
    SHMoffset* offset = m_shm->offsets;
    Stats* stats = &shm->statistics;
    int giorno = stats->current_day;

    stats->daily_avg_users_served = (float) stats->total_users_served / (giorno+1);
    stats->daily_avg_services_provided = (float) stats->total_services_provided / (giorno+1);

    if (stats->daily_served_user > 0) {
        float total_daily_wait_time = 0;
        float total_daily_service_time = 0;

        for (int i = 0; i < services_number; i++) {
            total_daily_wait_time += offset->service_stats[i].daily_wait_time;
            total_daily_service_time += offset->service_stats[i].daily_wait_service;
        }

        stats->daily_avg_user_wait_time = total_daily_wait_time / stats->daily_served_user;
        stats->daily_avg_service_time = total_daily_service_time / stats->daily_served_user;
    } else {
        stats->daily_avg_user_wait_time = 0;
        stats->daily_avg_service_time = 0;
    }

    stats->avg_user_wait_time += stats->daily_avg_user_wait_time;
    stats->avg_service_time += stats->daily_avg_service_time;
    if(stats->current_day != 0){
        stats->avg_user_wait_time /= 2;
        stats->avg_service_time /= 2;
    }
    
    stats->active_operators_total += stats->active_operators_daily;
    stats->total_pauses += stats->daily_pause;

    if (stats->active_operators_daily > 0) {
        stats->daily_avg_pauses = (float) stats->daily_pause / stats->active_operators_daily;
    } else {
        stats->daily_avg_pauses = 0;
    }
    
    int n_sportelli = shm->lista_sportelli.n_sportelli;
    int base_index = giorno * n_sportelli;
    float ratio = 0.0;
    for (int k = 0; k < n_sportelli; k++) {
        
        if (stats->active_operators_daily > 0) {
            ratio = (float)m_shm->offsets->sportelli[k].occupied_daily / stats->active_operators_daily;
        }
        offset->operator_sportelli_ratio[base_index + k] = ratio;
    }
    
    int next_valid = shm->coda_ticket.first;
    ServiceStats* s;
    for(int i = 0; i < shm->coda_ticket.n_ticket; i++){
        if (next_valid == -1) break;
        
        int s_type = offset->lista_ticket[next_valid].service_type;
        s = &offset->service_stats[s_type - 1];
        
        s->total_services_not_provided++;
        stats->total_services_not_provided++;
        stats->non_served_users_day++;
        next_valid = offset->lista_ticket[next_valid].next_ticket;
    }
    
    stats->daily_avg_services_not_provided = (float) stats->total_services_not_provided / (giorno+1);
    float before_wait_val = 0.0;
    float before_service_val = 0.0;
    for (int i = 0; i < services_number; i++) {
        s = &offset->service_stats[i];
        
        s->daily_avg_users_served = (float) s->total_services_provided / (giorno+1);
        
        s->daily_avg_users_not_served = (float) s->total_services_not_provided / (giorno+1);

        if (s->daily_served_user > 0) {
            before_wait_val = s->daily_avg_wait_time;
            before_service_val = s->daily_avg_service_time;
            s->daily_avg_wait_time = s->daily_wait_time / s->daily_served_user;
            s->daily_avg_service_time = s->daily_wait_service / s->daily_served_user;
        } else {
            s->daily_avg_wait_time = 0;
            s->daily_avg_service_time = 0;
        }
        
        s->avg_wait_time += s->daily_avg_wait_time;
        s->avg_service_time += s->daily_avg_service_time;
        
        if(before_wait_val != 0)
            s->avg_wait_time /= 2;
        if(before_service_val != 0)
            s->avg_service_time /= 2;
    
    }
    stats->current_day++;
}

/**
 * @brief Calculates the final statistics at the end of the simulation.
 * 
 * Normalizes global averages by dividing by the total number of simulated days.
 */
void aggiornaStatisticheFinali() {
    Stats* stats = &m_shm->shm->statistics;
    //printf("pre final stats\n");
    int giorni = stats->current_day;
    if (giorni <= 0) return;
    //printf("Final stats\n");
    stats->avg_user_wait_time /= giorni;
    stats->avg_service_time /= giorni;
    for (int i = 0; i < services_number; i++) {
        m_shm->offsets->service_stats[i].avg_wait_time /= giorni;
        m_shm->offsets->service_stats[i].avg_service_time /= giorni;
    }
}

/**
 * @brief Resets daily statistical counters.
 * 
 * Prepares the statistics structure for the start of a new day,
 * zeroing out the counters related to the previous day.
 */
void resetStatisticheGiornaliere() {
    Stats* stats = &m_shm->shm->statistics;
    stats->daily_served_user = 0;
    stats->non_served_users_day = 0;
    stats->daily_avg_user_wait_time = 0;
    stats->daily_avg_service_time = 0;
    stats->daily_pause = 0;
    stats->daily_avg_pauses = 0;
    ServiceStats* s;
    for (int i = 0; i < services_number; i++) {
        s = &m_shm->offsets->service_stats[i];
        s->daily_served_user = 0;
        s->daily_wait_time = 0;
        s->daily_wait_service = 0;
        s->daily_avg_users_served = 0;
        s->daily_avg_users_not_served = 0;
        s->daily_avg_wait_time = 0;
        s->daily_avg_service_time = 0;
    }
    stats->active_operators_daily = 0;
}

/**
 * @brief Reads the services enabled for the current day from the pipe.
 * 
 * Receives from the director process the array of active services for the counters.
 * In case of a read error, it signals the director and terminates.
 */
void aggiorna_servizi_giornalieri() {
    if(read(fd_pipe, servizi_abilitati, n_sportelli*sizeof(size_t))<0){
        if (fine_simulazione) {
            return;
        }
        printf("Error reading enabled services\n");
        kill(direttore_pid, SIGUSR2);
        _exit(1);
    }
}

/**
 * @brief Manages user requests for tickets.
 *
 * This is the main loop for the ticket dispenser. It waits for user messages,
 * checks service availability, issues tickets, and handles end-of-day/simulation
 * procedures including statistics updates.
 */
void gestisci_richieste() {
    struct msgbuf msg;
    struct sembuf start[2];
    struct sembuf stats = {3, 0, 0};
    struct sembuf wait = {4, -2, 0};
    bool first_day = true;
    struct timespec insert_time;
    size_t ticket_id;

    start[0].sem_num = 0;
    start[0].sem_op = -1;
    start[0].sem_flg = 0;
    start[1].sem_num = 2;
    start[1].sem_op = 0;
    start[1].sem_flg = 0;
    
    while (!fine_simulazione) {
        fine_giornata = 0;
        if(fine_simulazione)
            break;
        if(!first_day){
            if(semop(sem_avvio_id, &wait, 1) == -1){
                if(fine_simulazione)
                    break;
                perror("Attesa reset statistiche");
                kill(direttore_pid, SIGUSR2);
                _exit(1);
            }
            resetStatisticheGiornaliere();
        }
        if(fine_simulazione)
            break;
        if (semop(sem_avvio_id, &start[0], 1) == -1) {
            if(fine_simulazione)
                break;
            perror("Attesa avvio simulazione sem 0");
            kill(direttore_pid, SIGUSR2);
            _exit(1);
        }
        if (semop(sem_avvio_id, &start[1], 1) == -1) {
            if(!fine_simulazione){
                perror("Attesa avvio simulazione sem 1");
                kill(direttore_pid, SIGUSR2);
                _exit(1);
            }else
                break;
        }
        
        aggiorna_servizi_giornalieri();
        if(fine_simulazione)
            break;
        
        while (!fine_giornata) {
            if (msgrcv(msg_queue_id, &msg, sizeof(msg.mtext), -6, 0) == -1) {
                if (errno == EINTR && !fine_giornata){
                    perror("msgrcv");
                    kill(direttore_pid, SIGUSR2);
                    _exit(1);
                    
                }else{
                    if(fine_giornata)
                        continue;
                }
            }
            clock_gettime(CLOCK_REALTIME, &insert_time);
            size_t service_type = (size_t)msg.mtype;
            pid_t sender_pid = atoi(msg.mtext);
            
            if (!servizio_disponibile(service_type)) {
                snprintf(msg.mtext, MSG_SIZE, "%d", -1);
                msg.mtype = sender_pid;
                msgsnd(msg_queue_id, &msg, sizeof(msg.mtext), 0);
                continue;
            }else{
                if(fine_giornata)
                    continue;
                
                if(!sem_lock(mem_sem_id, 1)){
                    if(fine_giornata)
                        continue;
                    else{
                        printf("Errore lock semaforo ticket\n");
                        kill(direttore_pid, SIGUSR2);
                        _exit(1);
                    }
                }
                ticket_id = insertTicket(m_shm, service_type, sender_pid, insert_time);
                
                if(!sem_unlock(mem_sem_id, 1)){
                    if(fine_giornata)
                        continue;
                    else{
                        printf("Errore unlock semaforo ticket\n");
                        kill(direttore_pid, SIGUSR2);
                        _exit(1);
                    }
                }
                
                if(ticket_id != 0)
                    snprintf(msg.mtext, MSG_SIZE, "%zu", ticket_id);
                else{
                    snprintf(msg.mtext, MSG_SIZE, "%d", -2);
                }
                if(fine_giornata)
                    continue;
                    
                msg.mtype = sender_pid;
                if (msgsnd(msg_queue_id, &msg, sizeof(msg.mtext), 0) == -1) {
                    if(fine_giornata)
                        continue;
                    else{
                        perror("msgsnd");
                        kill(direttore_pid, SIGUSR2);
                        _exit(1);
                    }
                    
                }
            }
        }
        
        if (fine_giornata) {
            first_day = false;
            if (semop(sem_avvio_id, &stats, 1) == -1){
                perror("Attesa avvio simulazione fine day");
                kill(direttore_pid, SIGUSR2);
                kill(getpid(), SIGSTOP);
                
            }

            sem_lock(mem_sem_id, 2);
            aggiornaStatisticheGiornaliere();
            sem_unlock(mem_sem_id, 2);
            sem_unlock(sem_avvio_id, 4); 
            fine_giornata = 0;
        }
    }
    sem_lock(mem_sem_id, 2);
    aggiornaStatisticheFinali();
    sem_unlock(mem_sem_id, 2);
    sem_unlock(sem_avvio_id, 4);
}

/**
 * @brief Main functon of the program.
 *
 * This function initializes the simulation values and starts the @p gestisci_richieste() function.
 *
 * @param argc number of arguments
 * @param argv the program arguments
 *
 * @return 0 if succesfull
 */
int main(int argc, char* argv[]) {
    if (argc < 6) {
        fprintf(stderr, "Usage: %s <shmid> <msgid> <sem_avvio> <mem_sem> <fd_pipe> <array_servizi>\n", argv[0]);
        kill(getppid(), SIGUSR2);
        kill(getpid(), SIGTSTP);
    }
    signal(SIGUSR1, handle_signal);
    signal(SIGUSR2, handle_signal);
    signal(SIGTERM, handle_signal);

    //printf("Erogatore creato %d\n", getpid());
    shmid = atoi(argv[1]);
    msg_queue_id = atoi(argv[2]);
    sem_avvio_id = atoi(argv[3]);
    mem_sem_id = atoi(argv[4]);
    fd_pipe = atoi(argv[5]);
    
    direttore_pid = getppid();

    m_shm = malloc(sizeof(ManageSHM));
    m_shm->offsets = malloc(sizeof(SHMoffset));
    m_shm->shm = attachSHM(shmid, m_shm->offsets);
    if (!m_shm->shm) {
        perror("attachSHM fallita");
        kill(direttore_pid, SIGUSR2);
        kill(getpid(), SIGSTOP);

    }
    n_sportelli = m_shm->shm->lista_sportelli.n_sportelli;
    servizi_abilitati = malloc(n_sportelli * sizeof(size_t));

    gestisci_richieste();
    free(servizi_abilitati);
    return 0;
}
