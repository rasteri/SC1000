#include <math.h>
#include <stdio.h>
#include "biquad.h"

/* requires an include to fsl_common to run this, TODO */
#define status_t int

#define _PI        3.14159265358979323846   /* pi    */
#define _LN2       0.69314718055994530942   /* ln(2) */
#define _LN_CONST  (_LN2 / 2)               /* ln(2)/2 */
#define _TWO_PI    (_PI * 2)                /* 2*pi */
#define _FS        (48000.0f)               /* sampling frequency */

status_t lowpass(biquad_t *filter, double center_freq, double Q)
{
    /* LOWPASS
     * configure biquad to attenuate frequencies beyond `center_freq` Hz
     * `center_freq` a double between 0 and 24,000 Hz (nyquist freq)
     * `Q` is the quality factor, or peak about the cutoff frequency
     * `filter` points to an initialized biquad_t strucutre
     */

    double b0, b1, b2, a0, a1, a2;
    double W0, S, C, alpha;

    W0 = (_TWO_PI * center_freq) / _FS;
    S  = sinf(W0);
    C  = cosf(W0);
    alpha = S / (2*Q);

    b0 =  (1 - C)/2;
    b1 =   1 - C;
    b2 =  (1 - C)/2;
    a0 =   1 + alpha;
    a1 =  -2*C;
    a2 =   1 - alpha;

    /* Normalize the filter w.r.t. a0 */
    a1 /= a0;
    a2 /= a0;
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a0 /= a0;
  
    
    printf("LOWPASS W/ CenterFreq @ %.2f Hz, Q = %.2f\r\n", center_freq, Q);
    printf("a0 = %f\r\n", a0);
    printf("a1 = %f\r\n", a1);
    printf("a2 = %f\r\n", a2);
    printf("b0 = %f\r\n", b0);
    printf("b1 = %f\r\n", b1);
    printf("b2 = %f\r\n", b2);
    

    BQ_setDen(filter, a1, a2);
    BQ_setNum(filter, b0, b1, b2);

    return 0;
}

status_t highpass(biquad_t *filter, double center_freq, double Q)
{

    /* HIGHPASS
     * configure biquad to attenuate frequencies below `center_freq` Hz
     * `center_freq` a double between 0 and 24,000 Hz (nyquist freq)
     * `Q` is the quality factor, or peak about the cutoff frequency
     * `filter` points to an initialized biquad_t strucutre
     */

    double b0, b1, b2, a0, a1, a2;
    double W0, S, C, alpha;

    W0 = (_TWO_PI * center_freq) / _FS;
    S  = sinf(W0);
    C  = cosf(W0);
    alpha = S / (2*Q);

    b0 =  (1 + C)/2;
    b1 =   1 + C;
    b2 =  (1 + C)/2;
    a0 =   1 + alpha;
    a1 =  -2*C;
    a2 =   1 - alpha;

    /* Normalize the filter w.r.t. a0 */
    a1 /= a0;
    a2 /= a0;
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a0 /= a0;

    /*
    * printf("LOWPASS W/ CenterFreq @ %.2f Hz, Q = %.2f\r\n", center_freq, Q);
    * printf("a0 = %f\r\n", a0);
    * printf("a1 = %f\r\n", a1);
    * printf("a2 = %f\r\n", a2);
    * printf("b0 = %f\r\n", b0);
    * printf("b1 = %f\r\n", b1);
    * printf("b2 = %f\r\n", b2);
    */

    BQ_setDen(&filter, a1, a2);
    BQ_setNum(&filter, b0, b1, b2);

    return 0;
}

status_t bandpass(biquad_t *filter, double center_freq, double Q)
{

    double b0, b1, b2, a0, a1, a2;
    double W0, S, C, alpha;

    W0 = (_TWO_PI * center_freq) / _FS;
    S  = sinf(W0);
    C  = cosf(W0);
    alpha = S / (2*Q);

    b0 =  alpha;
    b1 =   0;
    b2 =  -alpha;
    a0 =   1 + alpha;
    a1 =  -2*C;
    a2 =   1 - alpha;

    /* Normalize the filter w.r.t. a0 */
    a1 /= a0;
    a2 /= a0;
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a0 /= a0;
   
    BQ_setDen(filter, a1, a2);
    BQ_setNum(filter, b0, b1, b2);

    return 0;
}

status_t notch(biquad_t *filter, double center_freq, double Q)
{
    

    double b0, b1, b2, a0, a1, a2;
    double W0, S, alpha;

    W0 = (_TWO_PI * center_freq) / _FS;
    S  = sinf(W0);
    alpha = S / (2*Q);

    b0 =  1;
    b1 = -2*cos(W0);
    b2 =  1;
    a0 =  1 + alpha;
    a1 = -2*cos(W0);
    a2 =  1 - alpha;

    /* Normalize the filter w.r.t. a0 */
    a1 /= a0;
    a2 /= a0;
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a0 /= a0;
  
	  BQ_setDen(filter, a1, a2);
    BQ_setNum(filter, b0, b1, b2);

    return 0;
}

