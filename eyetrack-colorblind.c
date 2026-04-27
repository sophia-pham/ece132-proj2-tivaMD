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
#include "driverlib/pwm.h"   // PWM driverlib functions

//Variable Declarations
#define GPIO_PA0_U0RX       0x00000001
#define GPIO_PA1_U0TX       0x00000401
#define DELAY               100000000


unsigned long ulPeriod; // Stores PWM period in clock ticks
int divider;    // Clock divider to calculate PWM

//Function Prototypes
void UART_String(char string[]);
void led_setup(void);
void pwm_setup(void);
void colorblind(void);
void eye_track(void);

void main(){

    //Setting the internal clock
    SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ); //set up clock

    //Port Configurations
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    //UART Configurations
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0); //Enable UART hardware
    GPIOPinConfigure(GPIO_PA0_U0RX); // Configure GPIO pin for UART RX line
    GPIOPinConfigure(GPIO_PA1_U0TX); //Configure GPIO pin for UART TX
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1); // Set Pins for UART

    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));


    //set up periphs
    led_setup();
    pwm_setup();

    while(1){
        //UART_String("Starting Colorblind Test...");
        //colorblind();

        UART_String("Starting Eye Tracker Test...");
        eye_track();
        SysCtlDelay(DELAY);
    }
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

    UART_String("Moving to the left...");
    PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, ((5 * ulPeriod) / 100)); //move to the left
    SysCtlDelay(DELAY/3);

    UART_String("Moving to the center...");
    PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, ((75 * ulPeriod) / 1000)); //move to the center
    SysCtlDelay(DELAY/3);

    UART_String("Moving to the right...");
    PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, ((10 * ulPeriod) / 100)); //move to the right
    SysCtlDelay(DELAY/3);

    //AFTER TESTING THE SERVO ARM IS MOVING FROM THE LEFT, TO CENTER, TO RIGHT AS EXPECTED BUT NOT AT FULL MASS AND A BIT FINICKY
    //THESE NUMBERS NEED TO BE PLAYED WITH

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
