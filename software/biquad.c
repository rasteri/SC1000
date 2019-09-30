/* biquad filter API */
#include <stdio.h>
#include "rb.h"
#include "biquad.h"

void BQ_init(biquad_t *B)
{
    RB3_init(&B->input_delay);
    RB3_init(&B->output_delay);
    RB3_init(&B->num_coeff);
    RB3_init(&B->den_coeff);
}

void BQ_setNum(biquad_t *B, double x0, double x1, double x2)
{
    RB3_set(&B->num_coeff, x0, x1, x2);
}

void BQ_setDen(biquad_t *B, double y1, double y2)
{
    /* why is y0 = 0? you know it */
    RB3_set(&B->den_coeff, y1, y2, 0);
}

double BQ_process(biquad_t *B, double sampleIn)
{
    double sampleOut = 0.0f;

    /* x[n] = sampleIn, delay previous inputs */
    RB3_push(&B->input_delay, sampleIn);

    /* sampleOut += a0*x[n] + a1*x[n-1] + a2*x[n-2] */
    sampleOut += RB3_innerProduct(&B->input_delay, &B->num_coeff);

    /* sampleOut -= b1*y[n-1] + b2*y[n-2] */
    sampleOut -= RB3_innerProduct(&B->output_delay, &B->den_coeff);

    /* y[n] = sampleOut, delay previous outputs */
    RB3_push(&B->output_delay, sampleOut);

    /* Prevent clipping */
    /*if(sampleOut >= 1.0f)
    {
        sampleOut = 0.9999f;
    }*/

    return sampleOut;
}
