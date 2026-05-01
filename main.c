//******************ECE 132*************************
//Names: Kasey White and Sophia Pham
//Lab section: Tuesday 1:35
//*************************************************
//Date Started: 4/21/2026
//Date of Last Modification: 5/1/2026
//Assignment: Proj. 2
//*************************************************
//Purpose of program: 
//The program is for the prototype of an integrative health test that features general health exams such
//as color blindness, BMI, and an eye tracker test. The tests utilize a number of input and output peripherals as well as
//foundational embedded systems techniques.
//
//Program Inputs: UART, IR Sensor, Potentiometer, Button Switches
//Program Outputs: UART, LEDs, Servo
//*************************************************

#include <stdbool.h>
#include <stdint.h>
#include <math.h> //for float calculation
#include <ctype.h> //for toupper()
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
#include "driverlib/pwm.h"
#include "driverlib/watchdog.h"

/*------MACRO DECLARATIONS------*/
#define DELAY           100000000 //for the 16mhz clk, 100M cycles is about 6.5s
#define BUFF_SIZE 1000
//index to state mappings
#define S_OFF 0
#define S_START 1
#define S_IDLE 2
#define S_COLOR 3
#define S_EYE 4
#define S_BMI 5
//pin assignments
#define PINS_OFF 0x00
#define PORT_A_UART 0x03 //PA0(RX) and PA1(TX)
#define PORT_E_LEDS 0x03 //PE0 (Red) and PE1 (Green)
#define PORT_F_SERVO 0x04 //PF2
#define PORT_F_IR 0x08 //PF3
//specs for bmi
#define MAX_WEIGHT 280
#define MIN_WEIGHT 80
#define MAX_HEIGHT 78 //6' 6"
#define MIN_HEIGHT 48 //4'

/*------FUNCTION PROTOTYPES------*/
void watchdog_setup(void);
void pwm_setup(void);
void ir_setup(void);
void adc_setup(void);
void uart_setup(void);
void onboard_led_setup(void);
void led_setup(void);
void switch_setup(void);
void uart_string(char string[]); //prints a string, return, new line
void uart_string_no_new(char string[]); //prints a string without new line
void uart_float_no_new(float f, int precision); //prints a float without new line
void uart_new(void); //prints a new line 

void WatchDogIntHandler(void);
void ir_isr(void); 

void buzz(int mode);

void weight_isr(void); //gpio interrupt isr; when button press, display or confirm weight
void height_isr(void); //"" display or confirm height

void colorblind(void);
void eye_track(void);
void bmi(void);

/*------GLOBAL VARIABLES------*/
int user_in = -1; //parsed from user input; informs state transitions
volatile bool g_bWatchDogFeed = false; //tracker for watchdog
bool input_wait = false; //flag to tell watchdog that we are waiting for user input 

//for pwm
unsigned long ulPeriod; // Stores PWM period in clock ticks
unsigned long buzzPeriod; // Period for PWM to buzzer
int divider;    // Clock divider to calculate PWM

//for bmi test
float weight = -1;
float height = -1;

//set up FSM struct and states
struct state{
    int id;
    char* message; //message to print to uart when this is current state
    int colorFlag; //flag to trigger color test
    int eyeFlag; //trigger eye test
    int bmiFlag; //trigger bmi test
    int wait; //delay
    unsigned int next[6];
};

typedef struct state stype; //define type
stype cstate; //current state

