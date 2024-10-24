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
#include <stdint.h>
#include "SysTick.h"
#include "button.h"

#include "clock.h"
#include "lpit.h"
#include "tpmPwm.h"
#include "triColorLedPWM.h"
#include "main.h"

void sequenceLED();

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
bool pressedB2_ev;

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

/*----------------------------------------------------------------------------
  task2pollB2
  
  This task polls button B2. Keep this task.
*----------------------------------------------------------------------------*/
int b2State ;        // Current state - corresponds to position
int b2BounceCount ;

void initPollB2Task() {
    b2State = BOPEN ;
    pressedB2_ev = false ; 
    b1BounceCount = 0 ;
}

void pollB2Task() {
    if (b2BounceCount > 0) b2BounceCount -- ;
    switch (b2State) {
        case BOPEN:
            if (isPressed(B2)) {
                b2State = BCLOSED ;
                pressedB2_ev = true ; 
            }
          break ;

        case BCLOSED:
            if (!isPressed(B2)) {
                b2State = BBOUNCE ;
                b2BounceCount = BOUNCEDELAY ;
            }
            break ;

        case BBOUNCE:
            if (isPressed(B2)) {
                b2State = BCLOSED ;
            }
            else if (b2BounceCount == 0) {
                b2State = BOPEN ;
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

void LPIT0_IRQHandler() {
  NVIC_ClearPendingIRQ(LPIT0_IRQn);

  // check source of interrupt - LPIT0 channel 0
  if (LPIT0->MSR & LPIT_MSR_TIF0_MASK) {
     sequenceLED();
  }

//  // check source of interrupt - LPIT0 channel xxxx
//  if (LPIT0->MSR & LPIT_MSR_TIFxxxx_MASK) {
//     add code here
//  }

  // Clear all
  LPIT0->MSR = LPIT_MSR_TIF0(1) | LPIT_MSR_TIF1(1) | LPIT_MSR_TIF2(1)
             | LPIT_MSR_TIF3(1) ; // write to clear
}  

typedef enum {
    SEQUENCE_A,
    SEQUENCE_B
} sequence_pattern_t;

sequence_pattern_t sequencePattern;
uint8_t sequenceBrightness[3];

void updateLEDs() {
    setLEDBrightness(Red, sequenceBrightness[Red]);
    setLEDBrightness(Green, sequenceBrightness[Green]);
    setLEDBrightness(Blue, sequenceBrightness[Blue]);
}

void initSequenceLED() {
    sequencePattern = SEQUENCE_A;
    initSequenceLED_patternA();
}


void sequenceLED() {
    if (pressedB2_ev) {
        // sequencePattern ^= 1;
        pressedB2_ev = false;
    }

    switch (sequencePattern) {
        case SEQUENCE_A:
            sequenceLED_patternA();
            break;
        case SEQUENCE_B:
            sequenceLED_patternB();
            break;
    }
}

typedef enum {
    sequenceB_stateW,
    sequenceB_stateX,
    sequenceB_stateY,
    sequenceB_stateZ,
} sequence_t_patternB;

sequence_t_patternB sequenceState_patternB;

void initSequenceLED_patternB() {
    sequenceState_patternB = sequenceB_stateW;
    sequenceBrightness[Red] = 0;
    sequenceBrightness[Blue] = 0;
    sequenceBrightness[Green] = 0;
    updateLEDs();
}

void sequenceLED_patternB() {
    switch (sequenceState_patternB) {
        case sequenceB_stateW:
            if (sequenceBrightness[Red] >= MAXBRIGHTNESS && sequenceBrightness[Blue] >= MAXBRIGHTNESS) {
                sequenceState_patternB = sequenceB_stateX;
            } else {
                sequenceBrightness[Red] = ++sequenceBrightness[Blue];
            }
            break;
        case sequenceB_stateX:
            if (sequenceBrightness[Green] >= MAXBRIGHTNESS && sequenceBrightness[Blue] <= 0) {
                sequenceState_patternB = sequenceB_stateY;
            } else {
                ++sequenceBrightness[Green];
                --sequenceBrightness[Blue];
            }
            break;
        case sequenceB_stateY:
            if (sequenceBrightness[Blue] >= MAXBRIGHTNESS && sequenceBrightness[Red] <= 0) {
                sequenceState_patternB = sequenceB_stateZ;
            } else {
                ++sequenceBrightness[Red];
                ++sequenceBrightness[Blue];
            }
            break;
        case sequenceB_stateZ:
            if (sequenceBrightness[Blue] <= 0 && sequenceBrightness[Green] <= 0) {
                sequenceState_patternB = sequenceB_stateW;
            } else {
                sequenceBrightness[Green] = --sequenceBrightness[Blue];
            }
            break;
    }
    updateLEDs();
}

typedef enum {
    BLUE_INC,
    RED_DEC,
    GREEN_INC,
    BLUE_DEC,
    RED_INC,
    GREEN_DEC,
} sequence_t_patternA;

sequence_t_patternA sequenceState_patternA;

void initSequenceLED_patternA() {
    sequenceState_patternA = BLUE_INC;
    sequenceBrightness[Red] = MAXBRIGHTNESS;
    sequenceBrightness[Blue] = 0;
    sequenceBrightness[Green] = 0;
    updateLEDs();
}

void sequenceLED_patternA() {
    switch (sequenceState_patternA) {
            case BLUE_INC:
                if (sequenceBrightness[Blue] >= MAXBRIGHTNESS) {
                    sequenceState_patternA = RED_DEC;
                } else {
                     ++sequenceBrightness[Blue];
                }
                break;
            case RED_DEC:
                if (sequenceBrightness[Red] <= 0) {
                    sequenceState_patternA = GREEN_INC;
                } else {
                     --sequenceBrightness[Red];
                }
                break;
            case GREEN_INC:
                if (sequenceBrightness[Green] >= MAXBRIGHTNESS) {
                    sequenceState_patternA = BLUE_DEC;
                } else {
                     ++sequenceBrightness[Green];
                }
                break;
            case BLUE_DEC:
                if (sequenceBrightness[Blue] <= 0) {
                    sequenceState_patternA = RED_INC;
                } else {
                     --sequenceBrightness[Blue];
                }
                break;
            case RED_INC:
                if (sequenceBrightness[Red] >= MAXBRIGHTNESS) {
                    sequenceState_patternA = GREEN_DEC;
                } else {
                     ++sequenceBrightness[Red];
                }
                break;
            case GREEN_DEC:
                if (sequenceBrightness[Green] <= 0) {
                    sequenceState_patternA = BLUE_INC;
                } else {
                     --sequenceBrightness[Green];
                }
                break;
    }
    updateLEDs();
}

/*----------------------------------------------------------------------------
   Task: toggleRateTask

   Toggle the rate of upadtes to the LEDs on every signal

   KEEP THIS TASK, but changes may be needed
*----------------------------------------------------------------------------*/

typedef enum {
    SLOW = 375999,
    MEDIUM = 207999,
    FAST = 79999
} rate_t; // cycle time in s

int rateState ;  // this variable holds the current state

// initial state of task
void initToggleRateTask() {
    setTimer(0, SLOW) ;
    rateState = SLOW ;
}

void toggleRateTask() {
    switch (rateState) {
        case FAST:  
            if (pressedB1_ev) {                   // signal received
                pressedB1_ev = false ;            // acknowledge
                setTimer(0, MEDIUM) ;  // update PIT
                rateState = MEDIUM ;           // ... the new state
            }
            break ;

        case MEDIUM:
            if (pressedB1_ev) {                   // signal received
                pressedB1_ev = false ;            // acknowledge
                setTimer(0, SLOW) ;  // update PIT
                rateState = SLOW ;           // ... the new state
            }
            break;
            
        case SLOW:
            if (pressedB1_ev) {                   // signal received
                pressedB1_ev = false ;            // acknowledge
                setTimer(0, FAST) ;  // update PIT
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
  
    // Configure buttons
    configureButtons(B1 | B2, false);
  
    initSequenceLED();

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
    initPollB2Task();
    initToggleRateTask();
    
    // start the PIT
    startTimer(0) ;
    waitSysTickCounter(10) ;  // initialise delay counter
    while (1) {      // this runs forever
        pollB1Task() ;       // Poll button B1
        pollB2Task() ;       // Poll button B2
        toggleRateTask();    // Toggle LED update rate on every press signal
        waitSysTickCounter(10) ;  // cycle every 10 ms 
    }
}
