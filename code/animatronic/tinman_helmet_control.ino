/**
 * Realtime Arduino Animatronic Controller (6 channel, PWM version)
 * 
 * This Arduino script uses the "FastRCReader" library to read PWM input channels from a RC receiver such as the Flysky FS-iA6,
 * It then applies various logic to these inputs based on a chosen animatronic model (tentacle, eye, etc.) and assigns corresponding
 * positions of an array of servo motors via a PCA9685 PWM controller.
 */

// DEFINES
// For efficient, non-blocking reading of the PWM inputs, the FastRCReader uses pin change interrupts
// Arduino pins are grouped into 3 "ports" that share a pin change interrupt, so you need to set the 
// following PORTS_TO_USE value to specify which group of pins are used for the RC channel inputs
// PORTS_TO_USE 1 for pins D8-D13
// PORTS_TO_USE 2 for pins A0-A5
// PORTS_TO_USE 3 for pins D0-D7
#define PORTS_TO_USE 3

// INCLUDES
// Wire library used for I2C communication
#include <Wire.h>
// PCA9685 16-channel PWM controller
// See: https://github.com/NachtRaveVL/PCA9685-Arduino
#include "src/PCA9685/PCA9685.h"
// Helper library for reading PPM signals from RC receiver
// See: https://github.com/timoxd7/FastRCReader
#include "src/FastRCReader/FastRCReader.h"

// CONSTANTS
// I'm using a FS-IA6 6-channel receiver
const int numInputChannels = 4;
// Each channel will be assigned a unique pin in the range defined by PORTS_TO_USE
uint8_t channelPins[numInputChannels] = {7,6,5,4}; //D7(channel 1), D6(channel 2), D5(channel 3), D4(channel 4), D3(channel 5), & D2(channel 6).
// This is the number of output channels (i.e. servo motors) to be controlled by the script
// Note that the logic applied by the script means that there is not a 1:1 mapping between inputs and outputs
// so this number can be greater or less than the number of inputs 
const byte numOutputChannels = 16
;

// Define some common animatronic behaviour types
enum Behaviour {
  Eye, // One eye with both x & y movement. Upper and lower eyelids that open and close.
  TinMan, // Two eyes with both x & y movement. Two sets of eyelids that open and close. Two eye brows that rotate.
};

// GLOBALS
PCA9685 pwmController;
PCA9685_ServoEvaluator pwmServo;
FastRCReader RC;
Behaviour behaviour = Behaviour::TinMan;

// Array of input channel values received from FS-iA6
int channelInput[numInputChannels];
// Array of output channel values to send to PCA9685
int channelOutput[numOutputChannels];

// This function processes inputs and calculates the appropriate outputs
// based on the selected model
void ApplyLogic(){
  // Behaviour depends on the chosen model
  switch(behaviour) {

        case Behaviour::Eye: {
     // The ON period of almost all RC pulses ranges from 1000us to 2000us.
      // We'll remap this to an angle
      int eyeX = map(channelInput[0], 1000, 2000, -30, 30);
      channelOutput[0] = pwmServo.pwmForAngle(eyeX);
      int eyeY = map(channelInput[1], 1000, 2000, -25, 25);
      channelOutput[1] = pwmServo.pwmForAngle(eyeY);
    
      int eyesWideOpen = map(channelInput[2], 1000, 2000, -10, 10);
      int topEyelid = eyesWideOpen + eyeY / 2;
      channelOutput[2] = pwmServo.pwmForAngle(topEyelid);
      int bottomEyelid = eyesWideOpen - eyeY / 2;
      channelOutput[3] = pwmServo.pwmForAngle(bottomEyelid);
      }
      break;
       
        case Behaviour::TinMan: {
     // The ON period of almost all RC pulses ranges from 1000us to 2000us.
      // We'll remap this to an angle 
      
      //Right and Left Eye X Movement (Channel 1 & Outputs 0 & 8)
      int eyeXright = map(channelInput[0], 1000, 2000, -50, 50);
      channelOutput[8] = pwmServo.pwmForAngle(eyeXright);
      int eyeXleft = map(channelInput[0], 1000, 2000, -50, 50);
      channelOutput[0] = pwmServo.pwmForAngle(eyeXleft);

      //Right and Left Eye Y Movement (Channel 2 & Outputs 1 & 9)
      int eyeYright = map(channelInput[1], 1000, 2000, -30, 30);
      channelOutput[9] = pwmServo.pwmForAngle(eyeYright);
      int eyeYleft = map(channelInput[1], 1000, 2000, 30, -30);
      channelOutput[1] = pwmServo.pwmForAngle(eyeYleft);
      
      //Wide open for both eyes
      int eyesWideOpentopR = map(channelInput[2], 1000, 2000, -45, 25);
      int eyesWideOpenbottomR = map(channelInput[2], 1000, 2000, 25, -65);//Bottom Lid Closes,Bottome Lid Opens
      int eyesWideOpentopL = map(channelInput[2], 1000, 2000, 45, -45);//Top Lid
      int eyesWideOpenbottomL = map(channelInput[2], 1000, 2000, -65, 25);//Bottom Lid Opens,Bottom Lid Closes

      //Top Eye Lid movement for both eyes
      int topEyelidright = eyesWideOpentopR;
      channelOutput[10] = pwmServo.pwmForAngle(topEyelidright);
      int topEyelidleft = eyesWideOpentopL;
      channelOutput[2] = pwmServo.pwmForAngle(topEyelidleft);

      //Bottom Eye Lid movement for both eyes
      int bottomEyelidright = eyesWideOpenbottomR;
      channelOutput[11] = pwmServo.pwmForAngle(bottomEyelidright);
      int bottomEyelidleft = eyesWideOpenbottomL;
      channelOutput[3] = pwmServo.pwmForAngle(bottomEyelidleft);

      //Eye Brow for both eyes
      int eyeBright = map(channelInput[3],1000,2000, -30, 30);
      channelOutput[12] = pwmServo.pwmForAngle(eyeBright);
      int eyeBleft = map(channelInput[3],1000,2000, 30, -30);
      channelOutput[4] = pwmServo.pwmForAngle(eyeBleft);
      }
      break;
   
    default: 
      break;
  }
}


void setup() {
  // Initialise the serial connection (used for monitoring for Arduino IDE Serial Monitor)
  Serial.begin(9600);

  // Start the interface for the RC receiver
  RC.begin();
  RC.addChannel(channelPins, numInputChannels);

  // Initialise I2C interface used for the PCA9685 PWM controller
  Wire.begin();
  // Supported baud rates are 100kHz, 400kHz, and 1000kHz
  Wire.setClock(400000);
  // Initialise PCA9685
  pwmController.resetDevices();
  pwmController.init();
  // 50Hz provides 20ms standard servo phase length
  pwmController.setPWMFrequency(50);  
}

void loop() {
  // Read all the current channel input values into the input array
  for(int i=0; i<numInputChannels; i++) {
    channelInput[i] = RC.getFreq(channelPins[i]);
      Serial.print(channelInput[i]);
      Serial.print(",");
  }
  Serial.println();
  
  // Now perform whatever logic is necessary to convert from the array of input channels
  // into the array of output values
  ApplyLogic();

  // Now, send the output values to the PWM servo controller
  for(int i=0; i<numOutputChannels; i++) {
    pwmController.setChannelPWM(i, channelOutput[i]);
  }
}
