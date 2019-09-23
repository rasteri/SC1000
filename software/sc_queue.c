#include <stdio.h>
#include <string.h>
#include "sc_queue.h"
#include "player.h"

// Peek at the queue without altering its contents.
// Aheadby = 0 for the queue tail, 1 for one after that, don't use any other aheadby values
char fifoPeek(statequeue *queue, unsigned int aheadby, inputstate *state)
{
   if (queue->head == queue->tail)
      return 0;
   unsigned int newtail = (queue->tail + aheadby) % BUFFER_SIZE;
   if (newtail == queue->head)
      return 0;

   memcpy(state, &queue->buffer[(newtail+1)  % BUFFER_SIZE], sizeof(inputstate));
   //*state = queue->buffer[newtail];

   return 1;
}

char fifoRead(statequeue *queue, inputstate *state)
{
   if (queue->head == queue->tail)
      return 0;
   queue->tail = (queue->tail + 1) % BUFFER_SIZE;
   //*state = queue->buffer[queue->tail];
   //printf ("POP : %u\n",state->timestamp);
   return 1;
}

char fifoWrite(statequeue *queue, inputstate *val)
{
   // If queue is empty, delete the last one from the end
   if ((queue->head + 1) % BUFFER_SIZE == queue->tail){
      printf ("OVERFLOWING :%d %d\n",queue->head, queue->tail);
      queue->tail = (queue->tail + 1) % BUFFER_SIZE;
   }
   queue->head = (queue->head + 1) % BUFFER_SIZE;
   queue->buffer[queue->head] = *val;
   //printf ("PUSH :%d %d\n",queue->head, queue->tail);
   return 1;
}

void fifoInit(statequeue *queue){
   queue->head = 0;
   queue->tail = 0;
}

double lastVal = 0;




