 
// Note that there is no #define for E0_B:.
// it's a non-standard pin, check out setupEncoder0().
#define E1_A_PIN  7
#define E1_B_PIN  23
#define E0_A_PIN  26

#define L_PWM_PIN 10
#define L_DIR_PIN 16
#define R_PWM_PIN  9
#define R_DIR_PIN 15

#define DIR_FWD LOW
#define DIR_BKD HIGH

#define MM_PER_COUNT (float)0.15

// Volatile Global variables used by Encoder ISR.
volatile long count_right_e; // used by encoder to count the rotation
volatile bool oldE1_A;       // used by encoder to remember prior state of A
volatile bool oldE1_B;       // used by encoder to remember prior state of B

volatile long count_left_e; // used by encoder to count the rotation
volatile bool oldE0_A;      // used by encoder to remember prior state of A
volatile bool oldE0_B;      // used by encoder to remember prior state of B

// Global volatile timestamps to determine
// speed from within the encoder0 ISR.
volatile unsigned long left_e_interval;
volatile unsigned long left_e_last_time;

volatile unsigned long right_e_interval;
volatile unsigned long right_e_last_time;

// Global volatile to hold the determined
// speed of wheel fixed to encoder0
float left_e_prev_speed;
volatile float left_e_speed;

float right_e_prev_speed;
volatile float right_e_speed;

float move_speed;
unsigned long total_time;

// put your setup code here, to run once:
void setup() {

  setupMotors();
  
  // These two function set up the pin
  // change interrupts for the encoders. 
  setupLeftEncoder();
  setupRightEncoder();
  
  total_time = millis();
  
  left_e_last_time = micros();
  left_e_interval = 0.0;

  left_e_prev_speed = 0.0;
  left_e_speed = 0.0;

  right_e_last_time = micros();
  right_e_interval = 0.0;

  right_e_prev_speed = 0.0;
  right_e_speed = 0.0;

  move_speed = 200.0;
  
  analogWrite( L_PWM_PIN, move_speed);
  analogWrite( R_PWM_PIN, move_speed );

  // Initialise the Serial communication
  // so that we can inspect the values of
  // our encoder using the Monitor.
  Serial.begin( 9600 );
  Serial.println(" ***Reset*** ");
}

// put your main code here, to run repeatedly:
void loop() {
    
  // Print out the determined speed for
  // wheel on encoder 0.
//  Serial.print("left speed: ");
  Serial.print( left_e_speed );
  Serial.print(",");
  Serial.println( right_e_speed );

  // Recorded speed goes down to 0  
  if (left_e_prev_speed == left_e_speed) {
    left_e_speed = 0;
  } else {
    left_e_prev_speed = left_e_speed;
  }

  if (right_e_prev_speed == right_e_speed) {
    right_e_speed = 0;
  } else {
    right_e_prev_speed = right_e_speed;
  }

  unsigned long total_elapsed_time = millis() - total_time; 
  if (total_elapsed_time > 3000) {
    updateSpeed(0);
  }
  
  delay(2);
  
}

void reset() {
  updateSpeed(0);
  count_left_e = 0;
  count_right_e = 0;
}

void rotate() {
  digitalWrite( L_DIR_PIN, DIR_FWD  );
  digitalWrite( R_DIR_PIN, DIR_BKD  );
  updateSpeed(move_speed);
 
}

void move_forwards() {
  digitalWrite( L_DIR_PIN, DIR_FWD  );
  digitalWrite( R_DIR_PIN, DIR_FWD  );
  updateSpeed(move_speed);
}


void updateSpeed(byte new_speed) {
  analogWrite( L_PWM_PIN, new_speed );
  analogWrite( R_PWM_PIN, new_speed );
}

