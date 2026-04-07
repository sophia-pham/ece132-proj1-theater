
//******************ECE 132*************************
//Names: Kasey White and Sophia Pham
//Lab section: Tuesday 1:35
//*************************************************
//Date Started: 3/31/2026
//Date of Last Modification: 4/7/2026
//Assignment: Proj. 1
//*************************************************
//Purpose of program: Implement an FSM for a theater's lighting/effects control system.
//Program Inputs: Onboard SW1 and SW2, 1 external IR sensor
//Program Outputs: 4 external LEDs (1 house, 2 visual, 1 spotlight), simplified for prototype
//*************************************************

//File include Statements
#include <stdbool.h>
#include <stdint.h>
#include "inc/tm4c123gh6pm.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "driverlib/adc.h"
#include "driverlib/systick.h"

//index to state mappings
#define S_OFF 0
#define S_HOUSE 1
#define S_SPEAKER 2
#define S_MUSIC 3
//transition modes to support transitional house lights between states
//these go directly to their next state after some delay
#define S_TOFF 4
#define S_THOUSE 5
#define S_TSPEAK 6
#define S_TMUSIC 7

//default delay value
#define DELAY 10000000

//light mappings, per our own assignments/hardware setup
#define PINS_OFF 0x00
#define PORT_B_SPEAKER 0x01 //the spotlight output pin, port b
#define PORT_E_HOUSE 0x01 //the house light output pin, port e
#define PORT_E_MUSIC 0X06 //the visuals output pins, port e

//prototype funcs
void update_led(); //called in main while loop to update outputs
void switch_setup(); //setup for port F switches
void led_setup(); //setup for port B and E external LEDs
void ir_setup(); //setup for port E external IR sensor
void blink(); //systick handler to trigger LED blink in music mode
void systick(int reload_value); //systick setup to use for blinking


//set up FSM struct and states
struct state{
    int id; //state index to check for speaker mode in update_led()
    int outB; //bit pattern for GPIO Port B outputs (spotlight LEDs for speaker mode)
    int outE; //bit pattern for GPIO Port E outputs (LEDs for house and music modes)
    int wait; //delay
    unsigned int next[4]; //4 possible input combinations -> particular next states
};
typedef struct state stype; //define type for ease of use

//global vars
stype cstate; //current state

void main() {
    //set up peripherals (4 input LEDs, 1 IR sensor, 2 SWs)
    led_setup();
    switch_setup();
    ir_setup();

    //{id - output on B - output on E - delay - next state}
    stype fsm[8] = {
        //off requires both buttons to turn on system and go to house mode
        {S_OFF, PINS_OFF, PINS_OFF, DELAY, {S_OFF, S_OFF, S_OFF, S_THOUSE}}, //0: off

        //house can go to any mode (though must go to transition first)
        {S_HOUSE, PINS_OFF, PORT_E_HOUSE, DELAY, {S_HOUSE, S_TMUSIC, S_TSPEAK, S_TOFF}}, //1: house

        //speaker mode can go to any mode (though must go to transition first); if already in speaker, then don't go to TSPEAKER
        {S_SPEAKER, PORT_B_SPEAKER, PINS_OFF, DELAY, {S_SPEAKER, S_TMUSIC, S_THOUSE, S_TOFF}}, //2: speaker

        //music mode can go to any mode (though must go to transition first); if already in music, then don't go to TMUSIC
        {S_MUSIC, PINS_OFF, PORT_E_MUSIC, DELAY, {S_MUSIC, S_THOUSE, S_TSPEAK, S_TOFF}}, //3: music

        //transition modes w/ house lights on, only when switching BETWEEN modes, not when staying in same mode
        {S_TOFF, PINS_OFF, PORT_E_HOUSE, DELAY, {S_OFF, S_OFF, S_OFF, S_OFF}}, //4: transition to off
        {S_THOUSE, PINS_OFF, PORT_E_HOUSE, DELAY, {S_HOUSE, S_HOUSE, S_HOUSE, S_HOUSE}}, //5: transition to house, redundant but consistent with system
        {S_TSPEAK, PINS_OFF, PORT_E_HOUSE, DELAY, {S_SPEAKER, S_SPEAKER, S_SPEAKER, S_SPEAKER}}, //6: transition to speaker
        {S_TMUSIC, PINS_OFF, PORT_E_HOUSE, DELAY, {S_MUSIC, S_MUSIC, S_MUSIC, S_MUSIC}} //7: transition to music
    };
    cstate = fsm[0]; //initialize state to off

    //set up interrupts
    systick(0x001312CF); //init systick with reload
    SysTickIntRegister(blink); //set blink function as systick handler

    //FSM logic
    int input = 0b00; //input combinations: 00, 01, 10, 11, where 1 means that switch (Left or Right) is ON
    while(1){
        //update output based on current state
        update_led();
        //for music mode, its LEDs will be interrupted by the systick timer #rave

        //wait using TivaWare function
        SysCtlDelay(cstate.wait);

        //sample button values to determine next state
        if ((GPIO_PORTF_DATA_R & 0x10) == 0) { //left button, activate left bit (position 1)
            input += 0b10;
        }
        if ((GPIO_PORTF_DATA_R & 0x01) == 0) { //right button, activate right bit (position 0)
            input += 0b01;
        }

        //next state logic based on button inputs
        cstate=fsm[cstate.next[input]]; //for transition states, they go directly to their designated NS

        //reset input before next iteration
        input = 0;
    }
}

