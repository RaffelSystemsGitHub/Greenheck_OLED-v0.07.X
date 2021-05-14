/*
 * @file i2x.c
 * @author xpress_embedo
 * @date 1 Feb, 2020
 * 
 * @brief This file contains I2C drivers for PIC18F micro-controller
 *
 */

#include "I2C.h"
#include <xc.h>
#include <pic16f1778.h>
#include <stdint.h>
#include "mcc_generated_files/mcc.h"
/* Function Definition */


/**
 * @brief I2C Initialization
 * Call this function as follow
 * @code
 * I2C_Init(I2C_STANDARD_SPEED);
 * @endcode
 * @param speed Parameter to control I2C Speed
 */
void I2C_Init( uint8_t speed )
{
  /*
  Serial Clock(SCL) - RB1
  Serial Data(SDA)  - RB0
  Configure these pins as Input
  */
  TRISC |= 0b00011000; //RC3 and RC4 set as inputs.
  /* Slew Rate Disabled for Standard Speed & Enabled for High Speed */
  if( speed == I2C_HIGH_SPEED )
  {
    SSP1STAT |= 0x80; 
  }
  else
  {
    SSP1STAT &= ~0x80; //0b0111 1111 
  }
  /*
   * WCOL:0
   * SSPOV:0
   * SSPEN:1 -> Enables Serial Port & configures the SDA and SCL as serial port pins
   * CKP:0
   * SSPM3:SSPM0:1000 -> I2C Master Mode, clock=FOSC/(4*(SSPADD+1))
   */
  SSP1CON1 = 0b00101000; 
  SSP1ADD = 7u; //If 32MHz Fosc, 1MHz I2C clock.  - Alex L.
  SSP1CON3 &= 0b11110111;
}

/**
 * @brief I2C Start Signal
 */
void I2C_Start( void )
{
  /* Initiate Start Condition on SDA and SCL Lines */
  SSP1CON2bits.SEN = 1;
  /* Wait till start condition is over, this bit is automatically cleared by HW */
  while (SSP1CON2bits.SEN == 1){  
    continue;
  }
}

/**
 * @breif I2C Re-Start Signal
 */
void I2C_Restart( void )
{
  /* Initiate Re-start Condition on SDA and SCL Lines */
  SSP1CON2bits.RSEN = 1;
  /* Wait till start condition is over, this bit is automatically cleared by HW */
  while (SSP1CON2bits.RSEN == 1)
    continue;
}

/**
 * @breif I2C Stop Signal
 */
void I2C_Stop(void)
{
  /* Initiate Stop condition on SDA & SCL Lines*/
  SSP1CON2bits.PEN = 1;
  while(SSP1CON2bits.PEN==1)  //HERE
    continue;
}

/**
 * @breif I2C Wait Signal
 */
void I2C_Wait(void)
{
  /* Wait till transmission is in progress */
  while( SSP1STATbits.R_nW == 1 )
    continue;
  /* If ACKSTAT bit is 0 Acknowledgment Received Successfully else not*/
  if( SSP1CON2bits.ACKSTAT == 1 )
  {
    I2C_Stop();
  }
}

/**
 * @breif I2C Send Data
 * @param data data to sent over I2C Bus
 */
void I2C_Send( uint8_t data )
{
  SSP1BUF = data;            /* Move data to SSPBUF */
  while(SSP1STATbits.BF);    /* wait till complete data is sent from buffer */
  I2C_Wait();               /* wait for any pending transfer */
}

/**
 * @breif I2C Receive Data
 * @return Data read from the I2C Bus
 */
uint8_t I2C_Read( void )
{
  uint8_t temp;
  SSP1CON2bits.RCEN = 1;         /* Enable data reception */
  while(SSP1STATbits.BF == 0)    /* wait for buffer full */
    continue;
  temp = SSP1BUF;                /* Read serial buffer and store in temp register */
  I2C_Wait();                   /* wait to check any pending transfer */
  SSP1CON2bits.ACKDT=1;			/* send not acknowledge */
  SSP1CON2bits.ACKEN=1;
  while(SSP1CON2bits.ACKEN == 1) 
    continue;
  //I2C_Stop();
  return temp;                  /* Return the read data from bus */
}
