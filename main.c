#include "msp.h"
#include <driverlib.h>
#include <grlib.h>
#include "Crystalfontz128x128_ST7735.h"
#include <stdio.h>
#include <time.h>

char alarm_ampm=1;
char alarm_ampms[] = {'a','p'};
int alarm_hour=12;
int alarm_min=0;
char time_ampm=1;
char time_ampms[] = {'a','p'};
int time_hour=1;
int last_button;
int time_min=5;
int alarm_set=0;
int resultsBuffer[2];
int lastresultsBuffer[2];
int temp_time[];
int song_time[] = {400,100,400,100};
int song_freq[] = {4000,0,4000,0};
int array_count=0;
int play=0;
int i=0;
int k=0;

Graphics_Context g_sContext;


void mapports();
void init_timer();

const eUSCI_UART_Config uartConfig =
{
    EUSCI_A_UART_CLOCKSOURCE_SMCLK,          // SMCLK Clock Source
    6,                                       // BRDIV = 6
    8,                                       // UCxBRF = 8
    32,                                      // UCxBRS = 32
    EUSCI_A_UART_NO_PARITY,                  // No Parity
    EUSCI_A_UART_LSB_FIRST,                  // MSB First
    EUSCI_A_UART_ONE_STOP_BIT,               // One stop bit
    EUSCI_A_UART_MODE,                       // UART mode
    EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION  // Oversampling
};

void main(void) {

    /* Halting WDT  */
    MAP_WDT_A_holdTimer();
    MAP_Interrupt_disableMaster();

    /* Initializes Clock System */
    MAP_CS_setDCOCenteredFrequency(CS_DCO_FREQUENCY_12);
    //MAP_CS_initClockSignal(CS_SMCLK, CS_MODCLK_SELECT, CS_CLOCK_DIVIDER_8 );

    /* Initializes display */
    Crystalfontz128x128_Init();
    Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP);

    /* Initializes graphics context */
    Graphics_initContext(&g_sContext, &g_sCrystalfontz128x128);
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_RED);
    Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
    GrContextFontSet(&g_sContext, &g_sFontFixed6x8);
    Graphics_clearDisplay(&g_sContext);

    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P3, GPIO_PIN2 | GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION);

    UART_initModule(EUSCI_A2_BASE, &uartConfig);
    UART_enableModule(EUSCI_A2_BASE);

    MAP_UART_enableInterrupt(EUSCI_A2_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT);

    /* Configures Pin 6.0 and 4.4 as ADC input */
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P6, GPIO_PIN0, GPIO_TERTIARY_MODULE_FUNCTION);
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P4, GPIO_PIN4, GPIO_TERTIARY_MODULE_FUNCTION);

    /* Initializing ADC */
    MAP_ADC14_enableModule();
    MAP_ADC14_initModule(ADC_CLOCKSOURCE_SMCLK, ADC_PREDIVIDER_64, ADC_DIVIDER_8, 0);

    /* Configuring ADC Memory (ADC_MEM0 - ADC_MEM1 (A15, A9)  with repeat)
         * with internal 2.5v reference */
    MAP_ADC14_configureMultiSequenceMode(ADC_MEM0, ADC_MEM1, true);
    MAP_ADC14_configureConversionMemory(ADC_MEM0,ADC_VREFPOS_AVCC_VREFNEG_VSS,ADC_INPUT_A15, ADC_NONDIFFERENTIAL_INPUTS);
    MAP_ADC14_configureConversionMemory(ADC_MEM1,ADC_VREFPOS_AVCC_VREFNEG_VSS,ADC_INPUT_A9, ADC_NONDIFFERENTIAL_INPUTS);

    /* Enabling the interrupt when a conversion on channel 1 (end of sequence)
     *  is complete and enabling conversions */
    MAP_ADC14_enableInterrupt(ADC_INT1);
    MAP_ADC14_enableSampleTimer(ADC_AUTOMATIC_ITERATION);

    /* Triggering the start of the sample */
    MAP_ADC14_enableConversion();
    MAP_ADC14_toggleConversionTrigger();

     mapports();
 	init_timer();

    WDT_A->CTL = WDT_A_CTL_PW |				// 'password' to enable access
	            WDT_A_CTL_SSEL__SMCLK |         // clock source = SMCLK
	            WDT_A_CTL_TMSEL |               // this bit turns on interval mode
	            WDT_A_CTL_CNTCL |               // clear the internal WDT counter
	            WDT_A_CTL_IS_5;                 // specifies the divisor.  value 5 => 8K

    P3->DIR &=~BIT5; // clear button directions
    P5->DIR &=~BIT1;
    MAP_GPIO_enableInterrupt(GPIO_PORT_P3, GPIO_PIN5);
    MAP_GPIO_enableInterrupt(GPIO_PORT_P5, GPIO_PIN1);

    MAP_Interrupt_enableInterrupt(INT_EUSCIA2);
    MAP_Interrupt_enableInterrupt(INT_PORT3);
    MAP_Interrupt_enableInterrupt(INT_PORT5);
    MAP_Interrupt_enableMaster();

    MAP_Interrupt_disableSleepOnIsrExit();

    while(1) {
        PCM_gotoLPM0();
    }
}

