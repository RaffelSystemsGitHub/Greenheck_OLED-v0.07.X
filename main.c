/**
  Generated Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    Greenheck HOA controller, utilizing SSD1306 0.96" OLED 128x64 display. Peripheral communication is I2C and non-volatile memory is
    High-Endurance Flash memory.
 * 
 *  -Buffer size was reduced to fit in program memory, disables the bottom right corner of the OLED display.
 *  -Needs optimization and debugging for consistent button presses. Code is too bloated.
 
  Description:
    This header file provides implementations for driver APIs for all modules selected in the GUI.
    Generation Information :
        Product Revision  :  PIC10 / PIC12 / PIC16 / PIC18 MCUs - 1.81.6
        Device            :  PIC16F1933
        Driver Version    :  2.00

*/

/*
    (c) 2018 Microchip Technology Inc. and its subsidiaries. 
    
    Subject to your compliance with these terms, you may use Microchip software and any 
    derivatives exclusively with Microchip products. It is your responsibility to comply with third party 
    license terms applicable to your use of third party software (including open source software) that 
    may accompany Microchip software.
    
    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER 
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY 
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS 
    FOR A PARTICULAR PURPOSE.
    
    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND 
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP 
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO 
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL 
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT 
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS 
    SOFTWARE.
*/

#include "mcc_generated_files/mcc.h"
#include <xc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "HEFlash.h" //
#include "I2C.h"     //Non-standard files. 
#include "OLED.h"    //



#define DRY_INPUT       RA3    //Input pin active on continuity.
#define SPEED_INPUT     AN9    //ADC pin for external auto speed reference.
#define MODE_BUTTON     (!RB0)    //Input pin to toggle mode.
#define INCREASE_BUTTON (!RB1)    //Input pin to increase speed reference.
#define DECREASE_BUTTON (!RB2)    //Input pin to decrease speed reference.
#define WET_INPUT       RB4    //Input pin photocoupled to 12-120V. 
#define FIREMAN_INPUT   RB5    //Input pin photocoupled to 12-120V.

#define MOTOR_OUT    DAC1OUT1   //Output to motor control input, may be incorrect definition.
#define AUX_CONTACT  RA0        //Output to close power relay.
#define RUN_STATUS   RA1        //Output to close solid-state switch.
#define POWER_LED    RA6        //Output to POWER_LED

#define MODE                0   //Address for mode state (HAND/AUTO/OFF).
#define HAND_SPEED          1   //Address for hand speed reference.
#define FIREMAN_SPEED       2   //Address for fireman speed reference.
#define AUTO_LOCAL_SPEED    3   //Address for auto local speed reference

#define HAND          0   //       
#define OFF           1   //Toggle mode states.       
#define AUTO_LOCAL    2   //
#define AUTO_REMOTE   3   //
#define FIREMAN_SET   4   //Fireman "mode" state. Only set by ISR and used to display fireman speed reference.

#define INCREASE_SPEED          0b0100
#define DECREASE_SPEED          0b0010
#define TOGGLE_MODE             0b0001 
#define SET_FIREMAN_SPEED       0b0110
#define FACTORY_RESET           0b0011


#define AUX_HIGH                1.9
#define AUX_THRESHOLD           1.85
#define AUX_LOW                 1.8
#define RUN_STATUS_HIGH         2.05
#define RUN_STATUS_THRESHOLD    2.0
#define RUN_STATUS_LOW          1.95

#define FIREMAN_SET_TIMEOUT 30000   //15 sec timeout with TMR0 set for 200us interrupt.
#define FACTORY_RESET_DELAY 3000    //Button hold time for factory reset.
#define FIREMAN_SET_DELAY   3000    //button hold time for fireman set
#define DEBOUNCE_CNT        20   //Tune debounce sensitivity.
#define SPEED_1_TIME        300



volatile int decrement = DEBOUNCE_CNT;
volatile int increase_btn_debounce = DEBOUNCE_CNT;
volatile int decrease_btn_debounce = DEBOUNCE_CNT;
volatile int mode_btn_debounce = DEBOUNCE_CNT;
volatile int factory_reset_dec = FACTORY_RESET_DELAY;
volatile int fireman_inc = FIREMAN_SET_TIMEOUT;

bool mode_change_flag = 0;
bool fireman_set = 0;
bool fireman_out = 0;


