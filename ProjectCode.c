/* 
 * File:   CFinal.c
 * Author: Alain
 *
 * Created on August 4, 2023, 1:22 PM
 */


#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "LiquidCrystal.h"

# define _XTAL_FREQ 1000000
# define S_LED PORTBbits.RB1
void __interrupt(low_priority) ADC_TIMER_INTERuPT(void);
void __interrupt(high_priority) toggle_STOP(void);

unsigned int num_pot1;
unsigned int num_pot2;
int i=0;
int delay=0;
int step_value=0;
int s_flag=0;
int main() 
{
    TRISD=0x00;
    TRISE=0x00;
    TRISBbits.RB1=0;
    TRISBbits.RB0=1;
    pin_setup(&PORTD, &PORTE);
    begin(16, 2, LCD_5x8DOTS);
    
    PR2 = 249;
    
    T2CONbits.T2CKPS = 0b00; // Prescaler 1:1
    T2CONbits.TMR2ON = 1;
    // setup CCP2 in PWM mode
    TRISCbits.RC1 = 0;
    CCP2CONbits.CCP2M = 0b1100;

    // * Configure analog pins, voltage reference and digital I/O 
        ADCON1 = 0x0D;
        TRISAbits.RA0 = 1;
        TRISAbits.RA1 = 1;
    // * Select A/D acquisition time
    // * Select A/D conversion clock

    ADCON2bits.ADCS = 0; // FOSC/2
    ADCON2bits.ACQT = 1; // ACQT = 2 TAD
    ADCON2bits.ADFM = 0; // right justified
    
    // * Select A/D input channel
    ADCON0bits.CHS = 0; // Channel 0 (AN0)
    
    // * Turn on A/D module
    ADCON0bits.ADON = 1;
    
    INTCONbits.INT0E = 1; // Enable INT0
    INTCONbits.INT0IF = 0; // reset INT0 flag
    INTCON2bits.INTEDG0 = 0; // falling edge
    
    // 2 - Configure A/D interrupt (if desired)
    // * Clear ADIF bit
    // * Set ADIE bit
    // * Select interrupt priority ADIP bit
    // * Set GIE bit
    PIR1bits.ADIF = 0;
    PIE1bits.ADIE = 1;
    IPR1bits.ADIP = 0; 
    RCONbits.IPEN = 1; // enable priority levels
    INTCONbits.GIEH = 1; // enable all high priority
    INTCONbits.GIEL = 1; // enable all low priority
    INTCONbits.PEIE = 1; // enable peripheral interrupts
    INTCONbits.GIE = 1;
    
    // setup timer0 
    T0CONbits.PSA = 0; // Prescaler is assigned
    T0CONbits.T0PS = 0x02;
    T0CONbits.T0CS = 0; // clock source is internal instruction cycle
    T0CONbits.T08BIT = 0; // operate in 16 bit mode now
    T0CONbits.TMR0ON = 1; // Turn on timer
    TMR0 = 65223; // initial timer value to get to the 2 seconds delay
    
    // setting up timer0 as an interrupt
    INTCONbits.TMR0IE = 1;
    INTCONbits.TMR0IF = 0;
    INTCON2bits.TMR0IP = 0; // low priority 


    S_LED=0;
    int current_duty=0;
    int s_i=0;
    while (1)
    {
        ADCON0bits.GO = 1;
        //Different cases for calculations to prevent inaccurate numbers
        if(num_pot1<100){
           step_value= (num_pot1*10000)/num_pot2;
           current_duty=(i*step_value)/10000; 
        }
        else if(num_pot1>=100 && num_pot1<300){
            step_value= (num_pot1*100)/num_pot2;
            current_duty=(i*step_value)/100;
        }
        else{
            step_value= (num_pot1*10)/num_pot2;
            current_duty=(i*step_value)/10;
        }
        CCPR2L = (current_duty >>2);
        CCP2CONbits.DC2B= (current_duty & 0x03);
        home();
        print("Max Speed=");
        print_int(num_pot1);
        print("     ");
        
        delay=10*num_pot2;
        
        setCursor(0, 1);
        print("Delay=");
        print_int(delay);
        print("ms     ");
        // STOPPED ACTIVATE
        if (s_flag==1){
        s_i=i;
        home();    
        clear();
       while(s_flag==1){
        home();
        i=0;
        S_LED=1;
        print("STOPPED");
        CCPR2L = 0;
        CCP2CONbits.DC2B= 0;
       }
        S_LED=0;
        i=s_i;
        }
    }
}


void __interrupt(low_priority) ADC_TIMER_INTERuPT(void)
{
    if (INTCONbits.TMR0IE && INTCONbits.TMR0IF)
    {
        INTCONbits.TMR0IF = 0;
        TMR0 = 65223; // because after the interrupt this will be 0
        i=i++;
        if(i>=num_pot2){
            i=0;
        }
    }
    
    if (PIR1bits.ADIF && PIE1bits.ADIE)
    {
        // 5 Wait for A/D conversion to complete by either
        // * Polling for the GO/Done bit to be cleared
        // * Waiting for the A/D interrupt

        // 6 - Read A/D result registers (ADRESH:ADRESL); clear bit ADIF, if required
 
        PIR1bits.ADIF = 0; 
        if (ADCON0bits.CHS == 0) // channel AN0 (speed)
        {
            num_pot1 = (ADRESH << 2);
            ADCON0bits.CHS = 1;
        }
        else if (ADCON0bits.CHS == 1) // channel AN1 (delay))
        {
            num_pot2 = (ADRESH << 2);
            ADCON0bits.CHS = 0;
        }
    }
    
    return;
}

void __interrupt(high_priority) toggle_STOP(void)
{
    if (INTCONbits.INT0IE && INTCONbits.INT0IF)
    {
        INTCONbits.INT0IF = 0;
        if (s_flag==0){
            s_flag=1;
        }
        else{
            s_flag=0;
        }
    }
    // INT0
    
    return;
}
