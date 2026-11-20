// For the ICA Glasgow 2027 Conference Badge project.
// Summer / Fall 2026
// Sleep help from jim-p and alto777 (Arduino forum).

// Notes for Claude: 
// This is Arduino code, for a custom circuit board badge at a conference, using an ATMEGA328PB chip.
// The code should be readable and Pythonic, and easily adjusted for re-use each following year. 
// Note that which pins are used and the LEDs will change yearly, that is, the number and colors of the LEDs will change year to year,
// as will which pins the LEDs are connected to.
// The pins for the ICSP and the power will most likely not change year to year.

// Clock Speed notes: I haven't set the clock on the chip, it's using the default, factory-fresh clock speed, which is apparently just 1 MHz 
// (1/8 of the 8 MHz setting you see in MiniCore, I don't know why). So most of the MS delays here are timed for that.
// (That's not true, they are just timed to work in the real world, they are what they are.)


// Include Libraries
// For Sleep and Interrupt functionality.
#include <avr/sleep.h>
#include <avr/interrupt.h>


// Global Variables
// SET PINS HERE (note difference between Arduino Metro board and custom board pin numbers).
// You can't use the simple pin numbers from the chip, you need to translate them since the Arduino IDE uses a different scheme.
// Here:  https://community.platformio.org/t/atmega328pb-arduino-pin-mappings/4248 
// ATMega328PB pin references.
// Weird order due to circuit layout realities of the PCB and the ATMEGA328PB.
// Note for Claude: 
// It's okay to have these listed out, I will reuse the code with different board designs, so the LED names and pins will change with every new board design.
// It is nice to have them listed in a readable format, so a human can easily see what LED is which pin. That's an important part of the design of the code!
const int led1 = PIN_PB6;
const int led2 = PIN_PE1;
const int led3 = PIN_PD5;
const int led4 = PIN_PB7;
const int led5 = PIN_PD7;
const int led6 = PIN_PB0;
const int led7 = PIN_PB1;
const int led8 = PIN_PB2;
const int led9 = PIN_PD6;
// PB6/PB7 also need a fuse setting (says Claude, but they work fine so far).
// PB6 and PB7 are the crystal pins. If the board is set to use an external crystal, they can't be used as I/O at all. In MiniCore he should pick Clock: Internal 8 MHz,
// then run Burn Bootloader once so the fuses actually change. Without that, LED 1 and LED 4 still won't work even with the pin numbers fixed.

// Different online schematics show different pin desginations. Beware!
const int the_button = PIN_PD3; // Button for sleep/mode, physical PULL UP RESISTOR.

// Pattern varaibles.
const int num_patterns = 11; // How many pattens do I have. Claude, it is ok to set this manually.
int pattern = 1; // 1 to num_patterns for # patterns of LED blinking. Will start at this pattern number. 
bool first_push = 1; // This is a boolean to help set the random seed (set upon first push of the button).

// The ISR SUPER IMPORTANT, MCU and schematic specific.  Updated Sept 2026, c/o Claude. 
// Interrupt Service Routine. The ISR is a software process invoked by an interrupt request from a hardware device. 
// It handles the request and sends it to the CPU, interrupting the active process. (Copied from somewhere online.)
ISR(PCINT2_vect) {
  PCICR &= ~_BV(PCIE2);
}

void setup() {
  set_unused_pins(); // Set unused pins as inputs with pull-ups enabled (prevents floating pins from drawing current).

  pinMode(led1, OUTPUT); // Set LED pins AFTER setting unused pins, otherwise the LEDs are dim, I don't know why. 
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
  pinMode(led4, OUTPUT);
  pinMode(led5, OUTPUT);
  pinMode(led6, OUTPUT);
  pinMode(led7, OUTPUT);
  pinMode(led8, OUTPUT);
  pinMode(led9, OUTPUT);

  // Ports B, C, D, and E all have internal resistors that can be set.
  pinMode(the_button, INPUT_PULLUP);  // INPUT_PULLUP if I set it like that, is that chip-internal? (I have a resistor on the board.)
      // Input pullups essentially set the default state of the pin to HIGH or 1, so that the pin is not affected by interference.
      // Do not need a resistor in this case. (I have one on the board.)

	set_sleep_mode(SLEEP_MODE_PWR_DOWN);  // Set sleep mode to full power down when it sleeps.

  ADCSRA &= ~(1 << ADEN);  // Disable the ADC, must be re-endabled if needed (not needed here).
  ACSR |= (1 << ACD);      // Disable the analog comparator, must be re-endabled if needed (not needed here).

  init_off(); // Make sure all LEDs are set to LOW to start.
} // end setup


