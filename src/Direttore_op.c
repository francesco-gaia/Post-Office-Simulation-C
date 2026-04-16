/**
 * @file Direttore_op.c
 *
 * @brief Implementation of functions for the Director process.
 *
 * This file contains the implementations of auxiliary functions used exclusively
 * by the Director process to manage shared resources, spawn and monitor child processes,
 * and handle signals within the simulation. These functions are not accessible by
 * other simulation processes.
 *
 * @see Direttore_op.h
 */
#include "../include/Direttore_op.h"
#include "../include/ufficio.h"
#include <libgen.h> //dirname()
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h> //open()
/**
 * @brief Flag set when SIGUSR1 is received
 *
 * This variable is set to 1 when the process receives a SIGUSR1 signal.
 * The SIGUSR1 signal is used to notify the Director process to allow the creation of new user processes.
 * It is declared volatile sig_atomic_t to ensure safe access in signal handlers, preventing compiler optimizations that could lead to inconsistent values.
 */
volatile sig_atomic_t ricevuto_SIGUSR1 = 0;
/**
 * @brief Flag set when SIGUSR2 is received.
 *
 * This variable is set to 1 when the process receives a SIGUSR2 signal.
 * The SIGUSR2 signal is used to notify the Director process that a critical error has occurred.
 * The signal is sent by child processes and causes the simulation to end prematurely in a controlled manner.
 * It is declared volatile sig_atomic_t to ensure safe access in signal handlers.
 */
volatile sig_atomic_t ricevuto_SIGUSR2 = 0;
/**
 * @brief Flag set when SIGINT is received.
 *
 * This variable is set to 1 when the process receives a SIGINT signal.
 * The signal causes the simulation to end in a controlled manner.
 * It is declared volatile sig_atomic_t to ensure safe access in signal handlers.
 */
volatile sig_atomic_t ricevuto_SIGINT = 0;
/**
 * @brief Utility function used to prematurely terminate the process.
 *
 * This function prints the critical error that occurred and terminates the process.
 *
 * @param msg Pointer to the string message
 * @throws calls the _exit() function, terminating the process
 */
void error_close(const char* msg){
    perror(msg);
    _exit(1);
}
/**
 * @brief Signal handler of the process
 *
 * This function implements the handler of the process.
 * When one signal arrives, the corresponding flag is set.
 *
 * @param signum signal number
 */
void handler(int signum, siginfo_t *info, void *context){
    (void)context; //for debug
    (void)info; //for debug
    //printf("Dir Segnale %d da PID %d\n", signum, info->si_pid);
    if(signum == SIGUSR1)
        ricevuto_SIGUSR1 = 1;
    if(signum == SIGUSR2){
        ricevuto_SIGUSR2 = 1;
    }
    if(signum == SIGINT){
        raise(SIGTERM);
        //_exit(1);
    }
}
/**
 * @brief Empty handler used to ignore non-used signals
 *
 * @param signum signal number
 */
void ignore_handler(int signum){
    (void)signum;
}
/**
 * @brief Sets simulation masks and handlers
 *
 * This function sets 2 different masks and handlers, to correctly catch and manage the signals.
 * The function `handler` is assigned to SIGUSR1, SIGUSR2 and SIGINT.
 * The function `ignore_handler` is assigned to SIGTERM, SIGTSTP, SIGHUP and SIGQUIT
 */
