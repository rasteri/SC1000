//#define _XTAL_FREQ 16000000
#include <xc.h>
#include "mcc_generated_files/mcc.h"


void I2C_Slave_Init(short address) {
    SSPSTAT = 0b10000000;
    SSPADD = address; //Setting address
    SSPCON1 = 0b00110110; // Enabled, No clock stretching, I2C Slave, 7bit address
    SSPCON2 = 0x01;


    SSPIF = 0; // Clear the serial port interrupt flag
    BCLIF = 0; // Clear the bus collision interrupt flag
    BCLIE = 1; // Enable bus collision interrupts
    SSPIE = 1; // Enable serial port interrupts
    PEIE = 1; // Enable peripheral interrupts
    GIE = 1; // Enable global interrupts
}

unsigned int index_i2c = 0; // used as an index pointer in array
unsigned char junk = 0; // used to place unnecessary data
unsigned char first = 1; // used to determine whether data address 

unsigned char STATUSDATA[] = {0, 0, 0, 0, 0, 0};

void interrupt ISR(void) {
    if (SSPIF) // check to see if SSP interrupt
    {
        if (SSPSTATbits.R_nW) { // Master read (R_nW = 1)
            junk = SSPBUF;
            SSPBUF = STATUSDATA[index_i2c];
            SSPCON1bits.CKP = 1; // Release CLK
        }
        if (!SSPSTATbits.R_nW) { //  Master write (R_nW = 0)

            if (!SSPSTATbits.D_nA) { // Last byte was an address (D_nA = 0)
                first = 1; //last byte was address, next will be data location
                junk = SSPBUF; // read buffer to clear BF
                SSPCON1bits.CKP = 1; // Release CLK
            }
            if (SSPSTATbits.D_nA) // Last byte was data (D_nA = 1)
            {
                if (first) {
                    index_i2c = SSPBUF; // load index with array location
                    first = 0; // now clear this
                } else {
                    junk = SSPBUF; // Master did two data writes, could use this to send data to pic, maybe capsense calibration data
                }

                if (SSPCON1bits.WCOL) { // Did a write collision occur?

                    SSPCON1bits.WCOL = 0; //  clear WCOL
                    junk = SSPBUF; // dummy read to clear BF bit
                }
                SSPCON1bits.CKP = 1; // Release CLK
            }
        }
    }
    if (BCLIF) // Did a bus collision occur?
    {

        junk = SSPBUF; // dummy read SSPBUF to clear BF bit
        BCLIF = 0; // clear bus collision Int Flag bit
        SSPCON1bits.CKP = 1; // Release CLK
    }
    SSPIF = 0; // clear SSPIF flag bit
}

unsigned int getADC(unsigned char channel) {

    ADCON0bits.CHS = channel;
    ADCON0bits.ADON = 1;
    __delay_us(10);
    Nop(); Nop(); Nop();
    ADCON0bits.GO_DONE = 1;
    while (ADCON0bits.GO_DONE);
    return ((signed int) ADRESH << 8) | ADRESL;
}

void main(void) {

    unsigned int touchDelay = 0;
    char touchState = 0;
    char oldTouchState = 1;
    char calibrationMode = 1;
    unsigned int calibrationCount;
    unsigned int threshold = 0xffff;
    unsigned int touchedNum = 0;
    unsigned int tmp1, tmp2, tmp3, tmp4, tmp5;
    unsigned int touchAverage = 32768;
    unsigned int schmittedThreshold = 0xffff;

    // Initialize the device
    SYSTEM_Initialize();

    PORTA = 0;
    LATA = 0;
    PORTB = 0;
    LATB = 0;
    PORTC = 0;
    LATC = 0;

    TRISB4 = 1; //I2C SDA
    TRISB5 = 1; //Capsense
    TRISB6 = 1; //I2C SCL
    TRISB7 = 1; //TS_D11
    TRISC0 = 0; //Dummy pin for capsense charge
    TRISC1 = 1; //XFADER1
    TRISC2 = 1; //XFADER2
    TRISC3 = 1; //POT2
    TRISC4 = 1; //TS_D21
    TRISC5 = 1; //TS_D22
    TRISC6 = 1; //POT1
    TRISC7 = 1; //TS_D12

    RC0 = 1;

    ANSEL = 0b10000000; //AN7/RC3 is analog
    ANSELH = 0b00000001; //AN8/RC6 is analog

    /* 16Tosc conversion clock, 6Tad acquisition time, ADC Result Right Justified */
    ADCON2 = 0b10011101;

    I2C_Slave_Init(0xD2);

    // Delay while we wait for everything to settle
    for (calibrationCount = 0; calibrationCount < 60000; calibrationCount++);


    calibrationCount = 0;
    while (1) {

        tmp1 = getADC(5);
        tmp2 = getADC(6);
        tmp3 = getADC(7);
        tmp4 = getADC(8);

        // Charge the internal capacitor by pointing it at a dummy pin we know is set to VDD (RC0/AN4)-
        RC0 = 1;
        ADCON0bits.CHS = 4;

        // Ground Sensor to discharge any residual charge
        TRISB5 = 0;
        ANSELHbits.ANS11 = 0;
        RB5 = 0;

        // Now make it an input and get result
        TRISB5 = 1;
        tmp5 = getADC(11);
        
        #define TOUCHREACTIONSPEED 50
        #define HYSTERESIS 0x1000
        
        if (tmp5 << 6 > touchAverage)
            touchAverage += TOUCHREACTIONSPEED;
        else if (tmp5 << 6 < touchAverage)
            touchAverage -= TOUCHREACTIONSPEED;

        if (calibrationMode) {

            if (touchAverage < threshold)
                threshold = touchAverage;

            calibrationCount++;
            if (calibrationCount > 3000) {
                calibrationMode = 0;
                threshold -= 0x1500; // Give ourselves some margin
                schmittedThreshold = threshold;
            }
            
        } else {

            if (touchAverage < schmittedThreshold) {
                touchState = 1;
                schmittedThreshold = threshold + HYSTERESIS;
            } else {
                touchState = 0;
                schmittedThreshold = threshold;
            }


        }

        // ADC LSBs
        STATUSDATA[0] = (unsigned char) (tmp1 & 0xFF);
        STATUSDATA[1] = (unsigned char) (tmp2 & 0xFF);
        STATUSDATA[2] = (unsigned char) (tmp3 & 0xFF);
        STATUSDATA[3] = (unsigned char) (tmp4 & 0xFF);

        // ADC MSBs
        STATUSDATA[4] = (unsigned char) (((tmp1 & 0x300) >> 8) | ((tmp2 & 0x300) >> 6) | ((tmp3 & 0x300) >> 4) | ((tmp4 & 0x300) >> 2));

        // Digital I/Os and capsense
        STATUSDATA[5] = (unsigned char) ((PORTBbits.RB7) | (PORTCbits.RC7 << 1) | (PORTCbits.RC4 << 2) | (PORTCbits.RC5 << 3) | touchState << 4);
        //STATUSDATA[5] = touchAverage >> 8;
    }
}
/**
 End of File
 */