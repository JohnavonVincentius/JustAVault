Just A vault is an ENSC100 project.

It's basically a Vault that can use:
  Fingerprint
  PIN (whatever long-digit password)
  TOTP (Time-based PIN, or simply Google Authenticator)
  NFC (Any Keycard)

// The Required Libraries
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include "TOTP.h"
#include <SoftwareSerial.h>
#include <Adafruit_Fingerprint.h>
#include <DS3231.h>
#include <Servo.h>


More info later...
