#include "sc_queue.h"

char fifoRead(statequeue *queue, inputstate *state) {
   if (queue->head == queue->tail) return 0;
   queue->tail = (queue->tail + 1) % BUFFER_SIZE;
   *state = queue->buffer[queue->tail];
   //printf ("POP : %u\n",state->timestamp);
   return 1;
}
 
char fifoWrite(statequeue *queue, inputstate *val) {
   if ((queue->head + 1) % BUFFER_SIZE == queue->tail) return 0;
   queue->head = (queue->head + 1) % BUFFER_SIZE;
   queue->buffer[queue->head] = *val;
   	printf ("PUSH :%d %u\n",queue->head, (*val).timestamp);
   return 1;
}