// This ISR handles just Encoder 1
// ISR to read the Encoder1 Channel A and B pins
// and then look up based on  transition what kind of
// rotation must have occured.
ISR( INT6_vect ) {
  // First, Read in the new state of the encoder pins.
  // Standard pins, so standard read functions.
  boolean newE1_B = digitalRead( E1_B_PIN );
  boolean newE1_A = digitalRead( E1_A_PIN );

  // Some clever electronics combines the
  // signals and this XOR restores the
  // true value.
  newE1_A ^= newE1_B;

  // Create a bitwise representation of our states
  // We do this by shifting the boolean value up by
  // the appropriate number of bits, as per our table
  // header:
  //
  // State :  (bit3)  (bit2)  (bit1)  (bit0)
  // State :  New A,  New B,  Old A,  Old B.
  byte state = 0;
  state = state | ( newE1_A  << 3 );
  state = state | ( newE1_B  << 2 );
  state = state | ( oldE1_A  << 1 );
  state = state | ( oldE1_B  << 0 );


  // This is an inefficient way of determining
  // the direction.  However it illustrates well
  // against the lecture slides.
  switch ( state ) {
    case 0:                   break; // No movement.
    case 1:  count_right_e--; break;  // clockwise?
    case 2:  count_right_e++; break;  // anti-clockwise?
    case 3:                   break;  // Invalid
    case 4:  count_right_e++; break;  // anti-clockwise?
    case 5:                   break;  // No movement.
    case 6:                   break;  // Invalid
    case 7:  count_right_e--; break;  // clockwise?
    case 8:  count_right_e--; break;  // clockwise?
    case 9:                   break;  // Invalid
    case 10:                  break;  // No movement.
    case 11: count_right_e++; break;  // anti-clockwise?
    case 12:                  break;  // Invalid
    case 13: count_right_e++; break;  // anti-clockwise?
    case 14: count_right_e--; break;  // clockwise?
    case 15:                  break;  // No movement.
  }

  // Save current state as old state for next call.
  oldE1_A = newE1_A;
  oldE1_B = newE1_B;

  // Calculate speed in mm/s
  right_e_interval = micros() - right_e_last_time;
  float ms_interval = (float)right_e_interval/1000;
  right_e_speed = MM_PER_COUNT/ms_interval;

  right_e_last_time = micros();

}


// This ISR handles just Encoder 0
// ISR to read the Encoder0 Channel A and B pins
// and then look up based on  transition what kind of
// rotation must have occured.
ISR( PCINT0_vect ) {

  // First, Read in the new state of the encoder pins.
  // Mask for a specific pin from the port.
  // Non-standard pin, so we access the register
  // directly.  
  // Reading just PINE would give us a number
  // composed of all 8 bits.  We want only bit 2.
  // B00000100 masks out all but bit 2
  boolean newE0_B = PINE & (1<<PINE2);
  //boolean newE0_B = PINE & B00000100;  // Does same as above.

  // Standard read fro the other pin.
  boolean newE0_A = digitalRead( E0_A_PIN ); // 26 the same as A8

  // Some clever electronics combines the
  // signals and this XOR restores the 
  // true value.
  newE0_A ^= newE0_B;
  
  // Create a bitwise representation of our states
  // We do this by shifting the boolean value up by
  // the appropriate number of bits, as per our table
  // header:
  //
  // State :  (bit3)  (bit2)  (bit1)  (bit0)
  // State :  New A,  New B,  Old A,  Old B.
  byte state = 0;                   
  state = state | ( newE0_A  << 3 );
  state = state | ( newE0_B  << 2 );
  state = state | ( oldE0_A  << 1 );
  state = state | ( oldE0_B  << 0 );

  // This is an inefficient way of determining
  // the direction.  However it illustrates well
  // against the lecture slides.  
  switch ( state ) {
    case 0:                  break; // No movement.
    case 1:  count_left_e--; break;  // clockwise?
    case 2:  count_left_e++; break;  // anti-clockwise?
    case 3:                  break;  // Invalid
    case 4:  count_left_e++; break;  // anti-clockwise?
    case 5:                  break;  // No movement.
    case 6:                  break;  // Invalid
    case 7:  count_left_e--; break;  // clockwise?
    case 8:  count_left_e--; break;  // clockwise?
    case 9:                  break;  // Invalid
    case 10:                 break;  // No movement.
    case 11: count_left_e++; break;  // anti-clockwise?
    case 12:                 break;  // Invalid
    case 13: count_left_e++; break;  // anti-clockwise?
    case 14: count_left_e--; break;  // clockwise?
    case 15:                 break;  // No movement.
  }
     
  // Save current state as old state for next call.
  oldE0_A = newE0_A;
  oldE0_B = newE0_B; 

  // Calculate speed in mm/s
  left_e_interval = micros() - left_e_last_time;
  float ms_interval = (float)left_e_interval/1000;
  left_e_speed = MM_PER_COUNT/ms_interval;

  left_e_last_time = micros();
}