void main(){

    //Setting the internal clock
    SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

    //set up periphs
    watchdog_setup();
    uart_setup();
    pwm_setup();
    adc_setup();
    ir_setup();
    onboard_led_setup();
    led_setup();
    switch_setup();

    //configure interrupt for when IR data falls to 0 (active low)
    GPIOIntTypeSet(GPIO_PORTB_BASE, GPIO_PIN_3, GPIO_FALLING_EDGE); //add pin0 (sw2) where applicable
    //tie handler to isr
    GPIOIntRegister(GPIO_PORTB_BASE, ir_isr);
    //enable int
    GPIOIntEnable(GPIO_PORTB_BASE, GPIO_INT_PIN_3);

    stype fsm[6] = {
        //id-message-colorFlag-eyeFlag-bmiFlag-delay-next state 
        //OFF STATE
        {S_OFF, "Turning system off...\n", 0, 0, 0, DELAY,
        {S_OFF, S_START, S_OFF, S_OFF, S_OFF, S_OFF}}, //only goes to start

        //START STATE  
        {S_START, "System starting...\n", 0, 0, 0, DELAY,
        {S_OFF, S_START, S_IDLE, S_START, S_START, S_START}}, //goes to off, state, or idle

        //IDLE STATE
        {S_IDLE, "Entering idle stage and opening menu...\n", 0, 0, 0, DELAY,
        {S_OFF, S_IDLE, S_IDLE, S_COLOR, S_EYE, S_BMI}}, //goes to off, idle, color, eye, bmi

        //COLOR BLIND TEST STATE
        {S_COLOR, "Starting the colorblind test...\n", 1, 0, 0, DELAY,
        {S_OFF, S_COLOR, S_IDLE, S_COLOR, S_COLOR, S_COLOR}}, //goes to off or idle

        //EYE TRACKER TEST STATE 
        {S_EYE, "Starting the eye tracker test...\n", 0, 1, 0, DELAY,
        {S_OFF, S_EYE, S_IDLE, S_EYE, S_EYE, S_EYE}}, //goes to off or idle

        //BMI TEST STATE 
        {S_BMI, "Starting the BMI test...\n", 0, 0, 1, DELAY,
        {S_OFF, S_BMI, S_IDLE, S_BMI, S_BMI, S_BMI}} //goes to off or idle 

    };

    //variable declarations for fsm logic
    cstate = fsm[S_OFF]; //set S_OFF as the first state
    char userIN; //test input
    int input = 0;
    uart_string_no_new("System is off (S_OFF). Press 'S' to Start: ");

    while(1){

        //collect user input
        if (cstate.id != S_OFF) input_wait = true; //waiting for user input
        userIN = UARTCharGet(UART0_BASE); //user input
        UARTCharPut(UART0_BASE, userIN); //print it back
        g_bWatchDogFeed = true; //cause we got a user input
        input_wait = false; //no longer waiting for user input 
        uart_new(); //newline for next output
        switch(toupper(userIN)){
                case 'O': //S_OFF, waits for a user to start the process
                    input = 0;
                    break;
                case 'S': //S_START, collects user info
                    input = 1;
                    break;
                case 'I': //S_IDLE, displays test menu
                    input = 2;
                    break;
                case 'C': //S_COLOR, triggers color blind test
                    input = 3;
                    break;
                case 'E': //S_EYE, triggers eye track test
                    input = 4;
                    break;
                case 'B': //S_BMI, triggers bmi test
                    input = 5;
                    break;
                default:
                    continue; //input should be whatever it was before
            }

        //transition to the next state given input
        cstate = fsm[cstate.next[input]];
        uart_string(cstate.message); //UART outputs current state 

        //use flags to trigger tests
        if (cstate.colorFlag || cstate.eyeFlag || cstate.bmiFlag){
            uart_string("Ensure you stay in front of the sensor during the exam."); //watchdog warning
            g_bWatchDogFeed = false; //can't assume the state of the user when we are not asking for uart input
        }
        if(cstate.colorFlag == 1){
            colorblind();
        }
        if(cstate.eyeFlag == 1){
            eye_track();
        }
        if(cstate.bmiFlag == 1){
            bmi();
        }


        //after test states transition back to idle
        if(cstate.colorFlag || cstate.eyeFlag || cstate.bmiFlag){
            cstate = fsm[cstate.next[S_IDLE]]; //transition to idle
        }

        //prompt user inputs
        if(cstate.id == S_OFF){
            uart_string_no_new("System is off (S_OFF). Press 'S' to Start: ");
        }
        else if(cstate.id == S_START){
            uart_string_no_new("Are you ready? Press 'I' to Choose Test: ");
        }
        else if(cstate.id == S_IDLE){
            uart_string("Tests available:");
            uart_string("Colorblind exam (C)");
            uart_string("Eye track exam (E)");
            uart_string("BMI exam (B)");
            uart_string_no_new("Choose your test (C, E, B) or turn system off (O): ");
        }
        g_bWatchDogFeed = false; //wait for them to input; if a user is sensed, the flag will turn on and user will be prompted to enter input
    }
}

/*----------HEALTH EXAMS----------*/
/**
 * BMI Test (s5)
 *
 * Measure the patient's height and weight, calculate their BMI,
 * compare to healthy standards, and print result.
 */