volatile uint32_t current_time = 0;
volatile uint32_t tempTime = 0;
volatile int intCounter = 0;

/* EUSCI_A2_ISR - received UART data */
void EUSCI_A2_ISR(void) {

    uint32_t status = UART_getEnabledInterruptStatus(EUSCI_A2_BASE);

    UART_clearInterruptFlag(EUSCI_A2_BASE, status);

    if(status & EUSCI_A_UART_RECEIVE_INTERRUPT) {

        if (intCounter == 0) {
            tempTime = 0;
        }

        uint32_t rxVal = UART_receiveData(EUSCI_A2_BASE);


        int shamt = (3 - intCounter) * 8;
        rxVal = rxVal << shamt;
        tempTime += rxVal;

        intCounter += 1;

        if (intCounter == 4) {
            intCounter = 0;

            /* Current "time since 1900" to "time since 1970" */
            current_time = tempTime - 2208988800;

            display_time();
        }
    }
}

void display_time() {
    if (!alarm_set) {

        time_t rawtime = current_time - 18000;

        struct tm * ptm;

        ptm = localtime ( &rawtime );

        int hour = ptm->tm_hour;

        if (ptm->tm_hour == 0) {
            hour = 12;
        } else if (ptm->tm_hour > 12)  {
            hour = ptm->tm_hour - 12;
        }

        time_hour = hour;
        time_min = ptm->tm_min;
        time_ampm = ptm->tm_hour < 12 ? 0: 1;

        char string[16];
        if (ptm->tm_min > 9) {
            sprintf(string, "%2d:%2d %cm", hour, ptm->tm_min, ptm->tm_hour < 12 ? 'a': 'p');
        }
        else {
            sprintf(string, "%2d:0%d %cm", hour, ptm->tm_min, ptm->tm_hour < 12 ? 'a': 'p');
        }

    	Graphics_drawStringCentered(&g_sContext,(int8_t *)string,16,64,50,OPAQUE_TEXT);


        if (time_hour==alarm_hour && time_min==alarm_min && time_ampm==alarm_ampm) {
            // play alarm sound/song
            if (!play) {
                for (i=0; i<4; i++) {
                    temp_time[i]=song_time[i];
                }
                play = 1;
                MAP_Interrupt_enableInterrupt(INT_WDT_A);
                TIMER_A0->CTL ^= TIMER_A_CTL_MC_0;
            }
        }
    }

}

void mapports(){
	PMAP->KEYID=PMAP_KEYID_VAL; // unlock PMAP
	P2MAP->PMAP_REGISTER7=PMAP_TA0CCR0A;  // map TA0CCR0 to P2.7 as primary module
	PMAP->KEYID=0;				// relock PMAP until next hard reset
}

void init_timer(){              // initialize and start timer
	TIMER_A0->CTL |= TIMER_A_CTL_CLR ;// reset clock
	TIMER_A0->CTL =  TIMER_A_CTL_TASSEL_2  // clock source = SMCLK
	                | TIMER_A_CTL_ID_0      // clock prescale=1
					| TIMER_A_CTL_MC_1;     // Up mode
	TIMER_A0->EX0 = TIMER_A_EX0_TAIDEX_2;  // divisor=3

	// set CCR0 compare mode, output mode 4 (toggle), flag clear
	TIMER_A0->CCTL[0]= TIMER_A_CCTLN_OUTMOD_4;

	P2->SEL0|=BIT7; // connect timer output to pin
	P2->DIR |=BIT7; // output mode on P2.7
}