status_t APF(biquad_t *filter, double center_freq, double Q)
{
   

    double b0, b1, b2, a0, a1, a2;
    double W0, S, alpha;

    W0 = (_TWO_PI * center_freq) / _FS;
    S  = sinf(W0);
    alpha = S / (2*Q);

    b0 =  1 - alpha ;
    b1 = -2 * cos(W0) ;
    b2 =  1 + alpha ;
    a0 =  1 + alpha ;
    a1 = -2 * cos(W0) ;
    a2 =  1 - alpha ;

    /* Normalize the filter w.r.t. a0 */
    a1 /= a0;
    a2 /= a0;
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a0 /= a0;

    BQ_setDen(filter, a1, a2);
    BQ_setNum(filter, b0, b1, b2);

    return 0;
}

status_t peakingEQ(biquad_t *filter, double center_freq, double Q, double A)
{

    double b0, b1, b2, a0, a1, a2;
    double W0, S, alpha;

	
    W0 = (_TWO_PI * center_freq) / _FS;
    S  = sinf(W0);
    alpha = S / (2*Q);


    b0 =  1 - alpha*A ;
    b1 = -2 * cos(W0)   ;
    b2 =  1 - alpha*A ;
    a0 =  1 + alpha/A ;
    a1 = -2 * cos(W0)   ;
    a2 =  1 - alpha/A ;

    /* Normalize the filter w.r.t. a0 */
    a1 /= a0;
    a2 /= a0;
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a0 /= a0;
  
    BQ_setDen(&filter, a1, a2);
    BQ_setNum(&filter, b0, b1, b2);
  
    return 0;
}

status_t lowshelf(biquad_t *filter, double center_freq, double Q, double A)
{

    double b0, b1, b2, a0, a1, a2;
    double W0, S, alpha;

	
    W0 = (_TWO_PI * center_freq) / _FS;
    S  = sinf(W0);
    alpha = S / (2*Q);
    

    b0 =  A*( (A+1) - (A-1)*cos(W0) + 2*sqrt(A)*alpha );
    b1 =2*A*( (A-1) - (A+1)*cos(W0)                 );
    b2 =  A*( (A+1) - (A-1)*cos(W0) - 2*sqrt(A)*alpha );
    a0 =  	  (A+1) + (A-1)*cos(W0) + 2*sqrt(A)*alpha  ;
    a1 = -2*( (A-1) + (A+1)*cos(W0)                   );
    a2 =   	  (A+1) + (A-1)*cos(W0) - 2*sqrt(A)*alpha  ;

    /* Normalize the filter w.r.t. a0 */
    a1 /= a0;
    a2 /= a0;
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a0 /= a0;

    BQ_setDen(filter, a1, a2);
    BQ_setNum(filter, b0, b1, b2);

    return 0;
}

status_t highshelf(biquad_t *filter, double center_freq, double Q, double A)
{

    double b0, b1, b2, a0, a1, a2;
    double W0, S, alpha;
	
    W0 = (_TWO_PI * center_freq) / _FS;
    S  = sinf(W0);
    alpha = S / (2*Q);

    b0 =    A*( (A+1) - (A-1)*cos(W0) + 2*sqrt(A)*alpha);
    b1 = -2*A*( (A-1) - (A+1)*cos(W0) 				   );
    b2 =  A*( (A+1) - (A-1)*cos(W0) - 2*sqrt(A)*alpha  );
    a0 =  	  (A+1) + (A-1)*cos(W0) + 2*sqrt(A)*alpha   ;
    a1 =  2*( (A-1) + (A+1)*cos(W0)                    );
    a2 =   	  (A+1) + (A-1)*cos(W0) - 2*sqrt(A)*alpha	;

    /* Normalize the filter w.r.t. a0 */
    a1 /= a0;
    a2 /= a0;
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a0 /= a0;

    BQ_setDen(filter, a1, a2);
    BQ_setNum(filter, b0, b1, b2);

    return 0;
}

/*int main()
{
    biquad_t LP;
    BQ_init(&LP);
    lowpass(&LP, 6000.0f, 1.0f);
    highpass(&LP, 6000.0f, 1.0f);
    bandpass(&LP, 6000.0f, 1.0f);
    notch(&LP, 6000.0f, 1.0f);
    APF(&LP, 6000.0f, 1.0f);
    peakingEQ(&LP, 6000.0f, 1.0f, 1);
    lowshelf(&LP, 6000.0f, 1.0f, 1);
    highshelf(&LP, 6000.0f, 1.0f, 1);
    return 0;
}*/
