/**
 * @file utente.c
 *
 * @brief Implementation of the User process.
 *
 * This file contains the implementation of the User process, which simulates a client
 * requesting services in the simulated office environment. The user selects a set of
 * service requests, sends them to the system via message queues, and interacts with
 * operators through synchronized communication mechanisms. It responds to specific
 * signals to control execution and gracefully handle simulation termination.
 *
 * This process does not interact with shared memory directly.
 *
 * Function implementations are provided in `utente_op.c`.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/sem.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "../include/msg.h"
#include "../include/utente_op.h"

/**
 * @brief Main function of the User process.
 *
 * This function initializes the user process, parses command-line arguments,
 * sets up signal handling, and executes the main simulation loop.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 *
 * Arguments expected:
 * - argv[1]: User ID.
 * - argv[2]: Message queue ID for sending requests (toQu).
 * - argv[3]: Message queue ID for communication with operators (comQu).
 * - argv[4]: Semaphore ID for simulation synchronization (simSem).
 * - argv[5]: Number of service requests to make (n_req).
 * - argv[6]: Minimum service time (p_serv_min).
 * - argv[7]: Maximum service time (p_serv_max).
 * - argv[8]: Time to wait in nanoseconds (n_nano_secs).
 *
 * @return 0 on success, 1 on error.
 */
int main(int argc, char* argv[]){
    if(argc < 9){
        perror("Errore: parametri errati utente");
        kill(getppid(), SIGUSR2);
        exit(1);
    }
    int ID = atoi(argv[1]);
    //printf("Utente creato ID:%d pid:%d\n", ID, getpid());
    int toQu = atoi(argv[2]);
    int comQu = atoi(argv[3]);
    int simSem = atoi(argv[4]);
    int n_req = atoi(argv[5]);
    int p_serv_min = atoi(argv[6]);
    int p_serv_max = atoi(argv[7]);
    int n_nano_secs = atol(argv[8]);
    int *req_arr;
    int size;
    
    handler_init();
    
    unsigned int seed = (unsigned int)(
        time(NULL) ^
        getpid() ^
        (getppid() << 16) ^
        clock() ^
        (uintptr_t)&seed
        ^ ID
    );
    srand(seed);
    
    bool continue_sim = true;
    struct sembuf op [2];
    int i = 0;
    bool stop;
    long pid = getpid();
    long op_pid;
    int ticket;
    sigset_t mask, oldmask;
    
    sigemptyset(&mask);
    sigemptyset(&oldmask);
    sigaddset(&mask, SIGUSR2);

    while(continue_sim){
        
        if(ricevuto_SIGUSR1 != 1){
            op[0].sem_num = 1;
            op[0].sem_op = -1;
            op[0].sem_flg = 0;
            op[1].sem_num = 2;
            op[1].sem_op = 0;
            op[1].sem_flg = 0;
            
            if (semop(simSem, &op[0], 1) == -1) {
                if (errno == EINTR) {
                    continue;
                }else
                    error_close("user error in semop 1");
            }
            if (semop(simSem, &op[1], 1) == -1) {
                if (errno == EINTR) {
                    continue;
                }else
                    error_close("user error in semop 2");
            }
            
            //printf("Utente inizia giornata\n");
            stop = false;
            
            if(ricevuto_SIGUSR2 == 1){
                stop = true;
                ricevuto_SIGUSR2 = 0;
            }
            
            if(!intention_choose(p_serv_min, p_serv_max) && !stop){
                req_arr = selectRequests(n_req, &size);
                if(!req_arr){
                    error_close("Errore malloc scelta richieste user\n");
                }
                /*for(int i = 0; i < size; i++){
                    printf("Utente ID:%D sceglie %d\n", ID, req_arr[i]);
                }*/
                
                wait_visit(n_nano_secs, ID);
                if(ricevuto_SIGUSR2 == 1){
                    stop = true;
                    ricevuto_SIGUSR2 = 0;
                }
                
                while(i < size && !stop){

                    if(ricevuto_SIGUSR2 == 1){
                        stop = true;
                        ricevuto_SIGUSR2 = 0;
                    }else{
                        if(!msg_send(toQu, (long)req_arr[i], pid)){
                            if(!checkForSIG2())
                                error_close("user error on msgsend");
                            else{
                                stop = true;
                                ricevuto_SIGUSR2 = 0;
                            }
                            
                        }else{
                            //printf("Utente %d ha richiesto un ticket service %d\n", ID, req_arr[i]);
                            ticket = msg_read(toQu, pid);
                            //printf("Utente %d ha ricevuto ticket %d\n", ID, ticket);
                            while(ticket == -2){
                                if(!msg_send(toQu, (long)req_arr[i], pid)){
                                    if(!checkForSIG2())
                                        error_close("user error on msgsend");
                                    else{
                                        stop = true;
                                        ricevuto_SIGUSR2 = 0;
                                    }
                                }
                                ticket = msg_read(toQu, pid);
                            }
                            if(ticket != -1){ // Available
                                if((op_pid = msg_read(comQu, pid)) == -2){
                                    if(checkForSIG2()){
                                        ricevuto_SIGUSR2 = 0;
                                        stop = true;
                                    }else
                                        error_close("user error on msgread");
                                }else{
                                    if (sigprocmask(SIG_BLOCK, &mask, &oldmask) == -1) {
                                        error_close("sigprocmask block");
                                    }
                                    if(!msg_send(comQu, op_pid, (long)1)){
                                        if(checkForSIG2()){ stop = true; ricevuto_SIGUSR2 = 0;}
                                        else error_close("user error on msgsend");
                                    }
                                    
                                    if(msg_read(comQu, pid) == -2){
                                        if(checkForSIG2()){ stop = true; ricevuto_SIGUSR2 = 0;}
                                        else error_close("user error on msg_read");
                                    }
                                    if (sigprocmask(SIG_SETMASK, &oldmask, NULL) == -1) {
                                        error_close("sigprocmask unblock");
                                    }
                                }
                            }/*else
                                printf("Ticket non disponibile, continuo %d\n", ID);// If not available continue*/
                        }
                        i++;
                    }
                }
            
                free(req_arr);
                i = 0;
                
            }else{
                if(!stop)
                    printf("Utente ID:%d pid:%d non si presenta oggi", ID, getpid());
            }
        }else
            continue_sim = false;
    }
    return 0;
}
