#ifndef _uint10_h_
#define _uint10_h_
/*
 * uint10_t type according to mcp3008 10 bit channel resolution. 
 */
typedef unsigned short int uint10_t;
/*
 * uint10_t max value 
 */
#define UINT10_MAX (1023)

#define UINT10_VALIDATION_MASK 0x3FF	/* 0b0000001111111111 */

#endif
