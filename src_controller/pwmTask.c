/*
 * pwm.c
 *
 *  Created on: 1/08/2019
 *      Author: lbr63
 *
 *  Uses a modified version of pwnGen.c, by P.J. Bones UCECE
 *
 */
#include <stdint.h>
#include <stdbool.h>
#include "stdio.h"
#include "stdlib.h"

#include <priorities.h>
#include "uart.h"

#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/adc.h"
#include "driverlib/pwm.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/interrupt.h"
#include "driverlib/debug.h"
#include "driverlib/pin_map.h"

#include "utils/ustdlib.h"
#include "circBufT.h"
#include "pwmTask.h"
#include "sharedConstants.h"


#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"




//*****************************************************************************
//
// The stack size for the PWM task.
//
//*****************************************************************************
#define PWMTASKSTACKSIZE        128         // Stack size in words

// FreeRTOS structures.
extern xSemaphoreHandle g_pUARTMutex;
extern xQueueHandle g_pwmWriteQueue;
extern OperatingData_t g_programStatus;



//*****************************************************************************
// This task handles PWM for the helirig,  ...     ...
//*****************************************************************************
static void
pwmTask(void *pvParameters)
{

    portTickType ui16LastTime;
    uint32_t ui32PollDelay = 25;

    // Get the current tick count.
    ui16LastTime = xTaskGetTickCount ();


    xSemaphoreTake(g_pUARTMutex, BLOCK_TIME_MAX);
    UARTprintf("PWMTask starting.\n");
    xSemaphoreGive(g_pUARTMutex);

    //
    // Loop forever
    while(1) {

        // Modes: idle, calibrate, landed, flying, landing

        if (g_programStatus.mode == flying || g_programStatus.mode == landing) {
            PWMOutputState (PWM_MAIN_BASE, PWM_MAIN_OUTBIT, true);

            // Get values from program status
            uint32_t mainMotorPWMDuty = g_programStatus.mainMotorPWMDuty;

            //
            // Set duty cycle of main rotor
            setDutyCycle(mainMotorPWMDuty, MAIN_ROTOR);

        } else {
            PWMOutputState (PWM_MAIN_BASE, PWM_MAIN_OUTBIT, false);
        }

		// Wait for the required amount of time.
		vTaskDelayUntil (&ui16LastTime, ui32PollDelay / portTICK_RATE_MS);
    }
}


//*****************************************************************************
// Initializes the PWM task.
//*****************************************************************************
uint32_t
pwmTaskInit(void)
{
    //
    // Initialize PWM things
    initPWM();

    //
    // Create the PWM task.
    if(xTaskCreate(pwmTask, (const portCHAR *)"PWM",
                   PWMTASKSTACKSIZE, NULL, tskIDLE_PRIORITY +
                   PRIORITY_PWM_TASK, NULL) != pdTRUE)
    {
        return(1);
    }

    UARTprintf("PWM task initialized.\n");

    //
    // Success.
    return(0);


}


//*****************************************************************************
// Initialisation functions for the PWM. Initialises module 0 and module 1.
// Using the constants defined in pwmTask.h
//*****************************************************************************
void
initPWM (void)
{

    // Set the PWM clock rate (using the prescaler)
    SysCtlPWMClockSet(PWM_DIVIDER_CODE);

    // Initialise main rotor
    SysCtlPeripheralEnable(PWM_MAIN_PERIPH_PWM);
    SysCtlPeripheralEnable(PWM_MAIN_PERIPH_GPIO);

    GPIOPinConfigure(PWM_MAIN_GPIO_CONFIG);
    GPIOPinTypePWM(PWM_MAIN_GPIO_BASE, PWM_MAIN_GPIO_PIN);

    PWMGenConfigure(PWM_MAIN_BASE, PWM_MAIN_GEN,
                    PWM_GEN_MODE_UP_DOWN | PWM_GEN_MODE_NO_SYNC);

    // Set the initial PWM parameters
    // PWMGenPeriodSet(PWM_MAIN_BASE, PWM_MAIN_GEN, SysCtlClockGet() / PWM_DIVIDER / PWM_FIXED_RATE_HZ);

    PWMGenEnable(PWM_MAIN_BASE, PWM_MAIN_GEN);

    PWMOutputState(PWM_MAIN_BASE, PWM_MAIN_OUTBIT, true);

}


//*****************************************************************
//  Function to set the duty cycle of the main, or secondary rotor
// ****************************************************************
void
setDutyCycle (uint32_t ui32Duty, uint8_t rotor)
{

    // Calculate the PWM period corresponding to the freq.
    uint32_t ui32Period =
    SysCtlClockGet() / PWM_DIVIDER / PWM_FIXED_RATE_HZ;

    PWMGenPeriodSet(PWM_MAIN_BASE, PWM_MAIN_GEN, ui32Period);

    PWMPulseWidthSet(PWM_MAIN_BASE, PWM_MAIN_OUTNUM,
    ui32Period * ui32Duty / 100);
}