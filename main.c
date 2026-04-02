
//******************ECE 132*************************
//Names: Kasey White and Sophia Pham
//Lab section: Tuesday 1:35
//*************************************************
//Date Started: 3/31/2026
//Date of Last Modification:
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

//light mappings, per our own assignments/hardware setup
#define PINS_OFF 0x00
#define PORT_B_SPEAKER 0x01 //the spotlight output pin, port b
#define PORT_E_HOUSE 0x01 //the house light output pin, port e
#define PORT_E_MUSIC 0X06 //the visuals output pins, port e

//prototype funcs
void button_isr(); //any button press triggers isr
void update_led();
void switch_setup(); //port F
void led_setup(); //port B and E
void ir_setup();
void blink();
void systick(int reload_value); //systick setup


//set up FSM struct and states
struct state{
    int id;
    int outB; //bit pattern for GPIO Port B outputs
    int outE; //bit pattern for GPIO Port E outputs
    int wait; //delay in ms?
    unsigned int next[4]; //4 possible input combinations
};
typedef struct state stype;

//global vars
stype cstate; //current state
int input; //00, 01, 10, 11 to represent L and R switches

void main() {
    //set up peripherals (4 input LEDs, 1 IR sensor, 2 SWs)
    led_setup();
    switch_setup();
    ir_setup();

    //id - output on B - output on E - delay - next state
    stype fsm[4] = {
        {S_OFF, PINS_OFF, PINS_OFF, 10000, {S_OFF, S_MUSIC, S_SPEAKER, S_HOUSE}}, //0: off
        {S_HOUSE, PINS_OFF, PORT_E_HOUSE, 10000, {S_HOUSE, S_MUSIC, S_SPEAKER, S_OFF}}, //1: house
        {S_SPEAKER, PORT_B_SPEAKER, PINS_OFF, 10000, {S_SPEAKER, S_MUSIC, S_HOUSE, S_OFF}}, //2: speaker
        {S_MUSIC, PINS_OFF, PORT_E_MUSIC, 10000, {S_MUSIC, S_HOUSE, S_SPEAKER, S_OFF}} //3: music
    };
    cstate = fsm[0];

    //set up interrupts
    systick(0x001312CF); //init systick with reload
    SysTickIntRegister(blink);

    //FSM logic
    int input = 0b00; //input combinations: 00, 01, 10, 11, where 1 means that switch is ON
    while(1){
        //update output based on current state
        update_led();

        //wait?
        SysCtlDelay(cstate.wait);

        //sample button values to determine next state
        if ((GPIO_PORTF_DATA_R & 0x10) == 0) {
            input += 0b10;
        }
        if ((GPIO_PORTF_DATA_R & 0x01) == 0) {
            input += 0b01;
        }
        cstate=fsm[cstate.next[input]];

        input = 0;
    }
}

/* toggles LED on systick interrupt*/
void blink(){
    if (cstate.id == S_MUSIC){
        GPIO_PORTE_DATA_R ^= PORT_E_MUSIC; //XOR toggles
    }
}

/* called from main
 * updates LED based on current state */
void update_led(){ //this will need to be updated with IR constraints later
    if(cstate.id == S_SPEAKER)
        GPIO_PORTB_DATA_R = cstate.outB;//port B outputs (8-bit pattern)
    else GPIO_PORTB_DATA_R = PINS_OFF;

    GPIO_PORTE_DATA_R = cstate.outE;//port E outputs
}

/*port output setup (leds)*/
void led_setup(void){
    //clocks
    SYSCTL_RCGCGPIO_R |= 0x10; // port E
    SYSCTL_RCGCGPIO_R |= 0x02; //port B

    //direction for output
    GPIO_PORTE_DIR_R |= 0x0F; //direction for PE0 - PE3 house, visuals leds
    GPIO_PORTB_DIR_R |= 0x23; //direction for PB5, PB0, PB1 spotlight leds

    //data enable
    GPIO_PORTE_DEN_R |= 0x0F;  //data for PE0 - PE3 house, visuals leds
    GPIO_PORTB_DEN_R |= 0x23; //data for PB0 - PB2 spotlight leds
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

/*IR input setup (port B)*/
void ir_setup() {
    int in_pins = 0x08;
    //configure the Clock
    SYSCTL_RCGCGPIO_R |= 0b00100000; //port F

    // set Direction for in_pins, set to 0 for input
    GPIO_PORTB_DIR_R &= ~in_pins; //if dir = 01111111 then &= ~(b00010001) then dir = 01101110
    // set Pull Up Resistor, PUR = 1 for in_pins
    GPIO_PORTB_PUR_R |= in_pins; //PUR is HIGH for inputs
    // set Data Enable on for in_pins
    GPIO_PORTB_DEN_R |= in_pins;
}

/*---SYSTICK SETUP---*/
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
