#include <XBOXONE.h>

// Satisfy the IDE for includes, but do add to actual compilation target
#ifdef _USING_ARDUINO_IDE
#   include <spi4teensy3.h>
#endif
#include <SPI.h>

// Set this value to true if you want to see the positions of the buttons and joysticks
#define _ENABLE_MODE_DEBUG false
#define _ENABLE_MODE_DEBUG2 true

namespace USBLibrary {
    extern USB     usbDriver  {};
    extern XBOXONE xboxDriver {&usbDriver};
}

struct i16Pair {
    int16_t X;
    int16_t Y;
};

struct XboxInputScheme {

    XboxInputScheme() = default;

    i16Pair LH;
    i16Pair RH;
    
    uint16_t LTR;
    uint16_t RTR;

    bool toggleX;
    bool toggleY;
    bool toggleA;
    bool toggleB;
    bool toggleLB;
    bool toggleRB;

    getLatestData() {
        LH = { 
            USBLibrary::xboxDriver.getAnalogHat(LeftHatX), // Integer from -32768 to 32767 for all getAnalogHat calls
            USBLibrary::xboxDriver.getAnalogHat(LeftHatY),
        };

        RH = {
            USBLibrary::xboxDriver.getAnalogHat(RightHatX),
            USBLibrary::xboxDriver.getAnalogHat(RightHatY),
        };

        LTR = USBLibrary::xboxDriver.getButtonPress(LT); //Integer from 0 to 1023
        RTR = USBLibrary::xboxDriver.getButtonPress(RT);

        toggleX = USBLibrary::xboxDriver.getButtonClick(X) ? !toggleX : toggleX;
        toggleY = USBLibrary::xboxDriver.getButtonClick(Y) ? !toggleY : toggleY;
        toggleA = USBLibrary::xboxDriver.getButtonClick(A) ? !toggleA : toggleA;
        toggleB = USBLibrary::xboxDriver.getButtonClick(B) ? !toggleB : toggleB;

        if (USBLibrary::xboxDriver.getButtonClick(LB)) {
            toggleLB = !toggleLB;
            toggleRB = 0;
        }
        if (USBLibrary::xboxDriver.getButtonClick(RB)) {
            toggleRB = !toggleRB;
            toggleLB = 0;
        }
    }
};

XboxInputScheme xis {};
double throttle = 0;
double scale = 0;
int absX = 0;

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
    constexpr uint32_t baudeRate = 115200;
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
    if(!USBLibrary::xboxDriver.XboxOneConnected)
        return;

    /*
    Important note for the joysticks: 
    Due to something called stick drift, when the joysticks are at rest (not being touched), their value isn't zero, but rather some number roughly around 1500 to 3000
    For our purposes, we can consider any value between -7500 and 7500 to be "zero".
    That is, when the position of the joystick (either x or y) is in this range, the joystick can be considered to be at rest and not doing anything
    Make sure to keep this in mind as you're developing your control code.

    The letter buttons are already mapped to toggle variables
    Pressing any of these buttons toggles their corresponding variable between false and true (off and on)
    Useful for toggling lights, etc

    Unmapped buttons (LeftButton and RightButton)
    Turn signals maybe?
    If so, you need to use toggle variables similar to the letter buttons but make sure that only one turn signal is on at a time

    Add additional code here
    */
    xis.getLatestData();

    constexpr double minTurnRatio = 50.0;

    //Throttle code
    throttle = ((xis.RTR/1023.0)*100);
    absX = abs(xis.LH.X);

    //Steering
    if(absX > 7500) {
        scale = abs > 31000? minTurnRatio/100 : ((((absX-7500.0)*-(100.0-minTurnRatio)) / (32767.0-7500.0)) + 100)/100.0;
    }
    
    if(xis.LH.X < -7500) {
        MotorR = throttle;
        MotorL = throttle*scale;
    } else if(xis.LH.X > 7500) {
        MotorL = throttle;
        MotorR = throttle*scale;
    } else {
        MotorL = throttle;
        MotorR = throttle;
    }

    //Lights
    TurnL = xis.toggleLB;
    TurnR = xis.toggleRB;

    /*
    YOUR OBJECTIVE: change these variables based on the controller inputs and the tank drive control scheme. 
    These are the raw outputs that determine the electrical signals sent to each device, and you have to account for this.
    For example, if TurnL is true, that means the left turn signal light is on (solid, not blinking). It's YOUR job to make sure that you make
    some sort of loop or something that makes TurnL go true,false,true,false,true,false, etc so it blinks.
    Make sure the variables always stay in the bounds defined below.
    */
    MotorL = 0;           //0-100
               //0-100
    Brake = 0;            //0-100
    Lights = false;       //true or false
    BrakeLights = false;  //true or false
    TurnL = false;        //true or false
    TurnR = false;        //true or false

    delay(2);

    // Print debug information
#if _ENABLE_MODE_DEBUG
    Serial.print(F("LX: "));
    Serial.print(xis.LH.X);
    Serial.print(F(" RX: "));
    Serial.print(xis.RH.X);
    Serial.print(F(" LT: "));
    Serial.print(xis.LTR);
    Serial.print(F(" RT: "));
    Serial.print(xis.RTR);
    Serial.print(F(" toggle Y: "));
    Serial.print(xis.toggleY);
    Serial.print(F(" toggle B: "));
    Serial.print(xis.toggleB);
    Serial.print(F(" toggle A: "));
    Serial.print(xis.toggleA);
    Serial.print(F(" toggle X: "));
    Serial.print(xis.toggleX);
    Serial.println();
#endif 

#if _ENABLE_MODE_DEBUG2
    Serial.print(F("T: "));
    Serial.print(throttle);
    Serial.print(F(" S: "));
    Serial.print(scale);
    Serial.print(F(" ML: "));
    Serial.print(MotorL);
    Serial.print(F(" MR: "));
    Serial.print(MotorR);
    Serial.print(F(" LT: "));
    Serial.print(TurnL);
    Serial.print(F(" RT: "));
    Serial.print(TurnR);
    Serial.println();
#endif 
}
