/**
 *@file ufficio.h
 *
 *@brief This file is used to define and interact with sportelli and tickets.
 *
 *This library contains the definitions of:
 *-struct used to define a sportello
 *-struct used to implement a record of sportelli
 *-struct used to define a ticket
 *-struct used to implement a FIFO queue of tickets
 *-struct used to correctly implement data to be saved in shared memory
 *-functions to interact with the aforementioned structures
 *
 *@see ufficio.c
 */
#ifndef ufficio_h
#define ufficio_h
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>

#if defined(__linux__)
// Linux usually does NOT define semun, so we define it ourselves
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};
#endif

/**
 * @struct ServiceStats
 * @brief Structure for statistics of a service type.
 */
typedef struct ServiceStats{
    int service_type; ///< Service type
    int total_services_provided; ///< Total number of services (and thus users) provided for this type
    int total_services_not_provided; ///< Total number of services not provided for this type
    int daily_served_user; ///< Total number of users served in the current day
    float daily_avg_users_served; ///< Average number of users served per day (thus average number of services provided per day)
    float daily_avg_users_not_served; ///< Average number of users not served per day (thus average number of services not provided per day)
    float daily_wait_time; ///< Total daily wait time for this service type
    float daily_wait_service; ///< Total daily service time for this service type
    float avg_wait_time; ///< Average wait time for this service type in the simulation
    float daily_avg_wait_time; ///< Average wait time for this service type in the current day
    float avg_service_time; ///< Average service time for this service type in the simulation
    float daily_avg_service_time; ///< Average service time for this service type in the current day
} ServiceStats;

/**
 * @struct Stats
 * @brief Structure for general simulation statistics.
 */
typedef struct{
    int total_users_served;    ///< Total number of users served in the simulation
    int total_erog_services; ///< Total number of services provided in the simulation
    int daily_served_user; ///< Total number of users served in the current day
    float daily_avg_users_served; ///< Average number of users served per day
    int total_services_provided; ///< Total number of services provided in the day
    int total_services_not_provided; ///< Total number of services not provided in the simulation
    int non_served_users_day; ///< Number of users not served in the current day
    float daily_avg_services_provided; ///< Average number of services provided per day
    float daily_avg_services_not_provided; ///< Average number of services not provided per day
    float avg_user_wait_time; ///< Average user wait time in the simulation
    float daily_avg_user_wait_time; ///< Average user wait time in the day
    float avg_service_time; ///< Average service time in the simulation
    float daily_avg_service_time; ///< Average service time in the day
    int active_operators_daily; ///< Number of active operators during the day
    int active_operators_total; ///< Number of active operators during the simulation
    int daily_pause; ///< Number of pauses taken in the current day
    float daily_avg_pauses; ///< Average number of pauses taken in the day
    int total_pauses; ///< Total number of pauses taken in the simulation
    size_t offset_operator_sportelli_ratio; ///< offset for Ratio between available operators and existing counters for each day. array with N_DAYS * N_SPORTELLI positions.. positions [0] -> [6] belong to the first day, positions [7] -> [13] to the second and so on.
    int n_sportelli; ///< Number of counters
    int current_day; ///< current day, initialized to 1, incremented by the erogatore at the end of the day
    int total_days; ///< total number of days of the simulation
    size_t offset_service_stats; ///< offset for structure containing statistics for each service type. Array of ServiceStats.
} Stats;

/**
 * @struct Ticket
 * @brief Structure representing a ticket.
 */
typedef struct{
    size_t ticket_id;       ///< Progressive ticket number, 0 if free
    int service_type;    ///< Requested service type
    pid_t user_pid;      ///< PID of the user who requested the ticket, 0 if invalid
    struct timespec orario_inserimento; ///< Ticket insertion time in the queue. used to calculate user wait time.
    int next_ticket; ///< index of next valid ticket
    int prev; ///< index of previous ticket
}Ticket;

/**
 * @struct ticketQueue
 * @brief Structure representing the ticket queue.
 */
typedef struct{
    size_t offset_lista_ticket; ///< offset for array containing tickets
    int first; ///< index of the first element of the queue
    int last; ///< index of the last element of the queue
    int next_free; ///< index of the first available slot
    int n_ticket; ///< number of tickets present in the queue
    int max_ticket; ///< maximum number of tickets present
    int semaforo; ///< initialized to 0, indicates the number of tickets in the queue
}ticketQueue;

