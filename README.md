Just A vault is an ENSC100 project.

It's basically a Vault that can use: </br>
&ensp; Fingerprint </br>
&ensp;  PIN (whatever long-digit password) </br>
&ensp;  TOTP (Time-based PIN, or simply Google Authenticator) </br>
&ensp;  NFC (Any Keycard) </br>
 </br>
 
// The Required Libraries </br>
#include <Wire.h> </br>
#include <Adafruit_PN532.h> </br>
#include <LiquidCrystal_I2C.h> </br>
#include <Keypad.h> </br>
#include "TOTP.h" </br>
#include <SoftwareSerial.h> </br>
#include <Adafruit_Fingerprint.h> </br>
#include <DS3231.h> </br>
#include <Servo.h> </br>


More info later...
