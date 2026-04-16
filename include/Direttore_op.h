/**
 * @file Direttore_op.h
 * @brief Header file for the Director process operations.
 *
 * This file contains the function prototypes and data structures used by the Director process
 * to manage the simulation, including configuration reading, IPC initialization, and process control.
 *
 * 
 */

#ifndef Direttore_op_h
#define Direttore_op_h
#include <stdio.h>
#include <sys/sem.h>
#include <stdbool.h>
#include "ufficio.h"

/**
 * @brief Flag set when SIGUSR1 is received
 *
 * This variable is set to 1 when the process receives a SIGUSR1 signal.
 * The SIGUSR1 signal is used to notify the Director process to allow the creation of new user procesess.
 * It is declared volatile sig_atomic_t to ensure safe access in signal handlers, preventing compiler optimizations that could lead to inconsistent values.
 */
extern volatile sig_atomic_t ricevuto_SIGUSR1;

/**
 * @brief Flag set when SIGUSR2 is received.
 *
 * This variable is set to 1 when the process receives a SIGUSR2 signal.
 * The SIGUSR2 signal is used to notify the Director process that a critical error has occurred.
 * The signal is sent by child processes and causes the simulation to end prematurely in a controlled manner.
 * It is declared volatile sig_atomic_t to ensure safe access in signal handlers.
 */
extern volatile sig_atomic_t ricevuto_SIGUSR2;

/**
 * @brief Flag set when SIGINT is received.
 *
 * This variable is set to 1 when the process receives a SIGINT signal.
 * The signal causes the simulation to end in a controlled manner.
 * It is declared volatile sig_atomic_t to ensure safe access in signal handlers.
 */
extern volatile sig_atomic_t ricevuto_SIGINT;

/**
 * @brief Structure to hold general configuration settings.
 */
typedef struct general_conf{
    int NOF_USERS;        /**< Number of users to simulate. */
    int P_SERV_MIN;       /**< Minimum service time percentage. */
    int P_SERV_MAX;       /**< Maximum service time percentage. */
    int N_REQUEST;        /**< Number of requests per user. */
    int NOF_WORKERS_SEAT; /**< Number of service desks. */
    int NOF_WORKERS;      /**< Number of operator processes. */
    int NOF_PAUSE;        /**< Number of allowed pauses. */
    int MAX_TICKETS;      /**< Maximum number of tickets in the queue. */
} general;

/**
 * @brief Structure to hold timeout configuration settings.
 */
typedef struct timout_conf{
    int SIM_DURATION;   /**< Duration of the simulation in days. */
    long N_NANO_SECS;   /**< Nanoseconds per simulation unit. */
    int WORKING_HOURS;  /**< Working hours per day. */
} timout;

/**
 * @brief Structure to hold explode configuration settings.
 */
typedef struct explode_conf{
    int EXPLODE_THRESHOLD; /**< Threshold for unserved users before "explosion". */
} explode;

/**
 * @brief Prints an error message and terminates the process.
 * @param msg The error message to print.
 */
void error_close(const char* msg);

/**
 * @brief Signal handler for the Director process.
 * @param signum The signal number.
 * @param info Signal info structure.
 * @param context Context pointer.
 */
void handler(int signum, siginfo_t *info, void *context);

/**
 * @brief Signal handler that ignores the signal.
 * @param signum The signal number.
 */
void ignore_hanler(int signum);

/**
 * @brief Initializes the signal handlers for the Director process.
 */
void handler_init();

/**
 * @brief Gets the directory path of the executable.
 * @param path The path of the executable (argv[0]).
 * @return The directory path string.
 */
char* get_directory_path(char* path);

/**
 * @brief Constructs the full path to a configuration file.
 * @param str The base directory path.
 * @param name The name of the configuration file.
 * @return The full path string.
 */
char* getConfigPath(char* str, char* name);

/**
 * @brief Reads the user configuration from the file.
 * @param infile The file pointer to the configuration file.
 * @param g_conf Pointer to the general configuration structure to populate.
 * @return true if successful, false otherwise.
 */
bool readUsers(FILE* infile, general* g_conf);

/**
 * @brief Reads the service desk configuration from the file.
 * @param infile The file pointer to the configuration file.
 * @param g_conf Pointer to the general configuration structure to populate.
 * @return true if successful, false otherwise.
 */
bool readSportelli(FILE* infile, general* g_conf);

/**
 * @brief Reads the operator configuration from the file.
 * @param infile The file pointer to the configuration file.
 * @param g_conf Pointer to the general configuration structure to populate.
 * @return true if successful, false otherwise.
 */
bool readOp(FILE* infile, general* g_conf);

