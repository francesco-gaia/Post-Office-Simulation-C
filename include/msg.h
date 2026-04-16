/**
 * @file msg.h
 * @brief Definition of the message buffer structure for IPC.
 *
 * This file defines the structure used for message queue communication
 * between processes in the simulation.
 *
 */

#ifndef msg_h
#define msg_h

/**
 * @brief Size of the message text.
 */
#define MSG_SIZE 21

/**
 * @brief Structure for message queue communication.
 *
 * This structure is used to send and receive messages via the system V message queue.
 */
struct msgbuf {
    long mtype;           /**< Message type */
    char mtext[MSG_SIZE]; /**< Message content. */
}; 

#endif /* msg_h */
