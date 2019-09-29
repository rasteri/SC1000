#include <stdio.h>
#include <string.h>
#include "sc_queue.h"

/*
 * Return: the cubic interpolation of the sample at position 2 + mu
 * Stolen from player.c
 */
#define SQ(x) ((x) * (x))
double fcubic_interpolate(double y0, double y1, double y2, double y3, double mu)
{
   double a0, a1, a2, a3;
   double mu2;

   mu2 = SQ(mu);
   a0 = y3 - y2 - y0 + y1;
   a1 = y0 - y1 - a0;
   a2 = y2 - y0;
   a3 = y1;

   return (mu * mu2 * a0) + (mu2 * a1) + (mu * a2) + a3;
}

// Peek at the queue without altering its contents.
// Aheadby = amount to look ahead by, will return 0 if there isn't enough in the queue
char fifoPeek(statequeue *queue, unsigned int aheadby, inputstate *state)
{

   unsigned int newtail;

   // check we have enough in the queue to be able to look this far ahead
   for (int i = 0; i < aheadby; i++)
   {
      newtail = (queue->tail + i) % queue->size;
      if (newtail == queue->head)
         return 0;
   }

   newtail = (queue->tail + aheadby) % queue->size;

   memcpy(state, &queue->buffer[(newtail + 1) % queue->size], sizeof(inputstate));
   //*state = queue->buffer[newtail];

   return 1;
}

char fifoRead(statequeue *queue, inputstate *state)
{
   if (queue->head == queue->tail)
      return 0;
   queue->tail = (queue->tail + 1) % queue->size;
   *state = queue->buffer[queue->tail];
   //printf ("POP : %u\n",state->timestamp);
   return 1;
}

char fifoWrite(statequeue *queue, inputstate *val, char overwriteold)
{
   // If queue is empty, delete the last one from the end
   if ((queue->head + 1) % queue->size == queue->tail)
   {
      if (overwriteold)
      {
         //printf("OVERFLOWING :%d %d\n", queue->head, queue->tail);
         queue->tail = (queue->tail + 1) % queue->size;
      }
      else
      {
         return 0;
      }
   }
   queue->head = (queue->head + 1) % queue->size;
   queue->buffer[queue->head] = *val;
   //printf ("PUSH :%d %d\n",queue->head, queue->tail);
   return 1;
}
double lastVal = 0;
char InterpolateQueue(statequeue *queue, double *timestamp, double *val)
{
   inputstate y0, y1, y2, y3;
   while (1) // will end when returning value
   {
      // Need 4 values for cubic interpolation
      if (!fifoPeek(queue, 0, &y0))
         return 0;
      if (!fifoPeek(queue, 1, &y1))
         return 0;
      if (!fifoPeek(queue, 2, &y2))
         return 0;
      if (!fifoPeek(queue, 3, &y3))
         return 0;

      // If the timestamp requested is before y1, move us forward
      if (*timestamp < y1.timestamp)
      {
         *timestamp = y1.timestamp;
      }

      // Sanity check, make sure the timestamp requested is between firstTime and secondTime
      if (*timestamp < y2.timestamp)
      {
         //interpolate
         double firsttotime = ((*timestamp) - y1.timestamp);
         double timetosecond = (y2.timestamp - (*timestamp));
         double totalTime = (y2.timestamp - y1.timestamp);

         *val = ((timetosecond / totalTime) * y1.target_position) +
                ((firsttotime / totalTime) * y2.target_position);

         lastVal = *val;

         /**val = fcubic_interpolate(
            y0.target_position,
            y1.target_position,
            y2.target_position,
            y3.target_position, 
            (firsttotime / totalTime)
         );*/

         return 1;
      }

      // move up the queue
      fifoRead(queue, &y1);
   }
}

void fifoInit(statequeue *queue, unsigned int size){
   queue->head = 0;
   queue->tail = 0;
   queue->size = size;
}