/**
 * @brief Reads the ticket configuration from the file.
 * @param infile The file pointer to the configuration file.
 * @param g_conf Pointer to the general configuration structure to populate.
 * @return true if successful, false otherwise.
 */
bool readTickets(FILE* infile, general* g_conf);

/**
 * @brief Reads the entire general configuration from the file.
 * @param infile The file pointer to the configuration file.
 * @return Pointer to the allocated general configuration structure, or NULL on failure.
 */
general* readGCONF(FILE* infile);

/**
 * @brief Reads the general configuration from the default path.
 * @param dir_path The directory path containing the configuration file.
 * @return Pointer to the allocated general configuration structure, or NULL on failure.
 */
general* readGeneralConf(char* dir_path);

/**
 * @brief Reads the timeout configuration from the file.
 * @param infile The file pointer to the configuration file.
 * @return Pointer to the allocated timeout configuration structure, or NULL on failure.
 */
timout* readTCONF(FILE* infile);

/**
 * @brief Reads the timeout configuration from the default path.
 * @param dir_path The directory path containing the configuration file.
 * @return Pointer to the allocated timeout configuration structure, or NULL on failure.
 */
timout* readTimoutConf(char* dir_path);

/**
 * @brief Reads the explode configuration from the file.
 * @param infile The file pointer to the configuration file.
 * @return Pointer to the allocated explode configuration structure, or NULL on failure.
 */
explode* readECONF(FILE* infile);

/**
 * @brief Reads the explode configuration from the default path.
 * @param dir_path The directory path containing the configuration file.
 * @return Pointer to the allocated explode configuration structure, or NULL on failure.
 */
explode* readExplodeConf(char* dir_path);

/**
 * @brief Starts the shared memory segment.
 * @param shmid The shared memory ID.
 * @param conf General configuration structure.
 * @param sim_duration Simulation duration.
 * @param offsets Pointer to the shared memory offsets structure.
 * @return Pointer to the attached shared memory segment.
 */
SharedMemory* startSHM(int shmid, general conf, int sim_duration, SHMoffset* offsets);
/**
 * @brief Initializes the shared memory management structure.
 * @param shmid Pointer to store the shared memory ID.
 * @param conf General configuration structure.
 * @param sim_duration Simulation duration.
 * @return Pointer to the managed shared memory structure.
 */
ManageSHM* initSHM(int* shmid, general conf, int sim_duration);
/**
 * @brief Initializes the service desks in shared memory.
 * @param local_shm Pointer to the shared memory management structure.
 * @param conf Pointer to the general configuration structure.
 */
void initSportelli(ManageSHM* local_shm, general* conf);
/**
 * @brief Initializes the ticket queue in shared memory.
 * @param local_shm Pointer to the shared memory management structure.
 * @param MAX_TICKETS Maximum number of tickets.
 */
void initQueue(ManageSHM* local_shm, int MAX_TICKETS);
/**
 * @brief Initializes the statistics in shared memory.
 * @param local_shm Pointer to the shared memory management structure.
 * @param NOF_WORKERS_SEAT Number of service desks.
 * @param days Number of simulation days.
 * @param SERVICES_NUMBER Number of services.
 */
void initStats(ManageSHM* local_shm, int NOF_WORKERS_SEAT, int days, int SERVICES_NUMBER);
/**
 * @brief Initializes the FIFO for IPC.
 * @param fd Pointer to store the file descriptor.
 * @param dir_path Directory path for the FIFO.
 * @return Path to the created FIFO.
 */
char* init_fifo(int** fd, char* dir_path);
/**
 * @brief Writes data to the FIFO.
 * @param fd File descriptor of the FIFO.
 * @param data Data string to write.
 * @return true if successful, false otherwise.
 */
bool write_fifo(int fd, char* data);
/**
 * @brief Initializes the FIFO (alternative function).
 * @param fd Pointer to store the file descriptor.
 * @param dir_path Directory path for the FIFO.
 * @return Path to the created FIFO.
 */
char* initFifo(int** fd, char* dir_path);
/**
 * @brief Initializes the semaphores.
 * @param simSem Pointer to store the simulation semaphore ID.
 * @param memSem Pointer to store the memory semaphore ID.
 * @param n_op Number of operators.
 * @param n_usr Number of users.
 * @param shm Pointer to the shared memory structure.
 * @return true if successful, false otherwise.
 */
bool init_sem(int* simSem, int* memSem, int n_op, int n_usr, SharedMemory* shm);
/**
 * @brief Initializes the message queues.
 * @param toQu Pointer to store the 'to' queue ID.
 * @param comQu Pointer to store the 'communication' queue ID.
 * @return true if successful, false otherwise.
 */