volatile bool ext_ref_flag = 0;          //Remote enable status changed, update "Enable/Disable" text.
volatile bool dry_state = 0;             //Last state of dry input, updated at Timer0 overflow.
volatile bool wet_state = 0;             //Last state of wet input, updated at Timer0 overflow.
bool factory_reset_enable = 0;
bool freeze_mode = 0;
bool press = 0;                 //Button debounce flag.
static char buttons = 0;               //State of all buttons.
static char last_buttons = 0;               //State of all buttons.

char btn_count = 0;
char updateAutoRemoteDelay = 0;

unsigned char mode = 0;        //OFF == 0/HAND == 1/AUTO_LOCAL == 2/AUTO_REMOTE == 3.
unsigned char speed = 50;       //Range from 0-10 volts. (DAC range 0-100)
unsigned char frmn_speed = 100;   //Range from 0-10 volts. (DAC range 0-100)
unsigned short ext_speed = 0;    //Range from 0-10 volts. (Range 0-1024)
unsigned char  speedChangeState = 0;
unsigned int speedChangeTimer = 0;
unsigned int fireman_set_debounce = FIREMAN_SET_DELAY;
void clearText(char*);

void clearText(char* textToClear){
    for(int i = 0; i < TEXT_ARRAY_SIZE; i++){
        textToClear[i] = ' ';
    }
}


