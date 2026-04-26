//******************ECE 132*************************
//Names: Kasey White and Sophia Pham
//Lab section: Tuesday 1:35
//*************************************************
//Date Started: 4/21/2026
//Date of Last Modification:
//Assignment: Proj. 2
//*************************************************
//Purpose of program:
//Program Inputs:
//Program Outputs:
//*************************************************

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
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

#include "driverlib/ms_dht11.h" //temp sensor driver

//Macro Declarations
#define DELAY           100000000
    //index to state mappings
#define S_OFF 0
#define S_START 1
#define S_IDLE 2
#define S_COLOR 3
#define S_EYE 4
#define S_BMI 5
#define S_TEMP 6


//Function Prototypes
void ir_setup(void);
void adc_setup(void);
void uart_setup(void);
void led_setup(void);
void UART_String(char string[]);

void bmi_isr(void);

void colorblind(void);
void temperature(void);
void bmi(void);

//Global Variables
DHT_TypeDef th; //temperature in celsius and humidity

void main(){

    //Setting the internal clock
    SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

    //set up periphs
    uart_setup();
    adc_setup();
    ir_setup();
    led_setup();
    dht_init();

    //initialize globals
    th.celsius_temp = 0;
    th.humidity = 0;

    //initialize UART interrupt for bmi test
//    UARTIntRegister(UART0_BASE, bmi_isr); //it will call bmi_isr function when the interrupt is enabled and triggered

    while(1){
        UART_String("Starting Colorblind Test...");
        colorblind();
        SysCtlDelay(DELAY);
    }
}

void bmi(void){
    UART_String("Please enter your weight (lbs.) using the potentiometer as a scale");
    //while loop, poll the potentiometer and measure the weight as the ratio between the current resistance and the max
    //if range is between 80 and 280 lbs, then minimum pot value is 80 and max is 280
    bool w_loop = true;
    while (w_loop){
        //output the current weight selection every second, any input to the UART triggers an interrupt

        w_loop = false;
    }
}

void temperature(void){
    UART_String("Please place hand on the temperature sensor.");
    SysCtlDelay(1000); //arbitrary delay to give a digestible and readable output
    UART_String("Reading temperature...");

    dht_readTH(&th); //read first temperature into the th object
    float tot_temp = 0; //total temperature
    int i;
    for (i = 0; i < 10; i++){ //total of 10 samples for averaging
        dht_readTH(&th);
        tot_temp += th.celsius_temp;
        SysCtlDelay(1000); //wait so there is some time between samples
    }

    float avg_temp = tot_temp / 10; //should never have a div by 0 error
    float f_temp = avg_temp * 9/5 + 32; //convert celsius to fahrenheit

    //display temp to terminal
    char* s = NULL;
    sprintf(s, "Your temperature: %.1f", f_temp); //from man sprintf 3, it writes formatted output to character string
    UART_String(s);

    if (f_temp < 80.0){
        UART_String("Warning: Temperature below 80 degrees Fahrenheit");
    } else if (f_temp > 100.0) {
        UART_String("Warning: Temperature above 100 degrees Fahrenheit");
    }
}

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
