#include <XBOXONE.h>
#include <Adafruit_MCP4728.h>
#include <Wire.h>


// Satisfy the IDE for includes, but do add to actual compilation target
#ifdef _USING_ARDUINO_IDE
#include <spi4teensy3.h>
#endif
#include <SPI.h>

// Set this value to true if you want to see the positions of the buttons and joysticks
#define _ENABLE_MODE_INPUT_DEBUG  false
#define _ENABLE_MODE_OUTPUT_DEBUG true

namespace USBLibrary
{
    extern USB usbDriver{}; // If this causes a linker issue, remove "extern" or switch to "inline" if >= C++17
    extern XBOXONE xboxDriver{&usbDriver};
}

struct i16Pair {
    int16_t X;
    int16_t Y;
};

struct f64Pair {
    double L;
    double R;
};

static double clamp(double value, double min, double max);

//Turn light stuff
uint8_t turnLightTickCounter;
bool onState = true;

//Brake servo
const int PWM_PIN = 3;
const int pwmMin = 16; //duty cycle% //DO NOT GO LOWER THAN 16
const int pwmMax = 80; //duty cycle% //DO NOT GO HIGHER THAN 67
const uint32_t PERIOD_US = 20000UL; // 50Hz

void setupPwm50Hz();
void setDutyPercent(double dutyPct);

// =========================================
//  SETTINGS
// =========================================
constexpr double minTurnRatio      = 5.0;    // Minimum motor scale
constexpr double deadZone          = 0.1;     // Absolute deadzone value
constexpr double throttleExp       = 1.5;     // Throttle ramping exponent
constexpr double brakeExp          = 1.0;     // Brake ramping exponent
constexpr double steeringExp       = 2.0;     // Brake ramping exponent
constexpr double maxJoyRawMagn     = 32768.0; // Max magnitude of raw joystick input
constexpr double maxTriggerRawMagn = 1024.0;  // Max magnitude of raw trigger input

struct XboxInputScheme
{
    XboxInputScheme() = default;

    i16Pair LH;
    i16Pair RH;

    uint16_t LT;
    uint16_t RT;

    bool toggleX;
    bool toggleY;
    bool toggleA;
    bool toggleB;
    bool toggleLB;
    bool toggleRB;

    void getLatestData()
    {
        LH = {
            USBLibrary::xboxDriver.getAnalogHat(AnalogHatEnum::LeftHatX), // Integer from -32768 to 32767 for all getAnalogHat calls
            USBLibrary::xboxDriver.getAnalogHat(AnalogHatEnum::LeftHatY),
        };

        RH = {
            USBLibrary::xboxDriver.getAnalogHat(AnalogHatEnum::RightHatX),
            USBLibrary::xboxDriver.getAnalogHat(AnalogHatEnum::RightHatY),
        };

        LT = USBLibrary::xboxDriver.getButtonPress(ButtonEnum::LT); // Integer from 0 to 1023
        RT = USBLibrary::xboxDriver.getButtonPress(ButtonEnum::RT);

        toggleX = USBLibrary::xboxDriver.getButtonClick(ButtonEnum::X) ? !toggleX : toggleX;

        if (USBLibrary::xboxDriver.getButtonClick(ButtonEnum::Y))
        {
            toggleY = !toggleY;
            turnLightTickCounter = 0;
            onState = true;
        }
    
        toggleA = USBLibrary::xboxDriver.getButtonClick(ButtonEnum::A) ? !toggleA : toggleA;
        toggleB = USBLibrary::xboxDriver.getButtonClick(ButtonEnum::B) ? !toggleB : toggleB;

        if (USBLibrary::xboxDriver.getButtonClick(ButtonEnum::LB))
        {
            turnLightTickCounter = 0;
            onState = true;
            toggleLB = !toggleLB;
            toggleRB = false; 
            
        }
        if (USBLibrary::xboxDriver.getButtonClick(ButtonEnum::RB))
        {
            turnLightTickCounter = 0;
            onState = true;
            toggleRB = !toggleRB;
            toggleLB = false;
        }
    }
};

struct VehicleControl {
    VehicleControl() = default;
   
    f64Pair motors; // Percentage 0..1 of max channel voltage
    double  brake;  // Percentage 0..1 of max channel voltage

    bool brakeLight;
    bool headLight;
    bool leftTurnLight;
    bool rightTurnLight;