/*
   This setup routine enables interrupts for
   encoder1.  The interrupt is automatically
   triggered when one of the encoder pin changes.
   This is really convenient!  It means we don't
   have to check the encoder manually.
*/
void setupRightEncoder() {

  // Initialise our count value to 0.
  count_right_e = 0;

  // Initialise the prior A & B signals
  // to zero, we don't know what they were.
  oldE1_A = 0;
  oldE1_B = 0;

  // Setup pins for encoder 1
  pinMode( E1_A_PIN, INPUT );
  pinMode( E1_B_PIN, INPUT );

  // Now to set up PE6 as an external interupt (INT6), which means it can
  // have its own dedicated ISR vector INT6_vector

  // Page 90, 11.1.3 External Interrupt Mask Register – EIMSK
  // Disable external interrupts for INT6 first
  // Set INT6 bit low, preserve other bits
  EIMSK = EIMSK & ~(1<<INT6);
  //EIMSK = EIMSK & B1011111; // Same as above.
  
  // Page 89, 11.1.2 External Interrupt Control Register B – EICRB
  // Used to set up INT6 interrupt
  EICRB |= ( 1 << ISC60 );  // using header file names, push 1 to bit ISC60
  //EICRB |= B00010000; // does same as above

  // Page 90, 11.1.4 External Interrupt Flag Register – EIFR
  // Setting a 1 in bit 6 (INTF6) clears the interrupt flag.
  EIFR |= ( 1 << INTF6 );
  //EIFR |= B01000000;  // same as above

  // Now that we have set INT6 interrupt up, we can enable
  // the interrupt to happen
  // Page 90, 11.1.3 External Interrupt Mask Register – EIMSK
  // Disable external interrupts for INT6 first
  // Set INT6 bit high, preserve other bits
  EIMSK |= ( 1 << INT6 );
  //EIMSK |= B01000000; // Same as above

}

void setupLeftEncoder() {

    // Initialise our count value to 0.
    count_left_e = 0;

    // Initialise the prior A & B signals
    // to zero, we don't know what they were.
    oldE0_A = 0;
    oldE0_B = 0;

    // Setting up E0_PIN_B:
    // The Romi board uses the pin PE2 (port E, pin 2) which is
    // very unconventional.  It doesn't have a standard
    // arduino alias (like d6, or a5, for example).
    // We set it up here with direct register access
    // Writing a 0 to a DDR sets as input
    // DDRE = Data Direction Register (Port)E
    // We want pin PE2, which means bit 2 (counting from 0)
    // PE Register bits [ 7  6  5  4  3  2  1  0 ]
    // Binary mask      [ 1  1  1  1  1  0  1  1 ]
    //    
    // By performing an & here, the 0 sets low, all 1's preserve
    // any previous state.
    DDRE = DDRE & ~(1<<DDE6);
    //DDRE = DDRE & B11111011; // Same as above. 

    // We need to enable the pull up resistor for the pin
    // To do this, once a pin is set to input (as above)
    // You write a 1 to the bit in the output register
    PORTE = PORTE | (1<< PORTE2 );
    //PORTE = PORTE | 0B00000100;

    // Encoder0 uses conventional pin 26
    pinMode( E0_A_PIN, INPUT );
    digitalWrite( E0_A_PIN, HIGH ); // Encoder 0 xor

    // Enable pin-change interrupt on A8 (PB4) for encoder0, and disable other
    // pin-change interrupts.
    // Note, this register will normally create an interrupt a change to any pins
    // on the port, but we use PCMSK0 to set it only for PCINT4 which is A8 (PB4)
    // When we set these registers, the compiler will now look for a routine called
    // ISR( PCINT0_vect ) when it detects a change on the pin.  PCINT0 seems like a
    // mismatch to PCINT4, however there is only the one vector servicing a change
    // to all PCINT0->7 pins.
    // See Manual 11.1.5 Pin Change Interrupt Control Register - PCICR
    
    // Page 91, 11.1.5, Pin Change Interrupt Control Register 
    // Disable interrupt first
    PCICR = PCICR & ~( 1 << PCIE0 );
    // PCICR &= B11111110;  // Same as above
    
    // 11.1.7 Pin Change Mask Register 0 – PCMSK0
    PCMSK0 |= (1 << PCINT4);
    
    // Page 91, 11.1.6 Pin Change Interrupt Flag Register – PCIFR
    PCIFR |= (1 << PCIF0);  // Clear its interrupt flag by writing a 1.

    // Enable
    PCICR |= (1 << PCIE0);
}

void setupMotors() {
  pinMode( L_PWM_PIN, OUTPUT );
  pinMode( L_DIR_PIN, OUTPUT );
  pinMode( R_PWM_PIN, OUTPUT );
  pinMode( R_DIR_PIN, OUTPUT );

  // Set initial direction for l and r wheels
  digitalWrite( L_DIR_PIN, DIR_FWD  );
  digitalWrite( R_DIR_PIN, DIR_FWD );
}