void bmi(void){
    //prompt the user for weight entry
    uart_string("Please enter your weight (in pounds) using the potentiometer as a scale.\n\rUse the left button to check the current value, and the right button to submit.");
    //register and activate the isr that allows button press to print value
    GPIOIntTypeSet(GPIO_PORTF_BASE, GPIO_INT_PIN_4 | GPIO_INT_PIN_0, GPIO_FALLING_EDGE); //add pin0 (sw2) where applicable
    GPIOIntRegister(GPIO_PORTF_BASE, weight_isr);
    GPIOIntEnable(GPIO_PORTF_BASE, GPIO_INT_PIN_4 | GPIO_INT_PIN_0); //left or right button press activates

    while(weight < 0); //wait for user to be done with weight
    GPIOIntDisable(GPIO_PORTF_BASE, GPIO_INT_PIN_4 | GPIO_INT_PIN_0); //disable the weight interrupt to reconfigure for height

    //prompt the user for height entry
    uart_string("Please enter your height (in inches) using the potentiometer as a scale.\n\rUse the left button to check the current value, and the right button to submit.");
    //register and activate the height isr
    GPIOIntRegister(GPIO_PORTF_BASE, height_isr);
    GPIOIntEnable(GPIO_PORTF_BASE, GPIO_INT_PIN_4 | GPIO_INT_PIN_0); //left or right button press activates

    while(height < 0);
    GPIOIntDisable(GPIO_PORTF_BASE, GPIO_INT_PIN_4 | GPIO_INT_PIN_0); //disable the height interrupt as to not disrupt main prog

    //calculate bmi
    float bmi = weight / (height * height) * 703; //formula from the CDC
    uart_string_no_new("Your BMI: ");
    uart_float_no_new(bmi, 1);
    uart_new();

    //healthy ranges from the CDC
    if (bmi < 18.5){
        uart_string("Warning: BMI below 18.5 (Underweight)");
        buzz(2); //failed buzz
    } else if (bmi > 25.0) {
        uart_string("Warning: BMI above 25.0 (Overweight)");
        buzz(2); //failed buzz
    }
    else {
        uart_string("Your BMI is within healthy weight range.");
        buzz(1); //success buzz
    }
    uart_new();

    //clear the vars before returning to main
    weight = -1; height = -1;
}

/* ISR for button press in the weight collection stage; calculates then displays or sets weight.
 * measure the weight as the ratio between the current resistance and the max
 * if range is between 80 and 280 lbs, then minimum pot value is 80 and max is 280
 */
void weight_isr(){
    //get current pot setting
    uint32_t INPUT; //store the adc value
    ADCProcessorTrigger(ADC0_BASE, 0);//This function will trigger the sample sequence
    ADCSequenceDataGet(ADC0_BASE, 0,&INPUT); //This function will store the resultant data in INPUT
    //it helps to read from the correct pin

    //calculate weight
    float curr_voltage = .0008057 * INPUT;
    //curr voltage out of total scales between min and max weight
    float curr_weight = (MAX_WEIGHT - MIN_WEIGHT) * (curr_voltage / 3.3) + MIN_WEIGHT;

    //determine what to do with calculated value
    //when SW1 is pushed (bit 4), display the current weight
    if (0 == (GPIO_PORTF_DATA_R & 0b00010000)){ // if 0bxxx0xxxx & 0b00000001, then we know bit 4 is 0
        uart_string_no_new("Current Weight: "); uart_float_no_new(curr_weight,1); uart_string(" pounds");
    }
    //when SW2 is pushed (bit 0), confirm weight selection and move on to height
    else if (0 == (GPIO_PORTF_DATA_R & 0b00000001)){ // if 0bxxx0xxxx & 0b00010000, then we know bit 0 is 0
        uart_string_no_new("Weight Selected: "); uart_float_no_new(curr_weight,1); uart_string(" pounds");
        weight = curr_weight;
    }
//    uart_string("Gonna clear interrupt");
    GPIOIntClear(GPIO_PORTF_BASE , GPIO_INT_PIN_4 | GPIO_INT_PIN_0); //clear the flag so that we can interrupt again in the future
//    uart_string("Leaving weight isr");
}

