#include <stdio.h>
#include <string.h>
#include "sc_queue.h"

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
   *state = queue->buffer[queue->tail];
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
double lastVal = 0;
char InterpolateQueue(statequeue *queue, double *timestamp, double *val)
{
   inputstate firstReading, secondReading;
   while (1) // will end when returning value
   {
      if (!fifoPeek(queue, 0, &firstReading))
         return 0;
      if (!fifoPeek(queue, 1, &secondReading))
         return 0;

      // If the timestamp requested is before the first time in the buffer, move us forward
      if (*timestamp < firstReading.timestamp){
         *timestamp = firstReading.timestamp;
      }

      // Sanity check, make sure the timestamp requested is between firstTime and secondTime
      if (*timestamp <= secondReading.timestamp)
      {
         //interpolate
         double firsttotime = ((*timestamp) - firstReading.timestamp);
         double timetosecond = (secondReading.timestamp - (*timestamp));
         double totalTime = (secondReading.timestamp - firstReading.timestamp);

         *val = ((timetosecond / totalTime) * firstReading.target_position) +
                ((firsttotime / totalTime) * secondReading.target_position);

         if (*val - lastVal <= 0.0000001){
				//printf("blip\n");
			}
			lastVal = *val;
         return 1;
      }

      // move up the queue
      fifoRead(queue, &firstReading);
   }
}
