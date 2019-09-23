#ifndef SC_QUEUE_H
#define SC_QUEUE_H


#define BUFFER_SIZE 20000

typedef struct inputstate {
    double timestamp;
    double target_position;
    unsigned int target_fader;
} inputstate;


typedef struct statequeue {
    unsigned int tail, head;
    inputstate buffer[BUFFER_SIZE];
} statequeue;


char fifoRead(statequeue *queue, inputstate *state);
void fifoInit(statequeue *queue);
char fifoWrite(statequeue *queue, inputstate *val);
char fifoPeek(statequeue *queue, unsigned int aheadby, inputstate *state);

#endif