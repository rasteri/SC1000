#ifndef SC_QUEUE_H
#define SC_QUEUE_H

#define BUFFER_SIZE 256

typedef struct inputstate {
    long long unsigned int timestamp;
    double target_position;
    unsigned int target_fader;
} inputstate;


typedef struct statequeue {
    unsigned int tail, head;
    inputstate buffer[BUFFER_SIZE];
} statequeue;


char fifoRead(statequeue *queue, inputstate *state);
 
char fifoWrite(statequeue *queue, inputstate *val);
#endif