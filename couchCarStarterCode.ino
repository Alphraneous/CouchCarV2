#include <XBOXONE.h>

// Satisfy the IDE for includes, but do add to actual compilation target
#ifdef _USING_ARDUINO_IDE
#include <spi4teensy3.h>
#endif
#include <SPI.h>

// Set this value to true if you want to see the positions of the buttons and joysticks
#define _ENABLE_MODE_DEBUG false
#define _ENABLE_MODE_DEBUG2 true

namespace USBLibrary
{
    extern USB usbDriver{};
    extern XBOXONE xboxDriver{&usbDriver};
}

struct i16Pair
{
    int16_t X;
    int16_t Y;
};

struct XboxInputScheme
{

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

    getLatestData()
    {
        LH = {
            USBLibrary::xboxDriver.getAnalogHat(LeftHatX), // Integer from -32768 to 32767 for all getAnalogHat calls
            USBLibrary::xboxDriver.getAnalogHat(LeftHatY),
        };

        RH = {
            USBLibrary::xboxDriver.getAnalogHat(RightHatX),
            USBLibrary::xboxDriver.getAnalogHat(RightHatY),
        };

        LTR = USBLibrary::xboxDriver.getButtonPress(LT); // Integer from 0 to 1023
        RTR = USBLibrary::xboxDriver.getButtonPress(RT);

        toggleX = USBLibrary::xboxDriver.getButtonClick(X) ? !toggleX : toggleX;
        toggleY = USBLibrary::xboxDriver.getButtonClick(Y) ? !toggleY : toggleY;
        toggleA = USBLibrary::xboxDriver.getButtonClick(A) ? !toggleA : toggleA;
        toggleB = USBLibrary::xboxDriver.getButtonClick(B) ? !toggleB : toggleB;

        if (USBLibrary::xboxDriver.getButtonClick(LB))
        {
            toggleLB = !toggleLB;
            toggleRB = 0;
        }
        if (USBLibrary::xboxDriver.getButtonClick(RB))
        {
            toggleRB = !toggleRB;
            toggleLB = 0;
        }
    }
};

XboxInputScheme xis{};
double throttle = 0;
double scale = 0;
int absX = 0;

// Goal variables
int MotorL = 0;
int MotorR = 0;
double Brake = 0;
bool Lights = false;
bool BrakeLights = false;
bool TurnL = false;
bool TurnR = false;

// Runs once on startup
void setup()
{
    constexpr uint32_t baudeRate = 115200;
    Serial.begin(baudeRate);

    while (!Serial)
        ; // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection

    if (USBLibrary::usbDriver.Init() == -1)
    {
        Serial.print(F("\r\nOSC did not start"));
        while (1)
            ; // Halt
    }

    Serial.print(F("\r\nXBOX ONE USB Library Started"));
}

void loop()
{
    USBLibrary::usbDriver.Task();
    if (!USBLibrary::xboxDriver.XboxOneConnected)
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

    // =========================================
    //  SETTINGS
    // =========================================
    constexpr double minTurnRatio = 50.0; // scale factor floor
    constexpr double deadZone = 7500.0;   // joystick raw deadzone
    constexpr double maxJoyRaw = 32768.0;
    constexpr double expo = 1.6; // recommended game-like exponent

    int32_t xRaw = xis.LH.X; // -32768..32767
    int32_t rtr = xis.RTR;   // 0..1023
    int32_t ltr = xis.LTR;   // 0..1023

    // Throttle
    double tNorm = rtr / 1023.0;
    double tCurve = pow(tNorm, expo);
    double throttle = tCurve * 100.0;

    // Brake
    double bNorm = ltr / 1023.0;
    double Brake = pow(bNorm, expo) * 100.0;
    bool BrakeLights = Brake > 2;

    // Steering
    double absX = fabs((double)xRaw);
    absX = fmin(absX, maxJoyRaw);
    double scale = 1.0; // default = 100%

    if (absX > deadZone)
    {
        // Normalize turning: deadZone → 0, max → 1
        double turnNorm = (absX - deadZone) / (maxJoyRaw - deadZone);
        if (turnNorm < 0)
            turnNorm = 0;
        if (turnNorm > 1)
            turnNorm = 1;

        // Apply exponential game curve (SAME exponent as throttle)
        double turnCurve = pow(turnNorm, expo);

        // Map 0..1 → 1.0..0.50
        double minScale = minTurnRatio / 100.0;
        scale = 1.0 - (turnCurve * (1.0 - minScale));
    }

    double MotorL = throttle;
    double MotorR = throttle;

    if (xRaw < -deadZone)
    {
        MotorL = throttle * scale;
        MotorR = throttle;
    }
    else if (xRaw > deadZone)
    {
        MotorL = throttle;
        MotorR = throttle * scale;
    }

    // Lights
    TurnL = xis.toggleLB;
    TurnR = xis.toggleRB;
    Lights = xis.toggleX;

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
    Serial.print(F(" B: "));
    Serial.print(Brake);
    Serial.print(F(" ML: "));
    Serial.print(MotorL);
    Serial.print(F(" MR: "));
    Serial.print(MotorR);
    Serial.print(F(" TL: "));
    Serial.print(TurnL);
    Serial.print(F(" TR: "));
    Serial.print(TurnR);
    Serial.println();
#endif
}