/*isr for button press in the height collection stage; calculates then displays or sets weight*/
void height_isr(){
    //get current pot setting
    uint32_t INPUT; //store the adc value
    ADCProcessorTrigger(ADC0_BASE, 0);//This function will trigger the sample sequence
    ADCSequenceDataGet(ADC0_BASE, 0,&INPUT); //This function will store the resultant data in INPUT

    //calculate height
    float curr_voltage = .0008057 * INPUT;
    //curr voltage out of total scales between min and max height
    float curr_height = (MAX_HEIGHT - MIN_HEIGHT) * (curr_voltage / 3.3) + MIN_HEIGHT;


    //determine what to do with calculated value
    //when SW1 is pushed (bit 4), display the current height
    if (0 == (GPIO_PORTF_DATA_R & 0b00010000)){ // if 0bxxx0xxxx & 0b00000001, then we know bit 4 is 0
        uart_string_no_new("Current Height: "); uart_float_no_new(curr_height,1); uart_string(" inches");
    }
    //when SW2 is pushed (bit 0), confirm height selection
    else if (0 == (GPIO_PORTF_DATA_R & 0b00000001)){ // if 0bxxx0xxxx & 0b00010000, then we know bit 0 is 0
        uart_string_no_new("Height Selected: "); uart_float_no_new(curr_height,1); uart_string(" inches");
        height = curr_height;
    }
    GPIOIntClear(GPIO_PORTF_BASE , GPIO_INT_PIN_4 | GPIO_INT_PIN_0); //clear the flag so that we can interrupt again in the future
}

/**
 * Eye Tracking Test (s4)
 *
 * Moves a servo motor and asks the user which way it moved,
 * then displays the result of the test.
 */
//eye track test
void eye_track(){
    char answer; //user input variable
    
    //Set duty cycles
    //Servo has period of 20ms or 50Hz
    //~1ms is all the way to the left (-90 degrees) = 5% DC
    //~1.5ms is centered (0 degrees) = 7.5% DC
    //~2ms is all the way to the right (90 degrees) = 10% DC
    //Above duty cycles are recommendations from the data sheet, duty cycles changed slightly with testing 

    //sequence through 3 full movements (left to right) for the test
    int j;
    for(j = 1; j < 4; j++){
        float i;
        
        //using a for loop to increment through duty cycle for a more continuous movement
        //increments from 2.5 to 12 % DC to move from left to right
        for(i = 2.5; i < 12.0; i = i + 0.1){
             PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, (int)((i * ulPeriod) / 100));    SysCtlDelay(DELAY/200);
         }

         //increments from 12% to 2.5% DC to move from right to left 
         for(i = 12.0; i > 2.5; i = i - 0.1){
             PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, (int)((i * ulPeriod) / 100));    SysCtlDelay(DELAY/200);
         }
         g_bWatchDogFeed = true; //after each outer loop, feed the watchdog; if this somehow fails, the watchdog will be angry
    }
    PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, 0);

    //Prompt tester for test feedback 
    uart_string("Did the patient's eyes follow the servo arm? Enter (Y/N)");
    input_wait = true; //waiting for user input 
    answer = UARTCharGet(UART0_BASE);
    UARTCharPut(UART0_BASE, answer); //print it back
    uart_new();
    g_bWatchDogFeed = true;
    input_wait = false; //no longer waiting for user input

    if(answer == 'Y' || answer == 'y'){
        uart_string("Test successful!");
        buzz(1); //successful buzz
    }else if(answer == 'N' || answer == 'n'){
        uart_string("Test failed!");
        buzz(2); //failed buzz
    }else{
        uart_string("Test inconclusive!");
        buzz(2); //failed buzz
    }

    uart_new();
}

/**
 * Colorblind Test (s3)
 *
 * Displays a red and green LED to the patient in a specified sequence,
 * determines if they can tell the difference between them, and prints result.
 */
