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

    stype fsm[6] = {
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
    cstate = fsm[S_OFF]; //set S_OFF as the first state
    char userIN; //test input
    int input = 0;
	UART_String("In S_OFF. Press 'S' to Start.");

    while(1){
   
        userIN = UARTCharGet(UART0_BASE); //user input 

        switch(userIN){
                case 'O': //S_OFF
                    input = 0;
                    break;
                case 'S': //S_START
                    input = 1;
                    break;
                case 'I': //S_IDLE
                    input = 2;
                    break;
                case 'C': //S_COLOR
                    input = 3;
                    break;
                case 'E': //S_EYE
                    input = 4;
                    break;
                case 'B': //S_BMI
                    input = 5;
                    break;
                default: 
                    continue;
            }

        //transition to the next state given input
        cstate = fsm[cstate.next[input]]; 
		
		//use flags to trigger tests 
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
			input = 2; //idle state input 
			cstate = fsm[cstate.next[input]]; //transition to idle 
		
		}
		
		//prompt user inputs
		if(cstate.id == S_OFF){
			UART_String("In S_OFF. Press 'S' to Start.");
		}
		else if(cstate.id == S_START){
			UART_String("In S_START. Press 'I' to Choose Test.");
		}
        else if(cstate.id == S_IDLE){
            UART_String("Choose your test (C, E, B) or turn system off (O)");
        }
    }
}