bool init_msgqueue(int* toQu, int* comQu);

/**
 * @brief Chooses the services for the service desks.
 * @param arr Array to store the chosen services.
 * @param offsets Shared memory offsets.
 * @param N_SPORTELLI Number of service desks.
 */
void service_choose(size_t* arr, SHMoffset* offsets, int N_SPORTELLI);
/**
 * @brief Applies the chosen services to the shared memory.
 * @param arr Array of chosen services.
 * @param shm Pointer to the shared memory structure.
 * @param N_SPORTELLI Number of service desks.
 * @return true if successful, false otherwise.
 */
bool service_apply(size_t* arr, SharedMemory* shm, int N_SPORTELLI);
/**
 * @brief Writes the chosen services to the pipe.
 * @param fdWPipe Write file descriptor of the pipe.
 * @param arr Array of chosen services.
 * @param n_sportelli Number of service desks.
 * @return true if successful, false otherwise.
 */
bool pipe_write(int fdWPipe, size_t* arr, int n_sportelli);
/**
 * @brief Starts the ticket dispenser process.
 * @param shmid Shared memory ID.
 * @param toQu Message queue ID.
 * @param simSem Simulation semaphore ID.
 * @param memSem Memory semaphore ID.
 * @param fdPipe Pipe file descriptors.
 * @param dir_path Directory path.
 * @param erog Pointer to store the PID of the dispenser process.
 * @return true if successful, false otherwise.
 */
bool erog_start(int shmid, int toQu, int simSem, int memSem, int fdPipe[2], char* dir_path, pid_t* erog);
/**
 * @brief Initializes the ticket dispenser process.
 * @param shmid Shared memory ID.
 * @param toQu Message queue ID.
 * @param simSem Simulation semaphore ID.
 * @param memSem Memory semaphore ID.
 * @param fdRPipe Read file descriptor of the pipe.
 * @param dir_path Directory path.
 * @return true if successful, false otherwise.
 */
bool init_erog(int shmid, int toQu, int simSem, int memSem, int fdRPipe, char* dir_path);
/**
 * @brief Starts the operator processes.
 * @param ID Pointer to the operator ID.
 * @param shmid Shared memory ID.
 * @param simSem Simulation semaphore ID.
 * @param memSem Memory semaphore ID.
 * @param comQu Communication queue ID.
 * @param NOF_PAUSE Number of allowed pauses.
 * @param dir_path Directory path.
 * @param operators Array to store operator PIDs.
 * @param NOF_WORKERS Number of operators.
 * @param n_nano_secs Nanoseconds per simulation unit.
 * @return The ID of the started operator, or -1 on failure.
 */
int op_start(int* ID, int shmid, int simSem, int memSem, int comQu, int NOF_PAUSE, char* dir_path, pid_t* operators, int NOF_WORKERS, long n_nano_secs);
/**
 * @brief Initializes an operator process.
 * @param ID Operator ID.
 * @param shmid Shared memory ID.
 * @param simSem Simulation semaphore ID.
 * @param memSem Memory semaphore ID.
 * @param comQu Communication queue ID.
 * @param MAX_PAUSE Maximum number of pauses.
 * @param service Service ID.
 * @param dir_path Directory path.
 * @param n_nano_secs Nanoseconds per simulation unit.
 * @return true if successful, false otherwise.
 */
bool init_op(int ID, int shmid, int simSem, int memSem, int comQu, int MAX_PAUSE, int service, char* dir_path, long n_nano_secs);
/**
 * @brief Starts the user processes.
 * @param ID Pointer to the user ID.
 * @param users Array to store user PIDs.
 * @param toQu Message queue ID (to users).
 * @param comQu Message queue ID (communication).
 * @param simSem Simulation semaphore ID.
 * @param dir_path Directory path.
 * @param NOF_USERS Number of users.
 * @param N_REQUEST Number of requests per user.
 * @param P_SERV_MIN Minimum service time percentage.
 * @param P_SERV_MAX Maximum service time percentage.
 * @param n_nano_secs Nanoseconds per simulation unit.
 * @return The ID of the started user, or -1 on failure.
 */
int user_start(int* ID, pid_t* users, int toQu, int comQu, int simSem, char* dir_path, int NOF_USERS, int N_REQUEST, int P_SERV_MIN, int P_SERV_MAX, long n_nano_secs);
/**
 * @brief Initializes a user process.
 * @param ID User ID.
 * @param toQU Message queue ID (to users).
 * @param comQu Message queue ID (communication).
 * @param simSem Simulation semaphore ID.
 * @param dir_path Directory path.
 * @param N_REQUEST Number of requests.
 * @param P_SERV_MIN Minimum service time percentage.
 * @param P_SERV_MAX Maximum service time percentage.
 * @param n_nano_secs Nanoseconds per simulation unit.
 * @return true if successful, false otherwise.
 */