void main(void)
{   
    // initialize the device
    SYSTEM_Initialize();
    
    // When using interrupts, you need to set the Global and Peripheral Interrupt Enable bits
    // Use the following macros to:

    // Enable the Global Interrupts
    INTERRUPT_GlobalInterruptEnable();

    // Enable the Peripheral Interrupts
    INTERRUPT_PeripheralInterruptEnable();

    // Disable the Global Interrupts
    //INTERRUPT_GlobalInterruptDisable();

    // Disable the Peripheral Interrupts
    //INTERRUPT_PeripheralInterruptDisable();
    
    __delay_ms(100); //Increased start-up time.
    HEFLASH_readBlock(&mode, MODE, sizeof(mode)); //Read in mode from settings. 
    HEFLASH_readBlock(&speed, HAND_SPEED, sizeof(speed)); //Read in hand speed from settings. 
    HEFLASH_readBlock(&frmn_speed, FIREMAN_SPEED, sizeof(frmn_speed)); //Read in fireman speed from settings.
    if(speed > 100){
        speed = 50;
        HEFLASH_writeBlock(HAND_SPEED, &speed, sizeof(speed));
        HEFLASH_writeBlock(AUTO_LOCAL_SPEED, &speed, sizeof(speed));
    }
    if(frmn_speed > 100){
        frmn_speed = 100;
        HEFLASH_writeBlock(FIREMAN_SPEED, &frmn_speed, sizeof(frmn_speed));
    }
    
    OLED_Init();
    __delay_ms(100);
    
    
    while (1)
    {      
        //Fireman override loop.
        if(FIREMAN_INPUT == 0)
        {
            fireman_out = 1;

            HEFLASH_readBlock(&frmn_speed, FIREMAN_SPEED, sizeof(frmn_speed)); //Read fireman_speed state from memory.


            clearText(newTextLine1);
            sprintf(newTextLine1,"FIREMAN");

            clearText(newTextLine2);

            clearText(newTextLine3);

            clearText(newTextLine4);
            sprintf(newTextLine4,"SET:%d.%dV", frmn_speed/10, (frmn_speed%10));

            //Output speed control via DAC1.
            DAC1_Load10bitInputData((float)frmn_speed *((float)1000/104)); //Scaled based on variable range of 0-100.
            
            if((float)frmn_speed/10 > AUX_THRESHOLD) //ext_speed range: 0-1023 / speed range : 0-100 
            {
                //AUX_CONTACT closes, set ports.
                AUX_CONTACT = 1;
            }
            else//ext_speed range: 0-1023 / speed range : 0-100 
            {
                //AUX_CONTACT opens, set ports.
                AUX_CONTACT = 0;
            }
            if(frmn_speed/10 >= RUN_STATUS_THRESHOLD) //ext_speed range: 0-1023 / speed range : 0-100 
            {
                //RUN_STATUS provides continuity, set ports.
                RUN_STATUS = 1;
            }
            else//ext_speed range: 0-1023 / speed range : 0-100 
            {
                //RUN_STATUS removes continuity, set ports.
                RUN_STATUS = 0;
            }
            
            UpdateScreen();
            unsigned int power_led_flash_counter = 0;
            while(FIREMAN_INPUT == 0){  //wait until FIREMAN mode turned off
                if(power_led_flash_counter){
                    power_led_flash_counter--;
                }else{
                    power_led_flash_counter = 1000;
                }
                if(power_led_flash_counter>500){
                    POWER_LED = 0;
                }else{
                    POWER_LED = 1;
                }
                __delay_ms(1);
            }
        }
        POWER_LED = 1;
        fireman_out = 0;

        clearText(newTextLine1);
        clearText(newTextLine2);
        clearText(newTextLine3);
        clearText(newTextLine4);
        
        
        //Screen and Output Updates
        switch(mode) 
        {
            case OFF :
                
                //Pull down SPEED_OUTPUT and display.
  
                sprintf(newTextLine1,"OFF");
                
                //Output speed control via DAC1.
                DAC1_Load10bitInputData(0); //Set to zero/off.
    
                //AUX_CONTACT goes low, set ports.
                AUX_CONTACT = 0;
                //RUN_STATUS_THRESHOLD goes low, set ports.
                RUN_STATUS = 0;
                                
            break;
            
            case HAND :

                HEFLASH_readBlock(&speed, HAND_SPEED, sizeof(speed)); //Read speed state from memory.
                
                //LINE 1
                sprintf(newTextLine1,"HAND");
                
                //LINE 4
                sprintf(newTextLine4,"SET:%d.%dV", speed/10, (speed%10));
                
                //Output speed control via DAC1.
                DAC1_Load10bitInputData((float)speed *((float)1000/104)); //Scaled based on variable range of 0-100.
                
                if((float)speed/10 > AUX_THRESHOLD) //ext_speed range: 0-1023 / speed range : 0-100 
                {
                    //AUX_CONTACT closes, set ports.
                    AUX_CONTACT = 1;
                }
                else    //ext_speed range: 0-1023 / speed range : 0-100 
                {
                    //AUX_CONTACT opens, set ports.
                    AUX_CONTACT = 0;
                }

                if(speed/10 >= RUN_STATUS_THRESHOLD) //ext_speed range: 0-1023 / speed range : 0-100 
                {
                    //RUN_STATUS provides continuity, set ports.
                    RUN_STATUS = 1;
                }
                else    //ext_speed range: 0-1023 / speed range : 0-100 
                {
                    //RUN_STATUS provides continuity, set ports.
                    RUN_STATUS = 0;
                }
                
            break;
            
            case AUTO_LOCAL :
     
                HEFLASH_readBlock(&speed, AUTO_LOCAL_SPEED, sizeof(speed)); //Read speed state from memory.  
                //LINE 1
                sprintf(newTextLine1,"AUTO LOCAL");
  
                //LINE 4
                sprintf(newTextLine4,"SET:%d.%dV", speed/10, (speed%10));
 
                if( (WET_INPUT == 0) || (DRY_INPUT == 1) )                                                                                                                                                                                                                                                
                {
                    //Read AUTO LOCAL speed setting from memory and display.
                    //LINE 2
                    sprintf(newTextLine2,"Enabled");   

                    //Output speed control via DAC1.
                    DAC1_Load10bitInputData((float)speed  *((float)1000/104)); //Scaled based on variable range of 0-100.

                    if((float)speed/10 > AUX_THRESHOLD) //ext_speed range: 0-1023 / speed range : 0-100 
                    {
                        //AUX_CONTACT closes, set ports.
                        AUX_CONTACT = 1;
                    }
                    else    //ext_speed range: 0-1023 / speed range : 0-100 
                    {
                        //AUX_CONTACT opens, set ports.
                        AUX_CONTACT = 0;
                    }

                    if(speed/10 >= RUN_STATUS_THRESHOLD) //ext_speed range: 0-1023 / speed range : 0-100 
                    {
                        //RUN_STATUS provides continuity, set ports.
                        RUN_STATUS = 1;
                    }
                    else    //ext_speed range: 0-1023 / speed range : 0-100 
                    {
                        //RUN_STATUS provides continuity, set ports.
                        RUN_STATUS = 0;
                    }
                }
                else
                {
                    //LINE 2
                    sprintf(newTextLine2,"Disabled");

                    //Output speed control via DAC1.
                    DAC1_Load10bitInputData(0); //Set to zero/off.

                    //AUX_CONTACT goes low, set ports.
                    AUX_CONTACT = 0;
                    //RUN_STATUS_THRESHOLD goes low, set ports.
                    RUN_STATUS = 0;
                    
                }
                        
            break;    
            
            case AUTO_REMOTE :
                ext_speed = (((unsigned int)ADRESH<<8) + (unsigned int)ADRESL);  //For right-justified setting. Hardware range limited from 0-1000.
                ext_speed = (float)ext_speed*1.05;
                unsigned int integer = ext_speed*10/1000;
                unsigned int decimal = ((unsigned long)ext_speed*100/1000) % 10;  
                
                //LINE 1
                sprintf(newTextLine1,"AUTO REMOTE");                     
                
                //LINE 3
                sprintf(newTextLine3," ");
                    
                //LINE 4
                //only update displayed voltage periodically to prevent frequent screen updates from interfering with button press timing.
                if(!updateAutoRemoteDelay){ 
                    updateAutoRemoteDelay = DEBOUNCE_CNT; 
                    sprintf(newTextLine4,"READ:%d.%dV", (unsigned int)integer, (unsigned int)decimal);
                }else{
                    //Keep previous text if text is not being updated
                    for(int i = 0; i < TEXT_ARRAY_SIZE;i++){
                        newTextLine4[i] = textLine4[i];
                    }
                }
                
                if( (WET_INPUT == 0) || (DRY_INPUT == 1) ){ //Read AUTO speed setting from ADC channel AN9 and display,
                    
                    //LINE 2
                    sprintf(newTextLine2,"Enabled");
                    
                    //Output speed control via DAC1.
                    DAC1_Load10bitInputData(((float)ext_speed/1.05) /** (1024/1000)*/); //Scaled based on hardware input range of 0-10008.
                    
                    //Hysteresis added using checks against AUX_HIGH and AUX_LOW to prevent relay chatter near threshold
                    //This is only needed for AUTO_REMOTE because the remote voltage control is analog and may have noise
                    if((float)ext_speed/100 > AUX_HIGH) //ext_speed range: 0-1023 / speed range : 0-100 
                    {
                        //AUX_CONTACT closes, set ports.
                        AUX_CONTACT = 1;
                    }
                    else if ((float)ext_speed/100 < AUX_LOW)//ext_speed range: 0-1023 / speed range : 0-100 
                    {
                        //AUX_CONTACT opens, set ports.
                        AUX_CONTACT = 0;
                    }
               
                    //Hysteresis added using checks against HIGH and LOW thresholds to prevent OPTO from turning on and off repeatedly near threshold
                    //This is only needed for AUTO_REMOTE because the remote voltage control is analog and may have noise
                    if((float)ext_speed/100 > RUN_STATUS_HIGH) //ext_speed range: 0-1023 / speed range : 0-100 
                    {
                        //RUN_STATUS provides continuity, set ports.
                        RUN_STATUS = 1;
                    }
                    else if((float)ext_speed/100 < RUN_STATUS_LOW) //ext_speed range: 0-1023 / speed range : 0-100 
                    {
                        //RUN_STATUS provides continuity, set ports.
                        RUN_STATUS = 0;
                    }
                }
                else
                {   
                    //LINE 2
                    sprintf(newTextLine2,"Disabled");

                    //Output speed control via DAC1.
                    DAC1_Load10bitInputData(0); //Set to zero/off.

                    //AUX_CONTACT goes low, set ports.
                    AUX_CONTACT = 0;
                    //RUN_STATUS_THRESHOLD goes low, set ports.
                    RUN_STATUS = 0;
                }    
            break;
                                   
            case FIREMAN_SET :

                HEFLASH_readBlock(&frmn_speed, FIREMAN_SPEED, sizeof(frmn_speed)); //Read speed state from memory. 

                sprintf(newTextLine1,"FIREMANSET");

                sprintf(newTextLine4,"SET:%d.%dV", frmn_speed/10, (frmn_speed%10));
                
                //Output speed control via DAC1.
                DAC1_Load10bitInputData((float)frmn_speed *((float)1000/104)); //Scaled based on variable range of 0-100.

                if((float)frmn_speed/10 > AUX_THRESHOLD) //ext_speed range: 0-1023 / speed range : 0-100 
                {
                    //AUX_CONTACT closes, set ports.
                    AUX_CONTACT = 1;
                }
                else    //ext_speed range: 0-1023 / speed range : 0-100 
                {
                    //AUX_CONTACT opens, set ports.
                    AUX_CONTACT = 0;
                }

                if(frmn_speed/10 >= RUN_STATUS_THRESHOLD) //ext_speed range: 0-1023 / speed range : 0-100 
                {
                    //RUN_STATUS provides continuity, set ports.
                    RUN_STATUS = 1;
                }
                else    //ext_speed range: 0-1023 / speed range : 0-100 
                {
                    //RUN_STATUS provides continuity, set ports.
                    RUN_STATUS = 0;
                }

            break;
            
            default :
                
                //Initialize mode.
                sprintf(newTextLine1,"Press Mode");

                //Output speed control via DAC1.
                DAC1_Load10bitInputData(0); //Set to zero/off.
                
                //AUX_CONTACT goes low, set ports.
                AUX_CONTACT = 0;
                //RUN_STATUS_THRESHOLD goes low, set ports.
                RUN_STATUS = 0;
                    
            break;
        } 
        
        UpdateScreen();
        
        btn_count = 0;
        
        if(INCREASE_BUTTON){
            btn_count++;
            if(increase_btn_debounce){
                increase_btn_debounce--;
            }
        }else{
            increase_btn_debounce = DEBOUNCE_CNT;
        }
        
        if(DECREASE_BUTTON){
            btn_count++;
            if(decrease_btn_debounce){
                decrease_btn_debounce--;
            }
        }else{
            decrease_btn_debounce = DEBOUNCE_CNT;
        }
        
        if(MODE_BUTTON){
            btn_count++;
            if(mode_btn_debounce){
                mode_btn_debounce--;
            }
        }else{
            mode_btn_debounce = DEBOUNCE_CNT;
        }
        
        //Buttons Inputs
        if(btn_count==1){
            if(decrement){
                decrement--;
            }else{
                if(!increase_btn_debounce){                             //if increase button is pressed
                    if((FIREMAN_INPUT == 1) && (DECREASE_BUTTON != 1)){ //ignore press if fireman input or decrease button are active
                        if(!speedChangeTimer){                          //check whether speed is ready to increment
                            if(speedChangeState<4){                     //make the first few increments happen slowly
                                speedChangeState++;
                                speedChangeTimer = SPEED_1_TIME;
                            }
                            //Increase based on state and updated the speed in memory
                            if(fireman_set){
                                if(frmn_speed < 100){                
                                    frmn_speed += 1;
                                    HEFLASH_writeBlock(FIREMAN_SPEED, &frmn_speed, sizeof(frmn_speed)); //Write speed state to memory.
                                }                  
                            }else{   
                                if((speed < 100)){ 
                                    if(mode == HAND){
                                        speed += 1;
                                        HEFLASH_writeBlock(HAND_SPEED, &speed, sizeof(speed)); //Write speed state to memory.
                                    }else if(mode==AUTO_LOCAL){
                                        speed += 1;
                                        HEFLASH_writeBlock(AUTO_LOCAL_SPEED, &speed, sizeof(speed)); //Write speed state to memory.
                                    }
                                }
                            }
                        }
                    } 
                }
                if(!decrease_btn_debounce){                                //see increase_button comments
                    if((FIREMAN_INPUT == 1) && (INCREASE_BUTTON != 1)){
                        if(!speedChangeTimer){
                            if(speedChangeState<4){
                                speedChangeState++;
                                speedChangeTimer = SPEED_1_TIME;
                            }
                            if(fireman_set)
                            {
                                if(frmn_speed > 0)
                                {
                                    frmn_speed -= 1;
                                    HEFLASH_writeBlock(FIREMAN_SPEED, &frmn_speed, sizeof(frmn_speed)); //Write speed state to memory.
                                }
                            }
                            else
                            {
                                if((speed > 0)){
                                    if(mode == HAND){
                                        speed -= 1;
                                        HEFLASH_writeBlock(HAND_SPEED, &speed, sizeof(speed)); //Write speed state to memory.
                                    }else if(mode==AUTO_LOCAL){
                                        speed -= 1;
                                        HEFLASH_writeBlock(AUTO_LOCAL_SPEED, &speed, sizeof(speed)); //Write speed state to memory.
                                    }
                                }
                            }
                        }
                    }
                }
                if(!mode_btn_debounce){
                    if(!mode_change_flag){  //only change mode once per button press
                        mode_change_flag = 1;
                        if(fireman_set){        //kick out of fireman_set without changing the mode by ending the fireman_set timeout.
                            fireman_inc = 0;
                        }else{                        
                            if((INCREASE_BUTTON != 1) && (DECREASE_BUTTON != 1)){
                                //Toggle mode.
                                if(mode == FIREMAN_SET){
                                    //Reset fireman speed reference adjustment state.
                                    fireman_set = 0;            
                                    HEFLASH_readBlock(&mode, MODE, sizeof(mode)); //Read in mode from settings.
                                }
                                if(mode < 3)
                                {
                                    mode++;
                                    HEFLASH_writeBlock(MODE, &mode, sizeof(mode)); //Write current mode state to memory.
                                }
                                else
                                {
                                    mode = 0;
                                    HEFLASH_writeBlock(MODE, &mode, sizeof(mode)); //Write current mode state to memory.
                                }
                            } 
                        }
                    }
                }
            }
        }else{
            decrement = DEBOUNCE_CNT;
        }
        
        if(updateAutoRemoteDelay){
            updateAutoRemoteDelay--;
        }
        
        if(!fireman_inc){
            //Reset fireman speed reference adjustment state.
            fireman_set = 0;
            HEFLASH_readBlock(&mode, MODE, sizeof(mode)); //Read in mode from settings.
        }

        if(!factory_reset_dec){
            fireman_set = 0;
            fireman_inc = 0;

            ext_ref_flag = 0;          //Remote enable status changed, update "Enable/Disable" text.
            dry_state = 0;             //Last state of dry input, updated at Timer0 overflow.
            wet_state = 0;             //Last state of wet input, updated at Timer0 overflow.
            press = 0;                 //Button debounce flag.

            mode = 5;        //OFF == 0/HAND == 1/AUTO_LOCAL == 2/AUTO_REMOTE == 3/FACTORY_RESET == 5
            speed = 50;       //Range from 0-10 volts. (DAC range 0-100)
            frmn_speed = 100;   //Range from 0-10 volts. (DAC range 0-100)
            ext_speed = 0;    //Range from 0-10 volts. (Range 0-1024)

            HEFLASH_writeBlock(MODE, &mode, sizeof(mode)); //Initialize speed in memory.
            HEFLASH_writeBlock(HAND_SPEED, &speed, sizeof(speed)); //Initialize hand speed in memory.
            HEFLASH_writeBlock(AUTO_LOCAL_SPEED, &speed, sizeof(speed)); //Initialize auto local speed in memory.
            HEFLASH_writeBlock(FIREMAN_SPEED, &frmn_speed, sizeof(frmn_speed)); //Initialize speed in memory.
        }
    }
}