    void setMotorPercentage(const XboxInputScheme& xis, double throttle) {
        // Steering
        double absLXNorm = clamp(fabs(xis.LH.X) / maxJoyRawMagn, 0.0, 1.0);

        if (fabs(absLXNorm - deadZone) < 1e-3) // If the joysticks are held still / in deadzone, continue both motors moving straight
        {
            motors = { throttle, throttle }; 
            return; 
        }

        // Apply exponential game curve
        double turnCurve = pow(absLXNorm, steeringExp);
        double minScale = minTurnRatio / 100.0;
        double scale = 1.0 - (turnCurve * (1.0 - minScale)); // Scale of one motor as a percentage of another

        if (xis.LH.X < 0) // If the joystick is pushed to the left, turn left
        {
            motors.L = throttle * scale;
            motors.R = throttle;
        }
        else if (xis.LH.X > 0) // If the joystick is pushed to the right, turn right
        {
            motors.L = throttle;
            motors.R = throttle * scale;
        }
    }

    void setBrakePercentage(const XboxInputScheme& xis) {
        double bNorm = clamp(xis.LT / maxTriggerRawMagn, 0.0, 1.0);
        brake = pow(bNorm, brakeExp);
        int brakePulse = pwmMax + brake * (pwmMin - pwmMax);
        brakePulse = constrain(brakePulse, pwmMin, pwmMax);
        setDutyPercent(brakePulse);

        //  TODO: Not implemented
    }

    // Light output
    void setLights(const XboxInputScheme& xis) {
        constexpr uint8_t brakeLightPin = 4; // Set Arduino output pins used for lights
        constexpr uint8_t headLightPin  = 5; 
        constexpr uint8_t leftTurnPin   = 6; 
        constexpr uint8_t rightTurnPin  = 7; 

        // Assign member light toggle values and output to Arduino pins
        // Display the brake light if the value of brake is greater than epsilon
        (brakeLight = brake > 1e-3) ? digitalWrite(brakeLightPin, HIGH) : digitalWrite(brakeLightPin, LOW);
        (headLight = xis.toggleX)   ? digitalWrite(headLightPin, HIGH)  : digitalWrite(headLightPin, LOW);

        
        turnLightTickCounter++;
        if (turnLightTickCounter > 80) // Arbitrary tick count for 2ms * x ticks = approximately 2x ms
        {
           turnLightTickCounter = 0;   
           onState = !onState;
        }

        leftTurnLight  = onState && (xis.toggleY || xis.toggleLB);
        rightTurnLight = onState && (xis.toggleY || xis.toggleRB);

        if (!onState) {
            digitalWrite(leftTurnPin, LOW); 
            digitalWrite(rightTurnPin, LOW);
            return;
        }

        digitalWrite(leftTurnPin,  (xis.toggleY || xis.toggleLB ? HIGH : LOW));
        digitalWrite(rightTurnPin, (xis.toggleY || xis.toggleRB ? HIGH : LOW));
    }
};
      
XboxInputScheme  xis{};
VehicleControl   vc{};
Adafruit_MCP4728 mcp{};


// Runs once on startup
void setup()
{
    pinMode(4,OUTPUT);
    pinMode(5,OUTPUT);
    pinMode(6,OUTPUT);
    pinMode(7,OUTPUT);
    constexpr uint32_t baudeRate = 115200;
    Serial.begin(baudeRate);

    while (!Serial)
        ; // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection

    // Start USB Host
    if (USBLibrary::usbDriver.Init() == -1)
    {
        Serial.print(F("\r\nOSC did not start"));
        while (1)
            ; // Halt
    }

    Serial.print(F("\r\nXBOX ONE USB Library Started"));

    // Start MCP
    if (!mcp.begin())
    {
        Serial.println("Failed to find MCP4728 chip");
        while (1)
        {
            delay(10);
        }
    }
    Serial.print(F("\r\nMCP DAC Initialized"));
    Serial.println();

    //servo shi
    setupPwm50Hz();
    setDutyPercent(pwmMax);

}