/* toggles LED on systick interrupt*/
void blink(){
    if (cstate.id == S_MUSIC){ //if in music mode, toggle the music LEDs
        GPIO_PORTE_DATA_R ^= PORT_E_MUSIC; //XOR toggles
    }
}

/* called from main
 * updates LED based on current state */
void update_led(){
    //if in speaker mode and IR sensor is on, turn on the spotlight
    if((cstate.id == S_SPEAKER) && ((GPIO_PORTF_DATA_R & 0x08)==0))
        GPIO_PORTB_DATA_R = cstate.outB;//spotlight output
    else GPIO_PORTB_DATA_R = PINS_OFF; //speaker when IR sensor off and other modes don't use spotlight

    GPIO_PORTE_DATA_R = cstate.outE;//port E outputs (house and music lights)
}

/*port output setup (leds)*/
void led_setup(void){
    //clocks
    SYSCTL_RCGCGPIO_R |= 0x10; // port E
    SYSCTL_RCGCGPIO_R |= 0x02; //port B

    //direction for output
    GPIO_PORTE_DIR_R |= 0x07; //direction for PE0 - PE2 house, visuals leds
    GPIO_PORTB_DIR_R |= 0x01; //direction for PB0 spotlight leds

    //data enable
    GPIO_PORTE_DEN_R |= 0x07;  //data for PE0 - PE3 house, visuals leds
    GPIO_PORTB_DEN_R |= 0x01; //data for PB0 - PB2 spotlight leds
}

/*switch input setup (port F)*/
void switch_setup() {
    int in_pins = 0x11;
    //configure the Clock
    SYSCTL_RCGCGPIO_R |= 0b00100000;

    //ensure to unlock and commit so that SW2 could be activated via this function in the future; code from lab2
    //unlock sw2
    GPIO_PORTF_LOCK_R |= 0x4C4F434B;
    // mask operation to configure the commit register
    GPIO_PORTF_CR_R |= in_pins;

    // set Direction for in_pins, set to 0 for input
    GPIO_PORTF_DIR_R &= ~in_pins; //if dir = 01111111 then &= ~(b00010001) then dir = 01101110
    // set Pull Up Resistor, PUR = 1 for in_pins
    GPIO_PORTF_PUR_R |= in_pins; //PUR is HIGH for inputs
    // set Data Enable on for in_pins
    GPIO_PORTF_DEN_R |= in_pins;
}

/*IR input setup (port F)*/
void ir_setup() {
    int in_pins = 0x08;
    //configure the Clock
    SYSCTL_RCGCGPIO_R |= 0b00100000; //port F

    // set Direction for in_pins, set to 0 for input
    GPIO_PORTF_DIR_R &= ~in_pins; //if dir = 01111111 then &= ~(b00010001) then dir = 01101110
    // set Pull Up Resistor, PUR = 1 for in_pins
    GPIO_PORTF_PUR_R |= in_pins; //PUR is HIGH for inputs
    // set Data Enable on for in_pins
    GPIO_PORTF_DEN_R |= in_pins;
}

/*systick timer setup*/
void systick(int reload_value){
    //Initialization of the control register to 0 so we are not trying to count while we set things up
    NVIC_ST_CTRL_R= 0;

    //Setting of the reload value, max is 0x00FFFFFF (24 bits)
    NVIC_ST_RELOAD_R = reload_value; //when counter reaches 0, automatically jumps back to reload_val

    //Resetting of the current value that is used for counting
    NVIC_ST_CURRENT_R = 0;

    //Initialization of the control register to allow counting to happen AND allow interrupts
    NVIC_ST_CTRL_R = 0b11; //bit 0 is counter enable, bit 1 is interrupt enable
    // INTEN means that when count reaches 0, an interrupt is sent to NVIC
}
