#ifndef __RB_H_
#define __RB_H_

/******************************************/
/*             Data Structures            */
/******************************************/

typedef struct sample {
    double value;
    struct sample *next;
} sample_t;

typedef struct {
    sample_t *entry;
    sample_t a;
    sample_t b;
    sample_t c;
} rb3_t;

/******************************************/
/*               Prototypes               */
/******************************************/

/* link and zero initialize a ring buffer struct */
void RB3_init(rb3_t *rb);

/* insert value into ring buffer at entry */
void RB3_insert(rb3_t *rb, double value);

/* set ring buffer entry point to oldest sample */
void RB3_rotate(rb3_t *rb);

/* combination of clockwise rotation & insert */
void RB3_push(rb3_t *rb, double value);

/* A[n] = a, A[n-1] = b, A[n-2] = c */
void RB3_set(rb3_t* rb, double a, double b, double c);

/* (A[n]*B[n]) + (A[n-1]*B[n-1]) + (A[n-2]*B[n-2]) */
double RB3_innerProduct(rb3_t *A, rb3_t *B);

/* print debugger helper */
void RB3_print(rb3_t *rb);

#endif
