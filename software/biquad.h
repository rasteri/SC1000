#ifndef __BIQUAD_H_
#define __BIQUAD_H_

#include "rb.h"

/******************************************/
/*             Data Structures            */
/******************************************/

typedef struct biquad {
    rb3_t input_delay;
    rb3_t output_delay;
    rb3_t num_coeff;
    rb3_t den_coeff;
} biquad_t;

/******************************************/
/*               Prototypes               */
/******************************************/

/* zero-init and link the strcuture */
void BQ_init(biquad_t *B);

/* set numerator coeffecients of biquad transfer function */
void BQ_setNum(biquad_t *B, double x0, double x1, double x2);

/* set denominator coeffecients of biquad transfer function */
void BQ_setDen(biquad_t *B, double y1, double y2);

/* push sampleIn to input_delay, return biquad output and push 
   output onto output_delay ring buffer */
double BQ_process(biquad_t *B, double sampleIn);

#endif
