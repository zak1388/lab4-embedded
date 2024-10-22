/* ------------------------------------------
       ECS642/ECS714 Lab4

   The green LED is displayed at different brightness levels using PWM
   The PIT is used to time the transition between the brightness levels
   A button press switches between two rates (by changing the PIT load value): 
       * a fast one cycles through all the brightness levels in 2 s
       * a slow one takes 10 s
  -------------------------------------------- */

#include <MKL28Z7.h>
#include <stdbool.h>
#include "../inc/SysTick.h"
#include "../inc/button.h"

#include "../inc/clock.h"
#include "../inc/lpit.h"
#include "../inc/TPMPWM.h"
#include "../inc/triColorLedPWM.h"


/* --------------------------------------
     Documentation
     =============
     This is a cyclic system with a cycle time of 10ms

     The file has a main function, two tasks
       1. pollB1Task: this polls shield button B1
       2. toggleRateTask: this toggles between a fast and slow rate for changing the LED brightness
     and the PIT interrupt service routine which changes the brightness of 
     one of the LEDs
 -------------------------------------- */
 
/* --------------------------------------------
  Variables for communication
*----------------------------------------------------------------------------*/
bool pressedB1_ev ;  // set by task1 (polling) and cleared by task 2

/*----------------------------------------------------------------------------
  task1pollB1
  
  This task polls button B1. Keep this task.
*----------------------------------------------------------------------------*/
int b1State ;        // Current state - corresponds to position
int b1BounceCount ;

void initPollB1Task() {
    b1State = BOPEN ;
    pressedB1_ev = false ; 
    b1BounceCount = 0 ;
}

void pollB1Task() {
    if (b1BounceCount > 0) b1BounceCount -- ;
    switch (b1State) {
        case BOPEN:
            if (isPressed(B1)) {
                b1State = BCLOSED ;
                pressedB1_ev = true ; 
            }
          break ;

        case BCLOSED:
            if (!isPressed(B1)) {
                b1State = BBOUNCE ;
                b1BounceCount = BOUNCEDELAY ;
            }
            break ;

        case BBOUNCE:
            if (isPressed(B1)) {
                b1State = BCLOSED ;
            }
            else if (b1BounceCount == 0) {
                b1State = BOPEN ;
            }
            break ;
    }                
}

/* -------------------------------------
    Programmable Interrupt Timer (PIT) interrupt handler

      Check each channel to see if caused interrupt
      Write 1 to TIF to reset interrupt flag

    Changes the LED brightness. KEEP THIS ISR but changes are needed
   ------------------------------------- */

// PIT load values
// The larger the count, the lower the frequency of interrupts
const uint32_t pitSlowCount = PITCLOCK * 10 / 32 ; // all 32 levels in 10 s
const uint32_t pitFastCount = PITCLOCK * 2 / 32 ; // all 32 levels in 2 s

// Brightness level
unsigned int bright = 0 ;  // the current brightness

void LPIT0_IRQHandler() {
  NVIC_ClearPendingIRQ(LPIT0_IRQn);

  // check source of interrupt - LPIT0 channel 0
  if (LPIT0->MSR & LPIT_MSR_TIF0_MASK) {
     bright = (bright + 1) % (MAXBRIGHTNESS + 1);        
     setLEDBrightness(Green, bright) ;
  }

//  // check source of interrupt - LPIT0 channel xxxx
//  if (LPIT0->MSR & LPIT_MSR_TIFxxxx_MASK) {
//     add code here
//  }

  // Clear all
  LPIT0->MSR = LPIT_MSR_TIF0(1) | LPIT_MSR_TIF1(1) | LPIT_MSR_TIF2(1)
             | LPIT_MSR_TIF3(1) ; // write to clear
}  

/*----------------------------------------------------------------------------
   Task: toggleRateTask

   Toggle the rate of upadtes to the LEDs on every signal

   KEEP THIS TASK, but changes may be needed
*----------------------------------------------------------------------------*/

#define FAST 0
#define SLOW 1

int rateState ;  // this variable holds the current state

// initial state of task
void initToggleRateTask() {
    setTimer(0, pitSlowCount) ;
    rateState = SLOW ;
}

void toggleRateTask() {
    switch (rateState) {
        case FAST:  
            if (pressedB1_ev) {                   // signal received
                pressedB1_ev = false ;            // acknowledge
                setTimer(0, pitSlowCount) ;  // update PIT
                rateState = SLOW ;           // ... the new state
            }
            break ;
            
        case SLOW:
            if (pressedB1_ev) {                   // signal received
                pressedB1_ev = false ;            // acknowledge
                setTimer(0, pitFastCount) ;  // update PIT
                rateState = FAST ;           // ... the new state
            }
            break ;
  }
}

/*----------------------------------------------------------------------------
  MAIN function
 *----------------------------------------------------------------------------*/
int main (void) {
    // enable the peripheralclock
    enablePeripheralClock() ;

    // Configure pin multiplexing
    configureLEDforPWM() ;            // Configure LEDs for PWM control
  
    // Configure button B1
    configureButtons(B1, false) ; // ConfigureButtons B1 for polling
  
    // Configure LPIT0 channel 0 to generate interrupts
    configureLPIT_interrupt(0) ;

    // Configure TPM
    configureTPMClock() ;    // clocks to all TPM modules
    configureTPM0forPWM() ;  // configure PWM on TPM0 (all LEDs)
   
    Init_SysTick(1000) ;  // initialse SysTick every 1 ms

    // start everything
    setLEDBrightness(Red, 0) ;
    setLEDBrightness(Green, 0) ;
    setLEDBrightness(Blue, 0) ;

    initPollB1Task() ;       // initialise task state
    initToggleRateTask() ;   // initialise task state
    
    // start the PIT
    startTimer(0) ;
    waitSysTickCounter(10) ;  // initialise delay counter
    while (1) {      // this runs forever
        pollB1Task() ;       // Poll button B1
        toggleRateTask();    // Toggle LED update rate on every press signal
        // delay
        waitSysTickCounter(10) ;  // cycle every 10 ms 
    }
}
