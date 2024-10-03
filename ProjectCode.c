/*
 * Author: Alain
 * 
 * Description: This code interfaces with two potentiometers, controls PWM 
 * based on their values, and displays real-time information on an LCD.
 * It also allows toggling between running and stopped states via an interrupt.
 * 
 * Created on August 4, 2023, 1:22 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "LiquidCrystal.h"

// Definitions
#define _XTAL_FREQ 1000000   // Define system clock frequency (1 MHz)
#define S_LED PORTBbits.RB1  // Define LED output on PORTB bit 1

// Interrupt function prototypes
void __interrupt(low_priority) ADC_TIMER_INTERuPT(void);
void __interrupt(high_priority) toggle_STOP(void);

// Global variables
unsigned int num_pot1;       // Potentiometer 1 value (max speed)
unsigned int num_pot2;       // Potentiometer 2 value (delay)
int i = 0;                   // Step index for PWM control
int delay = 0;               // Delay value calculated based on num_pot2
int step_value = 0;          // Step value for PWM adjustment
int s_flag = 0;              // Stop flag for toggling system state

int main() 
{
    // Configure I/O
    TRISD = 0x00;            // Set PORTD as output (LCD data pins)
    TRISE = 0x00;            // Set PORTE as output (LCD control pins)
    TRISBbits.RB1 = 0;       // Set RB1 (LED) as output
    TRISBbits.RB0 = 1;       // Set RB0 (INT0) as input (stop button)
    
    // Initialize LCD
    pin_setup(&PORTD, &PORTE);  // Setup LCD pins
    begin(16, 2, LCD_5x8DOTS);  // Initialize LCD as 16x2
    
    // Configure Timer2 for PWM
    PR2 = 249;                       // Set Timer2 period
    T2CONbits.T2CKPS = 0b00;         // Prescaler 1:1
    T2CONbits.TMR2ON = 1;            // Turn on Timer2
    TRISCbits.RC1 = 0;               // Set RC1 as output for PWM
    CCP2CONbits.CCP2M = 0b1100;      // Set CCP2 in PWM mode

    // Configure Analog-to-Digital Conversion (ADC)
    ADCON1 = 0x0D;                   // Set Vref as VSS/VDD and AN0, AN1 as analog
    TRISAbits.RA0 = 1;               // Set RA0 as input (potentiometer 1)
    TRISAbits.RA1 = 1;               // Set RA1 as input (potentiometer 2)
    ADCON2bits.ADCS = 0;             // ADC clock select (FOSC/2)
    ADCON2bits.ACQT = 1;             // Acquisition time = 2 TAD
    ADCON2bits.ADFM = 0;             // Left justified result
    ADCON0bits.CHS = 0;              // Start with channel AN0 (potentiometer 1)
    ADCON0bits.ADON = 1;             // Turn on ADC

    // Configure Interrupts for ADC and Timer
    PIR1bits.ADIF = 0;               // Clear ADC interrupt flag
    PIE1bits.ADIE = 1;               // Enable ADC interrupt
    IPR1bits.ADIP = 0;               // Low priority for ADC interrupt
    RCONbits.IPEN = 1;               // Enable priority levels
    INTCONbits.GIEH = 1;             // Enable high-priority interrupts
    INTCONbits.GIEL = 1;             // Enable low-priority interrupts
    INTCONbits.PEIE = 1;             // Enable peripheral interrupts

    // Configure Timer0 for system timing
    T0CONbits.PSA = 0;               // Assign prescaler to Timer0
    T0CONbits.T0PS = 0x02;           // Prescaler 1:8
    T0CONbits.T0CS = 0;              // Use internal clock source
    T0CONbits.T08BIT = 0;            // 16-bit mode
    T0CONbits.TMR0ON = 1;            // Turn on Timer0
    TMR0 = 65223;                    // Set initial Timer0 value (for delay)

    // Setup Timer0 interrupt
    INTCONbits.TMR0IE = 1;           // Enable Timer0 interrupt
    INTCONbits.TMR0IF = 0;           // Clear Timer0 interrupt flag
    INTCON2bits.TMR0IP = 0;          // Low priority for Timer0 interrupt

    // Initialize LED and PWM variables
    S_LED = 0;                       // Turn off LED initially
    int current_duty = 0;            // Initialize duty cycle for PWM
    int s_i = 0;                     // Store step value during stop mode

    // Main loop
    while (1)
    {
        ADCON0bits.GO = 1;           // Start ADC conversion

        // Handle different ranges of potentiometer values to adjust step calculation
        if(num_pot1 < 100){
           step_value = (num_pot1 * 10000) / num_pot2;
           current_duty = (i * step_value) / 10000; 
        }
        else if(num_pot1 >= 100 && num_pot1 < 300){
            step_value = (num_pot1 * 100) / num_pot2;
            current_duty = (i * step_value) / 100;
        }
        else{
            step_value = (num_pot1 * 10) / num_pot2;
            current_duty = (i * step_value) / 10;
        }

        // Set PWM duty cycle
        CCPR2L = (current_duty >> 2);
        CCP2CONbits.DC2B = (current_duty & 0x03);

        // Display max speed and delay on the LCD
        home();
        print("Max Speed=");
        print_int(num_pot1);
        print("     ");
        
        delay = 10 * num_pot2;       // Calculate delay based on num_pot2
        
        setCursor(0, 1);
        print("Delay=");
        print_int(delay);
        print("ms     ");

        // Stop mode: Activate if stop flag is set
        if (s_flag == 1){
            s_i = i;                 // Store current step value
            home();    
            clear();
            while(s_flag == 1){      // Wait in stop mode until flag is cleared
                home();
                i = 0;               // Freeze step index
                S_LED = 1;           // Turn on LED to indicate stop mode
                print("STOPPED");
                CCPR2L = 0;          // Set PWM duty cycle to 0
                CCP2CONbits.DC2B = 0;
            }
            S_LED = 0;               // Turn off LED when resuming
            i = s_i;                 // Restore step value
        }
    }
}

// Low-priority interrupt handler for ADC and Timer0
void __interrupt(low_priority) ADC_TIMER_INTERuPT(void)
{
    // Handle Timer0 interrupt (system timing)
    if (INTCONbits.TMR0IE && INTCONbits.TMR0IF)
    {
        INTCONbits.TMR0IF = 0;       // Clear interrupt flag
        TMR0 = 65223;                // Reset timer for next interrupt
        i++;                         // Increment step index
        if(i >= num_pot2){
            i = 0;                   // Reset step index based on num_pot2
        }
    }
    
    // Handle ADC interrupt (potentiometer readings)
    if (PIR1bits.ADIF && PIE1bits.ADIE)
    {
        PIR1bits.ADIF = 0;           // Clear ADC interrupt flag
        
        // Read potentiometer values
        if (ADCON0bits.CHS == 0)      // Channel AN0 (potentiometer 1)
        {
            num_pot1 = (ADRESH << 2); // Read and shift result for num_pot1
            ADCON0bits.CHS = 1;       // Switch to channel AN1
        }
        else if (ADCON0bits.CHS == 1) // Channel AN1 (potentiometer 2)
        {
            num_pot2 = (ADRESH << 2); // Read and shift result for num_pot2
            ADCON0bits.CHS = 0;       // Switch back to channel AN0
        }
    }
    
    return;
}

// High-priority interrupt handler for stop button (INT0)
void __interrupt(high_priority) toggle_STOP(void)
{
    // Handle external interrupt (INT0) for toggling stop mode
    if (INTCONbits.INT0IE && INTCONbits.INT0IF)
    {
        INTCONbits.INT0IF = 0;       // Clear INT0 interrupt flag
        
        // Toggle stop flag
        if (s_flag == 0){
            s_flag = 1;              // Enter stop mode
        } else {
            s_flag = 0;              // Exit stop mode
        }
    }
    
    return;
}