void handler_init(){
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO;
    struct sigaction s1;
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
    if(sigaction(SIGTERM, &s1, NULL) == -1){
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
    if(sigaction(SIGQUIT, &s1, NULL) == -1){
        perror("Errore nella sigaction per SIGQUIT");
        exit(EXIT_FAILURE);
    }
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
/**
 * @brief Returns the .conf file path
 *
 * This function returns the path where the given config file is stored.
 *
 * @param str the absolute path to the project directory structure
 * @param name the name of the config file
 * @return the absolute path to the config file
 */
char* getConfigPath(char* str, char* name){
    size_t len = strlen(str) + strlen("/conf/") + strlen(name) + 1;
    char* config_path = (char*)malloc(len);
    snprintf(config_path, len, "%s/conf/%s", str, name);
    return config_path;
}
/**
 * @brief Reads and stores the user process settings
 *
 * This function reads from the given config file the user settings,
 * and stores them in the corresponding struct.
 * Before ending it restores the reading point using fseek().
 *
 * @param infile Pointer to input file
 * @param g_conf Pointer to the struct containing the user information
 * @return true if succesfull, false otherwise
 * @see ufficio.h
 */
bool readUsers(FILE* infile, general* g_conf){
    bool ret = true;
    char line[256];
    char* token;
    char* value;
    int iter = 0;
    long last_read_pos = 0;
    while(fgets(line,sizeof(line),infile) && ret && iter<4){
        last_read_pos = ftell(infile);
        token = strtok(line, " ");
        token = strtok(token, "=");
        if((iter==0&&strcmp("NOF_USERS",token)==0)||(iter==1&&strcmp("P_SERV_MIN",token)==0)||(iter==2&&strcmp("P_SERV_MAX",token)==0)||(iter==3&&strcmp("N_REQUESTS",token)==0)){
            value = strtok(NULL, "=");
            if(value){
                switch(iter){
                    case 0:
                        g_conf->NOF_USERS = atoi(value);
                        break;
                    case 1:
                        g_conf->P_SERV_MIN = atoi(value);
                        break;
                    case 2:
                        g_conf->P_SERV_MAX = atoi(value);
                        break;
                    case 3:
                        g_conf->N_REQUEST = atoi(value);
                        break;
                }
            }else
                ret = false;
        }else
            ret = false;
            
        iter++;
    }
    if(ret)
        fseek(infile, last_read_pos, SEEK_SET);
    return ret;
}
/**
 * @brief Reads and stores the Sportelli settings.
 *
 * This function reads from the given config file the sportelli settings,
 * and stores them in the corresponding struct.
 * Before ending it restores the reading point using fseek().
 *
 * @param infile Pointer to input file
 * @param g_conf Pointer to the struct containing the sportelli information
 * @return true if succesfull, false otherwise
 * @see ufficio.h
 */
bool readSportelli(FILE* infile, general* g_conf){
    bool ret = true;
    char line[256];
    char* token;
    char* value;
    int iter = 0;
    long last_read_pos = 0;
    while(fgets(line,sizeof(line),infile) && ret && iter < 1){
        last_read_pos = ftell(infile);
        token = strtok(line, " ");
        token = strtok(token, "=");
        if(iter==0&&strcmp("NOF_WORKER_SEATS",token)==0){
            value = strtok(NULL, "=");
            if(value){
                g_conf->NOF_WORKERS_SEAT = atoi(value);
            }else
                ret = false;
        }else
            ret = false;
            
        iter++;
    }
    if(ret)
        fseek(infile, last_read_pos, SEEK_SET);
    return ret;
}
/**
 * @brief Reads and stores the Sportelli settings.
 *
 * This function reads from the given config file the operators settings,
 * and stores them in the corresponding struct.
 * Before ending it restores the reading point using fseek().
 *
 * @param infile Pointer to input file
 * @param g_conf Pointer to the struct containing the operator information
 * @return true if succesfull, false otherwise
 * @see ufficio.h
 */
bool readOp(FILE* infile, general* g_conf){
    bool ret = true;
    char line[256];
    char* token;
    char* value;
    int iter = 0;
    long last_read_pos = 0;
    while(fgets(line,sizeof(line),infile) && ret && iter<2){
        last_read_pos = ftell(infile);
        token = strtok(line, " ");
        token = strtok(token, "=");
        if((iter==0&&strcmp("NOF_WORKERS",token)==0)||(iter==1&&strcmp("NOF_PAUSE",token)==0)){
            value = strtok(NULL, "=");
            if(value){
                switch(iter){
                    case 0:
                        g_conf->NOF_WORKERS = atoi(value);
                        break;
                    case 1:
                        g_conf->NOF_PAUSE = atoi(value);
                        break;
                }
            }else
                ret = false;
        }else
            ret = false;
            
        iter++;
    }
    if(ret)
        fseek(infile, last_read_pos, SEEK_SET);
    return ret;
}
/**
 * @brief Reads and stores the ticket settings.
 *
 * This function reads from the given config file the ticket settings,
 * and stores them in the corresponding struct.
 * Before ending it restores the reading point using fseek().
 *
 * @param infile Pointer to input file
 * @param g_conf Pointer to the struct containing the ticket information
 * @return true if succesfull, false otherwise
 * @see ufficio.h
 */
bool readTickets(FILE* infile, general* g_conf){
    bool ret = true;
    char line[256];
    char* token;
    char* value;
    int iter = 0;
    long last_read_pos = 0;
    while(fgets(line,sizeof(line),infile) && ret && iter<1){
        last_read_pos = ftell(infile);
        token = strtok(line, " ");
        token = strtok(token, "=");
        if((iter==0&&strcmp("MAX_TICKETS",token)==0)){
            value = strtok(NULL, "=");
            if(value){
                switch(iter){
                    case 0:
                        g_conf->MAX_TICKETS = atoi(value);
                        break;
                }
            }else
                ret = false;
        }else
            ret = false;
            
        iter++;
    }
    if(ret)
        fseek(infile, last_read_pos, SEEK_SET);
    return ret;
}
/**
 * @brief Reads the `config_general.conf` file
 *
 * This function reads the different sections of the config file.
 * It calls a specific function for each section of the file.
 *
 * @param infile Pointer to the given file
 * @return pointer to new struct containing all information regarding the given file, NULL on error
 * @see readTickets, readOp, readSportelli, readUsers
 */
general* readGCONF(FILE* infile) {
    general* g_conf = malloc(sizeof(general));
    if (g_conf) {
        char line[256];
        int section = 0;
        bool check = true;
        while(fgets(line,sizeof(line),infile) && check){
            if(line[0] != '#' && line[0] != '\n'){ //evita commenti
                switch(section){
                    case 0:
                        check = readUsers(infile, g_conf);
                        break;
                    case 1:
                        check = readSportelli(infile, g_conf);
                        break;
                    case 2:
                        check = readOp(infile, g_conf);
                        break;
                    case 3:
                        check = readTickets(infile, g_conf);
                        break;
                }
                section++;
            }
        }
        if(!check){
            free(g_conf);
            g_conf = NULL;
        }
    }

    return g_conf;
}
/**
 * @brief Wrapper function used to read the `config_general.conf` file.
 *
 * This wrapper function is used to correctly open the config file.
 * It then calls the `readGCONF` function.
 *
 * @param dir_path absolute path to the project directory structure
 * @return pointer to new struct containing all information regarding the given file, NULL on error
 */
general* readGeneralConf(char* dir_path){
    
    char* config_path = getConfigPath(dir_path, "config_general.conf");
    general* conf = NULL;
    FILE* infile = fopen(config_path, "r");
    if(infile == NULL){
        perror("Errore apertura file conf");
    }else{
        conf = readGCONF(infile);
        fclose(infile);
        free(config_path);
    }
    return conf;
}
/**
 * @brief Reads the `config_timout.conf` file.
 *
 * This function reads the given config file, and stores
 * its information in the corresponding struct.
 *
 * @param infile Pointer to the given file
 * @return pointer to new struct containing all information regarding the given file, NULL on error
 */
timout* readTCONF(FILE* infile){
    timout* t_conf = malloc(sizeof(timout));
    if(t_conf){
        char line[256];
        int iter = 0;
        bool check = true;
        char* token;
        char* value;
        while(fgets(line,sizeof(line),infile) && check && iter < 3){
            if(line[0] != '#' && line[0] != '\n'){
                token = strtok(line, " ");
                token = strtok(token, "=");
                if((iter==0&&strcmp("SIM_DURATION",token)==0)||(iter==1&&strcmp("N_NANO_SECS",token)==0)||(iter==2&&strcmp("WORKING_HOURS",token)==0)){
                    value = strtok(NULL, token);
                    if(value){
                        switch (iter){
                        case 0:
                            t_conf->SIM_DURATION = atoi(value);
                            break;
                        case 1:
                            t_conf->N_NANO_SECS = atoi(value);
                            break;
                        case 2:
                            t_conf->WORKING_HOURS = atoi(value);
                            break;
                        }
                    }
                }else
                    check = false;
                iter++;
            }
        }
        if(!check){
            free(t_conf);
            t_conf = NULL;
        }
    }
    return t_conf;
}
/**
 * @brief Wrapper function used to read the `config_timout.conf` file.
 *
 * This wrapper function is used to correctly open the config file.
 * It then calls the `readTCONF` function.
 *
 * @param dir_path absolute path to the project directory structure
 * @return pointer to new struct containing all information regarding the given file, NULL on error
 */
timout* readTimoutConf(char* dir_path){
    
    char* config_path = getConfigPath(dir_path, "config_timout.conf");
    FILE* infile = fopen(config_path, "r");
    if(infile == NULL){
        perror("Errore apertura file conf timout");
        exit(1);
    }
    timout* t_conf = readTCONF(infile);
    fclose(infile);
    free(config_path);
    return t_conf;
}
/**
 * @brief Reads the `config_explode.conf` file.
 *
 * This function reads the given config file, and stores
 * its information in the corresponding struct.
 *
 * @param infile Pointer to the given file
 * @return pointer to new struct containing all information regarding the given file, NULL on error
 */
explode* readECONF(FILE* infile){
    explode* e_conf = malloc(sizeof(explode));
    if(e_conf){
        char line[256];
        int iter = 0;
        bool check = true;
        char* token;
        char* value;
        while(fgets(line,sizeof(line),infile) && check && iter < 1){
            if(line[0] != '#' && line[0] != '\n'){
                token = strtok(line, " ");
                token = strtok(token, "=");
                if((iter==0&&strcmp("EXPLODE_THRESHOLD",token)==0)){
                    value = strtok(NULL, token);
                    if(value){
                        switch (iter){
                        case 0:
                            e_conf->EXPLODE_THRESHOLD = atoi(value);
                            break;
                        }
                    }
                }else
                    check = false;
                iter++;
            }
        }
        if(!check){
            free(e_conf);
            e_conf = NULL;
        }
    }
    return e_conf;
}
/**
 * @brief Wrapper function used to read the `config_explode.conf` file.
 *
 * This wrapper function is used to correctly open the config file.
 * It then calls the `readECONF` function.
 *
 * @param dir_path absolute path to the project directory structure
 * @return pointer to new struct containing all information regarding the given file, NULL on error
 */
explode* readExplodeConf(char* dir_path){
    
    char* config_path = getConfigPath(dir_path, "config_explode.conf");
    FILE* infile = fopen(config_path, "r");
    if(infile == NULL){
        perror("Errore file e_conf");
        exit(1);
    }
    explode* e_conf = readECONF(infile);
    fclose(infile);
    free(config_path);
    return e_conf;
}
/**
 * @brief Initializes the shared memory.
 *
 * This function initializes the internal pointers and calulates the correct offset for each pointer.
 *
 * @param shmid shared memory id
 * @param conf struct containing simulation settings
 * @param sim_duration duration of the simulation expressed in days
 * @return pointer to SharedMemory struct if successful, NULL otherwise
 * @see ufficio.h
 */
SharedMemory* startSHM(int shmid, general conf, int sim_duration, SHMoffset* offsets){
    
    SharedMemory* shm = NULL;
    void* shared_mem = shmat(shmid, NULL, 0);
    if(shared_mem != (void*) -1){
        shm = (SharedMemory*) shared_mem;
        shm->lista_sportelli.offset_sportelli = sizeof(SharedMemory);
        offsets->sportelli = (Sportello*)((char*)shm+shm->lista_sportelli.offset_sportelli);
        shm->coda_ticket.offset_lista_ticket = shm->lista_sportelli.offset_sportelli + (conf.NOF_WORKERS_SEAT * sizeof(Sportello));
        offsets->lista_ticket = (Ticket*)((char*)shm+shm->coda_ticket.offset_lista_ticket);
        shm->statistics.offset_operator_sportelli_ratio = shm->coda_ticket.offset_lista_ticket + (conf.MAX_TICKETS * sizeof(Ticket));
        offsets->operator_sportelli_ratio = (float*)((char*)shm+shm->statistics.offset_operator_sportelli_ratio);
        shm->statistics.offset_service_stats = shm->statistics.offset_operator_sportelli_ratio + (sim_duration *conf.NOF_WORKERS_SEAT) * sizeof(float);
        offsets->service_stats = (ServiceStats*)((char*)shm+shm->statistics.offset_service_stats);
    }
    return shm;
}
/**
 * @brief Initializes the shared memory structure.
 *
 * This function allocates memory for the local shared memory management structure
 * and initializes the shared memory segment.
 *
 * @param shmid Pointer to the shared memory ID variable.
 * @param conf General configuration structure.
 * @param sim_duration Duration of the simulation.
 * @return Pointer to the initialized ManageSHM structure, or NULL on failure.
 */
ManageSHM* initSHM(int* shmid, general conf, int sim_duration){
    
    ManageSHM* local_shm = NULL;
    size_t mem_size = sizeof(SharedMemory) + (conf.NOF_WORKERS_SEAT * sizeof(Sportello)) + (conf.MAX_TICKETS * sizeof(Ticket)) + ((sim_duration * conf.NOF_WORKERS_SEAT) * sizeof(float)) + (sizeof(ServiceStats)*6);
    key_t memkey = ftok(__FILE__,'M');
    *shmid = shmget(memkey, mem_size, IPC_CREAT | 0666);
    if(*shmid != -1){
        local_shm = malloc(sizeof(ManageSHM));
        local_shm->offsets = malloc(sizeof(SHMoffset));
        local_shm->shm = startSHM(*shmid, conf, sim_duration, local_shm->offsets);
    }
    return local_shm;
}

/**
 * @brief Initializes the sportelli in shared memory.
 *
 * This function sets up the initial state for all sportelli, including their IDs
 * and default values.
 *
 * @param local_shm Pointer to the shared memory management structure.
 * @param conf Pointer to the general configuration structure.
 */
void initSportelli(ManageSHM* local_shm, general* conf){
    
    local_shm->shm->lista_sportelli.n_sportelli = conf->NOF_WORKERS_SEAT;
    local_shm->shm->lista_sportelli.semaforo = -1;
    for(int i=0; i<conf->NOF_WORKERS_SEAT; i++){
        local_shm->offsets->sportelli[i].id_s = i+1;
        local_shm->offsets->sportelli[i].service_type = -1;
        local_shm->offsets->sportelli[i].operator_pid = 0;
        local_shm->offsets->sportelli[i].occupied_daily = 0;
    }
}

/**
 * @brief Initializes the ticket queue in shared memory.
 *
 * This function sets up the ticket queue, resetting all tickets and queue pointers.
 *
 * @param local_shm Pointer to the shared memory management structure.
 * @param MAX_TICKETS Maximum number of tickets allowed.
 */
void initQueue(ManageSHM* local_shm, int MAX_TICKETS){
    
    SharedMemory* shm = local_shm->shm;
    SHMoffset* offsets = local_shm->offsets;
    shm->coda_ticket.max_ticket = MAX_TICKETS;
    shm->coda_ticket.semaforo = -1;
    shm->coda_ticket.first = -1;
    shm->coda_ticket.last = -1;
    shm->coda_ticket.n_ticket = 0;
    shm->coda_ticket.next_free = 0;
    for(int i = 0; i < MAX_TICKETS; i++){
        offsets->lista_ticket[i].ticket_id = 0;
        offsets->lista_ticket[i].service_type = 0;
        offsets->lista_ticket[i].user_pid = 0;
        offsets->lista_ticket[i].next_ticket = -1;
        offsets->lista_ticket[i].orario_inserimento.tv_sec = 0;
        offsets->lista_ticket[i].orario_inserimento.tv_nsec = 0;
    }
    
}
/**
 * @brief Initializes the portion of shared memory containing the statistics.
 *
 * This function initializes the portion of the shared memory that contains the statistics.
 *
 * @param shm pointer to the shared memory
 * @param NOF_WORKERS_SEAT number of sportelli
 * @param SERVICES_NUMBER number of services
 * @see ufficio.h
 */
void initStats(ManageSHM* local_shm, int NOF_WORKERS_SEAT, int days, int SERVICES_NUMBER){
    SharedMemory* shm = local_shm->shm;
    SHMoffset* offsets = local_shm->offsets;
    shm->statistics.total_users_served = 0;
    shm->statistics.daily_avg_users_served = 0;
    shm->statistics.total_services_provided = 0;
    shm->statistics.total_services_not_provided = 0;
    shm->statistics.daily_avg_services_provided = 0;
    shm->statistics.daily_avg_services_not_provided = 0;
    shm->statistics.avg_user_wait_time = 0;
    shm->statistics.daily_avg_user_wait_time = 0;
    shm->statistics.avg_service_time = 0;
    shm->statistics.daily_avg_service_time = 0;
    shm->statistics.active_operators_daily = 0;
    shm->statistics.active_operators_total = 0;
    shm->statistics.daily_avg_pauses = 0;
    shm->statistics.total_pauses = 0;
    shm->statistics.current_day = 0;
    shm->statistics.total_days = days;
    
    for(int i = 0; i < days + NOF_WORKERS_SEAT; i++){
        offsets->operator_sportelli_ratio[i] = 0;
    }
    for(int i = 0; i < SERVICES_NUMBER; i++){
        offsets->service_stats[i].service_type = i+1;
        offsets->service_stats[i].total_services_provided = 0;
        offsets->service_stats[i].total_services_not_provided = 0;
        offsets->service_stats[i].avg_wait_time = 0;
        offsets->service_stats[i].avg_service_time = 0;
        
    }
}
/**
 * @brief Creates and opens the FIFO file
 *
 * This function creates and opens the FIFO file.
 * The fifo is opened using the O_NONBLOCK specifier.
 *
 * @param fd pointer to variable storing the file descriptor
 * @param dir_path absolute path of the project directory stucture
 * @return pointer to string containing the path of the FIFO file, NULL otherwise
 * @note fd contains the file descriptor if the creation and/or opening is succesfull, -1 otherwise
 */
char* init_fifo(int** fd, char* dir_path){
    size_t len = strlen(dir_path) + strlen("/tmp/fifo") + 1;
    char* fifo_path = malloc(len);
    if(fifo_path){
        snprintf(fifo_path, len, "%s/tmp/fifo", dir_path);
        if (mkfifo(fifo_path, 0666) == -1) {
            if (errno != EEXIST) {
                perror("mkfifo failed");
                free(fifo_path);
                fifo_path = NULL;
            }else
                **fd = open(fifo_path, O_RDWR | O_NONBLOCK);
        }else
            **fd = open(fifo_path, O_RDWR | O_NONBLOCK);
    }else
        **fd = -1;
    return fifo_path;
}
/**
 * @brief Writes a string of data in the FIFO
 *
 * This function writes a string in the FIFO file
 *
 * @param fd file descriptor
 * @param data string to be written
 * @return true if succesfull, false otherwise
 */
bool write_fifo(int fd, char* data){
    bool ret = false;
    ssize_t res = write(fd, data, strlen(data)+1);
    if (res != -1) {
        ret = true;
    }
    return ret;
}
/**
 * @brief Manages the creation and inizilization of the FIFO file.
 *
 * The function calls the `init_fifo` function to create the FIFO.
 * It then writes the process pid in the FIFO.
 *
 * @param fd pointer to variable containing the file descriptor
 * @param dir_path pointer to string containing tha absolute path of the project directory structure
 * @return pointer to string containing the path of the FIFO file, NULL otherwise
 * @note fd contains the file descriptor if the creation and/or opening is succesfull, -1 otherwise
 */
char* initFifo(int** fd, char* dir_path){
    
    char* fifo_path = init_fifo(fd, dir_path);
    if(fifo_path && **fd != -1){
        char buffer[32];
        sprintf(buffer, "%d", getpid());
        if(!write_fifo(**fd, buffer)){
            printf("Errore scrittura sulla FIFO");
            free(fifo_path);
            fifo_path = NULL;
        }
    }else{
        if(fifo_path)
            free(fifo_path);
        fifo_path = NULL;
    }
    
    return fifo_path;
}
/**
 * @brief Creates and inizializes all semaphores used in the simulation.
 *
 * This function creates all the semapthores used in the simulation.
 * If creation is done succesfully, it inizializes the semaphores value.
 * The created semaphore are the following:
 * 1. simSem indicates a set of 4 semaphores, used to manage the simulation
 *      1.1 simSem[0] is initialized at N-OPERATORS + N-EROGATORI (N-OP + 1). it is used to be certain that all "workers" processes are ready.
 *      1.2 simSem[1] is initialized at N-USERS. It is used to be certain that all USERS processes are ready.
 *      1.3 simSem[2] is initialized at 1. It is used to start the simulation when the value is lowered at 0.
 *      1.4 simSem[3] is initialized at N-OPERATORS. It is used to notify the erogatore process of when all the operators processes have finishes updating the statistics.
 *      1.4 simSem[4] is inizialized at 1. It is used to synchronize the Direttore process and the erogatore process, so that the Direttore waits for the erogatore to have updated the statistics before printing them.
 * 2. memSem indicates a set of 3 semaphores, used to manage shared memory access
 *      2.1 memSem[0] is initialized at 1 and manages the access for the Sportelli memory section
 *      2.2 memSem[1] is initialized at 1 and manages the access for the ticket queue memory section
 *      2.3 memSem[2] is initialized at 1 and manages the access for the statistics memory section
 * 3. tSem indicates a set of 6 semaphores used to show if there are any avaible tickets per service
 *      3.1 tSem is initilized at 0 for each semaphore. When a new ticket is inserted the value is increased by 1 in the corresponding service semaphore. If the value is 0 it means that there are no avaible tickets.
 * 4. sSem indicates a set of 6 semaphores used to show if there are any free Sportelli that serves the corresponding service
 *      4.1 each individual semaphor is inizialized at the number of existing sportelli that serves the corresponding service. When a sportello is occupied, the value of the semaphore is decreased by 1. If the value is 0 it means that there are no avaible sportelli.
 *
 * @param simSem pointer to sempahore set ID
 * @param memSem pointer to semaphore set ID
 * @param n_op number of operator processes
 * @param n_usr number of user procesess
 * @param shm pointer to shared memory
 * @return true if succesfull, false otherwise
 *
 * @note the syntax sem[x] is used to simplify the explanation of the semaphore sets
 */
bool init_sem(int* simSem, int* memSem, int n_op, int n_usr, SharedMemory* shm){
    bool ret = true;
    
    key_t simSemKey = ftok(__FILE__,'S');
    *simSem = semget(simSemKey, 5, 0666 | IPC_CREAT);
                                             
    key_t memSemKey = ftok(__FILE__,'s');
    *memSem = semget(memSemKey, 3, 0666 | IPC_CREAT);
    
    key_t ticketSemKey = ftok(__FILE__,'T');
    int tSem = semget(ticketSemKey, 6, 0666 | IPC_CREAT);
    
    key_t sportelliSemKey = ftok(__FILE__, 'W');
    int sSem = semget(sportelliSemKey, 6, 0666 | IPC_CREAT);
    
    if(*simSem == -1 || *memSem == -1 || tSem == -1 || sSem == -1){
        ret = false;
        printf("Errore crazione set semafori");
    }else{
        union semun arg;
        arg.val = n_op+1;
        if (semctl(*simSem, 0, SETVAL, arg) == -1) {
            ret = false;
            printf("semctl 0 sim");
        }
        arg.val = n_usr;
        if (semctl(*simSem, 1, SETVAL, arg) == -1) {
            ret = false;
            printf("semctl 1 sim");
        }
        arg.val=1;
        if (semctl(*simSem, 2, SETVAL, arg) == -1) {
            ret = false;
            printf("semctl 2 sim");
        }
        arg.val = n_op;
        if (semctl(*simSem, 3, SETVAL, arg) == -1) {
            ret = false;
            printf("semctl 3 sim");
        }
        arg.val=0;
        if (semctl(*simSem, 4, SETVAL, arg) == -1) {
            ret = false;
            printf("semctl 2 sim");
        }
        arg.val=1;
        if (semctl(*memSem, 0, SETVAL, arg) == -1) {
            ret = false;
            printf("semctl 0 mem");
        }
        arg.val=1;
        if (semctl(*memSem, 1, SETVAL, arg) == -1) {
            ret = false;
            printf("semctl 1 mem");
        }
        arg.val=1;
        if (semctl(*memSem, 2, SETVAL, arg) == -1) {
            ret = false;
            printf("semctl 2 mem");
        }
        arg.val = 0;
        for(int i = 0; i < 6 && ret; i++){
            if (semctl(tSem, i, SETVAL, arg) == -1) {
                ret = false;
                printf("semctl tSem");
            }
            if (semctl(sSem, i, SETVAL, arg) == -1) {
                ret = false;
                printf("semctl sSem");
            }
        }
        shm->coda_ticket.semaforo = tSem;
        shm->lista_sportelli.semaforo = sSem;
    }
    return ret;
}
/**
 * @brief Crates the message queues
 *
 * This function creates the message queues used in the simulation.
 * There are 2 message queues:
 * 1. toQu is used as a communication tool between the erogatore process and the user procesess
 * 2. comQu is used as a communication tool between the operator procesess and the user procesess
 *
 * @param toQu pointer to erogatore-users queue ID
 * @param comQu pointer to operators-users queue ID
 * @return true if succesfull, false otherwise. the ID pointers are also updated
 */
bool init_msgqueue(int* toQu, int* comQu){
    bool ret = true;
    
    key_t toQuKey = ftok(__FILE__,'Q');
    *toQu = msgget(toQuKey, 0666 | IPC_CREAT);  // Totem-user message queue
    key_t comQuKey = ftok(__FILE__,'q');
    *comQu = msgget(comQuKey,0666 | IPC_CREAT); // Operator-user message queue
    if (*toQu == -1 || *comQu == -1) {
        printf("Errore creazione msgget");
        ret = false;
    }
    return ret;
}
/**
 * @brief Randomly choose services
 *
 * This function randomly chooses the Sportelli services, and stores them in a pre-allocated array.
 *
 * @param arr pointer to allocated array
 * @param shm pointer to shared memeory
 * @param N_SPORTELLI number of sportelli
 */
void service_choose(size_t* arr, SHMoffset* offsets, int N_SPORTELLI){
    
    size_t num;
    for (int i = 0; i < N_SPORTELLI; i++) {
        num = (rand() % 6) + 1;
        printf("Sportello %d scelto con service %zu\n",i,num);
        arr[i] = num;
        offsets->sportelli[i].service_type = num;
    }
}
/**
 * @brief Updates the value of each semaphore based on the number of sportelli assigned to each service.
 *
 *  This function takes an array of service types assigned to sportelli and updates the corresponding semaphores inside shared memory.
 *  Each service type is counted, and the value of the semaphore for that service is set to the number of sportelli that currently handle it.
 *
 * @param arr Ponter to an array of size  N_SPORTELLI containing service types (1–6) assigned to each sportello.
 * @param shm Pointer to the shared memory structure containing the semaphore set.
 * @param N_SPORTELLI Number of sportelli (size of the @p arr array).
 *
 * @note The function dynamically allocates a temporary array and frees it before returning.
 * @return true on success, false otherwise
 */
bool service_apply(size_t* arr, SharedMemory* shm, int N_SPORTELLI){
    bool ret = true;
    int* values = malloc(6 * sizeof(int));
    for(int i = 0; i < 6; i++){
        values[i] = 0;
    }
    for(int i = 0; i < N_SPORTELLI; i++){
        values[arr[i]-1] ++;
    }
    union semun arg;
    int sSem = shm->lista_sportelli.semaforo;
    for(int i = 0; i < 6; i++){
        arg.val = values[i];
        if (semctl(sSem, i, SETVAL, arg) == -1) {
            ret = false;
            printf("semctl ticket");
        }
    }
    free(values);
    return ret;
}
/**
 * @brief Writes an array of data in a given Pipe.
 *
 * The function writes an array of data in the pipe.
 * The data written in an integer array.
 *
 * @param fdWPipe writing file descriptor
 * @param arr pointer to allocated array where the services will be saved
 * @return true if succesfull, false otherwise
 */
bool pipe_write(int fdWPipe, size_t* arr, int n_sportelli){
    bool ret = true;
    if(write(fdWPipe, arr, n_sportelli * sizeof(size_t)) == -1){
        ret = false;
        printf("Errore write su pipe");
    }
    return ret;
}
/**
 * @brief Manages the creation of the erogatore process
 *
 * This function creates a new child using fork().
 * The child process closes the writing-end of the pipe and calls `init_erog`.
 * The father closes the reading-end of the pipe and continues the execution.
 *
 * @param shmid id of the shared memory
 * @param toQu id of the erogatore-users queue
 * @param simSem id of the semaphore set used to manage the simulation
 * @param memSem id of the semaphore set used to manage the shared memory access
 * @param fdPipe array containing the read-end of the pipe (fdPipe[0]) and the write-end of the pipe (fdPipe[1])
 * @param dir_path pointer to string containing tha absolute path of the project directory structure
 * @param erog pointer to variable that will store the pid of the child process
 *
 * @return true if succesfull, false otherwise
 *
 * @throws If the `init_erog` function fails, the child will signal the father using SIGUSR2
 *
 * @note If the `init_erog` function executes correctly, the child won't return to the current code.
 * @note If the `init_erog` function fails, after signaling the father, the child process will close itself.
 */
bool erog_start(int shmid, int toQu, int simSem, int memSem, int fdPipe[2], char* dir_path, pid_t* erog){
    
    bool ret = true;
    *erog = fork();
    if(*erog == 0){
        close(fdPipe[1]);
        if(!init_erog(shmid, toQu, simSem, memSem, fdPipe[0], dir_path)){
            perror("Errore avvio erogatore\n");
            kill(getppid(), SIGUSR2);
            exit(1);
        }
    }else if(*erog == -1){
        perror("Errore fork figlio erogatore\n");
        ret = false;
        }else
            close(fdPipe[0]);
    return ret;
}
/**
 * @brief Starts the erigatore process.
 *
 * This function correctly sets the argv[] array with the parameters and starts the erogatore process using the execve function.
 *
 * @param shmid id of the shared memory
 * @param toQu toQu id of the erogatore-users queue
 * @param simSem id of the semaphore set used to manage the simulation
 * @param memSem id of the semaphore set used to manage the shared memory access
 * @param fdPipe array containing the read-end of the pipe
 * @param dir_path pointer to string containing tha absolute path of the project directory structure
 *
 * @return false if an error occurs, in case of success the child executes the erogatore code and does not return to this code
 */
bool init_erog(int shmid, int toQu, int simSem, int memSem, int fdRPipe, char* dir_path){
    bool ret = true;
    char** argvE = (char**)malloc(sizeof(char*)*(6+1));
    char str[20];
    char* path = malloc(strlen(dir_path) + strlen("/bin/erogatore") + 1);
    strcpy(path, dir_path);
    strcat(path, "/bin/erogatore");
    argvE[0] = strdup(path);
    snprintf(str, sizeof(str), "%d", shmid);
    argvE[1] = strdup(str);
    snprintf(str, sizeof(str), "%d", toQu);
    argvE[2] = strdup(str);
    snprintf(str, sizeof(str), "%d", simSem);
    argvE[3] = strdup(str);
    snprintf(str, sizeof(str), "%d", memSem);
    argvE[4] = strdup(str);
    snprintf(str, sizeof(str), "%d", fdRPipe);
    argvE[5] = strdup(str);
    argvE[6] = NULL;
    execve(path, argvE, NULL);
    perror("Errore erogatore\n");
    ret = false;
    for(int i = 0; i < 6; i++){
        free(argvE[i]);
    }
    free(argvE);
    free(path);
    return ret;
}
/**
 * @brief manages the creation of the operators procesess.
 *
 * The function forks() N-OPERATORS times.
 * Each child calls the `init_op` function.
 *
 * @param ID uniqe ID to identify process in the simulation
 * @param shmid shared memory id
 * @param simSem id of the semaphore set used to manage the simulation
 * @param memSem id of the semaphore set used to manage the shared memory access
 * @param comQu id of the operators-users queue
 * @param NOF_PAUSE number of pauses that operators can take
 * @param dir_path pointer to string containing tha absolute path of the project directory structure
 * @param operators pointer to array that stores the pids of the child procesess
 * @param NOF_WORKERS number of workers to be created
 * @param n_nano_secs minutes in seconds
 * @return the number of operators created
 *
 * @throws if the `init_op` function fails, the child will signal the father using SIGUSR2
 *
 * @note if the `init_op` function is succesfull the child process won't return to the current code.
 * @note if the `init_op` function fails, after signaling the father, the child process will close itself.
 */
int op_start(int* ID, int shmid, int simSem, int memSem, int comQu, int NOF_PAUSE, char* dir_path, pid_t* operators, int NOF_WORKERS, long n_nano_secs) {
    
    int i = 0;
    int service;
    while (i < NOF_WORKERS) {
        service = (rand() % 6) + 1;
        operators[i] = fork();
        if (operators[i] == 0) {
            if (!init_op(*ID, shmid, simSem, memSem, comQu, NOF_PAUSE, service, dir_path, n_nano_secs)) {
                perror("Errore creazione figlio op");
                kill(getppid(), SIGUSR2);
                exit(1);
            }
        } else if (operators[i] == -1) {
            perror("Errore fork figlio operatore");
            break;
        } else {
            (*ID)++;
            
            i++;
        }
    }

    return i;
}
/**
 * @brief Starts the operator process.
 *
 * This function correctly sets the argv[] array with the parameters and starts the operator process using the execve function.
 *
 * @param ID unique ID to identify process in the simulation
 * @param shmid shared memory id
 * @param simSem id of the semaphore set used to manage the simulation
 * @param memSem id of the semaphore set used to manage the shared memory access
 * @param comQu id of the operator-users queue
 * @param MAX_PAUSE number of pauses that operators can take
 * @param service assigned service
 * @param dir_path pointer to string containing tha absolute path of the project directory structure
 *
 * @return false if an error occurs, if succesfull the child executes the operator code and does not return to this code
 */
bool init_op(int ID, int shmid, int simSem, int memSem, int comQu, int MAX_PAUSE, int service, char* dir_path, long n_nano_secs){
    bool ret = true;
    char** argvO = (char**)malloc(sizeof(char*)*(9+1));
    char str[20];
    char* path = malloc(strlen(dir_path) + strlen("/bin/operatore") + 1);
    strcpy(path, dir_path);
    strcat(path, "/bin/operatore");
    argvO[0] = strdup(path);
    snprintf(str, sizeof(str), "%d", ID);
    argvO[1] = strdup(str);
    snprintf(str, sizeof(str), "%d", shmid);
    argvO[2] = strdup(str);
    snprintf(str, sizeof(str), "%d", simSem);
    argvO[3] = strdup(str);
    snprintf(str, sizeof(str), "%d", memSem);
    argvO[4] = strdup(str);
    snprintf(str, sizeof(str), "%d", comQu);
    argvO[5] = strdup(str);
    snprintf(str, sizeof(str), "%d", MAX_PAUSE);
    argvO[6] = strdup(str);
    snprintf(str, sizeof(str), "%d", service);
    argvO[7] = strdup(str);
    snprintf(str, sizeof(str), "%lu", n_nano_secs);
    argvO[8] = strdup(str);
    argvO[9] = NULL;
    execve(path, argvO, NULL);
    perror("Errore operatore");
    ret = false;
    for(int i = 0; i < 8; i++){
        free(argvO[i]);
    }
    free(argvO);
    free(path);
    return ret;
}
/**
 * @brief Manages the creation of the user procesess.
 *
 * The function forks() N-USERS times.
 * Each child calls the `init_user` function.
 *
 * @param ID pointer to variable storing the uniqe ID to identify process in the simulation
 * @param users pointer to pre-allocated array that will store the child procesess ID's
 * @param toQu id of the erogatore-users queue
 * @param comQu id of the operators-users queue
 * @param simSem id of the semaphore set used to manage the simulation
 * @param dir_path pointer to string containing tha absolute path of the project directory structure
 * @param NOF_USERS number of user procesess to be created
 * @param N_REQUEST maximum number of ticket requests that the user can make at the postal office
 * @param P_SERV_MIN minimum probability that the process will show to the postal office
 * @param P_SERV_MAX maximum probability that the process will show to the postal office
 * @param n_nano_secs number of real seconds corrisponding to simulated seconds
 * @return the number of user created
 *
 * @throws if the `init_user` function fails, the child will signal the father using SIGUSR2
 *
 * @note if the `init_user` function is succesfull the child process won't return to the current code.
 * @note if the `init_user` function fails, after signaling the father, the child process will close itself.
 */
int user_start(int* ID, pid_t* users, int toQu, int comQu, int simSem, char* dir_path, int NOF_USERS, int N_REQUEST, int P_SERV_MIN, int P_SERV_MAX, long n_nano_secs) {

    int i = 0;
    while (i < NOF_USERS) {
        users[i] = fork();
        if (users[i] == 0) {
            if (!init_user(*ID, toQu, comQu, simSem, dir_path, N_REQUEST, P_SERV_MIN, P_SERV_MAX, n_nano_secs)) {
                perror("Errore creazione figlio utente");
                kill(getppid(), SIGUSR2);
                exit(1);
            }
        } else if (users[i] == -1) {
            perror("Errore fork figlio utente");
            break;
        } else {
            (*ID)++;
            i++;
        }
    }

    return i;
}
/**
 * @brief Starts the user process
 *
 * This function correctly sets the argv[] array with the parameters and starts the user process using the execve function.
 *
 * @param ID uniqe ID to identify process in the simulation
 * @param users pointer to pre-allocated array that will store the child procesess ID's
 * @param toQu id of the erogatore-users queue
 * @param comQu id of the operators-users queue
 * @param simSem id of the semaphore set used to manage the simulation
 * @param dir_path pointer to string containing tha absolute path of the project directory structure
 * @param N_REQUEST maximum number of ticket requests that the user can make at the postal office
 * @param P_SERV_MIN minimum probability that the process will show to the postal office
 * @param P_SERV_MAX maximum probability that the process will show to the postal office
 * @param n_nano_secs number of real seconds corrisponding to simulated seconds
 * @return false if an error occurs, if succesfull the child executes the user code and does not return to this code
 */
bool init_user(int ID, int toQU, int comQu, int simSem, char* dir_path, int N_REQUEST, int P_SERV_MIN, int P_SERV_MAX, long n_nano_secs){
    bool ret = true;
    char** argvU = (char**)malloc(sizeof(char*)*(9+1)); //9 argomenti + NULL finale
    char str[21];
    char* path = malloc(strlen(dir_path) + strlen("/bin/utente") + 1);
    strcpy(path, dir_path);
    strcat(path, "/bin/utente");
    argvU[0] = strdup(path);
    snprintf(str, sizeof(str), "%d", ID);
    argvU[1] = strdup(str);
    snprintf(str, sizeof(str), "%d", toQU);
    argvU[2] = strdup(str);
    snprintf(str, sizeof(str), "%d", comQu);
    argvU[3] = strdup(str);
    snprintf(str, sizeof(str), "%d", simSem);
    argvU[4] = strdup(str);
    snprintf(str, sizeof(str), "%d", N_REQUEST);
    argvU[5] = strdup(str);
    snprintf(str, sizeof(str), "%d", P_SERV_MIN);
    argvU[6] = strdup(str);
    snprintf(str, sizeof(str), "%d", P_SERV_MAX);
    argvU[7] = strdup(str);
    snprintf(str, sizeof(str), "%ld", n_nano_secs);
    argvU[8] = strdup(str);
    argvU[9] = NULL;
    execve(path, argvU, NULL);
    perror("Errore utente");
    ret = false;
    for(int i = 0; i < 9; i++){
        free(argvU[i]);
    }
    free(argvU);
    free(path);
    return ret;
}
/**
 * @brief Prematurely terminates the simulation.
 *
 * This function is called when any simulation or process error occurs.
 * The SIGTERM signal is used to close the active process.
 *
 * @param erog pid of the erogatore process
 * @param operators pointer to array containing the pids of the operator procesess
 * @param users pointer to array containing the pids of the user procesess
 * @param n_op number of operator procesess
 * @param n_usr number of user procesess
 */
void terminateSim(pid_t erog, pid_t* operators, pid_t* users, int n_op, int n_usr){
    kill(erog, SIGTERM);
    
    for(int i = 0; i < n_op; i++){
        kill(operators[i], SIGTERM);
    }
    
    for(int i = 0; i < n_usr; i++){
        kill(users[i], SIGTERM);
    }
}
/**
 * @brief Terminates the simulation.
 *
 * This function is called when the passed-days objective is been reached.
 * The signal SIGUSR1 signals the child procesess that the simulation has ended.
 *
 * @param erog pid of the erogatore process
 * @param operators pointer to array containing the pids of the operator procesess
 * @param users pointer to array containing the pids of the user procesess
 * @param n_op number of operator procesess
 * @param n_usr number of user procesess
 */
void endSim(pid_t erog, pid_t* operators, pid_t* users, int n_op, int n_usr){
    kill(erog, SIGUSR1);
    
    for(int i = 0; i < n_op; i++){
        kill(operators[i], SIGUSR1);
    }
    
    for(int i = 0; i < n_usr; i++){
        kill(users[i], SIGUSR1);
    }
}
/**
 * @brief Sets a new signal mask.
 *
 * This utility function sets a new active signal mask.
 *
 * @param mask pointer to the new mask
 * @return true if succesfull, false otherwise
 */
bool signalManage(sigset_t* mask){
    bool ret = true;
    if (sigprocmask(SIG_SETMASK, mask, NULL) == -1) {
        printf("sigprocmask");
        ret = false;
    }
    return ret;
}
/**
 * @brief Executes a given operation on the given semaphore
 *
 * This utility function calls semop to execute a given operation.
 *
 * @param semId id of the semaphore set
 * @param op pointer to sembuf struct containing the operation/s to be executed.
 * @param n_operation number of operations
 * @return true if succesfull, false otherwise
 */
bool sem_op(int semId, struct sembuf* op, int n_operations){
    bool ret = true;
    if (semop(semId, op, n_operations) == -1) {
        printf("semop wait for zero failed");
        ret = false;
    }
    return ret;
}
/**
 * @brief Checks if a stop signal has been received.
 *
 * This function checks if the SIGUSR2 or SIGINT signal have been received.
 *
 * @return true if one of the signals has been received, false otherwise.
 */
bool stop_received(){
    bool ret = false;
    if(ricevuto_SIGUSR2 == 1 || ricevuto_SIGINT == 1){
        //printf("SIGINT = %d SIGUSR2 = %d\n", ricevuto_SIGINT, ricevuto_SIGUSR2);
        ret = true;
        /*if(ricevuto_SIGUSR2)
            printf("Ricevuto SIGUSR2");
        if(ricevuto_SIGINT)
            printf("Ricevuto SIGINT");*/
    }
    return ret;
}
/**
 * @brief resets the ticketQueue.
 *
 * This function resets the ticket queue variables and the tickets information.
 *
 * @param shm pointer to shared memory
 */
void clearTickets(ManageSHM* local_shm){
    
    for(int i = 0; i < local_shm->shm->coda_ticket.max_ticket; i++){
        local_shm->offsets->lista_ticket[i].ticket_id = 0;
        local_shm->offsets->lista_ticket[i].service_type = 0;
        local_shm->offsets->lista_ticket[i].user_pid = 0;
        local_shm->offsets->lista_ticket[i].orario_inserimento.tv_sec = 0;
        local_shm->offsets->lista_ticket[i].orario_inserimento.tv_nsec = 0;
        local_shm->offsets->lista_ticket[i].next_ticket = -1;
    }
    local_shm->shm->coda_ticket.n_ticket = 0;
    local_shm->shm->coda_ticket.first = -1;
    local_shm->shm->coda_ticket.last = -1;
    local_shm->shm->coda_ticket.next_free = 0;
    for(int i = 0; i < local_shm->shm->lista_sportelli.n_sportelli; i++){
        local_shm->offsets->sportelli[i].occupied_daily = 0;
    }
}
/**
 * @brief Resets the IPCs of the simulation.
 *
 * This function resets all the semaphores used to manage the simulation flow.
 *
 * @param simSem id of the set of semaphores used to manage the simulation
 * @param n_op number of operators
 * @param n_usr number of users
 * @param shm pointer to shared memory
 * @return true if sucessfull, false otherwise
 */
bool resetSem(int simSem, int n_op, int n_usr, SharedMemory* shm){
    bool ret = true;
    union semun arg;
    arg.val=1;
    if (semctl(simSem, 2, SETVAL, arg) == -1) {
        ret = false;
        perror("semctl 2 sim");
    }
    arg.val = n_op+1;
    if (semctl(simSem, 0, SETVAL, arg) == -1) {
        ret = false;
        perror("semctl 0 sim");
    }
    arg.val = n_usr;
    if (semctl(simSem, 1, SETVAL, arg) == -1) {
        ret = false;
        perror("semctl 1 sim");
    }
    arg.val=n_op;
    if (semctl(simSem, 3, SETVAL, arg) == -1) {
        ret = false;
        perror("semctl 3 sim");
    }
    arg.val=0;
    if (semctl(simSem, 4, SETVAL, arg) == -1) {
        ret = false;
        perror("semctl 2 sim");
    }
    arg.val=0;
    for(int i = 0; i < 6; i++){
        if (semctl(shm->coda_ticket.semaforo, i, SETVAL, arg) == -1) {
            ret = false;
            perror("semctl 3 sim");
        }
    }
    
    return ret;
}
/**
 * @brief suspends the simulation
 *
 * This function sends the SIGUSR2 function to all child processes, signaling them that the current day has ended.
 *
 * @param erog pid of the erogatore process
 * @param operators pointer to array containing the pids of the operator procesess
 * @param users pointer to array containing the pids of the user procesess
 * @param n_op number of operator procesess
 * @param n_usr number of user procesess
 */
void suspendSim(pid_t erog, pid_t* operators, pid_t* users, int n_op, int n_usr){
    //printf("Inizio suspendSIm %d:\n", n_op);
    //printf("kill su erog %d\n", erog);
    kill(erog, SIGUSR2);
    for(int i = 0; i < n_op; i++){
        //printf("kill su op %d\n", operators[i]);
        kill(operators[i], SIGUSR2);
        kill(operators[i], SIGCONT);
    }
    for(int i = 0; i < n_usr; i++){
        //printf("kill su us %d\n", users[i]);
        kill(users[i], SIGUSR2);
    }
}
/**
 * @brief Prints the simulation statistics.
 *
 * @param shm pointer to shared memory
 * @param NOF_WORKERS_SEAT number of sportelli
 * @param endsim boolean variable showing if the maximum number of days has been reached
 * @return true if succesfull, false otherwise
 */

bool printStats(ManageSHM* local_shm, int NOF_WORKERS_SEAT, bool endsim) {
    SharedMemory* shm = local_shm->shm;
    SHMoffset* offsets = local_shm->offsets;
    Stats* stats = &shm->statistics;
    
    if(!endsim){
        int today_idx = stats->current_day;
        printf("\n--- RIEPILOGO GIORNO %d ---\n", today_idx);
        printf("Tempo medio di attesa degli utenti nella giornata odierna: %.2f\n", stats->daily_avg_user_wait_time);
        printf("Tempo medio di erogazione servizi nella giornata odierna: %.2f\n", stats->daily_avg_service_time);
    }

    printf("Utenti serviti nella simulazione: %d\n", stats->total_users_served);
    printf("Utenti serviti in media al giorno: %.2f\n", stats->daily_avg_users_served);
    printf("Numero di servizi erogati nella simulazione: %d\n", stats->total_services_provided);
    printf("Numero di servizi non erogati nella simulazione: %d\n", stats->total_services_not_provided);
    printf("Numero di servizi erogati in media al giorno: %.2f\n", stats->daily_avg_services_provided);
    printf("Numero di servizi non erogati in media al giorno: %.2f\n", stats->daily_avg_services_not_provided);
    printf("Tempo medio di attesa degli utenti nella simulazione: %.2f\n", stats->avg_user_wait_time);
    printf("Tempo medio di erogazione servizi nella simulazione: %.2f\n", stats->avg_service_time);
    
    printf("Staistiche relative a ogni servizio: \n\n");
    for(int i=0; i<6; i++){
        printf("Servizio: %d\n", offsets->service_stats[i].service_type);
        printf("Erogazioni totali: %d\n", offsets->service_stats[i].total_services_provided);
        printf("Erogazioni totali non avvenute: %d\n", offsets->service_stats[i].total_services_not_provided);
        printf("Numero di utenti serviti in media al giorno: %.2f\n", offsets->service_stats[i].daily_avg_users_served);
        printf("Numero di utenti non serviti in media al giorno: %.2f\n", offsets->service_stats[i].daily_avg_users_not_served);
        printf("Tempo medio di attesa erogazione nella simulazione: %.2f\n", offsets->service_stats[i].avg_wait_time);
        printf("Tempo medio di erogazione nella simulazione: %.2f\n", offsets->service_stats[i].avg_service_time);
        
        if(!endsim){
             printf("Tempo medio di attesa erogazione nella giornata odierna: %.2f\n", offsets->service_stats[i].daily_avg_wait_time);
             printf("Tempo medio di erogazione nella giornata odierna: %.2f\n", offsets->service_stats[i].daily_avg_service_time);
        }
    }
    printf("------------------------------------\n");
    if(!endsim){
        printf("Numero di operatori attivi nella giornata odierna: %d\n", stats->active_operators_daily);
        printf("Numero medio di pause effettuate nella giornata odierna: %.2f\n", stats->daily_avg_pauses);
    }
    printf("Numero di operatori attivi nella simulazione: %d\n", stats->active_operators_total);
    printf("Numero totale di pause effettuate nella simulazione: %d\n", stats->total_pauses);
    
    printf("Stampa rapporto operatori disponibili - sportelli esistenti, per ogni sportello per ogni giornata:\n\n");
    
    printf("Giorno\\Sportello");
    for(int i = 0; i < NOF_WORKERS_SEAT; i++){
        printf("\tS%d", i+1);
    }
    printf("\n");

    for(int j = 0; j < stats->current_day; j++){
        printf("G%d\t\t", j+1);
        for(int i = 0; i < NOF_WORKERS_SEAT; i++){
            int index = j * NOF_WORKERS_SEAT + i;
            printf("\t%.2f", offsets->operator_sportelli_ratio[index]);
        }
        printf("\n");
    }
    
    return true;
}
/**
 * @brief Reads from an active FIFO file.
 *
 * This functioon reads a positive integer value from the FIFO file.
 *
 * @param fdFifo pointer to fifo file descriptor
 * @return the read number if succesfull, -1 otherwise
 */
int fifoRead(int *fdFifo){
    int ret;
    char buf[32];
    ssize_t n = read(*fdFifo, buf, sizeof(buf));;
    if(n == -1){
        perror("Fifo read");
        ret = -1;
    }else
        ret = atoi(buf);
    return ret;
}

/**
 * @brief Prints the statistics in the given file in CSV format
 *
 * @param local_shm pointer to the shared memory management structure
 * @param stats_path pointer to string containing the path of the file
 * @param endsim boolean variable showing if it is the final simulation report
 * @param NOF_WORKERS_SEAT number of sportelli (desks)
 * @return true if successful, false otherwise
 */
bool dumpStats(ManageSHM* local_shm, char* stats_path, bool endsim, bool first_time, int NOF_WORKERS_SEAT) {
    bool ret = true;
    SharedMemory* shm = local_shm->shm;
    SHMoffset* offsets = local_shm->offsets;
    Stats* stats = &shm->statistics;
    
    FILE* f_out;
    if(first_time)
        f_out = fopen(stats_path, "w");
    else
        f_out = fopen(stats_path, "a");
        
    if (f_out == NULL) {
        ret = false;
    } else {
        
        if (!endsim) {
            fprintf(f_out, "\n--- RIEPILOGO GIORNO %d ---\n", shm->statistics.current_day);
            fprintf(f_out, "Tempo medio di attesa degli utenti nella giornata odierna,%.2f\n", shm->statistics.daily_avg_user_wait_time);
            fprintf(f_out, "Tempo medio di erogazione servizi nella giornata odierna,%.2f\n", shm->statistics.daily_avg_service_time);
            fprintf(f_out, "Utenti serviti nella simulazione,%d\n", shm->statistics.total_users_served);
            fprintf(f_out, "Utenti serviti in media al giorno,%.2f\n", shm->statistics.daily_avg_users_served);
            fprintf(f_out, "Numero di servizi erogati nella simulazione,%d\n", shm->statistics.total_services_provided);
            fprintf(f_out, "Numero di servizi non erogati nella simulazione,%d\n", shm->statistics.total_services_not_provided);
            fprintf(f_out, "Numero di servizi erogati in media al giorno,%.2f\n", shm->statistics.daily_avg_services_provided);
            fprintf(f_out, "Numero di servizi non erogati in media al giorno,%.2f\n", shm->statistics.daily_avg_services_not_provided);
            fprintf(f_out, "Tempo medio di attesa degli utenti nella simulazione,%.2f\n", shm->statistics.avg_user_wait_time);
            fprintf(f_out, "Tempo medio di erogazione servizi nella simulazione,%.2f\n", shm->statistics.avg_service_time);
        } else {
            fprintf(f_out, "\n--- STATISTICHE FINALI ---\n");
            fprintf(f_out, "Utenti serviti nella simulazione,%d\n", shm->statistics.total_users_served);
            fprintf(f_out, "Numero di servizi erogati nella simulazione,%d\n", shm->statistics.total_services_provided);
            fprintf(f_out, "Numero di servizi non erogati nella simulazione,%d\n", shm->statistics.total_services_not_provided);
            fprintf(f_out, "Tempo medio di attesa degli utenti nella simulazione,%.2f\n", shm->statistics.avg_user_wait_time);
            fprintf(f_out, "Tempo medio di erogazione servizi nella simulazione,%.2f\n", shm->statistics.avg_service_time);
        }

        fprintf(f_out, "\nStatistiche relative a ogni servizio:\n");
        
        fprintf(f_out, "Servizio,Erogazioni totali,Erogazioni totali non avvenute,Utenti serviti media giorno,Utenti non serviti media giorno,Tempo medio attesa sim,Tempo medio erogazione sim");
        if (!endsim) {
            fprintf(f_out, ",Tempo medio attesa oggi,Tempo medio erogazione oggi");
        }
        fprintf(f_out, "\n");

        for (int i = 0; i < 6; i++) {
             fprintf(f_out, "%d,%d,%d,%.2f,%.2f,%.2f,%.2f",
                offsets->service_stats[i].service_type,
                offsets->service_stats[i].total_services_provided,
                offsets->service_stats[i].total_services_not_provided,
                offsets->service_stats[i].daily_avg_users_served,
                offsets->service_stats[i].daily_avg_users_not_served,
                offsets->service_stats[i].avg_wait_time,
                offsets->service_stats[i].avg_service_time
            );

            if (!endsim) {
                 fprintf(f_out, ",%.2f,%.2f",
                        offsets->service_stats[i].daily_avg_wait_time,
                        offsets->service_stats[i].daily_avg_service_time
                );
            }
            fprintf(f_out, "\n");
        }

        fprintf(f_out, "------------------------------------\n");

        if (!endsim) {
            fprintf(f_out, "Numero di operatori attivi nella giornata odierna,%d\n", shm->statistics.active_operators_daily);
            fprintf(f_out, "Numero medio di pause effettuate nella giornata odierna,%.2f\n", shm->statistics.daily_avg_pauses);
        }

        fprintf(f_out, "Numero di operatori attivi nella simulazione,%d\n", shm->statistics.active_operators_total);
        fprintf(f_out, "Numero totale di pause effettuate nella simulazione,%d\n", shm->statistics.total_pauses);

        fprintf(f_out, "\nStampa rapporto operatori disponibili - sportelli esistenti:\n");
        
        fprintf(f_out, "Giorno\\Sportello");
        for (int k = 0; k < NOF_WORKERS_SEAT; k++) {
            fprintf(f_out, ",S%d", k + 1);
        }
        fprintf(f_out, "\n");

        for (int j = 0; j < shm->statistics.current_day; j++) {
            fprintf(f_out, "G%d", j + 1); // Day label
            
            for (int k = 0; k < NOF_WORKERS_SEAT; k++) {
                int index = j * NOF_WORKERS_SEAT + k;
                
                fprintf(f_out, ",%.2f", offsets->operator_sportelli_ratio[index]);
            }
            fprintf(f_out, "\n");
        }

        fclose(f_out);
    }

    return ret;
}
/**
 * @brief Prints the termination status on video on on the CSV file.
 *
 * @param stats_path pointer to string containing the path of the file
 * @param status pointer to string containing the termination status
 */
void printStatus(char* stats_path, char* status){
    printf("\n%s\n", status);
    FILE* f_out = fopen(stats_path, "a");
    if(!f_out)
        perror("Error on opening stats file\n");
    else{
        fprintf(f_out, "Ending result");
        fprintf(f_out, ",%s", status);
        fclose(f_out);
    }
}
/**
 * @brief Destorys the simulation's IPCS.
 *
 * This function is called after the simulation has ended.
 *
 * @param shm pointer to shared memory
 * @param fifo_path pointer to string containing the path of teh fifo file
 * @param shmid id of the shared memory
 * @param simSem id of the set of semaphores used to manage the simulation flow
 * @param memSem id of the set of semaphores used to manage shared memory access
 * @param toQu id of the eorgatore-users queue
 * @param comQu id of the operators-users queue
 */
void freeIPC(SharedMemory* shm, char* fifo_path, int shmid, int simSem, int memSem, int toQu, int comQu){
    
    if(fifo_path){
        if (unlink(fifo_path) == -1) {
            perror("Errore nel rimuovere il FIFO");
        }
        free(fifo_path);
    }
    int sem = shm->coda_ticket.semaforo;
    if(sem != -1){
        if (semctl(sem, 0, IPC_RMID) == -1)
            perror("Errore rimozione semaforo ticket");
    }
    sem = shm->lista_sportelli.semaforo;
    if(sem != -1){
        if (semctl(sem, 0, IPC_RMID) == -1)
            perror("Errore rimozione semaforo sportelli");
    }
    if(shmid != -1){
        if (shmctl(shmid, IPC_RMID, NULL) == -1)
            perror("shmctl IPC_RMID failed");
    }
    if(simSem != -1){
        if (semctl(simSem, 0, IPC_RMID) == -1)
            perror("Errore rimozione semaforo simulazione");
    }
    if (memSem != -1){
        if (semctl(memSem, 0, IPC_RMID) == -1)
            perror("Errore rimozione semaforo accesso memoria");
    }
    if(toQu != -1){
        if (msgctl(toQu, IPC_RMID, NULL) == -1) {
            perror("Errore msgctl");
        }
    }
    if(comQu != -1){
        if (msgctl(comQu, IPC_RMID, NULL) == -1) {
            perror("Errore msgctl");
        }
    }
}
