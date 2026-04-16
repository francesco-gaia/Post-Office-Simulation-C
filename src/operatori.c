/**
 * @file operatori.c
 * @brief Implementation of the operator process in the post office simulation.
 *
 * This file contains the logic for the operator process, which serves users at counters (sportelli).
 * It handles simulation synchronization, serving tickets, updating statistics, and taking pauses.
 *
 */

#include "../include/operatori.h"
#include "../include/msg.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <time.h>

/** @brief Shared memory ID. */
int shm_id;

/** 
 * @brief Semaphore ID for memory access.
 * 
 * - sem[0]: Sportelli
 * - sem[1]: Tickets
 * - sem[2]: Statistics
 */
int sem_id_mem; 

/** 
 * @brief Semaphore ID for simulation startup synchronization.
 * 
 * - sem[0]: Employees start (perform -1)
 * - sem[1]: Users start
 * - sem[2]: Simulation start (wait for 0)
 * - sem[3]: Operators modifying statistics (perform -1 after end of day)
 */
int sem_id_avvio; 

/** @brief Maximum number of pauses allowed per day. */
int MAX_PAUSE;

/** @brief Message queue ID for communication with users. */
int msg_id_comunicazione;

/** @brief Pointer to the array of counters (sportelli). */
Sportello *sportelli;

/** @brief Pointer to the shared memory management structure. */
ManageSHM* l_shm;

/** @brief PID of the director process. */
pid_t direttore_pid;

/** @brief Flag for SIGUSR1 signal (simulation end). */
volatile sig_atomic_t ricevuto_SIGUSR1 = 0;
/** @brief Flag for SIGUSR2 signal (end of day). */
volatile sig_atomic_t ricevuto_SIGUSR2 = 0;
/** @brief Flag for SIGINT signal. */
volatile sig_atomic_t ricevuto_SIGINT = 0;


/**
 * @brief Signal handler for the operator process.
 * 
 * Handles SIGUSR1 (simulation end), SIGUSR2 (end of day), SIGINT (interrupt), and SIGTERM (termination).
 * 
 * @param signum The signal number received.
 */
void handler(int signum){
    //printf("Operatore riceve %d\n", signum);
    if(signum == SIGUSR1)
        ricevuto_SIGUSR1 = 1;
    if(signum == SIGUSR2)
        ricevuto_SIGUSR2 = 1;
    if(signum == SIGINT){
        kill(direttore_pid, SIGUSR2);
        _exit(1);
    }
    if(signum == SIGTERM){
        _exit(1);
    }
}
/**
 * @brief Array of service times (in simulation minutes) for each service type.
 * 
 * - 0: Shipping and receiving packages (10 min)
 * - 1: Letters and registered mail (8 min)
 * - 2: Bancoposta withdrawals and deposits (6 min)
 * - 3: Postal bill payments (8 min)
 * - 4: Financial products purchase (20 min)
 * - 5: Watches and bracelets purchase (20 min)
 */
const long EROG_TIME[]={10, 8, 6, 8, 20, 20};

/**
 * @brief Synchronizes the operator start with the simulation.
 * 
 * Decrements the employee start semaphore and waits for the simulation start semaphore (value 0).
 * 
 * @param sem_id_avvio Semaphore ID for startup synchronization.
 */
void avvia_simulazione_operatore(int sem_id_avvio) {
    struct sembuf cops = {0, -1, 0};
    if (semop(sem_id_avvio, &cops, 1) == -1) {
        if(errno==EINTR){
            if(!ricevuto_SIGUSR1){
                perror("Errore nell'aumento sem della simulazione");
                kill(direttore_pid, SIGUSR2);
                _exit(1);
            }
        }
    }
    if(ricevuto_SIGUSR1 == 0){
        struct sembuf sops = {2, 0, 0};
        if (semop(sem_id_avvio, &sops, 1) == -1) {
            if((errno==EINTR && ricevuto_SIGUSR1 == 0) || errno != EINTR){
                perror("Errore nell'attesa di avvio della simulazione");
                kill(direttore_pid, SIGUSR2);
                _exit(1);
            }
        }
    }
}

