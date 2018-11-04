/**
  @Generated PIC10 / PIC12 / PIC16 / PIC18 MCUs Source File

  @Company:
    Microchip Technology Inc.

  @File Name:
    mcc.c

  @Summary:
    This is the mcc.c file generated using PIC10 / PIC12 / PIC16 / PIC18 MCUs

  @Description:
    This header file provides implementations for driver APIs for all modules selected in the GUI.
    Generation Information :
        Product Revision  :  PIC10 / PIC12 / PIC16 / PIC18 MCUs - 1.65.2
        Device            :  PIC18LF14K22
        Driver Version    :  2.00
    The generated drivers are tested against the following:
        Compiler          :  XC8 1.45 or later
        MPLAB             :  MPLAB X 4.15
*/

/*
    (c) 2018 Microchip Technology Inc. and its subsidiaries. 
    
    Subject to your compliance with these terms, you may use Microchip software and any 
    derivatives exclusively with Microchip products. It is your responsibility to comply with third party 
    license terms applicable to your use of third party software (including open source software) that 
    may accompany Microchip software.
    
    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER 
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY 
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS 
    FOR A PARTICULAR PURPOSE.
    
    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND 
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP 
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO 
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL 
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT 
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS 
    SOFTWARE.
*/

// Configuration bits: selected in the GUI

// CONFIG1H
#pragma config FOSC = IRC    // Oscillator Selection bits->Internal RC oscillator
#pragma config PLLEN = OFF    // 4 X PLL Enable bit->PLL is under software control
#pragma config PCLKEN = ON    // Primary Clock Enable bit->Primary clock enabled
#pragma config FCMEN = OFF    // Fail-Safe Clock Monitor Enable->Fail-Safe Clock Monitor disabled
#pragma config IESO = OFF    // Internal/External Oscillator Switchover bit->Oscillator Switchover mode disabled

// CONFIG2L
#pragma config PWRTEN = OFF    // Power-up Timer Enable bit->PWRT disabled
#pragma config BOREN = OFF    // Brown-out Reset Enable bits->Brown-out Reset disabled in hardware and software
#pragma config BORV = 19    // Brown Out Reset Voltage bits->VBOR set to 1.9 V nominal

// CONFIG2H
#pragma config WDTEN = OFF    // Watchdog Timer Enable bit->WDT is controlled by SWDTEN bit of the WDTCON register
#pragma config WDTPS = 32768    // Watchdog Timer Postscale Select bits->1:32768

// CONFIG3H
#pragma config HFOFST = ON    // HFINTOSC Fast Start-up bit->HFINTOSC starts clocking the CPU without waiting for the oscillator to stablize.
#pragma config MCLRE = ON    // MCLR Pin Enable bit->MCLR pin enabled, RA3 input pin disabled

// CONFIG4L
#pragma config STVREN = ON    // Stack Full/Underflow Reset Enable bit->Stack full/underflow will cause Reset
#pragma config LVP = OFF    // Single-Supply ICSP Enable bit->Single-Supply ICSP enabled
#pragma config BBSIZ = OFF    // Boot Block Size Select bit->1kW boot block size
#pragma config XINST = OFF    // Extended Instruction Set Enable bit->Instruction set extension and Indexed Addressing mode disabled (Legacy mode)
#pragma config DEBUG = ON    // Background Debugger Enable bit->Background debugger enabled, RA0 and RA1 are dedicated to In-Circuit Debug

// CONFIG5L
#pragma config CP0 = OFF    // Code Protection bit->Block 0 not code-protected
#pragma config CP1 = OFF    // Code Protection bit->Block 1 not code-protected

// CONFIG5H
#pragma config CPB = OFF    // Boot Block Code Protection bit->Boot block not code-protected
#pragma config CPD = OFF    // Data EEPROM Code Protection bit->Data EEPROM not code-protected

// CONFIG6L
#pragma config WRT0 = OFF    // Write Protection bit->Block 0 not write-protected
#pragma config WRT1 = OFF    // Write Protection bit->Block 1 not write-protected

// CONFIG6H
#pragma config WRTC = OFF    // Configuration Register Write Protection bit->Configuration registers not write-protected
#pragma config WRTB = OFF    // Boot Block Write Protection bit->Boot block not write-protected
#pragma config WRTD = OFF    // Data EEPROM Write Protection bit->Data EEPROM not write-protected

// CONFIG7L
#pragma config EBTR0 = OFF    // Table Read Protection bit->Block 0 not protected from table reads executed in other blocks
#pragma config EBTR1 = OFF    // Table Read Protection bit->Block 1 not protected from table reads executed in other blocks

// CONFIG7H
#pragma config EBTRB = OFF    // Boot Block Table Read Protection bit->Boot block not protected from table reads executed in other blocks