void colorblind(void){
    int redFlag = 0; //flag for red test successful
    int greenFlag = 0; //flag for green test successful
    char C; //keyboard input

    //TEST RED LIGHT
    GPIO_PORTE_DATA_R = 0x01; //turn on Red LED

    input_wait = true; //waiting for user input 
    uart_string("Choose a color (R or G)"); //ask the user for a color
    C = UARTCharGet(UART0_BASE); //keyboard input
    UARTCharPut(UART0_BASE, C); //print it back
    uart_new();
    g_bWatchDogFeed = true;
    input_wait = false; //no longer waiting for user input 
    if(C == 'R' || C == 'r'){ //if user input is R for red
        redFlag = 1; //mark flag
    }

    GPIO_PORTE_DATA_R = 0x00; //turn off red

    SysCtlDelay(500000); //wait

    //TEST GREEN LIGHT
    GPIO_PORTE_DATA_R = 0x02;//turn on green
    input_wait = true; //waiting for user input 
    uart_string("Choose a color (R or G)"); //ask the user for a color
    C = UARTCharGet(UART0_BASE);
    g_bWatchDogFeed = true;
    input_wait = false; //no longer waiting for user input 
    UARTCharPut(UART0_BASE, C); //print it back
    uart_new();
    if(C == 'G' || C == 'g'){ //if user input is G for green
        greenFlag = 1; //mark flag
    }

    GPIO_PORTE_DATA_R = 0x00;//turn off green

    if(greenFlag && redFlag){
        uart_string("Success: Not Colorblind");
        buzz(1); //success buzz
    }else{

        uart_string("Fail: Colorblind");
        buzz(2); //failed buzz
    }

    uart_new();
}

/*----------HELPER FUNCTIONS----------*/
//buzz the buzzer
void buzz(int mode){
    float* notes;
    switch (mode){
        //success
        case 1:
        {
            float yay[] = {440, 554.365, 659.255, 880}; //A, C#, E, A
            notes = yay;
            break;
        }
        //womp womp
        case 2:
        {
            float wompwomp[] = {293.665,277.183,261.626,246.942};
            notes = wompwomp;
            break;
        }
        //alert
        default:
        {
            float alert[] = {880, 1244.51, 880, 1244.51};
            notes = alert;
            break;
        }
    }

    int i;
    for (i = 0; i < 4; i++){
        buzzPeriod = SysCtlClockGet() / 64 / *(notes + i);
        PWMGenPeriodSet(PWM1_BASE, PWM_GEN_2, buzzPeriod);
        PWMPulseWidthSet(PWM1_BASE, PWM_OUT_5, (int)((50 * buzzPeriod) / 100)); //play note
        SysCtlDelay(DELAY/100); //give it time to play (long)
        PWMPulseWidthSet(PWM1_BASE, PWM_OUT_5, 0); //turn it off
        SysCtlDelay(DELAY/1000); //wait before the next note
    }
}

