/*

  Author : Vincent Jaunet


 This sketch is made for
  - Get RC from Rx receiver
  - transmit to Rpi pilot
  - Receive ESC and Servos input from Rpi Pilot

 based onrcarduino.blogspot.com

*/

// include the pinchangeint library - see the links in the related topics section above for details
#include <PinChangeInt.h>
#include <SPI.h>
#include <Servo.h>

//Define I2C variables
#define SLAVE_ADDRESS 0x11

//LED pin for checking
#define LED_PIN 13

// These bit flags are set in bUpdateFlagsShared to indicate which
// channels have new signals
#define THROTTLE_FLAG 1
#define YAW_FLAG 2
#define PITCH_FLAG 4
#define ROLL_FLAG 8

// Assign your channel in pins
#define THROTTLE_IN_PIN 8
#define YAW_IN_PIN 11
#define PITCH_IN_PIN 10
#define ROLL_IN_PIN 9


// holds the update flags defined above
volatile uint8_t bUpdateFlagsShared;

// shared variables are updated by the ISR and read by loop.
// In loop we immediatley take local copies so that the ISR can keep ownership of the
// shared ones. To access these in loop
// we first turn interrupts off with noInterrupts
// we take a copy to use in loop and the turn interrupts back on
// as quickly as possible, this ensures that we are always able to receive new signals
volatile uint16_t unThrottleInShared;
volatile uint16_t unYawInShared;
volatile uint16_t unPitchInShared;
volatile uint16_t unRollInShared;

// These are used to record the rising edge of a pulse in the calcInput functions
// They do not need to be volatile as they are only used in the ISR. If we wanted
// to refer to these in loop and the ISR then they would need to be declared volatile
uint32_t ulThrottleStart;
uint32_t ulYawStart;
uint32_t ulPitchStart;
uint32_t ulRollStart;


//servo variables
//Number of servos
#define SERVO_NUM 4
#define RC_MIN 1000

// Assign your channel out pins
#define FL_MOTOR_OUT_PIN A0
#define FR_MOTOR_OUT_PIN A1
#define BL_MOTOR_OUT_PIN A2
#define BR_MOTOR_OUT_PIN A3

//define Servo variables
Servo MOTOR[SERVO_NUM];

//define type of SPI request
uint8_t spireq = 0;
uint8_t spicount = 0;
uint8_t spimax_R = 2;
uint8_t ibyte=0;

union int_byt{
  uint8_t b[2];
  uint16_t un;
} rc_data, esc_data;



//setup function
void setup()
{
  Serial.begin(9600); //for debugging...

  pinMode(LED_PIN, OUTPUT);

  /*
    PWM measurement settings
  */

  // using the PinChangeInt library, attach the interrupts
  // used to read the channels
  PCintPort::attachInterrupt(THROTTLE_IN_PIN, calcThrottle,CHANGE);
  PCintPort::attachInterrupt(YAW_IN_PIN, calcYaw,CHANGE);
  PCintPort::attachInterrupt(PITCH_IN_PIN, calcPitch,CHANGE);
  PCintPort::attachInterrupt(ROLL_IN_PIN, calcRoll,CHANGE);

  // attach servo objects, these will generate the correct
  // pulses for driving Electronic speed controllers, servos or other devices
  // designed to interface directly with RC Receivers
  MOTOR[0].attach(FL_MOTOR_OUT_PIN);
  MOTOR[1].attach(FR_MOTOR_OUT_PIN);
  MOTOR[2].attach(BL_MOTOR_OUT_PIN);
  MOTOR[3].attach(BR_MOTOR_OUT_PIN);

  //Set servo values to min
  for (int i=0;i<SERVO_NUM;i++)
    {
      MOTOR[i].writeMicroseconds(RC_MIN);
    }

  /*
    SPI settings
  */

  //SPI.setDataMode(SPI_MODE0);

  // Declare MISO as output : have to send on
  //master in, *slave out*
  pinMode(MISO, OUTPUT);

  // turn on SPI in slave mode
  SPCR |= _BV(SPE);

  // now turn on interrupts
  SPI.attachInterrupt();


}

void loop()
{
  ////////
}

/*-----------------------
  SPI interrupt routine
------------------------*/
ISR (SPI_STC_vect)
{

  if (spireq == 0) {
    //we have nothing to send, grab a new command
    byte cmd = SPDR;
    switch (cmd) {
    case 'R': //ask for radio input and get ESC output
      spireq = 1;
      ibyte=0;

      // update rc_data
      rc_data.un = unThrottleInShared;

      //prepare for next interrupt
      SPDR = rc_data.b[ibyte+1];
    };

    return;
  }

  switch (spireq){
  case 1:
    if (spicount <= spimax_R){
      //echange values byte per byte

      //grab inputs
      esc_data.b[ibyte] = SPDR;

      //prepare for next call
      if (ibyte == 0) ibyte++;
      SPDR = rc_data.b[ibyte-1];

      spicount++;

    } else {
      //we finished send RC values
      Serial.print("Finished transaction :");
      Serial.print(esc_data.un);
      Serial.print(" ");
      Serial.println( rc_data.un);

      //prepare for next request
      SPDR = 0;
      spireq = 0;
      ibyte = 0;
    }
  }

}

/*----------------------------------
  simple interrupt service routine
  for PWM measurements
-----------------------------------*/
void calcThrottle()
{
  // if the pin is high, its a rising edge of the signal pulse, so lets record its value
  if(digitalRead(THROTTLE_IN_PIN) == HIGH)
    {
      ulThrottleStart = micros();
    }
  else
    {
      // else it must be a falling edge, so lets get the time and subtract the time of the rising edge
      // this gives use the time between the rising and falling edges i.e. the pulse duration.
      unThrottleInShared = (uint16_t)(micros() - ulThrottleStart);
    }
}

void calcYaw()
{
  if(digitalRead(YAW_IN_PIN) == HIGH)
    {
      ulYawStart = micros();
    }
  else
    {
      unYawInShared = (uint16_t)(micros() - ulYawStart);
    }
}

void calcPitch()
{
  if(digitalRead(PITCH_IN_PIN) == HIGH)
    {
      ulPitchStart = micros();
    }
  else
    {
      unPitchInShared = (uint16_t)(micros() - ulPitchStart);
    }
}

void calcRoll()
{
  if(digitalRead(ROLL_IN_PIN) == HIGH)
    {
      ulRollStart = micros();
    }
  else
    {
      unRollInShared = (uint16_t)(micros() - ulRollStart);
    }
}