void loop() {
  // If you change these, change var num_patterns above.
  if (pattern == 1)  blink();      // Starts on this one (unless changed above). 
  if (pattern == 2)  bounce_out();
  if (pattern == 3)  out_only();
  if (pattern == 4)  rotate();
  if (pattern == 5)  path(45);    // 45 is regular speed delay.
  if (pattern == 6)  path(20); 
  if (pattern == 7)  arms();
  if (pattern == 8)  rando(35); 
  if (pattern == 9)  rando(21); 
  if (pattern == 10) rando(7); 
  if (pattern == 11) adjacent();
} // main loop



// SLEEP FUNCTION hooray, pretty much all Claude Code at this point. 
void sleeper() {
    init_off();                 // turn off light before sleep
    wait_for_release();         // let the long press finish bouncing first
    cli();                      // disables global interrupts on AVR-based Arduino boards (such as Uno/Nano with ATmega328P). It’s an AVR macro/instruction, not a standard Arduino function.
    PCIFR   = _BV(PCIF2);
    PCMSK2 |= _BV(PCINT19);
    PCICR  |= _BV(PCIE2);
    sleep_enable();
    sleep_bod_disable();
    sei();                       // the instruction after sei always runs before any ISR -- sei() enable interrupts
    sleep_cpu();                 // sleeps here, will awaken with interrupt button push.

    sleep_disable();
    wait_for_release();          // finish the wake-up press
  } // end sleeper

void wait_for_release() {
    // Works with the sleeper() function to de-bounce.
    do { delay(20); } while (digitalRead(the_button) == LOW);
    delay(20);                   // let release bounce settle
  }


// This button code also c/o Claude Code.
bool handle_button() {
  // Called on every pass of every pattern loop.
  // Returns true when the pattern should restart from the top.
  if (digitalRead(the_button) == LOW) {   // LOW = pushed (pull-up resistor)
    return on_press();  // So, if pressed, go to the on_press() function.
  }
  return false;   // Button not pushed: keep running the pattern.
}

bool on_press() { // Button probably pressed!
  delay(10);      // Debounce: let contacts settle.
  if (digitalRead(the_button) == HIGH) {   // Already released: it was noise.
    return false;
    }  // If you get past here, the button was pressed. 

  if (first_push) {  // If first time pushed, using the global var first_push, sets the randomSeed.
    first_push = 0;
    randomSeed(micros()); 
    }

  init_off(); // Button pushed so can rest LEDs to off.

  unsigned long t0 = millis();
  while (digitalRead(the_button) == LOW) {   // While button is pushed...
    if (millis() - t0 >= 40) {               // long press = sleep
      sleeper();
      return true;  // Restart pattern cleanly on wake.
    }
  }
  delay(20);    // let release bounce settle
  increment();  // If here, was a short press, so increment the pattern number and then pop out to the main loop from the pattern you were in.
  return true;  // True restarts the (or a) pattern.
} // end on_press



