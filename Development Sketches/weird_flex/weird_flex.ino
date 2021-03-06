// Arduino sketch that returns calibration offsets for MPU6050 //   Version 1.1  (31th January 2014)
// Done by Luis Ródenas <luisrodenaslorda@gmail.com>
// Based on the I2Cdev library and previous work by Jeff Rowberg <jeff@rowberg.net>
// Updates (of the library) should (hopefully) always be available at https://github.com/jrowberg/i2cdevlib

// These offsets were meant to calibrate MPU6050's internal DMP, but can be also useful for reading sensors. 
// The effect of temperature has not been taken into account so I can't promise that it will work if you 
// calibrate indoors and then use it outdoors. Best is to calibrate and use at the same room temperature.
#include "I2Cdev.h"
//#include "MPU6050.h" THIS MAKES IT EXPLODE
#include "MPU6050_6Axis_MotionApps20.h"


#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
#endif



#include <Servo.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
// called this way, it uses the default address 0x40
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// you can also call it with a different address and I2C interface
//Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(&Wire, 0x40);

Servo thumbServo;  // Create servo object to control a servo
Servo indexServo;  // Create servo object to control a servo
Servo middleServo;  // Create servo object to control a servo
Servo ringServo;  // Create servo object to control a servo
Servo pinkyServo;  // Create servo object to control a servo

/* Debugging only
int longPos = 0;    // Store servo position
int shortPos = 0;    // Store servo position
*/

/******************************************************************************
Flex_Sensor_Example.ino
Example sketch for SparkFun's flex sensors
  (https://www.sparkfun.com/products/10264)
Jim Lindblom @ SparkFun Electronics
April 28, 2016
Create a voltage divider circuit combining a flex sensor with a 47k resistor.
- The resistor should connect from A0 to GND.
- Thx_1_z_yese flex sensor should connect from A0 to 3.3V
As the resistance of the flex sensor increases (meaning it's being bent), the
voltage at A0 should decrease.
Development environment specifics:
Arduino 1.6.7
******************************************************************************/
const int T_FLEX_PIN = PA0; // Pin connected to voltage divider output
const int I_FLEX_PIN = PA1; // Pin connected to voltage divider output
const int M_FLEX_PIN = PA2; // Pin connected to voltage divider output
const int R_FLEX_PIN = PA3; // Pin connected to voltage divider output
const int P_FLEX_PIN = PA4; // Pin connected to voltage divider output

// Measure the voltage at 5V and the actual resistance of your
// 47k resistor, and enter them below:
//const float VCC = 3.3; // Measured voltage of Ardunio 5V line
const float VCC = 5; // Measured voltage of Ardunio 5V line
const float LONG_R_DIV = 25000.0; // Measured resistance of 3.3k resistor
const float SHORT_R_DIV = 60500.0; // Measured resistance of 3.3k resistor

// Upload the code, then try to adjust these values to more
// accurately calculate bend degree.
//const float STRAIGHT_RESISTANCE = 37300.0; // resistance when straight
//const float BEND_RESISTANCE = 90000.0; // resistance at 90 deg

// Depending on your servo make, the pulse width min and max may vary, you 
// want these to be as small/large as possible without hitting the hard stop
// for max range. You'll have to tweak them as necessary to match the servos you
// have!
// Thumb GOOD
#define SERVOT_MIN  90 // this is the 'minimum' pulse length count (out of 4096)
#define SERVOT_MAX  500 // this is the 'maximum' pulse length count (out of 4096)

// Index GOOD
#define SERVOI_MIN  95 // this is the 'minimum' pulse length count (out of 4096)
#define SERVOI_MAX  530 // this is the 'maximum' pulse length count (out of 4096)

// Middle GOOD
#define SERVOM_MIN  102 // this is the 'minimum' pulse length count (out of 4096)
#define SERVOM_MAX  507 // this is the 'maximum' pulse length count (out of 4096)

// Ring GOOD
#define SERVOR_MIN  85 // this is the 'minimum' pulse length count (out of 4096)
#define SERVOR_MAX  486 // this is the 'maximum' pulse length count (out of 4096)