/**
 * @struct Sportello
 * @brief Structure representing a counter.
 */
typedef struct{
    size_t id_s;            ///< Unique identifier of the counter. also indicates the position in the counters array (Ex: 0)
    int service_type;       ///< Associated service type
    pid_t operator_pid;     ///< PID of the operator occupying it (0 if free)
    int occupied_daily;     ///< number of operators who have occupied the counter in the day
}Sportello;

/**
 * @struct Sportelli
 * @brief Structure managing the set of counters.
 */
typedef struct{
    int n_sportelli; ///< number of counters present
    int semaforo; ///< set of n-sportelli semaphores, used to determine the number of sportello avaible based on service type
    size_t offset_sportelli; ///< offset to access the counters array
}Sportelli;

/**
 * @struct SharedMemory
 * @brief Main structure of shared memory.
 */
typedef struct{
    Sportelli lista_sportelli; ///< Struct containing available counters
    ticketQueue coda_ticket; ///< Struct containing the ticket queue
    Stats statistics; ///< Struct containing simulation statistics
}SharedMemory;

/**
 * @struct SHMoffset
 * @brief Structure to manage pointers to arrays in shared memory.
 */
typedef struct{
    Sportello* sportelli; ///< pointer to sportelli array
    Ticket* lista_ticket; ///< pointer to ticket array
    float* operator_sportelli_ratio; ///< pointer to array containing the operator/sportelli ratio.
    ServiceStats* service_stats; ///< pointer to array containing the statistics of each service
}SHMoffset;

/**
 * @struct ManageSHM
 * @brief Structure to manage access to shared memory.
 */
typedef struct{
    SharedMemory* shm; ///< Pointer to the main shared memory structure
    SHMoffset* offsets; ///< Pointer to the offsets structure
}ManageSHM;

/*All these functions must be called after reserving the relative semaphore*/

/**
 * @brief Inserts a ticket into the queue.
 * 
 * @param local_shm Pointer to the shared memory management structure.
 * @param service_type Requested service type.
 * @param user_pid User PID.
 * @param orario_inserimento Ticket insertion time.
 *
 * @return The ticket code if the operation is successful, 0 for error.
 */
size_t insertTicket(ManageSHM* local_shm, int service_type, pid_t user_pid, struct timespec orario_inserimento);

/**
 * @brief Serves a ticket from the queue.
 * 
 * Searches if there is a ticket in the queue with the requested service_type.
 * 
 * @param local_shm Pointer to the shared memory management structure.
 * @param service_type Requested service type.
 * @param user_pid Pointer where to save the User PID (output).
 * @param insert_time Pointer where to save the insertion time (output).
 *
 * @return The ticket ID if found and removed, 0 if it does not exist.
 */
size_t serveTicket(ManageSHM* local_shm, int service_type, pid_t* user_pid, struct timespec* insert_time);

/**
 * @brief Reserves a counter for an operator.
 * 
 * Searches if a counter is available that can offer the indicated service.
 * 
 * @param local_shm Pointer to the shared memory management structure.
 * @param pid Operator PID.
 * @param service_type Offered service type.
 * @param mem_sem_id Shared memory semaphore ID.
 *
 * @return The ID of the reserved counter, 0 in case of error.
 */
size_t reserveSportello(ManageSHM* local_shm, pid_t pid, int service_type, int mem_sem_id);

/**
 * @brief Frees an occupied counter.
 * 
 * @param local_shm Pointer to the shared memory management structure.
 * @param id_s ID of the counter to free.
 * @param pid PID of the operator who is freeing it.
 *
 * @return true If the operation is successful.
 * @return false If the operation fails.
 */
bool freeSportello(ManageSHM* local_shm, size_t id_s, pid_t pid);

/**
 * @brief Attaches shared memory and initializes pointers.
 * 
 * @param shmid Shared memory ID.
 * @param offsets Pointer to the offsets structure to initialize.
 * @return SharedMemory* Pointer to the attached shared memory.
 */
SharedMemory* attachSHM(int shmid, SHMoffset* offsets);

#endif