void set_unused_pins() {
  // Set unused pins as inputs with pull-ups enabled (prevents floating pins from drawing current).
  // The pins here are never used in this design (ICA 2026 Scotland).
  // Top of 328PB
  pinMode(PIN_PD2, INPUT_PULLUP); // 32 ok
  pinMode(PIN_PD1, INPUT_PULLUP); // 31 ok
  pinMode(PIN_PD0, INPUT_PULLUP); // 30 ok
  pinMode(PIN_PC5, INPUT_PULLUP); // 28 ok
  pinMode(PIN_PC4, INPUT_PULLUP); // 27 ok
  pinMode(PIN_PC3, INPUT_PULLUP); // 26 ok
  pinMode(PIN_PC2, INPUT_PULLUP); // 25 ok

  // Right side of 328PB
  pinMode(PIN_PC1, INPUT_PULLUP); // 24 ok
  pinMode(PIN_PC0, INPUT_PULLUP); // 23 ok
  pinMode(PIN_PE3, INPUT_PULLUP); // 22 ok
  pinMode(PIN_PE2, INPUT_PULLUP); // 19 ok

  // Bottom of 328PB
  // These are the ISP programming pins.
  pinMode(PIN_PB3, INPUT_PULLUP); // 
  pinMode(PIN_PB4, INPUT_PULLUP); // 
  pinMode(PIN_PB5, INPUT_PULLUP); // 

  // Left side of 328PB
  pinMode(PIN_PD4, INPUT_PULLUP); // 2 ok
  pinMode(PIN_PE0, INPUT_PULLUP); // 3 ok
}


// SECONDARY FUNCTIONS (these assist the main pattern functions).

// Helper functions that set LEDs to various states.
void init_off() { 
  digitalWrite(led1, LOW);
  digitalWrite(led2, LOW);
  digitalWrite(led3, LOW);
  digitalWrite(led4, LOW);
  digitalWrite(led5, LOW);
  digitalWrite(led6, LOW);
  digitalWrite(led7, LOW);
  digitalWrite(led8, LOW);
  digitalWrite(led9, LOW);
}

void init_on() {
  digitalWrite(led1, HIGH);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, HIGH);
  digitalWrite(led4, HIGH);
  digitalWrite(led5, HIGH);
  digitalWrite(led6, HIGH);
  digitalWrite(led7, HIGH);
  digitalWrite(led8, HIGH);
  digitalWrite(led9, HIGH);
}

// For the mode/pattern button, which pattern by number.
void increment() {
  pattern++; // Pattern is a global var so it is just set, not returned.
  if (pattern > num_patterns) pattern = 1;
  init_off(); // Because if you're in this function, the mode button was pushed, so start new pattern.
}

// If the LEDs are on turn them off and vice-versa, for the blink pattern.
void reverse_all() {
  if (digitalRead(led9) == HIGH) init_off(); // If one is on, they are all on, turn all off.
  else init_on();
} // reverse_all

// Turn both LEDs in each arm on or off. 
void UR(int state) {
  if (state == 1) {
    digitalWrite(led7, HIGH);
    digitalWrite(led8, HIGH);
  }
  if (state == 0) {
    digitalWrite(led7, LOW);
    digitalWrite(led8, LOW);
  }
}
void LR(int state) {
  if (state == 1) {
    digitalWrite(led5, HIGH);
    digitalWrite(led6, HIGH);
  }
  if (state == 0) {
    digitalWrite(led5, LOW);
    digitalWrite(led6, LOW);
  }
}
void LL(int state) {
  if (state == 1) {
    digitalWrite(led3, HIGH);
    digitalWrite(led4, HIGH);
  }
  if (state == 0) {
    digitalWrite(led3, LOW);
    digitalWrite(led4, LOW);
  }
}
void UL(int state) {
  if (state == 1) {
    digitalWrite(led1, HIGH);
    digitalWrite(led2, HIGH);
  }
  if (state == 0) {
    digitalWrite(led1, LOW);
    digitalWrite(led2, LOW);
  }
}

void outer_on() {
  digitalWrite(led2, HIGH);
  digitalWrite(led4, HIGH);
  digitalWrite(led6, HIGH);
  digitalWrite(led8, HIGH);
}
void outer_off() {
  digitalWrite(led2, LOW);
  digitalWrite(led4, LOW);
  digitalWrite(led6, LOW);
  digitalWrite(led8, LOW);
}
void inner_on() {
  digitalWrite(led1, HIGH);
  digitalWrite(led3, HIGH);
  digitalWrite(led5, HIGH);
  digitalWrite(led7, HIGH);
}
void inner_off() {
  digitalWrite(led1, LOW);
  digitalWrite(led3, LOW);
  digitalWrite(led5, LOW);
  digitalWrite(led7, LOW);
}