/**
 * @brief Attempts to occupy a sportello for a specific service type.
 * 
 * @param service_type The type of service the operator provides.
 * @return The ID of the occupied counter, or 0 if an error occurred.
 */
int occupa_sportello(int service_type) {
    size_t ret=reserveSportello(l_shm, getpid(), service_type, sem_id_mem);
    return (int)ret;
}

/**
 * @brief Calculates the difference between two timespec structs in simulation minutes.
 * 
 * @param end The end time.
 * @param start The start time.
 * @param n_nano_secs Nanoseconds per simulation minute.
 * @return The time difference in simulation minutes.
 */
double timespec_diff_seconds(const struct timespec *end, const struct timespec *start, long n_nano_secs) {
    time_t sec = end->tv_sec - start->tv_sec;
    long nsec = end->tv_nsec - start->tv_nsec;
    if (nsec < 0) {
        sec -= 1;
        nsec += 1000000000L;
    }
    return (sec * 1e9 + nsec) / n_nano_secs;
}

/**
 * @brief Updates the statistics for the provided service.
 * 
 * Updates both the specific service statistics and the global statistics in shared memory.
 * 
 * @param service_type The type of service provided.
 * @param service_time The time taken to serve the user (in nanoseconds).
 * @param start_time The time when the service started.
 * @param ticket_time The time when the ticket was issued.
 * @param n_nano_secs Nanoseconds per simulation minute.
 */
void aggiorna_statistiche(int service_type, long service_time, struct timespec start_time, struct timespec ticket_time, long n_nano_secs) {
    struct sembuf sops = {2, -1, 0};
    if(semop(sem_id_mem, &sops, 1)==-1){
    	perror("Errore blocco semaforo statistiche");
	}
    
    l_shm->offsets->service_stats[service_type-1].daily_served_user++;
    l_shm->offsets->service_stats[service_type-1].total_services_provided++;
    l_shm->offsets->service_stats[service_type-1].daily_wait_time+=(float)timespec_diff_seconds(&start_time, &ticket_time, n_nano_secs);
    l_shm->offsets->service_stats[service_type-1].daily_wait_service+=(float)service_time/n_nano_secs;
    
    l_shm->shm->statistics.total_users_served++;
    l_shm->shm->statistics.total_erog_services++;
    l_shm->shm->statistics.daily_served_user++;
    l_shm->shm->statistics.total_services_provided++;
    
    
    struct sembuf release = {2, 1, 0};
    if(semop(sem_id_mem, &release, 1)==-1){
    	perror("Errore sblocco semaforo statistiche");
	}
}
/**
 * @brief Manages the operator's pause.
 *
 *  If rand returns a multiple of 5 and MAX_PAUSE is not reached the operator
 *  frees the sportello and suspends itself.
 *
 * @param sportello_id The ID of the counter currently occupied by the operator.
 */
void gestisci_pausa(int sportello_id) {
    struct sembuf sops = {2, -1, 0};
    if (rand() % 5 == 0 && l_shm->shm->statistics.daily_pause < MAX_PAUSE) {
        struct sembuf ab={0, -1, 0};
        if(semop(sem_id_mem, &ab, 1)==-1){
        	perror("errore nell occupare lo sportello");
		}
        freeSportello(l_shm, sportello_id, getpid());
        
        struct sembuf plu={0, 1, 0};
        if(semop(sem_id_mem, &plu, 1)==-1){
        	perror("errore nell'incremento del semaforo");
		}
        struct sembuf stat_st={2, -1, 0};
        semop(sem_id_mem, &stat_st, 1);
        l_shm->shm->statistics.daily_pause++;
        l_shm->shm->statistics.total_pauses++;
        struct sembuf stat_re={2, 1, 0};
        semop(sem_id_mem, &stat_re, 1);
        //printf("Op va in pausa");
        kill(getpid(), SIGSTOP);
    }
}    
/**
 * @brief Main function
 *
 *  This function initializes all the needed variables and structs.
 *  It manages the operatore flow of operations.
 *
 * @param argc number of arguments
 * @param argv arguments of the program
 *
 * @return 0 if succesfull, 1 on error
 */