//print strings
void uart_string(char string[]){
    int i;

    //loop until end of the c-string (null terminator)
    for(i = 0; string[i] != '\0'; i++){
        UARTCharPut(UART0_BASE, string[i]);
    }

    UARTCharPut(UART0_BASE, '\r'); //return
    UARTCharPut(UART0_BASE, '\n'); //new line
}
void uart_string_no_new(char string[]){
    int i;
    for (i = 0; string[i] != '\0'; i++){
        UARTCharPut(UART0_BASE, string[i]);
    }
}
void uart_float_no_new(float f, int precision){
    int power = pow(10, precision); //convert precision into power of 10
    bool round = (int)(f * power * 10) % 10 >= 5; //if the next decimal place is greater than 5, then round up
    int expanded  = f * power + (round ? 1 : 0); //contains all of the significant figures but no decimal point yet

    int whole = expanded / power;
    int fractional = expanded % power;

    //get magnitude in 10s (how many 10s will we have to multiply?)
    int whole_divisor = pow(10, (int)log10(whole)); //get the closest 10s power of the whole segment
    int fractional_divisor = pow(10, (int)log10(fractional)); //get the closest 10s power of the fractional segment

    // send the whole numbered segment from left to right
    if (whole_divisor < 1) { //if smaller than 1s place, then print 0
        UARTCharPut(UART0_BASE, 0x30);
    }
    else {
        while(whole_divisor > 0){ // this works because int division (ex. 3/10 = 0 due to truncating)
            int currentDigit = whole / whole_divisor; // get first digit

            UARTCharPut(UART0_BASE, currentDigit + 0x30); // send number and add 30 to compensate for ASCII conversion

            whole = whole % whole_divisor; // decrement the number (the remainder after getting rid of what was just displayed) (ex. 234 --> 34)
            whole_divisor /= 10; //decrement the divisor
        }
    }

    //send radix
    UARTCharPut(UART0_BASE, '.'); // send number and add 30 to compensate for ASCII conversion

    if (fractional_divisor < 1) {
        int i;
        for (i = 0; i < precision; i++) {
            UARTCharPut(UART0_BASE, 0x30);
        }
    }
    else {
        // send fractional segment
        while(fractional_divisor > 0){ // this works because int division (ex. 3/10 = 0 due to truncating)
            int currentDigit = fractional / fractional_divisor; // get first digit

            UARTCharPut(UART0_BASE, currentDigit + 0x30); // send number and add 30 to compensate for ASCII conversion

            fractional = fractional % fractional_divisor; // decrement the number (the remainder after getting rid of what was just displayed) (ex. 234 --> 34)
            fractional_divisor /= 10; //decrement the divisor
        }
    }
}
void uart_new(){
    UARTCharPut(UART0_BASE, '\r'); //return
    UARTCharPut(UART0_BASE, '\n'); //new line
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

/*set up colorblind test LEDs*/
void led_setup(void){
//clock for Port E
    SYSCTL_RCGCGPIO_R |= 0x10; // port E

    //direction for output
    GPIO_PORTE_DIR_R |= 0x03; //direction for PE0 and PE1, red and green leds

    //data enable
    GPIO_PORTE_DEN_R |= 0x03;  //data for PE0 and PE1, red and green leds
}

/*set up on board green LED for watchdog*/
void onboard_led_setup(){
    //GREEN LED
    SYSCTL_RCGCGPIO_R |= 0b00100000; // port F
    // set direction for pin 3, set to 1 for output
    GPIO_PORTF_DIR_R |= 0b00001000;
    // turn data enable on for pin 3
    GPIO_PORTF_DEN_R |= 0b00001000;
}

/*set up UART*/
void uart_setup(){
    //Port Configurations
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA); //to receive and send UART
    //UART Configurations
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0); //Enable UART hardware
    GPIOPinConfigure(GPIO_PA0_U0RX); // Configure GPIO pin for UART RX line
    GPIOPinConfigure(GPIO_PA1_U0TX); //Configure GPIO pin for UART TX
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1); // Set Pins for UART

    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
}

/*set up ADC0*/
void adc_setup(){
    //This will read the voltage off Port E Pin 3
    //This ADC will have the ability to read a voltage between 0mV and 3.3V
    //The resolution of the measurement is 0.8057mV
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0); //Enable ADC, told to add
    ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_IE | ADC_CTL_END | ADC_CTL_CH0);
    ADCSequenceEnable(ADC0_BASE, 0); //Enable ADC0 with sample sequence number 1

}

/*IR input setup (port B)*/
void ir_setup() {
    int in_pins = 0x08; //pin 3
    //configure the Clock
    SYSCTL_RCGCGPIO_R |= 0b00000010; //port B

    // set Direction for in_pins, set to 0 for input
    GPIO_PORTB_DIR_R &= ~in_pins; //if dir = 01111111 then &= ~(b00010001) then dir = 01101110
    // set Pull Up Resistor, PUR = 1 for in_pins
    GPIO_PORTB_PUR_R |= in_pins; //PUR is HIGH for inputs
    // set Data Enable on for in_pins
    GPIO_PORTB_DEN_R |= in_pins;
}