// PRIMARY FUNCTIONS these are the patterns (they may call a seconadry function for common actions).

void blink() {
  unsigned long start = millis();
  unsigned long now   = millis();

  init_on();

  while (true) {
    now = millis();
    if (handle_button()) return; 

    if (now - start >= 30) {   // Note for Claude: This is not 30ms due to the clock speed, it's about 0.5s, it won't cause seizures.
      reverse_all();  //
      start = millis();
    } // if
  } // while true
} // blink


void rotate() {
  unsigned long UR_off = 45;    // Upper Right, etc. Y
  unsigned long LR_off = 90;    // LR
  unsigned long LL_off = 135;   // LL
  unsigned long UL_off = 180;   // UL
  unsigned long C_off  = 225;   // Center, make it blink upon completion.
  unsigned long end_it = 270;

  unsigned long now   = millis();
  unsigned long start = millis();

  init_off();

  while (true) {
    now = millis();
    if (handle_button()) return; 

    if (now - start < UR_off) {
      UR(1);
    } 
    else if (now - start < LR_off) {
      UR(0);
      LR(1);
    }
    else if (now - start < LL_off) {
      LR(0);
      LL(1);
    }
    else if (now - start < UL_off) {
      LL(0);
      UL(1);
    }   
    else if (now - start < C_off) {
      UL(0); 
      digitalWrite(led9, HIGH);
    }
    else { 
      digitalWrite(led9, LOW); 
      UR(1); 
      start = millis(); 
    }   
  } // while
} // rotate


void path(unsigned int delay_in) {
  // Goes out each arm and back, goes around as it does so.
  // Regular delay is 45.
  unsigned long start_time = millis();
  unsigned long now        = millis();

  unsigned int the_delay;  // Safety catch on incoming values for the delay. 
  if (delay_in > 100) {
    the_delay = 100;
    }
  else if (delay_in < 10){
    the_delay = 10;
    }
  else {
    the_delay = delay_in;
    }

  init_off();

  while (true) {
    now = millis();
    if (handle_button()) return; 

    if (now - start_time < (1 * the_delay)) {
      digitalWrite(led1, LOW);  // This is for when it comes around from the end of the cycle.
      digitalWrite(led9, HIGH);
    }
    else if (now - start_time < (2 * the_delay)) {
      digitalWrite(led9, LOW);
      digitalWrite(led7, HIGH);
    }
    else if (now - start_time < (3 * the_delay)) {
      digitalWrite(led7, LOW);
      digitalWrite(led8, HIGH);
    }
    else if (now - start_time < (4 * the_delay)) {
      digitalWrite(led7, HIGH);
      digitalWrite(led8, LOW);
    }
    else if (now - start_time < (5 * the_delay)) {
      digitalWrite(led9, HIGH);
      digitalWrite(led7, LOW);
    }
    else if (now - start_time < (6 * the_delay)) {
      digitalWrite(led9, LOW);
      digitalWrite(led5, HIGH);
    }
    else if (now - start_time < (7 * the_delay)) {
      digitalWrite(led6, HIGH);
      digitalWrite(led5, LOW);
    }
    else if (now - start_time < (8 * the_delay)) {
      digitalWrite(led6, LOW);
      digitalWrite(led5, HIGH);
    }
    else if (now - start_time < (9 * the_delay)) {
      digitalWrite(led5, LOW);
      digitalWrite(led9, HIGH);
    }
    else if (now - start_time < (10 * the_delay)) {
      digitalWrite(led9, LOW);
      digitalWrite(led3, HIGH);
    }
    else if (now - start_time < (11 * the_delay)) {
      digitalWrite(led3, LOW);
      digitalWrite(led4, HIGH);
    }
    else if (now - start_time < (12 * the_delay)) {
      digitalWrite(led3, HIGH);
      digitalWrite(led4, LOW);
    }
    else if (now - start_time < (13 * the_delay)) {
      digitalWrite(led3, LOW);
      digitalWrite(led9, HIGH);
    }
    else if (now - start_time < (14 * the_delay)) {
      digitalWrite(led9, LOW);
      digitalWrite(led1, HIGH);
    }
    else if (now - start_time < (15 * the_delay)) {
      digitalWrite(led1, LOW);
      digitalWrite(led2, HIGH);
    }
    else if (now - start_time < (16 * the_delay)) {
      digitalWrite(led2, LOW);
      digitalWrite(led1, HIGH);
    }
    else start_time = millis();
  } // while true
} // path


