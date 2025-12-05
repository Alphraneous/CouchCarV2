#include <XBOXONE.h>

// Satisfy the IDE for includes, but do add to actual compilation target
#ifdef _USING_ARDUINO_IDE
#   include <spi4teensy3.h>
#endif
#include <SPI.h>

#include <utility>

// Set this value to true if you want to see the positions of the buttons and joysticks
#define _ENABLE_MODE_DEBUG true

namespace USBLibrary {
    extern USB     usbDriver  {};
    extern XBOXONE xboxDriver {&usbDriver};
}

struct XboxInputScheme {

    XboxInputScheme() {
        leftHat = { 
            USBLibrary::xboxDriver.getAnalogHat(AnalogHatEnum::LeftHatX),
            USBLibrary::xboxDriver.getAnalogHat(AnalogHatEnum::LeftHatY),
        };

        rightHat = {
            USBLibrary::xboxDriver.getAnalogHat(AnalogHatEnum::RightHatX),
            USBLibrary::xboxDriver.getAnalogHat(AnalogHatEnum::RightHatY),
        };

        leftTrigger = USBLibrary::xboxDriver.getButtonPress(AnalogHatEnum::LT);
        rightTrigger = USBLibrary::xboxDriver.getButtonPress(AnalogHatEnum::RT);
    }

    std::pair<int16_t, int16_t> leftHat;
    std::pair<int16_t, int16_t> rightHat;
    
    uint16_t leftTrigger;
    uint16_t rightTrigger;

    bool xToggle;
    bool yToggle;
    bool aToggle;
    bool bToggle;
    bool leftBbumperToggle;
    bool rightBumperToggle;
};

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

// Runs once on startup
void setup() 
{
    constexpr uint16_t baudeRate = 115200;
    Serial.begin(baudeRate);
    
    while(!Serial); // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
    
    if(USBLibrary::usbDriver.Init() == -1) {
        Serial.print(F("\r\nOSC did not start"));
        while(1); // Halt
    }

    Serial.print(F("\r\nXBOX ONE USB Library Started"));
}

void loop() 
{
    USBLibrary::usbDriver.Task();
    if!(USBLibrary::xboxDriver.XboxOneConnected)
        return;

    int16_t LHX = USBLibrary::xboxDriver.getAnalogHat(LeftHatX); //Integer from -32768 to 32767
    int16_t LHY = USBLibrary::xboxDriver.getAnalogHat(LeftHatY); //Integer from -32768 to 32767

    int16_t RHX = USBLibrary::xboxDriver.getAnalogHat(RightHatX); //Integer from -32768 to 32767
    int16_t RHY = USBLibrary::xboxDriver.getAnalogHat(RightHatY); //Integer from -32768 to 32767

    /*
    Important note for the joysticks: 
    Due to something called stick drift, when the joysticks are at rest (not being touched), their value isn't zero, but rather some number roughly around 1500 to 3000
    For our purposes, we can consider any value between -7500 and 7500 to be "zero".
    That is, when the position of the joystick (either x or y) is in this range, the joystick can be considered to be at rest and not doing anything
    Make sure to keep this in mind as you're developing your control code.
    */

    uint16_t LTR = USBLibrary::xboxDriver.getButtonPress(LT); //Integer from 0 to 1023
    uint16_t RTR = USBLibrary::xboxDriver.getButtonPress(RT); //Integer from 0 to 1023

    //The letter buttons are already mapped to toggle variables
    //Pressing any of these buttons toggles their corresponding variable between false and true (off and on)
    //Useful for toggling lights, etc

    if (USBLibrary::xboxDriver.getButtonClick(A)) {
        toggleA = !toggleA;
    }
    if (USBLibrary::xboxDriver.getButtonClick(B)) {
        toggleB = !toggleB;
    }
    if (USBLibrary::xboxDriver.getButtonClick(X)) {
        toggleX = !toggleX;
    }
    if (USBLibrary::xboxDriver.getButtonClick(Y)) {
        toggleY = !toggleY;
    }

    //Unmapped buttons (LeftButton and RightButton)
    //Turn signals maybe?
    //If so, you need to use toggle variables similar to the letter buttons but make sure that only one turn signal is on at a time
    //Also, you need to 
    if (USBLibrary::xboxDriver.getButtonClick(LB)) {

    }
        
    if (USBLibrary::xboxDriver.getButtonClick(RB)) {

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

    delay(2);


    // Print debug information
#if _ENABLE_MODE_DEBUG
    Serial.print(F("LX: "));
    Serial.print(LHX);
    Serial.print(F(" RX: "));
    Serial.print(RHX);
    Serial.print(F(" LT: "));
    Serial.print(LTR);
    Serial.print(F(" RT: "));
    Serial.print(RTR);
    Serial.print(F(" toggle Y: "));
    Serial.print(toggleY);
    Serial.print(F(" toggle B: "));
    Serial.print(toggleB);
    Serial.print(F(" toggle A: "));
    Serial.print(toggleA);
    Serial.print(F(" toggle X: "));
    Serial.print(toggleX);
    Serial.println();
#endif 
}