// Pinky GOOD
#define SERVOP_MIN  105 // this is the 'minimum' pulse length count (out of 4096)
#define SERVOP_MAX  440 // this is the 'maximum' pulse length count (out of 4096)

// Gyro
#define SERVOG_MIN  70 // this is the 'minimum' pulse length count (out of 4096)
#define SERVOG_MAX  490 // this is the 'maximum' pulse length count (out of 4096)

// our servo # counter
uint8_t servonum = 5;

// Counter used for averaging
#define SAMPLES 3 // Number of samples used for avergaing
uint16_t samples1[SAMPLES];
uint16_t samples2[SAMPLES];
uint16_t samples3[SAMPLES];
uint16_t samples4[SAMPLES];
uint16_t samples5[SAMPLES];

/**
 * Short:
 *  Straight: 26.25k
 *  Bend: 100k
 *  R2 = 65k
 *  
 * Long:
 *  Straight: 12.3k
 *  Bend: 36k
 *  R2 = 24k
 */
const float SHORT_STRAIGHT_RESISTANCE = 26250.0;
const float SHORT_BEND_RESISTANCE = 100000.0;
const float LONG_STRAIGHT_RESISTANCE = 10500.0;
const float LONG_BEND_RESISTANCE = 27000.0;

/**
 * Wiring
 * 
 * Long flex:
 * - LONG_FLEX_PIN = PA0;
 * - longServo (1 fin) = PA2
 * 
 * Short flex:
 * - SHORT_FLEX_PIN = PA1;
 * - shortServo (2 fin) = PA3
 */

bool blinkState = false;

// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer

// orientation/motion vars
Quaternion q;           // [w, x, y, z]         quaternion container
VectorInt16 aa;         // [x, y, z]            accel sensor measurements
VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity;    // [x, y, z]            gravity vector
float euler[3];         // [psi, theta, phi]    Euler angle container
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

// packet structure for InvenSense teapot demo
uint8_t teapotPacket[14] = { '$', 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x00, '\r', '\n' };

int isCalibrated = 0;
bool runOnce = false;

// ================================================================
// ===               INTERRUPT DETECTION ROUTINE                ===
// ================================================================

volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void dmpDataReady() {
  mpuInterrupt = true;
}

///////////////////////////////OFFSET CONFIGURATION   /////////////////////////////
//Change this 3 variables if you want to fine tune the skecth to your needs.
int buffersize=1000;     //Amount of readings used to average, make it higher to get more precision but sketch will be slower  (default:1000)
int acel_deadzone=8;     //Acelerometer error allowed, make it lower to get more precision, but sketch may not converge  (default:8)
int giro_deadzone=1;     //Giro error allowed, make it lower to get more precision, but sketch may not converge  (default:1)

// default I2C address is 0x68
// specific I2C addresses may be passed as a parameter here
// AD0 low = 0x68 (default for InvenSense evaluation board)
// AD0 high = 0x69
//MPU6050 mpu;
MPU6050 mpu(0x68); // <-- use for AD0 high

int16_t ax, ay, az,gx, gy, gz;

int16_t mean_ax,mean_ay,mean_az,mean_gx,mean_gy,mean_gz,state=0;
int16_t ax_offset,ay_offset,az_offset,gx_offset,gy_offset,gz_offset;