int main(int argc, char *argv[]){
    if (argc < 9){
        fprintf(stderr, "Parametri errati\n");
        kill(getppid(), SIGUSR2);
        exit(1);
    }
    
    signal(SIGINT, handler);
    signal(SIGUSR1, handler);
    signal(SIGUSR2, handler);
    signal(SIGTERM, handler);

    int ID = atoi(argv[1]);
    shm_id = atoi(argv[2]);
    sem_id_avvio = atoi(argv[3]);
    sem_id_mem = atoi(argv[4]);
    msg_id_comunicazione = atoi(argv[5]);
    MAX_PAUSE = atoi(argv[6]);
    int service_type = atoi(argv[7]);
    //printf("Operatore %d pid %d creato con service_type %d\n", ID, getpid(), service_type);
    long n_nano_secs = atof(argv[8]);
    direttore_pid = getppid();
    int sportello_id;
    int ticket_id;
    pid_t user;
    struct timespec* ticket_time = malloc(sizeof(struct timespec));
    unsigned int seed = (unsigned int)(
        time(NULL) ^
        getpid() ^
        (getppid() << 16) ^
        clock() ^
        (uintptr_t)&seed
        ^ ID
    );
    srand(seed);
    
    l_shm = malloc(sizeof(ManageSHM));
    if(!l_shm){
        perror("Errore malloc");
        kill(direttore_pid, SIGUSR2);
        _exit(1);
    }
    l_shm->offsets = malloc(sizeof(SHMoffset));
    l_shm->shm = attachSHM(shm_id, l_shm->offsets);
    if(l_shm->shm==NULL){
        perror("Errore nell attach della memoria condivisa");
        kill(direttore_pid, SIGUSR2);
        _exit(1);
    }
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigemptyset(&oldmask);
    sigaddset(&mask, SIGUSR2);
    struct sembuf op;
    op.sem_flg = 0;
    while(1){
        if(ricevuto_SIGUSR1){
            break;
        }
        avvia_simulazione_operatore(sem_id_avvio);
        if(ricevuto_SIGUSR1){
            break;
        }
        ricevuto_SIGUSR2 = 0;
        sportello_id = 0;
        if(ricevuto_SIGUSR1){
            break;
        }
        while(1){
            if(sportello_id == 0){
                op.sem_num = service_type - 1;
                op.sem_op = -1;
                if(semop(l_shm->shm->lista_sportelli.semaforo, &op, 1) == -1){
                    if(!ricevuto_SIGUSR2){
                        perror("errore semop");
                        kill(direttore_pid, SIGUSR2);
                        _exit(1);
                    }else
                        break;
                }

                sportello_id = occupa_sportello(service_type);

                if(sportello_id == 0){
                    if(!ricevuto_SIGUSR2){
                        perror("errore acquisizione sportello");
                        kill(direttore_pid, SIGUSR2);
                        _exit(1);
                    }else{
                        op.sem_op = 1;
                        semop(l_shm->shm->lista_sportelli.semaforo, &op, 1);
                        break;
                    }
                }
                op.sem_num = 2;
                op.sem_op = -1;
                if(semop(sem_id_mem, &op, 1) == -1){
                    if(ricevuto_SIGUSR2)
                        break;
                    else{
                        perror("errore semop");
                        kill(direttore_pid, SIGUSR2);
                        _exit(1);
                    }
                }
                l_shm->shm->statistics.active_operators_daily++;
                op.sem_op = 1;
                if(semop(sem_id_mem, &op, 1) == -1){
                    if(ricevuto_SIGUSR2)
                        break;
                    else{
                        perror("errore semop");
                        kill(direttore_pid, SIGUSR2);
                        _exit(1);
                    }
                }
                //printf("Sportello occupato %d %d\n",getpid(),sportello_id);
                if(ricevuto_SIGUSR2){
                    break;
                }
            }
            ticket_id = 0;
            op.sem_num = service_type-1;
            op.sem_op = -1;
            if(ricevuto_SIGUSR2)
                break;
            if(semop(l_shm->shm->coda_ticket.semaforo, &op, 1)==-1){
                if(ricevuto_SIGUSR2)
                    break;
                else{
                    perror("errore semop");
                    kill(direttore_pid, SIGUSR2);
                    _exit(1);
                }
            }
            op.sem_num = 1;
            op.sem_op = -1;
            if(ricevuto_SIGUSR2)
                break;
            if(semop(sem_id_mem, &op, 1) == -1){
                if(!ricevuto_SIGUSR2){
                    perror("errore semop");
                    kill(direttore_pid, SIGUSR2);
                    _exit(1);
                }else{
                    op.sem_num = service_type-1;
                    op.sem_op = 1;
                    semop(l_shm->shm->coda_ticket.semaforo, &op, 1);
                    break;
                }
            }
            ticket_id = serveTicket(l_shm, service_type, &user, ticket_time);
            op.sem_op = 1;
            if(semop(sem_id_mem, &op, 1) == -1){
                if(!ricevuto_SIGUSR2){
                    perror("errore semop");
                    kill(direttore_pid, SIGUSR2);
                    _exit(1);
                }else{
                    break;
                }
            }
            if(ticket_id == 0){
                perror("errore coda ticket");
                kill(direttore_pid, SIGUSR2);
                _exit(1);
            }
                
           if(ricevuto_SIGUSR2){
                ricevuto_SIGUSR2 = 0;
                break;
            }
            struct timespec start_time;
            clock_gettime(CLOCK_REALTIME, &start_time);;
            struct msgbuf msg;
            msg.mtype = user;
            snprintf(msg.mtext, sizeof(msg.mtext), "%d", getpid());

            if(msgsnd(msg_id_comunicazione, &msg, sizeof(msg.mtext), 0)==-1){
                if(ricevuto_SIGUSR2){
                     ricevuto_SIGUSR2 = 0;
                     break;
                }else{
                    perror("Errore msgsnd");
                    kill(direttore_pid, SIGUSR2);
                    _exit(1);
                }
			}
            sigprocmask(SIG_BLOCK, &mask, &oldmask);

			if(msgrcv(msg_id_comunicazione, &msg, sizeof(msg.mtext), getpid(), 0)==-1){
				perror("Errore msgrcv");
                kill(direttore_pid, SIGUSR2);
                exit(1);
			}

            long base_time = EROG_TIME[service_type-1] * n_nano_secs;
            long min_time=base_time/2;//-50%
            long max_time=base_time+min_time;//+50%
            long service_time=min_time+(rand()%(max_time-min_time+1));
            
            struct timespec ts;
            ts.tv_sec=service_time/1000000000L;
            ts.tv_nsec=service_time%1000000000L;
            nanosleep(&ts, NULL);
            
            
            msg.mtype = user;
            snprintf(msg.mtext, sizeof(msg.mtext), "fine");
            if(msgsnd(msg_id_comunicazione, &msg, sizeof(msg.mtext), 0)==-1){
                perror("Errore msgrcv");
                kill(direttore_pid, SIGUSR2);
                exit(1);
			}

			aggiorna_statistiche(service_type, service_time, start_time, *ticket_time, n_nano_secs);
            sigprocmask(SIG_SETMASK, &oldmask, NULL);

            if(ricevuto_SIGUSR2){
                ricevuto_SIGUSR2 = 0;
                break;
            }
            gestisci_pausa(sportello_id);
        }
        struct sembuf block={0, -1, 0};
        if(semop(sem_id_mem, &block, 1)==-1){
		perror("Errore blocco sportello");
		}
        freeSportello(l_shm, sportello_id, getpid());
        struct sembuf unlock={0, 1, 0};
        if(semop(sem_id_mem, &unlock, 1)==-1){
		perror("Errore blocco sportello");
		}
        struct sembuf sops = {3, -1, 0};
        if(semop(sem_id_avvio, &sops, 1)==-1){
        	perror("Errore su sem_id_avvio[3]");
        	kill(direttore_pid, SIGUSR2);
            _exit(1);
		}
        if(ricevuto_SIGUSR1){
            ricevuto_SIGUSR1 = 0;
            break;
        }
    }
    return 0;
} 
