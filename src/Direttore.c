/**
 * @file Direttore.c
 * @brief Implementation of the Director process.
 *
 * This file contains the main logic for the Director process, which manages the simulation,
 * initializes IPC resources, spawns child processes (Erogatore, Operators, Users), and
 * handles the daily simulation loop and statistics.
 */
#include "../include/Direttore_op.h"
#include "../include/ufficio.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>

/**
 * @brief Main function of the Director process.
 *
 * Initializes the simulation, reads configuration files, sets up IPC resources,
 * creates child processes, and manages the simulation loop.
 *
 * @param argc Argument count (unused).
 * @param argv Argument vector.
 * @return 0 on success, 1 on failure.
 */
int main(int argc, char* argv[]){
    (void)argc;
    //printf("Direttore creato con pid %d\n", getpid());
    
    handler_init();

    char* dir_path = get_directory_path(argv[0]);
    if(!dir_path)
        error_close("Error getting directory path");
    
    /* CONFIGURATION READING */
    general* g_conf;
    if(!(g_conf = readGeneralConf(dir_path)))
        error_close("Errore apertura file general conf");
    timout* t_conf;
    if(!(t_conf = readTimoutConf(dir_path)))
        error_close("Errore apertura file timout conf");
    explode* e_conf;
    if(!(e_conf = readExplodeConf(dir_path)))
        error_close("Errore apertura file explode conf");
    
    size_t len = strlen(dir_path) + strlen("/stats/Stats.csv") + 1;
    char* stats_path = malloc(len);
    if (!stats_path) {
        perror("malloc failed");
        return 1;
    }
    snprintf(stats_path, len, "%s/Stats/Stats.csv", dir_path);
    
    /* Correct time calculation */
    long n_nano_secs = t_conf->N_NANO_SECS;
    long total_nanosec_day = (n_nano_secs * 3600L) * t_conf->WORKING_HOURS; /**< Total nanoseconds per simulation day. */
    struct timespec req, rem;
    req.tv_sec = total_nanosec_day / 1000000000;
    req.tv_nsec = total_nanosec_day % 1000000000;
    int sim_duration = t_conf->SIM_DURATION;
    free(t_conf);
    
    int explode_threshold = e_conf->EXPLODE_THRESHOLD;
    free(e_conf);

    /* IPC RESOURCES INITIALIZATION */
    int shmid;
    ManageSHM* local_shm = initSHM(&shmid, *(g_conf), sim_duration);
    if(shmid == -1 || local_shm == NULL)
        error_close("Errore creazione memoria condivisa");
    
    unsigned int seed = (unsigned int)(
        time(NULL) ^
        getpid() ^
        (getppid() << 16) ^
        clock() ^
        (uintptr_t)&seed
        ^ ftok(__FILE__, 'D')
    );
    srand(seed);
    
    initSportelli(local_shm, g_conf);
    initQueue(local_shm, g_conf->MAX_TICKETS);
    initStats(local_shm, g_conf->NOF_WORKERS_SEAT, sim_duration, 6);
    
    int *fdFifo = malloc(sizeof(int));
    if(!fdFifo){
        freeIPC(local_shm->shm, NULL, shmid, -1, -1, -1, -1);
        error_close("Errore malloc");
    }
    char* fifo_path = initFifo(&fdFifo, dir_path);
    if(*fdFifo == -1 || !fifo_path){
        freeIPC(local_shm->shm, fifo_path, shmid, -1, -1, -1, -1);
        error_close("Errore fifo");
    }
    
    int fdPipe[2];
    if(pipe(fdPipe) == -1){
        freeIPC(local_shm->shm, fifo_path, shmid, -1, -1, -1, -1);
        error_close("Errore pipe");
    }
    
    int simSem;
    int memSem;
    if(!init_sem(&simSem, &memSem, g_conf->NOF_WORKERS, g_conf->NOF_USERS, local_shm->shm)){
        freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, -1, -1);
        error_close("Errore creazione e inizializzazione set semafori");
    }
    
    int toQu;
    int comQu;
    if(!init_msgqueue(&toQu, &comQu)){
        freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
        error_close("Errore creazione code messaggi");
    }
    
    /* CHILD PROCESSES PREPARATION AND START */
    
    size_t* service_arr = malloc(sizeof(size_t) * g_conf->NOF_WORKERS_SEAT);
    if(!service_arr){
        freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
        error_close("Malloc error");
    }
    service_choose(service_arr, local_shm->offsets, g_conf->NOF_WORKERS_SEAT);
    if(!service_apply(service_arr, local_shm->shm, g_conf->NOF_WORKERS_SEAT)){
        freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
        error_close("errore aggiornamento servizi");
    }
    
    pid_t* operators = malloc(sizeof(pid_t)*g_conf->NOF_WORKERS);
    if(!operators){
        freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
        error_close("Malloc error");
    }
    pid_t* users = malloc(sizeof(pid_t)*g_conf->NOF_USERS);
    if(!users){
        freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
        error_close("Malloc error");
    }
    
    pid_t erog;
    if(!erog_start(shmid, toQu, simSem, memSem, fdPipe, dir_path, &erog) || ricevuto_SIGUSR2 == 1){
        freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
        error_close("Errore inizializzazione erogatore");
    }
    
    int ID = 0;
    int n_op = g_conf->NOF_WORKERS;
    int n_usr = g_conf->NOF_USERS;
    int n_seats = g_conf->NOF_WORKERS_SEAT;
    
    int creat = op_start(&ID, shmid, simSem, memSem, comQu, g_conf->NOF_PAUSE, dir_path, operators, n_op, n_nano_secs);
    if(creat != n_op || ricevuto_SIGUSR2 == 1){
        terminateSim(erog, operators, NULL, creat, 0);
        freeIPC(local_shm->shm, fifo_path, shmid, simSem, memSem, toQu, comQu);
        error_close("Errore inizializzazione operatore");
    }
    
    int n_request = g_conf->N_REQUEST;
    int P_SERV_MIN = g_conf->P_SERV_MIN;
    int P_SERV_MAX = g_conf->P_SERV_MAX;
    
    creat = user_start(&ID, users, toQu, comQu, simSem, dir_path, n_usr, n_request, P_SERV_MIN, P_SERV_MAX, n_nano_secs);
    if(creat != n_usr || ricevuto_SIGUSR2 == 1){
        terminateSim(erog, operators, users, n_op, creat);
        freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
        error_close("Errore inizializzazione utente");
    }
    
    free(g_conf);
    
    /* SIMULATION LOOP */
    
    bool continue_sim = true;
    int new_users;
    struct sembuf op [3];
    char buffer[32];
    bool first_time = true;
    bool explode = false;
    bool timout = false;
    
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigemptyset(&oldmask);
    sigaddset(&mask, SIGUSR1);
    
    printf("Inizio simulazione\n");
    if(!signalManage(&mask)){
        terminateSim(erog, operators, users, n_op, n_usr);
        freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
        error_close("Errore aggiunta maschera");
        
    }
    while(continue_sim){
        if(!pipe_write(fdPipe[1], service_arr, n_seats)){
            terminateSim(erog, operators, users, n_op, n_usr);
            freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
            error_close("Errore scrittura pipe");
        }
        if(stop_received()){
            terminateSim(erog, operators, users, n_op, n_usr);
            freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
            error_close("Simulazione terminata anticipatamente 1");
        }
        
        if(!first_time){
            op[0].sem_num = 4;
            op[0].sem_op = 2;
            op[0].sem_flg = 0;
            if(!sem_op(simSem, &op[0], 1)){
                terminateSim(erog, operators, users,  n_op, n_usr);
                freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
                error_close("Errore semOp attesa operatori");
            }
        }
        op[0].sem_num = 0;
        op[0].sem_op = 0;
        op[0].sem_flg = 0;
        op[1].sem_num = 1;
        op[1].sem_op = 0;
        op[1].sem_flg = 0;
        
        if(!sem_op(simSem, &op[0], 1)){
            terminateSim(erog, operators, users,  n_op, n_usr);
            freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
            error_close("Errore semOp attesa operatori");
        }
        if(!sem_op(simSem, &op[1], 1)){
            terminateSim(erog, operators, users,  n_op, n_usr);
            freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
            error_close("Errore semOp att users");
        }
        
        op[2].sem_num = 2;
        op[2].sem_op = -1;
        op[2].sem_flg = 0;
        if(!sem_op(simSem, &op[2], 1)){
            terminateSim(erog, operators, users,  n_op, n_usr);
            freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
            error_close("Errore semOp avvio");
        }
        
        printf("Inizio giornata\n");
        if(stop_received()){
            terminateSim(erog, operators, users, n_op, n_usr);
            freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
            error_close("Simulazione terminata anticipatamente 2");
        }
       
       if(nanosleep(&req, &rem) == -1)
           printf("Sleep direttore interrotta\n");
       
       printf("Fine giornata\n");
       
       suspendSim(erog, operators, users, n_op, n_usr);
       
       if(stop_received()){
           terminateSim(erog, operators, users, n_op, n_usr);
           freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
           error_close("Simulazione terminata anticipatamente 3");
       }
        
        op[0].sem_num = 4;
        op[0].sem_op = -1;
        op[0].sem_flg = 0;
        if(!sem_op(simSem, &op[0], 1)){
            terminateSim(erog, operators, users,  n_op, n_usr);
            freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
            error_close("Errore semOp attesa erogatore");
        }
        
        if(stop_received()){
            terminateSim(erog, operators, users, n_op, n_usr);
            freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
            error_close("Simulazione terminata anticipatamente 4");
        }
        
        if(!printStats(local_shm, n_seats, false)){
            terminateSim(erog, operators, users, n_op, n_usr);
            freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
            error_close("Errore stampa sats");
        }
        if(!dumpStats(local_shm, stats_path, false, first_time, n_seats)){
            perror("Impossibile stampare statistiche su file\n");
        }
        first_time = false;

        if(!resetSem(simSem, n_op, n_usr, local_shm->shm)){
           terminateSim(erog, operators, users, n_op, n_usr);
           freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
           error_close("sem reset");
        }
        clearTickets(local_shm);
        
        if(!signalManage(&oldmask)){
            terminateSim(erog, operators, users, n_op, n_usr);
            freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
            error_close("Errore aggiunta maschera 2");
            
        }
        /* FIFO Management */
        if(ricevuto_SIGUSR1 == 1){
            //printf("Ricevuto SIGUSR1\n");
            if((new_users = fifoRead(fdFifo)) == -1){
                terminateSim(erog, operators, users, n_op, n_usr);
                freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
                error_close("Error reading from fifo");
            }
            pid_t* new_arr = realloc(users, (n_usr + new_users) * sizeof(pid_t));
            if(!new_arr){
                terminateSim(erog, operators, users, n_op, n_usr);
                freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
                error_close("realloc error for new users");
            }
            users = new_arr;
            
            op[0].sem_num = 1;
            op[0].sem_op = new_users;
            op[0].sem_flg = 0;
            if(!sem_op(simSem, op, 1)){
                terminateSim(erog, operators, users,  n_op, n_usr);
                freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
                error_close("Errore semOp add users");
            }
            
            
            for(int i = n_usr; i < n_usr + new_users; i++){
                users[i] = fork();
                if(users[i] == -1){
                    perror("Son creation");
                    terminateSim(erog, operators, users, n_op, n_usr);
                    freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
                    exit(1);
                }
                if(users[i] == 0){
                    init_user(ID, toQu, comQu, simSem, dir_path, n_request, P_SERV_MIN, P_SERV_MAX, n_nano_secs);
                    kill(getppid(), SIGUSR2);
                    _exit(1);
                }
                ID++;
            }
            n_usr+=new_users;
            ricevuto_SIGUSR1 = 0;
            
            sprintf(buffer, "%d", getpid());
            if(!write_fifo(*fdFifo, buffer)){
                perror("Errore scrittura sulla FIFO");
                terminateSim(erog, operators, users, n_op, n_usr);
                freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
                exit(1);
            }
        }

        if(ricevuto_SIGUSR2 == 1){
            terminateSim(erog, operators, users, n_op, n_usr);
            freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
            error_close("Errore interno");
        }

        if(!signalManage(&mask)){
            terminateSim(erog, operators, users, n_op, n_usr);
            freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
            error_close("Errore aggiunta maschera");
            
        }
        
        if(local_shm->shm->statistics.current_day == sim_duration){
            continue_sim = false;
            timout = true;
        }else{
            service_choose(service_arr, local_shm->offsets, n_seats);
            if(!service_apply(service_arr, local_shm->shm, n_seats)){
                terminateSim(erog, operators, users, n_op, n_usr);
                freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
                error_close("Errore aggiornamento servizi");
            }
        }

        if(local_shm->shm->statistics.non_served_users_day >= explode_threshold){
            continue_sim = false;
            explode = true;
        }
    }
    
    if(!resetSem(simSem, n_op, n_usr, local_shm->shm)){
        terminateSim(erog, operators, users, n_op, n_usr);
        freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
        error_close("sem reset");
    }
    
    endSim(erog, operators, users, n_op, n_usr);
    
    op[0].sem_num = 4;
    op[0].sem_op = -1;
    op[0].sem_flg = 0;
    if(!sem_op(simSem, &op[0], 1)){
        terminateSim(erog, operators, users,  n_op, n_usr);
        freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
        error_close("Errore semOp attesa finale Erogatore");
    }

    if(!dumpStats(local_shm, stats_path, true, false, n_seats)){
        perror("Errore stampa statistiche finali");
    }
    char* status = "ending error";
    if(timout)
        status = "timout";
    if(explode)
        status = "explode";
    
    printStatus(stats_path, status);
    free(stats_path);
    freeIPC(local_shm->shm, fifo_path,shmid, simSem, memSem, toQu, comQu);
    return 0;
}