void setup()
{
  // join I2C bus (I2Cdev library doesn't do this automatically)
  #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    Serial.println("Arduino Wire");
    Wire.begin();
    //TWBR = 24; // 400kHz I2C clock (200kHz if CPU is 8MHz)
  #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
    Serial.println("Builtin Fastwire");
    Fastwire::setup(400, true);
  #endif

  Serial.begin(9600);

  mpu.initialize();
  devStatus = mpu.dmpInitialize();

  Serial.println("devStatus = "+ String(devStatus));

  if (devStatus == 0) {

    mpu.setDMPEnabled(true);

    attachInterrupt(0, dmpDataReady, RISING);
    mpuIntStatus = mpu.getIntStatus();

    dmpReady = true;

    packetSize = mpu.dmpGetFIFOPacketSize();
  }

  if( !runOnce ) {
    runOnce = true;
    // start message
    Serial.println("\nMPU6050 Calibration Sketch");
    delay(1000);
    // verify connection
    Serial.println(mpu.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");
    resetGyroSensors();
  }

  // Initialize samples array
  for(int i = 0; i < SAMPLES; i++){
    samples1[i] = 0;
    samples2[i] = 0;
    samples3[i] = 0;
    samples4[i] = 0;
    samples5[i] = 0;
  }

  delay(1000);

  // Set up flex sensors
  pinMode(T_FLEX_PIN, INPUT);
  pinMode(I_FLEX_PIN, INPUT);  
  pinMode(M_FLEX_PIN, INPUT);
  pinMode(R_FLEX_PIN, INPUT);
  pinMode(P_FLEX_PIN, INPUT);
  
  // PWM Code:
  pwm.begin();
  
  pwm.setPWMFreq(50);  // Analog servos run at ~50 Hz updates

  delay(10);

}


void startCalibration() {
  Serial.println("\nstate = " + state);
  if (state==0){
    Serial.println("\nReading sensors for first time...");
    meansensors();
    state++;
    delay(1000);
  }

  if (state==1) {
    Serial.println("\nCalculating offsets...");
    calibration();
    state++;
    delay(1000);
  }

  if (state==2) {
    meansensors();
    Serial.println("\nFINISHED!");
    Serial.print("\nSensor readings with offsets:\t");
    Serial.print(mean_ax);
    Serial.print("\t");
    Serial.print(mean_ay); 
    Serial.print("\t");
    Serial.print(mean_az); 
    Serial.print("\t");
    Serial.print(mean_gx); 
    Serial.print("\t");
    Serial.print(mean_gy); 
    Serial.print("\t");
    Serial.println(mean_gz);
    Serial.print("Your offsets:\t");
    Serial.print(ax_offset); 
    Serial.print("\t");
    Serial.print(ay_offset); 
    Serial.print("\t");
    Serial.print(az_offset); 
    Serial.print("\t");
    Serial.print(gx_offset); 
    Serial.print("\t");
    Serial.print(gy_offset); 
    Serial.print("\t");
    Serial.println(gz_offset); 
    Serial.println("\nData is printed as: acelX acelY acelZ giroX giroY giroZ");
    Serial.println("Check that your sensor readings are close to 0 0 16384 0 0 0");
    Serial.println("If calibration was succesful write down your offsets so you can set them in your projects using something similar to mpu.setXAccelOffset(youroffset)");
  }
}

void getypr()
{
  if (!dmpReady) return;
  
  mpuInterrupt = false;
  mpuIntStatus = mpu.getIntStatus();

  // get current FIFO count
  fifoCount = mpu.getFIFOCount();

  // check for overflow (this should never happen unless our code is too inefficient)
  if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
    // reset so we can continue cleanly
    mpu.resetFIFO();

  } else if (mpuIntStatus & 0x02) {
    // wait for correct available data length, should be a VERY short wait
    while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();

    // read a packet from FIFO
    mpu.getFIFOBytes(fifoBuffer, packetSize);

    // track FIFO count here in case there is > 1 packet available
    // (this lets us immediately read more without waiting for an interrupt)
    fifoCount -= packetSize;


    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);

    blinkState = !blinkState;
  }
}