/*set up PWM (M1PWM6)*/
void pwm_setup(){
    //Using PWMDIV_64 b/c system clock is 80MHz but Servo has capacity of 2^16
    //Servo has period of 20ms or 50Hz = 1,600,000
    //80MHz / 64 = 1.25MHz and 1.25MHz/50Hz = 25,000
    //A ulperiod of 25,000 fits within the 16 bit servo register

    //set pwm clock
    SysCtlPWMClockSet(SYSCTL_PWMDIV_64);

    //enable port f 
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    //wait for the peripheral to be ready
    //this is a best practice
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF));

    // Enable PWM Module PWM1 (controls PF1 and PF2)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM1);

    //this is a best practice
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_PWM1));

    // Calculate divider for 50 Hz PWM frequency
    // SysCtlClockGet() provides 1ms
    divider = 50; // freq for the servo is 50 Hz
    ulPeriod = (SysCtlClockGet() / 32) / divider;

    // Calculate initial period for buzzer (440 Hz)
    buzzPeriod = *SysCtlClockGet() / 64) / 440;

    // Configure PF2 (M1PWM6) for PWM output
    // Information about GPIOPinConfigure is on page 266
    GPIOPinConfigure(GPIO_PF1_M1PWM5); // Map PF1 to M1PWM5, buzzer
    GPIOPinConfigure(GPIO_PF2_M1PWM6); // Map PF2 to M1PWM6, servo

    // GPIOPinTypePWM performs the task: it configures pins for use by the PWM peripheral
    // Pin PF2 is set up as GPIO_PIN_2
    // Set PF2 pin as PWM outputs
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2);

    // Configure PWM Generators PWM_GEN_2 and 3 (controls M1PWM6) in up down counting mode
    PWMGenConfigure(PWM1_BASE, PWM_GEN_2, PWM_GEN_MODE_UP_DOWN | PWM_GEN_MODE_NO_SYNC); //buzzer
    PWMGenConfigure(PWM1_BASE, PWM_GEN_3, PWM_GEN_MODE_UP_DOWN | PWM_GEN_MODE_NO_SYNC); //servo

    // Set PWM period for Generator 3 (M1PWM6)
    PWMGenPeriodSet(PWM1_BASE, PWM_GEN_2, buzzPeriod);
    PWMGenPeriodSet(PWM1_BASE, PWM_GEN_3, ulPeriod);

    //Enable PWM Generator 3 to start output
    PWMGenEnable(PWM1_BASE, PWM_GEN_2);
    PWMGenEnable(PWM1_BASE, PWM_GEN_3);

    // Enable PWM output PF2 (M1PWM6)
    PWMOutputState(PWM1_BASE, PWM_OUT_5_BIT | PWM_OUT_6_BIT, true);
}

/*Watchdog configuration and interrupt enable*/
void watchdog_setup(){
    //enable the periph
    SysCtlPeripheralEnable(SYSCTL_PERIPH_WDOG0);

    //wait for module to be ready
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_WDOG0));

    //enable watchdog interrupt
    IntEnable(INT_WATCHDOG);

    //unlock register access
    if(WatchdogLockState(WATCHDOG0_BASE) == true){
        WatchdogUnlock(WATCHDOG0_BASE);
    }

    //register the interrupt handler??
    WatchdogIntRegister(WATCHDOG0_BASE, WatchDogIntHandler);

    //enable the watchdog interupt
    WatchdogIntEnable(WATCHDOG0_BASE);
    WatchdogIntTypeSet(WATCHDOG0_BASE, WATCHDOG_INT_TYPE_INT);

    //set the period - reload timer 15 seconds, will reset after 30 seconds
    WatchdogReloadSet(WATCHDOG0_BASE, SysCtlClockGet() * 30);

    //enable resetting if not fed
    WatchdogResetEnable(WATCHDOG0_BASE);

    //lock in the setup configuration
    WatchdogLock(WATCHDOG0_BASE);

    //turn on Watchdog
    WatchdogEnable(WATCHDOG0_BASE);
}

/*Watchdog interrupt handler; after watchdog first timeout, clear the interrupt or else watchdog will reset system*/
void WatchDogIntHandler(){
    // Clear the watchdog interrupt.
    if (g_bWatchDogFeed)
    {
        if (input_wait) {
            uart_new();
            uart_string("Please enter an input."); //only notify the user if we are waiting on input from them (e.g. not in eye track test)
        }
        WatchdogIntClear(WATCHDOG0_BASE);
        GPIO_PORTF_DATA_R &= 0b11110111;//turn green led off
        if (input_wait)
            g_bWatchDogFeed = false; //they have to enter an input or else the LED will come up
        //if not waiting for input, the watchdog stays fed
    } else {
        GPIO_PORTF_DATA_R |= 0b00001000;//turn green led on
        if (input_wait) buzz(3); //if we need the user to do something to feed the watchdog, tell them
        //other case would be we are stalling in a function or waiting in off mode (in which case we automatically reset)
    }
}

/*ir ISR, activate the flag to feed the watchdog when the IR sensor activates*/
void ir_isr(){
    g_bWatchDogFeed = true;
    GPIOIntClear(GPIO_PORTB_BASE , GPIO_INT_PIN_3); //clear the flag so that we can interrupt again in the future

    //if active rn (we missed the warning timer), then clear it here
    if (WatchdogIntStatus(WATCHDOG0_BASE, true)){
        WatchdogIntClear(WATCHDOG0_BASE);
    }
}
