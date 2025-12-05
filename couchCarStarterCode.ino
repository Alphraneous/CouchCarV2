#include <XBOXONE.h>

// Satisfy the IDE, which needs to see the include statment in the ino too.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif
#include <SPI.h>

USB Usb;
XBOXONE Xbox(&Usb);

//Set this value to true if you want to see the positions of the buttons and joysticks
bool debug = false;

//Toggle variables
bool toggleA = false;
bool toggleB = false;
bool toggleX = false;
bool toggleY = false;
bool toggleLB = false;
bool toggleRB = false;

//Goal variables
int MotorL = 0;
int MotorR = 0;
int Brake = 0;
bool Lights = false;
bool BrakeLights = false;
bool TurnL = false;
bool TurnR = false;


//DON'T MESS WITH THIS CODE!!
//Runs once on startup
void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
  if (Usb.Init() == -1) {
    Serial.print(F("\r\nOSC did not start"));
    while (1); //halt
  }
  Serial.print(F("\r\nXBOX ONE USB Library Started"));
}

//Runs forever
void loop() {
  Usb.Task();
  if (Xbox.XboxOneConnected) {
    int16_t LHX = Xbox.getAnalogHat(LeftHatX); //Integer from -32768 to 32767
    int16_t LHY = Xbox.getAnalogHat(LeftHatY); //Integer from -32768 to 32767

    int16_t RHX = Xbox.getAnalogHat(RightHatX); //Integer from -32768 to 32767
    int16_t RHY = Xbox.getAnalogHat(RightHatY); //Integer from -32768 to 32767

    /*
    Important note for the joysticks: 
    Due to something called stick drift, when the joysticks are at rest (not being touched), their value isn't zero, but rather some number roughly around 1500 to 3000
    For our purposes, we can consider any value between -7500 and 7500 to be "zero".
    That is, when the position of the joystick (either x or y) is in this range, the joystick can be considered to be at rest and not doing anything
    Make sure to keep this in mind as you're developing your control code.
    */

    uint16_t LTR = Xbox.getButtonPress(LT); //Integer from 0 to 1023
    uint16_t RTR = Xbox.getButtonPress(RT); //Integer from 0 to 1023
    
    //The letter buttons are already mapped to toggle variables
    //Pressing any of these buttons toggles their corresponding variable between false and true (off and on)
    //Useful for toggling lights, etc

    if (Xbox.getButtonClick(A)) {
      toggleA = !toggleA;
    }
    if (Xbox.getButtonClick(B)) {
      toggleB = !toggleB;
    }
    if (Xbox.getButtonClick(X)) {
      toggleX = !toggleX;
    }
    if (Xbox.getButtonClick(Y)) {
      toggleY = !toggleY;
    }

    //Unmapped buttons (LeftButton and RightButton)
    //Turn signals maybe?
    //If so, you need to use toggle variables similar to the letter buttons but make sure that only one turn signal is on at a time
    //Also, you need to 
    if (Xbox.getButtonClick(LB)) {

    }
      
    if (Xbox.getButtonClick(RB)) {

    }

    //Add additional code here

    /*
    YOUR OBJECTIVE: change these variables based on the controller inputs and the tank drive control scheme. 
    These are the raw outputs that determine the electrical signals sent to each device, and you have to account for this.
    For example, if TurnL is true, that means the left turn signal light is on (solid, not blinking). It's YOUR job to make sure that you make
    some sort of loop or something that makes TurnL go true,false,true,false,true,false, etc so it blinks.
    Make sure the variables always stay in the bounds defined below.
    */
    MotorL = 0;           //0-100
    MotorR = 0;           //0-100
    Brake = 0;            //0-100
    Lights = false;       //true or false
    BrakeLights = false;  //true or false
    TurnL = false;        //true or false
    TurnR = false;        //true or false



    //DO NOT EDIT BELOW THIS LINE!!
    //THIS STUFF IS FOR DEBUGGING
    if(debug) {
      Serial.print(F("LX: "));
      Serial.print(LHX);
      Serial.print(F(" RX: "));
      Serial.print(RHX);
      Serial.print(F(" LT: "));
      Serial.print(LTR);
      Serial.print(F(" RT: "));
      Serial.print(RTR);
      Serial.print(F(" RT: "));
      Serial.print(RTR);
      Serial.print(F(" tY:"));
      Serial.print(toggleY);
      Serial.print(F(" tB:"));
      Serial.print(toggleB);
      Serial.print(F(" tA:"));
      Serial.print(toggleA);
      Serial.print(F(" tX:"));
      Serial.print(toggleX);
      Serial.println();
    }
  }
  delay(2);
}