void loop()
{
   if( isCalibrated == 0 ) {
    //resetGyroSensors();
    setOffsets();
    isCalibrated = 1;
  }
  getypr();
  Serial.print("ypr\t");
  Serial.print(ypr[0] * 180/M_PI);
  Serial.print("\t");
  Serial.print(ypr[1] * 180/M_PI);
  Serial.print("\t");
  Serial.print(ypr[2] * 180/M_PI);
  Serial.print( "\n" );
  //delay( 50 ); // FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  
  // Read the ADC, and calculate voltage and resistance from it
  //int flexADC = analogRead(FLEX_PIN);

  /* Old code
  uint16_t longFlexADC = analogRead(LONG_FLEX_PIN);
  uint16_t shortFlexADC = analogRead(SHORT_FLEX_PIN);
  */

  uint16_t thumbFlexADC = analogRead(T_FLEX_PIN);
  uint16_t indexFlexADC = analogRead(I_FLEX_PIN);
  uint16_t middleFlexADC = analogRead(M_FLEX_PIN);
  uint16_t ringFlexADC = analogRead(R_FLEX_PIN);
  uint16_t pinkyFlexADC = analogRead(P_FLEX_PIN);

  /* Old code
  float longFlexV = longFlexADC * VCC / 4096.0;
  float shortFlexV = shortFlexADC * VCC / 4096.0;
  */
  
  float thumbFlexV = thumbFlexADC * VCC / 4096.0;
  float indexFlexV = indexFlexADC * VCC / 4096.0;
  float middleFlexV = middleFlexADC * VCC / 4096.0;
  float ringFlexV = ringFlexADC * VCC / 4096.0;
  float pinkyFlexV = pinkyFlexADC * VCC / 4096.0;

  //Serial.println("longflexADC= "+ String(longFlexADC)+" longflexV = "+String(longFlexV));
  //Serial.println("shortflexADC= "+ String(shortFlexADC)+" shortflexV = "+String(shortFlexV));

  /* Old Code
  float longFlexR = LONG_R_DIV * (VCC / longFlexV - 1.0);
  float shortFlexR = SHORT_R_DIV * (VCC / shortFlexV - 1.0);
  */
  
  float thumbFlexR = SHORT_R_DIV * (VCC / thumbFlexV - 1.0);
  float indexFlexR = LONG_R_DIV * (VCC / indexFlexV - 1.0);
  float middleFlexR = LONG_R_DIV * (VCC / middleFlexV - 1.0);
  float ringFlexR = LONG_R_DIV * (VCC / ringFlexV - 1.0);
  float pinkyFlexR = SHORT_R_DIV * (VCC / pinkyFlexV - 1.0);
  
  //Serial.println("Long Resistance: " + String(longFlexR) + " ohms");
  //Serial.println("Short Resistance: " + String(shortFlexR) + " ohms");

  // Use the calculated resistance to estimate the sensor's
  // bend angle:
  //SHORT_STRAIGHT_RESISTANCE = 26250.0;
  //SHORT_BEND_RESISTANCE = 100000.0;
  //LONG_STRAIGHT_RESISTANCE = 10500.0;
  //LONG_BEND_RESISTANCE = 27000.0;
  /* Old code
  float longAngle = constrain(map(longFlexR, LONG_STRAIGHT_RESISTANCE, LONG_BEND_RESISTANCE, 0, 180.0), 0, 180);
  float shortAngle = constrain(map(shortFlexR, SHORT_STRAIGHT_RESISTANCE, SHORT_BEND_RESISTANCE, 0, 180.0), 0, 180);
  */
  
  float thumbAngle = constrain(map(thumbFlexR, SHORT_STRAIGHT_RESISTANCE, SHORT_BEND_RESISTANCE, 0, 180.0), 0, 180);
  float indexAngle = constrain(map(indexFlexR, LONG_STRAIGHT_RESISTANCE, LONG_BEND_RESISTANCE, 0, 180.0), 0, 180);
  float middleAngle = constrain(map(middleFlexR, LONG_STRAIGHT_RESISTANCE, LONG_BEND_RESISTANCE, 0, 180.0), 0, 180);
  float ringAngle = constrain(map(ringFlexR, LONG_STRAIGHT_RESISTANCE, LONG_BEND_RESISTANCE, 0, 180.0), 0, 180);
  float pinkyAngle = constrain(map(pinkyFlexR, SHORT_STRAIGHT_RESISTANCE, SHORT_BEND_RESISTANCE, 0, 180.0), 0, 180);
  
  //Serial.println("LongAngle: " + String(longAngle) + " degrees");
  //Serial.println("ShortAngle: " + String(shortAngle) + " degrees");

  //Middle and Ring finger are LONG sensors
  //Everything else is a SHORT sensor
  /* olde code
  float longPulseLen = constrain(map(longAngle, 0, 180.0, SERVOMIN, SERVOMAX), SERVOMIN, SERVOMAX);
  float shortPulseLen = constrain(map(shortAngle, 0, 180.0, SERVOMIN, SERVOMAX), SERVOMIN, SERVOMAX);
  */

  float thumbPulseLen = constrain(map(thumbAngle, 0, 180.0, SERVOT_MIN, SERVOT_MAX), SERVOT_MIN, SERVOT_MAX);
  float indexPulseLen = constrain(map(indexAngle, 0, 180.0, SERVOI_MIN, SERVOI_MAX), SERVOI_MIN, SERVOI_MAX);
  float middlePulseLen = constrain(map(middleAngle, 0, 180.0, SERVOM_MIN, SERVOM_MAX), SERVOM_MIN, SERVOM_MAX);
  float ringPulseLen = constrain(map(ringAngle, 0, 180.0, SERVOR_MIN, SERVOR_MAX), SERVOR_MIN, SERVOR_MAX);
  float pinkyPulseLen = constrain(map(pinkyAngle, 0, 180.0, SERVOP_MIN, SERVOP_MAX), SERVOP_MIN, SERVOP_MAX);

  /* old code
  Serial.println("LongPulseLen: " + String(longPulseLen));
  Serial.println("ShortPulseLen: " + String(shortPulseLen));
  */

  /*
  Serial.println("ThumbPulseLen: " + String(thumbPulseLen));
  Serial.println("IndexPulseLen: " + String(indexPulseLen));
  Serial.println("MiddlePulseLen: " + String(middlePulseLen));
  Serial.println("RingPulseLen: " + String(ringPulseLen));
  Serial.println("PinkyPulseLen: " + String(pinkyPulseLen));
  */

  // Shift array left
  for(int i = 0; i < SAMPLES - 1; i++){
    samples1[i] = samples1[i+1];
    samples2[i] = samples2[i+1];
    samples3[i] = samples3[i+1];
    samples4[i] = samples4[i+1];
    samples5[i] = samples5[i+1];
  }

  // Set last element in array
  samples1[SAMPLES-1] = thumbPulseLen;
  samples2[SAMPLES-1] = indexPulseLen;
  samples3[SAMPLES-1] = middlePulseLen;
  samples4[SAMPLES-1] = ringPulseLen;
  samples5[SAMPLES-1] = pinkyPulseLen;

  
  // Find average of samples
  uint16_t avg1 = 0;
  uint16_t avg2 = 0;
  uint16_t avg3 = 0;
  uint16_t avg4 = 0;
  uint16_t avg5 = 0;
  for(int i = 0; i < SAMPLES; i++){
    avg1 += (samples1[i]/SAMPLES);
    avg2 += (samples2[i]/SAMPLES);
    avg3 += (samples3[i]/SAMPLES);
    avg4 += (samples4[i]/SAMPLES);
    avg5 += (samples5[i]/SAMPLES);
  }

  /*
  avg1 = (avg1 / 10) * 10;
  avg2 = (avg2 / 10) * 10;
  avg3 = (avg3 / 10) * 10;
  avg4 = (avg4 / 10) * 10;
  avg5 = (avg5 / 10) * 10;
  *?
  Serial.println("avg1: " + String(avg1));
  Serial.println("avg1: " + String(avg1));
  /*
  // Write same angle to servo
  float longVal = abs(longAngle);
  float shortVal = abs(shortAngle);
  
  Serial.println("longVal: " + String(longVal));
  Serial.println("shortVal: "+ String(shortVal));
  longServo.write(longVal);
  shortServo.write(shortVal);
  */

  
  //longServo.write(longAngle);
  //shortServo.write(shortAngle);

  //pwm.setPWM(0, 0, shortPulseLen);
  //pwm.setPWM(1, 0, longPulseLen);

  // Set the finger servos
  /*
  pwm.setPWM(0, 0, avg1);
  pwm.setPWM(1, 0, avg2);
  pwm.setPWM(2, 0, avg3);
  pwm.setPWM(3, 0, avg4);
  pwm.setPWM(4, 0, avg5);
  */

  // Set the big servo based on pitch

  float gyroAngle = constrain(map(ypr[1], -90, 90, 0, 180.0), 0, 180);
  float gyroPulseLen = constrain(map(gyroAngle, 0, 180.0, SERVOG_MIN, SERVOG_MAX), SERVOG_MIN, SERVOG_MAX);

  Serial.println("ypr[1]: "+ String(ypr[1]));
  Serial.println("gyroAngle: "+ String(gyroAngle));
  Serial.println("gyroPulseLen: "+ String(gyroPulseLen));
  
  pwm.setPWM(5, 0, gyroPulseLen);
  

  /*
  float val = longPos;
  Serial.println(val);
  longServo.write(val);
  longPos += 5;
  */
  /*
  float val = shortPos;
  Serial.println(val);
  shortServo.write(val);
  shortPos += 2;
  */
  
  Serial.println();
  
  delay(1000);
}