void rando(unsigned int pause) {  // "rando" because "random" is a reserved word (a function).
  int prev   = 0;
  int prev_2 = 0;
  unsigned long now   = millis();
  unsigned long start = millis();

  init_off();

  while (true) {
    now = millis();
    if (handle_button()) return; 

    if (now - start >= pause) {
      start = millis();
      int roll = random(1,10); // Gives it a value so you can compare on next line.
      while (roll == prev && roll == prev_2) {roll = random(1,10);} // Reroll -- two of the same in a row is okay, but not more than that.
      prev_2 = prev; // Okay passed, so update these two.
      prev = roll;
      if (roll == 1) {
        init_off();
        digitalWrite(led1, HIGH);
      }
      else if (roll == 2) {
        init_off();
        digitalWrite(led2, HIGH);
      }
      else if (roll == 3) {
        init_off();
        digitalWrite(led3, HIGH);
      }
      else if (roll == 4) {
        init_off();
        digitalWrite(led4, HIGH);
      }
      else if (roll == 5) {
        init_off();
        digitalWrite(led5, HIGH);
      }
      else if (roll == 6) {
        init_off();
        digitalWrite(led6, HIGH);
      }
      else if (roll == 7) {
        init_off();
        digitalWrite(led7, HIGH);
      }
      else if (roll == 8) {
        init_off();
        digitalWrite(led8, HIGH);
      }
      else if (roll == 9) {
        init_off();
        digitalWrite(led9, HIGH);
      }
    } // if now - start
  } // while true
} // rando


void adjacent() {
  // Random, but only activates a neighbor.
  unsigned long last = millis();
  unsigned long now  = millis(); 
  const int pause = 40;
  int change = 1;

  init_off();
  digitalWrite(led9, HIGH);
  int which = 9; // Which LED, 1 to 9.

  while (true) {
    now = millis();
    if (handle_button()) return; 

    if (now - last >= pause) {   // Time to move it one.
      // See which LED is on, that determines possible moves. 9 options.
      if (which == 9) {
        change = random(1,5);         // 4 options from LED 9 in the center.
        if (change == 1) {
          init_off();
          digitalWrite(led1, HIGH);
          last = millis();
          which = 1;
        }
        else if (change == 2) {
          init_off();
          digitalWrite(led7, HIGH);
          last = millis();
          which = 7;
        }
        else if (change == 3) {
          init_off();
          digitalWrite(led5, HIGH);
          last = millis();
          which = 5;
        }
        else if (change == 4) {
          init_off();
          digitalWrite(led3, HIGH);
          last = millis();
          which = 3;
        }
      } // if which == 9
      else if (which == 7) {         // Do the cases where there are only 2 choices here.
        change = random(1,3);
        if (change == 1) {
          init_off();
          digitalWrite(led8, HIGH);
          last = millis();
          which = 8;
        }
        else {
          init_off();
          digitalWrite(led9, HIGH);
          last = millis();
          which = 9;
        }      
      }
      else if (which == 5) {
        change = random(1,3);
        if (change == 1) {
          init_off();
          digitalWrite(led6, HIGH);
          last = millis();
          which = 6;
        }
        else {
          init_off();
          digitalWrite(led9, HIGH);
          last = millis();
          which = 9;
        }      
      }
      else if (which == 3) {
        change = random(1,3);
        if (change == 1) {
          init_off();
          digitalWrite(led4, HIGH);
          last = millis();
          which = 4;
        }
        else {
          init_off();
          digitalWrite(led9, HIGH);
          last = millis();
          which = 9;
        }      
      }
      else if (which == 1) {
        change = random(1,3);
        if (change == 1) {
          init_off();
          digitalWrite(led2, HIGH);
          last = millis();
          which = 2;
        }
        else {
          init_off();
          digitalWrite(led9, HIGH);
          last = millis();
          which = 9;
        }      
      }
      else if (which == 8) {          // These are the ends of the arms, only one option for change. 
          init_off();
          digitalWrite(led7, HIGH);
          last = millis();
          which = 7;
      }
      else if (which == 6) {
          init_off();
          digitalWrite(led5, HIGH);
          last = millis();
          which = 5;
      }
      else if (which == 4) {
          init_off();
          digitalWrite(led3, HIGH);
          last = millis();
          which = 3;
      }
      else if (which == 2) {
          init_off();
          digitalWrite(led1, HIGH);
          last = millis();
          which = 1;
      }
    } // if time to move it one
  } // while true
} // adjacent