bool init_user(int ID, int toQU, int comQu, int simSem, char* dir_path, int N_REQUEST, int P_SERV_MIN, int P_SERV_MAX, long n_nano_secs);

/**
 * @brief Terminates the simulation and all child processes when a critical error occurs.
 * @param erog PID of the ticket dispenser.
 * @param operators Array of operator PIDs.
 * @param users Array of user PIDs.
 * @param n_op Number of operators.
 * @param n_usr Number of users.
 */
void terminateSim(pid_t erog, pid_t* operators, pid_t* users, int n_op, int n_usr);
/**
 * @brief Manages signals for the process.
 * @param mask Signal mask.
 * @return true if successful, false otherwise.
 */
bool signalManage(sigset_t* mask);
/**
 * @brief Performs a semaphore operation.
 * @param semId Semaphore ID.
 * @param op Pointer to the sembuf structure.
 * @param n_operations Number of operations.
 * @return true if successful, false otherwise.
 */
bool sem_op(int semId, struct sembuf* op, int n_operations);
/**
 * @brief Checks if a stop signal has been received.
 * @return true if stop signal received, false otherwise.
 */
bool stop_received();
/**
 * @brief Resets the semaphores.
 * @param simSem Simulation semaphore ID.
 * @param n_op Number of operators.
 * @param n_usr Number of users.
 * @param shm Pointer to the shared memory structure.
 * @return true if successful, false otherwise.
 */
bool resetSem(int simSem, int n_op, int n_usr, SharedMemory* shm);
/**
 * @brief Suspends the simulation.
 * @param erog PID of the ticket dispenser.
 * @param operators Array of operator PIDs.
 * @param users Array of user PIDs.
 * @param n_op Number of operators.
 * @param n_usr Number of users.
 */
void suspendSim(pid_t erog, pid_t* operators, pid_t* users, int n_op, int n_usr);
/**
 * @brief Ends the simulation.
 * @param erog PID of the ticket dispenser.
 * @param operators Array of operator PIDs.
 * @param users Array of user PIDs.
 * @param n_op Number of operators.
 * @param n_usr Number of users.
 */
void endSim(pid_t erog, pid_t* operators, pid_t* users, int n_op, int n_usr);

/**
 * @brief Prints the current statistics.
 * @param local_shm Pointer to the shared memory management structure.
 * @param NOF_WORKERS_SEAT Number of service desks.
 * @param endsim Boolean flag indicating if the simulation is ending.
 * @return true if successful, false otherwise.
 */
bool printStats(ManageSHM* local_shm, int NOF_WORKERS_SEAT, bool endsim);

/**
 * @brief Reads a PID from the FIFO.
 * @param fdFifo Pointer to the file descriptor of the FIFO.
 * @return The PID read from the FIFO, or -1 on error.
 */
int fifoRead(int *fdFifo);

/**
 * @brief Dumps the statistics to a CSV file.
 * @param local_shm Pointer to the shared memory management structure.
 * @param stats_path Path to the statistics file.
 * @param endsim Boolean flag indicating if the simulation is ending.
 * @param first_time Boolean flag indicating if this is the first dump (creates header).
 * @param NOF_WORKERS_SEAT Number of service desks.
 * @return true if successful, false otherwise.
 */
bool dumpStats(ManageSHM* local_shm, char* stats_path, bool endsim, bool first_time, int NOF_WORKERS_SEAT);

/**
 * @brief Prints the final status of the simulation to the stats file.
 * @param stats_path Path to the statistics file.
 * @param status The status string to print (e.g., "timeout", "explode").
 */
void printStatus(char* stats_path, char* status);

/**
 * @brief Frees all IPC resources.
 * @param shm Pointer to the shared memory structure.
 * @param fifo_path Path to the FIFO file.
 * @param shmid Shared memory ID.
 * @param simSem Simulation semaphore ID.
 * @param memSem Memory semaphore ID.
 * @param toQu Message queue ID (to users).
 * @param comQu Message queue ID (communication).
 */
void freeIPC(SharedMemory* shm, char* fifo_path, int shmid, int simSem, int memSem, int toQu, int comQu);

/**
 * @brief Clears all tickets from the queue.
 * @param local_shm Pointer to the shared memory management structure.
 */
void clearTickets(ManageSHM* local_shm);

#endif /* Direttore_op_h */