//////////////////////////////  OFFSET FUNCTIONS  ////////////////////////////////////
void meansensors(){
  long i=0,buff_ax=0,buff_ay=0,buff_az=0,buff_gx=0,buff_gy=0,buff_gz=0;

  while (i<(buffersize+101)){
    // read raw accel/gyro measurements from device
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    
    if (i>100 && i<=(buffersize+100)){ //First 100 measures are discarded
      buff_ax=buff_ax+ax;
      buff_ay=buff_ay+ay;
      buff_az=buff_az+az;
      buff_gx=buff_gx+gx;
      buff_gy=buff_gy+gy;
      buff_gz=buff_gz+gz;
    }
    if (i==(buffersize+100)){
      mean_ax=buff_ax/buffersize;
      mean_ay=buff_ay/buffersize;
      mean_az=buff_az/buffersize;
      mean_gx=buff_gx/buffersize;
      mean_gy=buff_gy/buffersize;
      mean_gz=buff_gz/buffersize;
    }
    i++;
    delay(2); //Needed so we don't get repeated measures
  }
}

void calibration(){
  ax_offset=-mean_ax/8;
  ay_offset=-mean_ay/8;
  az_offset=(16384-mean_az)/8;

  gx_offset=-mean_gx/4;
  gy_offset=-mean_gy/4;
  gz_offset=-mean_gz/4;
  while (1){
    int ready=0;
    mpu.setXAccelOffset(ax_offset);
    mpu.setYAccelOffset(ay_offset);
    mpu.setZAccelOffset(az_offset);

    mpu.setXGyroOffset(gx_offset);
    mpu.setYGyroOffset(gy_offset);
    mpu.setZGyroOffset(gz_offset);

    meansensors();
    Serial.println("...");

    if (abs(mean_ax)<=acel_deadzone) ready++;
    else ax_offset=ax_offset-mean_ax/acel_deadzone;

    if (abs(mean_ay)<=acel_deadzone) ready++;
    else ay_offset=ay_offset-mean_ay/acel_deadzone;

    if (abs(16384-mean_az)<=acel_deadzone) ready++;
    else az_offset=az_offset+(16384-mean_az)/acel_deadzone;

    if (abs(mean_gx)<=giro_deadzone) ready++;
    else gx_offset=gx_offset-mean_gx/(giro_deadzone+1);

    if (abs(mean_gy)<=giro_deadzone) ready++;
    else gy_offset=gy_offset-mean_gy/(giro_deadzone+1);

    if (abs(mean_gz)<=giro_deadzone) ready++;
    else gz_offset=gz_offset-mean_gz/(giro_deadzone+1);

    if (ready==6) break;
  }
}

void resetGyroSensors() {
  startCalibration();
  Serial.println( "RESETTING GYRO" );
  mpu.reset();
  delay( 1500 );
  setup();
}

void setOffsets() {
  Serial.println( "SETTING OFFSETS" );
  mpu.setXAccelOffset( ax_offset );
  mpu.setYAccelOffset( ay_offset );
  mpu.setZAccelOffset( az_offset );
  mpu.setXGyroOffset( gx_offset );
  mpu.setYGyroOffset( gy_offset );
  mpu.setZGyroOffset( gz_offset );
}
