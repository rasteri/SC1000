/* Length 3 Ring Buffer API */
#include <stdio.h>
#include "rb.h"


void RB3_init(rb3_t *rb)
{
    rb->entry = &rb->a;

    rb->a.next = &rb->b;
    rb->b.next = &rb->c;
    rb->c.next = &rb->a;

    rb->a.value = 0;
    rb->b.value = 0;
    rb->c.value = 0;
}

void RB3_insert(rb3_t *rb, double value)
{
    rb->entry->value = value;
}

void RB3_rotate(rb3_t *rb)
{
    rb->entry = rb->entry->next->next;
}

void RB3_push(rb3_t *rb, double value)
{
    // manually inlined functions
    rb->entry = rb->entry->next->next; // RB3_rotate()
    rb->entry->value = value;      // RB3_insert()
}

void RB3_set(rb3_t* rb, double a, double b, double c)
{
    rb->a.value = a;
    rb->b.value = b;
    rb->c.value = c;
}

double RB3_innerProduct(rb3_t *A, rb3_t *B)
{
    double ip = 0;
    int i = 0;

    for(i = 0; i < 3; ++i)
    {
        ip += A->entry->value * B->entry->value;
        RB3_rotate(A);
        RB3_rotate(B);
    }

    return ip;
}

void RB3_print(rb3_t *rb)
{
    printf("%1.4f -> %1.4f -> %f1.4\n",
           rb->entry->value,
           rb->entry->next->value,
           rb->entry->next->next->value);
}
