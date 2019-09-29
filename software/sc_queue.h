#ifndef SC_QUEUE_H
#define SC_QUEUE_H

#define BUFFER_SIZE 1024

typedef struct inputstate {
    double timestamp;
    double target_position;
    unsigned int target_fader;
} inputstate;


typedef struct statequeue {
    unsigned int tail, head, size;
    inputstate buffer[BUFFER_SIZE];
} statequeue;


char fifoRead(statequeue *queue, inputstate *state);
 
char fifoWrite(statequeue *queue, inputstate *val, char overwriteold);
double fcubic_interpolate(double y0, double y1, double y2, double y3, double mu);
char InterpolateQueue(statequeue *queue, double *timestamp, double *val);
void fifoInit(statequeue *queue, unsigned int size);
#endif