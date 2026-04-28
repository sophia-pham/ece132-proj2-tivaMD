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
#define S_START 1
#define S_IDLE 2
#define S_COLOR 3
#define S_EYE 4
#define S_BMI 5


//variable declarations
#define DELAY 100000000
unsigned long ulPeriod; // Stores PWM period in clock ticks
int divider;    // Clock divider to calculate PWM

//Output  Pin Assignments
#define PINS_OFF 0x00
#define PORT_A_UART 0x03 //PA0(RX) and PA1(TX)
#define PORT_E_LEDS 0x03 //PE0 (Red) and PE1 (Green)
#define PORT_F_SERVO 0x04 //PF2

//prototypes
void led_setup(void);
void uart_setup(void);
void adc_setup(void);
void ir_setup(void);
void pwm_setup(void);
void colorblind(void);
void eye_track(void);

//set up FSM struct and states
struct state{
    int id;
    int outA; //UART
    int outE; //CB Lights
    int outF; //Servo
    int colorFlag; //flag to trigger color test
    int eyeFlag; //trigger eye test
    int bmiFlag; //trigger bmi test
    int tempFlag; //trigger temp test
    int wait; //delay
    unsigned int next[6];
}

typedef struct state stype; //define type
stype cstate; //current state

void main(){

    //Setting the internal clock
    SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

    //Port Configurations
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    //set up periphs
    led_setup();
    pwm_setup();

    stpye fsm[6] = {
        //id-outA-outE-outF-colorFlag-eyeFlag-bmiFlag-tempFlag-delay-next state
        {S_OFF, PINS_OFF, PINS_OFF, PINS_OFF, 0, 0, 0, 0, DELAY,
        {S_OFF, S_START, S_OFF, S_OFF, S_OFF, S_OFF, S_OFF}},

        {S_START, PORT_A_UART, PINS_OFF, PINS_OFF, 0, 0, 0, 0, DELAY,
         {S_OFF, S_START, S_IDLE, S_START, S_START, S_START}},

        {S_IDLE, PORT_A_UART, PINS_OFF, PINS_OFF, 0, 0, 0, 0, DELAY,
        {S_OFF, S_IDLE, S_IDLE, S_COLOR, S_EYE, S_BMI}},

        {S_COLOR, PORT_A_UART, PORT_E_LEDS, PINS_OFF, 1, 0, 0, 0, DELAY, {S_OFF, S_COLOR, S_IDLE, S_COLOR, S_COLOR, S_COLOR}},

        {S_EYE, PORT_A_UART, PINS_OFF, PORT_F_SERVO, 0, 1, 0, 0, DELAY,
        {S_OFF, S_EYE, S_IDLE, S_EYE, S_EYE, S_EYE}},

        {S_BMI, PORT_A_UART, PINS_OFF, PINS_OFF, 0, 0, 1, 0, DELAY,
        {S_OFF, S_BMI, S_IDLE, S_BMI, S_BMI, S_BMI}},

        //temperature state removed
        //{S_TEMP, PORT_A_UART, PINS_OFF, PINS_OFF, 0, 0, 0, 1, DELAY,
        //{S_OFF, S_TEMP, S_IDLE, S_TEMP, S_TEMP, S_TEMP, S_TEMP}}

        };

    //variable declarations for fsm logic

    cstate = fsm[0]; //set S_OFF as the first state
    char userIN; //test input
    int input = 0;

    while(1){
        UART_String("FSM START: Currently in S_OFF");
        UART_String("Press 'S' on the keyboard to move to S_START");
        userIN = UARTCharGet(UART0_BASE);
        UART_String("Press 'I' on the keyboard to move to S_IDLE");

        switch(userIN){
            case 'O':
                input = 0;
                break;
            case 'S':
                input = 1;
                break;
            case 'I':
                input = 2;
                break;
            case 'C':
                input = 3;
                break;
            case 'E':
                input = 4;
                break;
            case 'B':
                input = 5;
                break;
        }

        cstate=fsm[cstate.next[input]]; //go to START
        //input will update now we are in S_START, input = 1


        UART_String("Press 'I' on the keyboard to move to S_IDLE");
        userIN = UARTCharGet(UART0_BASE);

        switch(userIN){
            case 'O':
                input = 0;
                break;
            case 'S':
                input = 1;
                break;
            case 'I':
                input = 2;
                break;
            case 'C':
                input = 3;
                break;
            case 'E':
                input = 4;
                break;
            case 'B':
                input = 5;
                break;
        }

        cstate=fsm[cstate.next[input]]; //go to IDLE
        //input will update now we are in S_IDLE, input = 2

        //now that we are in idle choose a test
        if(cstate.id == "S_IDLE"){
            UART_String("Choose your test. Use the keyboard to select the first letter of the test name ");

            switch(userIN){
                case 'O':
                    input = 0;
                    break;
                case 'S':
                    input = 1;
                    break;
                case 'I':
                    input = 2;
                    break;
                case 'C':
                    input = 3;
                    break;
                case 'E':
                    input = 4;
                    break;
                case 'B':
                    input = 5;
                    break;
            }

            cstate=fsm[cstate.next[input]]; //go to the next state defined by the user input

            if(cstate.colorFlag == 1){
                colorblind();
            }
            if(cstate.eyeFlag == 1){
                eye_track();
            }
            if(cstate.bmiFlag == 1){
                bmi(); //change this to the correct name
            }


        }

        if(cstate.id == S_COLOR || cstate.id == S_EYE || cstate.id == S_BMI){
            cstate = fsm[2]; //go to back to idle state
        }

        //reset input
        input = 0;
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


    //print strings
    void UART_String(char string[]){
        int i;

        //loop until end of the c-string (null terminator)
        for(i = 0; string[i] != '\0'; i++){
            UARTCharPut(UART0_BASE, string[i]);
        }
        UARTCharPut(UART0_BASE, '\r'); //return
        UARTCharPut(UART0_BASE, '\n'); //new line
    }

    //configure colorblind leds
    void led_setup(void){
    //clock for Port E
        SYSCTL_RCGCGPIO_R |= 0x10; // port E

         //direction for output
         GPIO_PORTE_DIR_R |= 0x03; //direction for PE0 and PE1, red and green leds

         //data enable
        GPIO_PORTE_DEN_R |= 0x03;  //data for PE0 and PE1, red and green leds
    }

    //configure pwm
    void pwm_setup(){
        //Using PWMDIV_64 b/c system clock is 80MHz but Servo has capacity of 2^16
        //Servo has period of 20ms or 50Hz = 1,600,000
        //80MHz / 64 = 1.25MHz and 1.25MHz/50Hz = 25,000
        //A ulperiod of 25,000 fits within the 16 bit servo register

        SysCtlPWMClockSet(SYSCTL_PWMDIV_64);
        // Enable PWM Module PWM1 (controls PF1 and PF2)
        SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM1);

        //this is a best practice
        while (!SysCtlPeripheralReady(SYSCTL_PERIPH_PWM1));

        // Calculate divider for 50 Hz PWM frequency
        // SysCtlClockGet() provides 1ms
        divider = 50; // freq for the servo is 50 Hz
        ulPeriod = (SysCtlClockGet() / 64) / divider;

        // Configure PF1 (M1PWM6) for PWM output
        // Information about GPIOPinConfigure is on page 266
        GPIOPinConfigure(GPIO_PF2_M1PWM6); // Map PF2 to M1PWM6

        // GPIOPinTypePWM performs the task: it configures pins for use by the PWM peripheral
        // Pin PF2 is set up as GPIO_PIN_2
        // Set PF2 pin as PWM outputs
        GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_2);

        // Configure PWM Generator PWM_GEN_3 (controls M1PWM6) in up down counting mode
        PWMGenConfigure(PWM1_BASE, PWM_GEN_3, PWM_GEN_MODE_UP_DOWN | PWM_GEN_MODE_NO_SYNC);

        // Set PWM period for Generator 3 (M1PWM6)
        PWMGenPeriodSet(PWM1_BASE, PWM_GEN_3, ulPeriod);

        //Enable PWM Generator 3 to start output
        PWMGenEnable(PWM1_BASE, PWM_GEN_3);

        // Enable PWM output PF2 (M1PWM6)
        PWMOutputState(PWM1_BASE, PWM_OUT_6_BIT, true);
    }

    //eye track test
    void eye_track(){
        //Set duty cycles
        //Servo has period of 20ms or 50Hz
        //~1ms is all the way to the left (-90 degrees) = 5% DC
        //~1.5ms is centered (0 degrees) = 7.5% DC
        //~2ms is all the way to the right (90 degrees) = 10% DC

    //    UART_String("Moving to the left...");
    //    PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, ((5 * ulPeriod) / 100)); //move to the left
    //    SysCtlDelay(DELAY/3);
    //
    //    UART_String("Moving to the center...");
    //    PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, ((75 * ulPeriod) / 1000)); //move to the center
    //    SysCtlDelay(DELAY/3);
    //
    //    UART_String("Moving to the right...");
    //    PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, ((10 * ulPeriod) / 100)); //move to the right
    //    SysCtlDelay(DELAY/3);

        //AFTER TESTING THE SERVO ARM IS MOVING FROM THE LEFT, TO CENTER, TO RIGHT AS EXPECTED BUT NOT AT FULL MASS AND A BIT FINICKY
        //THESE NUMBERS NEED TO BE PLAYED WITH

        //sequence through 3 full movements (left to right) for the test
        int j;
        for(j = 1; j < 4; j++){
            float i;
            //from left to right go from 5 to 10%
            //using a for loop tos increment through duty cycle for a more continuous movement

            for(i = 3.5; i < 11.0; i = i + 0.1){
                 PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, (int)((i * ulPeriod) / 100));    SysCtlDelay(DELAY/150);
             }

             //from left to right go from 10 to 5%
             //changed 10 to 11 and 5 to 2.0
             for(i = 11.0; i > 3.5; i = i - 0.1){
                 PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, (int)((i * ulPeriod) / 100));    SysCtlDelay(DELAY/150);
              }
        }

    }

    //color blind test
    void colorblind(void){
        int redFlag = 0; //flag for red test successful
        int greenFlag = 0; //flag for green test successful
        char C; //keyboard input

            //TEST RED LIGHT
            GPIO_PORTE_DATA_R = 0x01; //turn on Red LED

            UART_String("Choose a color"); //ask the user for a color
            C = UARTCharGet(UART0_BASE); //keyboard input
            if(C == 'R' || C == 'r'){ //if user input is R for red
                redFlag = 1; //mark flag
            }

            GPIO_PORTE_DATA_R = 0x00; //turn off red

            SysCtlDelay(500000); //wait

            //TEST GREEN LIGHT
            GPIO_PORTE_DATA_R = 0x02;//turn on green
            UART_String("Choose a color"); //ask the user for a color
            C = UARTCharGet(UART0_BASE);
            if(C == 'G' || C == 'g'){ //if user input is G for green
                greenFlag = 1; //mark flag
            }

            GPIO_PORTE_DATA_R = 0x00;//turn off green

            if(greenFlag && redFlag){
                UART_String("Success: Not Colorblind");
            }else{

                UART_String("Fail: Colorblind");
            }
    }