void bounce_out() {
  // A pulse from in to out and back, repeat.
  unsigned long now   = millis();
  unsigned long start = millis();

  while (true) {
    now = millis();
    if (handle_button()) return; 

    if (now - start < 40) {
      init_off();
      digitalWrite(led9, HIGH);
    }
    else if (now - start < 80) {
      digitalWrite(led9, LOW);
      inner_on();
    }
    else if (now - start < 120) {
      inner_off();
      outer_on();
    }
    else if (now - start < 160) {
      inner_on();
      outer_off();
    }
    else {
      start = millis();
    }
  } // while true
} // bounce out


void out_only() {
  // A pulse from in to out.
  unsigned long now   = millis();
  unsigned long start = millis();

  while (true) {
    now = millis();
    if (handle_button()) return; 

    if (now - start < 40) {
      init_off();
      digitalWrite(led9, HIGH);
    }
    else if (now - start < 80) {
      digitalWrite(led9, LOW);
      inner_on();
    }
    else if (now - start < 150) { // Slightly longer here.
      inner_off();
      outer_on();
    }
    else if (now - start < 170) {  // Slight pause off, is done. 
      init_off();
    }
    else {
      start = millis();
    }
  } // while true
} // out only


void arms() {
  // one arm, then the next, etc.
  unsigned long now   = millis();
  unsigned long start = millis();

  while (true) {
    now = millis();
    if (handle_button()) return; 

    if (now - start < 50) {
      UL(0);
      UR(0);
      LR(0);
      LL(0);
      digitalWrite(led9, HIGH);
    }
    else if (now - start < 100) {
      digitalWrite(led7, HIGH);
    }
    else if (now - start < 150) {
      digitalWrite(led8, HIGH);
    }
    else if (now - start < 200) {
      init_off();
    }
    else if (now - start < 250) {
      digitalWrite(led9, HIGH);
    }
    else if (now - start < 300) {
      digitalWrite(led5, HIGH);
    }
    else if (now - start < 350) {
      digitalWrite(led6, HIGH);
    }
    else if (now - start < 400) {
      init_off();
    }
    else if (now - start < 450) {
      digitalWrite(led9, HIGH);
    }
    else if (now - start < 500) {
      digitalWrite(led3, HIGH);
    }
    else if (now - start < 550) {
      digitalWrite(led4, HIGH);
    }
    else if (now - start < 600) {
      init_off();
    }
    else if (now - start < 650) {
      digitalWrite(led9, HIGH);
    }
    else if (now - start < 700) {
      digitalWrite(led1, HIGH);
    }
    else if (now - start < 750) {
      digitalWrite(led2, HIGH);
    }
    else if (now - start < 800) {
      init_off();
    }
    else {
      start = millis();
    }
  } // while true
} // arms


// END OF CODE