void ADC14_IRQHandler(void)
{
    uint64_t status;

    status = MAP_ADC14_getEnabledInterruptStatus();
    MAP_ADC14_clearInterruptFlag(status);

    /* ADC_MEM1 conversion completed */
    if(status & ADC_INT1 && alarm_set) {

        int button = P3->IN & BIT5;
        if (!button && last_button) {
            alarm_set = 0;
            MAP_Interrupt_disableInterrupt(INT_ADC14);
            Graphics_clearDisplay(&g_sContext);
            display_time();
    	    return;
        }
        last_button = P3->IN & BIT5;

        /* Store ADC14 conversion results*/
    	resultsBuffer[0] = ADC14_getResult(ADC_MEM0);
    	resultsBuffer[1] = ADC14_getResult(ADC_MEM1);

	 switch (alarm_set) {
	     case 1: // hour set
	            if ((resultsBuffer[0] > lastresultsBuffer[0] + 1000) && resultsBuffer[0]>15000) {
	                alarm_set++;
	            }
	            if ((resultsBuffer[0] < lastresultsBuffer[0] - 1000) && resultsBuffer[0]<1000) {
	                alarm_set=3;
	            }
	            if ((resultsBuffer[1] > lastresultsBuffer[1] + 1000) && resultsBuffer[1]>15000) {
	                alarm_hour++;
	                if (alarm_hour>12) {
	                    alarm_hour = 1;
	                }
	            }
	            else if ((resultsBuffer[1] < lastresultsBuffer[1] - 1000) && resultsBuffer[1]<1000) {
	                alarm_hour--;
	                if (alarm_hour<1) {
	                    alarm_hour = 12;
	                }
	            }
	            break;
	     case 2: // min set
	            if ((resultsBuffer[0] > lastresultsBuffer[0] + 1000) && resultsBuffer[0]>15000) {
	                alarm_set++;
	            }
	            if ((resultsBuffer[0] < lastresultsBuffer[0] - 1000) && resultsBuffer[0]<1000) {
	                alarm_set--;
	            }
	            if ((resultsBuffer[1] > lastresultsBuffer[1] + 1000) && resultsBuffer[1]>15000) {
	                alarm_min++;
	                if (alarm_min>59) {
	                    alarm_min = 0;
	                }
	            }
	            else if ((resultsBuffer[1] < lastresultsBuffer[1] - 1000) && resultsBuffer[1]<1000) {
	                alarm_min--;
	                if (alarm_min<0) {
	                    alarm_min = 59;
	                }
	            }
	            break;
	     case 3: // am/pm set
	            if ((resultsBuffer[0] > lastresultsBuffer[0] + 1000) && resultsBuffer[0]>15000) {
	                alarm_set=1;
	            }
	            if ((resultsBuffer[0] < lastresultsBuffer[0] - 1000) && resultsBuffer[0]<1000) {
	                alarm_set=1;
	            }
	            if ((resultsBuffer[1] > lastresultsBuffer[1] + 1000) && resultsBuffer[1]>15000) {
	                alarm_ampm = (alarm_ampm+1)%2;
	            }
	            else if ((resultsBuffer[1] < lastresultsBuffer[1] - 1000) && resultsBuffer[1]<1000) {
	                alarm_ampm = (alarm_ampm+1)%2;
	            }
	            break;
    	}
    	display_change();

    	for (i=0;i<2;i++) {
    	    lastresultsBuffer[i]=resultsBuffer[i];
    	}
    }
}

void display_change() {
    char string[15];
    if (alarm_min>9) {
        sprintf(string, "%2d:%2d %cm",alarm_hour,alarm_min,alarm_ampms[alarm_ampm]);
    }
    else {
        sprintf(string, "%2d:0%d %cm", alarm_hour, alarm_min, alarm_ampms[alarm_ampm]);
    }
    Graphics_drawStringCentered(&g_sContext,(int8_t *)string,15,64,50,OPAQUE_TEXT);
}

void PORT5_IRQHandler() {
	if (P5->IFG & BIT1){ // check that it is the button interrupt flag
		P5->IFG &= ~BIT1; // clear the flag to allow for another interrupt later.
		if (play) {
            array_count = 0;
		    play = 0;
			MAP_Interrupt_disableInterrupt(INT_WDT_A);
			TIMER_A0->CTL = TIMER_A_CTL_MC_0;
        } else {
         alarm_set = 1;
         Graphics_clearDisplay(&g_sContext);
         MAP_Interrupt_enableInterrupt(INT_ADC14);
        }
	}
}

void PORT3_IRQHandler() {
	if (P3->IFG & BIT5){ // check that it is the button interrupt flag
		P3->IFG &= ~BIT5; // clear the flag to allow for another interrupt later.
        if (play) {
            array_count = 0;
		    play = 0;
			MAP_Interrupt_disableInterrupt(INT_WDT_A);
			TIMER_A0->CTL = TIMER_A_CTL_MC_0;
        } else {
            intCounter = 0;
        }
	}
}

void WDT_A_IRQHandler() {
    TIMER_A0->CCR[0] = song_freq[array_count];
    if (--temp_time[array_count]==0){ // decrement time until 0
		if (array_count == 3){ // loop song
		    for (k=0; k<4; k++) {
				temp_time[k]=song_time[k]; // reset temp array with times
			}
			array_count = 0;
		}
		else {
		array_count++; // go to next note
		}
	}
}