void __interrupt() __ISR(void){   
    
    if(INTCONbits.TMR0IE == 1 && INTCONbits.TMR0IF == 1)
    {
        
        INTCONbits.TMR0IF = 0;  

        //Check to see if remote input changed, change line two text if true.
        if((WET_INPUT != wet_state)||(DRY_INPUT != dry_state)){
            ext_ref_flag = 1;
        }
        dry_state = DRY_INPUT; //Record new current dry_input.
        wet_state = WET_INPUT; //Record new current wet_input.
        
        
        if(fireman_set){
            if(fireman_inc){
                fireman_inc--;
            }
        }
        
        if(mode_btn_debounce && (!(increase_btn_debounce || decrease_btn_debounce))){
            if(fireman_set_debounce){
                fireman_set_debounce--;
                if(!fireman_set_debounce){
                    fireman_inc = FIREMAN_SET_TIMEOUT;
                    fireman_set = 1;
                    HEFLASH_writeBlock(MODE, &mode, sizeof(mode)); //Write current mode state to memory.
                    mode = FIREMAN_SET; //Set display to show fireman speed reference adjustment. 
                }
            }
        }else{
            fireman_set_debounce = FIREMAN_SET_DELAY;
        }
        
        if(increase_btn_debounce && !(decrease_btn_debounce || mode_btn_debounce)){
            if(factory_reset_dec){
                factory_reset_dec--;
            }
        }
        else{
            factory_reset_dec = FACTORY_RESET_DELAY;
        }
        if(increase_btn_debounce && decrease_btn_debounce && mode_btn_debounce){
            speedChangeTimer = 0;
            speedChangeState = 0;
            mode_change_flag = 0;
        }
        
        if(speedChangeTimer){
            speedChangeTimer--;
        }
    }
}

/*
 End of File
*/