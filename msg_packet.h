#ifndef MSG_PACKET_H
#define MSG_PACKET_H

/* Command definitions */
#define CMD_HELLO      0
#define CMD_HELLO_ACK  1
#define CMD_REQUEST    2
#define CMD_REPLY      3
#define CMD_IN_CS      4

/* Message structure for distributed mutex operations */
typedef struct {
    unsigned short command;       /* Command type: HELLO, HELLO_ACK, REQUEST, REPLY, IN_CS */
    unsigned short sequence;      /* Sequence number for duplicate avoidance */
    unsigned int tie_break;       /* Process's PID (or other unique identifier) for tiebreaking */
    unsigned short host_id;       /* Artificial host ID for convenience */
    unsigned short vec_clock[5];  /* Vector clock for up to 5 hosts */
    unsigned int acct_balance;    /* Account balance for balance update messages */
} msg_packet;

#endif /* MSG_PACKET_H */