void loop()
{
    USBLibrary::usbDriver.Task();
    if (!USBLibrary::xboxDriver.XboxOneConnected)
    {
        //empty object so dont go brr
        xis = XboxInputScheme(); 
        vc = VehicleControl();   

        digitalWrite(4, LOW); 
        digitalWrite(5, LOW); 
        digitalWrite(6, LOW); 
        digitalWrite(7, LOW); 

        double restVolt = 1.1; 
        uint16_t dacRest = (restVolt / 5.0) * 4095;         //CHECK CALC TS FROM GPT MIGHT BE WRONG
        mcp.setChannelValue(MCP4728_CHANNEL_B, dacRest);
        mcp.setChannelValue(MCP4728_CHANNEL_A, dacRest);
        return;
    }
    // -----------------------

    // If we reach here, controller is connected
    // (Existing code continues below...)
    xis.getLatestData();
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

    // Throttle
    double tNorm = clamp(xis.RT / maxTriggerRawMagn, 0.0, 1.0);
    double throttle = pow(tNorm, throttleExp);
    
    // Set motor percentage
    vc.setMotorPercentage(xis, throttle);

    // Set break percentage
    vc.setBrakePercentage(xis);
    vc.setLights(xis);

    auto motorToVoltage = [](double M) -> double {
        // Voltage constants
        constexpr double restV = 1.1;
        constexpr double minV  = 1.25;
        constexpr double maxV  = 3.7;

        if (M <= 0.01)
            return restV;

        return minV + M * (maxV - minV);
    };

    double voltA = motorToVoltage(vc.motors.L); // to VA
    double voltB = motorToVoltage(vc.motors.R); // to VB

    uint16_t dacA = voltA / 5.0 * 4095;
    uint16_t dacB = voltB / 5.0 * 4095;

    mcp.setChannelValue(MCP4728_CHANNEL_B, dacA);
    mcp.setChannelValue(MCP4728_CHANNEL_A, dacB);

    delay(2);

    // Print debug information
#if _ENABLE_MODE_INPUT_DEBUG
    Serial.print(F("LX: "));
    Serial.print(xis.LH.X);
    Serial.print(F(" RX: "));
    Serial.print(xis.RH.X);
    Serial.print(F(" LT: "));
    Serial.print(xis.LT);
    Serial.print(F(" RT: "));
    Serial.print(xis.RT);
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

#if _ENABLE_MODE_OUTPUT_DEBUG
    Serial.print(F("T: "));
    Serial.print(throttle);
    Serial.print(F(" B: "));
    Serial.print(vc.brake);
    Serial.print(F(" ML: ")); 
    Serial.print(vc.motors.L);
    Serial.print(F(" MR: "));
    Serial.print(vc.motors.R);
    Serial.print(F(" BL: "));
    Serial.print(vc.brakeLight);
    Serial.print(F(" HL: "));
    Serial.print(vc.headLight);
    Serial.print(F(" TL: "));
    Serial.print(vc.leftTurnLight);
    Serial.print(F(" TR: "));
    Serial.print(vc.rightTurnLight);
    Serial.println();
#endif
}

double clamp(double value, double min, double max) 
{
    if (value > max)
        return max;
    if (value < min)
        return min;
    return value;
}

//VOODO PWM SHIT DO NOT MESS WITH THIS
// ===============================
// True 50Hz duty-cycle PWM on D3 using Timer2 ISR
// ===============================

// Timer2 tick: prescaler 8 => 2MHz => 0.5us per tick
static inline uint32_t usToTicks(uint32_t us) { return us * 2UL; }
constexpr uint16_t MAX_TICKS = 255;

volatile uint32_t high_us = 10000;      // default high time
volatile uint32_t remaining_ticks = 0;  // countdown in Timer2 ticks
volatile bool pin_high = false;

// Forward decls already in your code:
// void setupPwm50Hz();
// void setDutyPercent(double dutyPct);

static inline void scheduleNextChunk() {
  uint16_t chunk = (remaining_ticks > MAX_TICKS) ? MAX_TICKS : (uint16_t)remaining_ticks;
  if (chunk == 0) chunk = 1;
  OCR2A = (uint8_t)chunk;
  remaining_ticks -= chunk;
}

ISR(TIMER2_COMPA_vect) {
  if (remaining_ticks > 0) {
    scheduleNextChunk();
    return;
  }

  if (!pin_high) {
    // Rising edge: start HIGH portion
    digitalWrite(PWM_PIN, HIGH);
    pin_high = true;

    uint32_t hu = high_us;
    remaining_ticks = usToTicks(hu);
    if (remaining_ticks == 0) remaining_ticks = 1;
    scheduleNextChunk();
  } else {
    // Falling edge: start LOW portion
    digitalWrite(PWM_PIN, LOW);
    pin_high = false;

    uint32_t lu = PERIOD_US - high_us;
    remaining_ticks = usToTicks(lu);
    if (remaining_ticks == 0) remaining_ticks = 1;
    scheduleNextChunk();
  }
}

void setupPwm50Hz() {
  pinMode(PWM_PIN, OUTPUT);
  digitalWrite(PWM_PIN, LOW);
  pin_high = false;

  // Configure Timer2 for CTC interrupts
  cli();
  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2  = 0;

  // CTC mode (TOP = OCR2A)
  TCCR2A |= (1 << WGM21);

  // Prescaler = 8 => 2MHz timer tick (0.5us)
  TCCR2B |= (1 << CS21);

  // Enable compare match A interrupt
  TIMSK2 |= (1 << OCIE2A);

  // Kick the ISR quickly
  remaining_ticks = 1;
  OCR2A = 1;

  sei();
}

// dutyPct: 0..100
void setDutyPercent(double dutyPct) {
  if (dutyPct < pwmMin) dutyPct = pwmMin;
  if (dutyPct > pwmMax) dutyPct = pwmMax;

  uint32_t hu = (uint32_t)((PERIOD_US * dutyPct) / 100.0);

  // Ensure within [0, PERIOD_US]
  if (hu > PERIOD_US) hu = PERIOD_US;

  noInterrupts();
  high_us = hu;
  interrupts();
}