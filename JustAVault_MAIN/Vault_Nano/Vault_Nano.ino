// ======================================== LIBRARIES ======================================== //
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include "TOTP.h"
#include <SoftwareSerial.h>
#include <Adafruit_Fingerprint.h>
#include <DS3231.h>
#include <Servo.h>

// ======================================== CONFIGS ======================================== //
    // ========== TOTP CONFIG ========== //
    #define TOTP_AUTH   "justavault"
    #define TOTP_LEN    10

    // ========== PIN CONFIG ========== //
    #define MASTER_PIN  "123456"

    #define OTHER_PIN_COUNT 3                                       
    #define OTHER_PIN   {"684353","197594","197384265"}


    // ========== RFID CONFIG ========== //
    #define MASTER_RFID {0xF3,0x2E,0x61,0xFA}

    #define SLAVE_COUNT 5
    #define OTHER_RFID  {{0xEA,0x28,0x83,0xC6},{0x6A,0x38,0xFD,0x1B},{0x11,0x12,0x13,0x19},{0x11,0x12,0x13,0x19},{0x11,0x12,0x13,0x19}}

    #define RFID_TIMEOUT 10      // (s)
    // ========== ETC. CONFIG ========== //
    #define SerTimeout 5        // (s)
    #define AuthTimeout 30       // (s)
    
    #define LOCK_PIN    13
    #define SERVO_LOCK 80
    #define SERVO_UNLOCK 0
    #define DEBUG

// ======================================== ADV. CONFIGS ======================================== //
    // ==================== KEYPAD CONFIGS ==================== //
    const byte ROWS = 4;
    const byte COLS = 4;
    char keys[ROWS][COLS] = {
        {'1','2','3','A'},
        {'4','5','6','B'},
        {'7','8','9','C'},
        {'*','0','#','D'}
    };

    byte rowPins[ROWS] = {A3,A2,A1,A0};
    byte colPins[COLS] = {9,10,11,12};


    // ==================== ESP CONFIGS ==================== //
    #define ESP_BAUD 9600   //ESP BAUD (Lower Better, Software serial is notorious of losing data)
    #define ESP_RX  2       //ESP RX, ARDUINO TX
    #define ESP_TX  3       //ESP TX, ARDUINO RX

        
    #define PN532_IRQ   (4)
    #define PN532_RESET (5) 

class AuthResp{
    private:
        bool Resp;
        int ID;
        int confidence;
        int method;
        String reason;
    public:
        AuthResp() : Resp(false),ID(-1),confidence(0),method(-1),reason(""){};
        void clear(){
            Resp = false;
            ID = -1;
            confidence = 0;
            method = -1;
            reason = "";
        }

        AuthResp success(const int &mthd, const int &id, const int &conf = 100){method = mthd; this->ID = id; Resp = true; confidence = conf; return *this;}
        AuthResp fail(const int &mthd, int conf){method = mthd; confidence = conf;return *this;}
        AuthResp fail(const int &mthd, const int &conf,const String &reas){method = mthd; confidence = conf;reason = reas;return *this;}


        bool fail(){return !Resp;}
        bool empty(){return (method == -1);}

        bool success(){return Resp;}
        void timeout(){reason = "TIMEOUT";}

        int getConf(){return confidence;}
        int getID(){return ID;}
        String getReason(){return reason;}
        String getAuthMethod() const{
            switch (method)
            {
            case 1:
                return "CODE";
            case 2:
                return "TOTP";
            case 3:
                return "NFC";
            case 4:
                return "FINGER";
            default:
                return "none";
            }
        };
};

LiquidCrystal_I2C       lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display
SoftwareSerial          ESP_Ser(ESP_TX, ESP_RX);
SoftwareSerial          F_Sensor(7, 6);
Adafruit_PN532          nfc(PN532_IRQ, PN532_RESET);
Adafruit_Fingerprint    finger(&F_Sensor);
Servo myservo;
DS3231 RTC_CLOCK;
RTClib myRTC;

Keypad  keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );
TOTP    totp(TOTP_AUTH,TOTP_LEN);
AuthResp gAuth,AuthOne;
bool timeStart(false);
unsigned long authTime = millis();
String keypad_code;
byte state = 0;

bool AuthAbort = false;

/*
1. Default State ("Press Any Button to Authennticate")
2. On KeyPad Auth
3. On Fingerptint Auth
4. On NFC Auth
*/

// ======================================== PROG FUNCTION ======================================== //
void renewTime(){
    #ifdef DEBUG
        Serial.println(F("Requesting Epoch Update... "));
    #endif
    ESP_Ser.print("EPOCH");
    bool data_wait = false;
    unsigned long timeout = millis();
    while(!data_wait){
        if((millis() - timeout) > (SerTimeout * 1000)){
            Serial.println(F("TIMEOUT"));
            break;
        }
        if(ESP_Ser.available()>0){
            data_wait++;
            unsigned long time = strtol(ESP_Ser.readString().c_str(),nullptr,10);            
            if(myRTC.now().unixtime() < time){
                RTC_CLOCK.setEpoch(time);
            }
            #ifdef DEBUG
                Serial.print(F("Success... EPOCH TIME: "));
                Serial.println(time);
            #endif
        }
    }
}

void waitNet(){
    #ifdef DEBUG
        Serial.println(F("Waiting Network"));
    #endif
    bool data_wait = false;
    unsigned long timeout = millis();
    while(!data_wait){
        if((millis() - timeout) > (SerTimeout * 1000)){
            Serial.println(F("TIMEOUT"));
            break;
        }
        if(ESP_Ser.available()>0){
            data_wait++;
            Serial.println(F("OK"));
        }
    }
}

void postAction(){
    #ifdef DEBUG
        Serial.println(F("Sending To Server"));
    #endif

    String request;
    request += AuthOne.getAuthMethod();
    request += "|";
    request += AuthOne.success();
    request += "|";
    request += AuthOne.getID();
    request += "|";
    request += AuthOne.getConf();
    request += "|";
    request += AuthOne.getReason();
    request += "|";
    request += "=";
    request += gAuth.getAuthMethod();
    request += "|";
    request += gAuth.success();
    request += "|";
    request += gAuth.getID();
    request += "|";
    request += gAuth.getConf();
    request += "|";
    request += gAuth.getReason();
    request += "|";
    ESP_Ser.print(request);
}

bool compArr(const uint8_t *arr1,const uint8_t *arr2, const int &len){
    for(int i = 0;i < len ; i++){
        if(arr1[i] != arr2[i]){
            return false;
        }
    }
    return true;
}

void displayCode(){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(F("   Enter CODE   "));
    lcd.setCursor(17/2-keypad_code.length()/2 , 1);
    for(int i = 0;i < keypad_code.length();i++)
        lcd.print('*');  
}

void getNFCAuth(){ //Req NFC Auth
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
    uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    uint8_t master[4] =  MASTER_RFID;
    uint8_t slave[5][4] = OTHER_RFID;

    unsigned long start = millis();
    if(nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength,RFID_TIMEOUT*1000)){
        nfc.PrintHex(uid, uidLength);
        if(compArr(uid,master,4)){
            gAuth.success(3,-1);
            return;
        }
        for(int i = 0 ; i<SLAVE_COUNT;i++){
            if(compArr(uid,slave[i],4)){
                gAuth.success(3,i);
                return;
            }
        }
        gAuth.fail(3,0);
        return;
    }
    timeoutFunc();
}

void getKEYAuth(){
    String OtherPin[] = OTHER_PIN;
    if(keypad_code == MASTER_PIN){
        gAuth.success(1, -1);
        return;
    }
    for(int i = 0;i<OTHER_PIN_COUNT;i++){
        if(keypad_code == OtherPin[i]){
            gAuth.success(1,i);
            return;
        }
    }
    if(keypad_code.length() == 6){
        String TOTP_CODE = totp.getCode(myRTC.now().unixtime());
        if(TOTP_CODE == keypad_code){
            gAuth.success(2,0);
            return;
        }
    }
    gAuth.fail(1,0);
    return;
}

void getFingerprintID() {
    uint8_t p = -1;    
    while(p != FINGERPRINT_OK){
        p = finger.getImage();
        switch (p) {
            case FINGERPRINT_OK:
            break;

            case FINGERPRINT_NOFINGER:
            #ifdef DEBUG
                Serial.println(F("."));
            #endif
            break;

            case FINGERPRINT_PACKETRECIEVEERR:
                #ifdef DEBUG
                    Serial.println(F("Communication error"));
                #endif
            break;

            case FINGERPRINT_IMAGEFAIL:
                #ifdef DEBUG
                    Serial.println(F("Imaging error"));
                #endif
            break;

            default:

                #ifdef DEBUG
                    Serial.println(F("Unknown error"));
                #endif
            break;
        }    
    }

    p = finger.image2Tz();
    switch (p) {
        case FINGERPRINT_OK:
        break;

        case FINGERPRINT_IMAGEMESS:
            #ifdef DEBUG
                Serial.println(F("Image too messy"));
            #endif
        gAuth.fail(4,0,F("Image too messy"));
        return;

        case FINGERPRINT_PACKETRECIEVEERR:
            #ifdef DEBUG
                Serial.println(F("Communication error"));
            #endif
        gAuth.fail(4,0,F("Communication error"));
        return;

        case FINGERPRINT_FEATUREFAIL:
            #ifdef DEBUG
                Serial.println(F("Could not find fingerprint features"));
            #endif
        gAuth.fail(4,0,F("Fingerprint Features Error"));
        return;

        case FINGERPRINT_INVALIDIMAGE:
            #ifdef DEBUG
                Serial.println(F("Invalid Image"));
            #endif
        gAuth.fail(4,0,F("Invalid Image"));
        return;

        default:
            #ifdef DEBUG
                Serial.println(F("Unknown error"));
            #endif
        gAuth.fail(4,0,F("Unknown Error"));
        return;
    }

  // OK converted!
    p = finger.fingerSearch();
    if(p == FINGERPRINT_OK) {
        gAuth.success(4,finger.fingerID,finger.confidence);
        return;
    } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
        #ifdef DEBUG
            Serial.println(F("Communication error"));
        #endif
        gAuth.fail(4,0,F("Communication Error"));

    } else if (p == FINGERPRINT_NOTFOUND) {
        #ifdef DEBUG
            Serial.println(F("No match"));
        #endif
        gAuth.fail(4,0,"No Match");
    } else {
        #ifdef DEBUG
            Serial.println(F("Unknown error"));
        #endif
        gAuth.fail(4,0,F("Unknown Error"));
    }
    return;
}

void timeoutFunc(){
    timeStart = false;
    authTime = -1;
    printScreen(F("   ACL Denied  "),F("    TIME OUT   "));
    gAuth.clear();
    AuthOne.clear();
    keypad_code ="";
    state = 0;
    delay(1000);
    mainScreen();
}

void accessDeny(){
    printScreen(F("   ACL Denied  "),F("  Auth Failure "));
    gAuth.clear();
    AuthOne.clear();
    delay(1000);
    mainScreen();
}

void lock(){
    printScreen(F("    ACL LOCK  "),F(""));
    myservo.write(SERVO_LOCK);
    timeStart = false;
    authTime = -1;
    gAuth.clear();
    AuthOne.clear();
    keypad_code ="";
    state = 0;
    delay(1000);
    mainScreen();
}

void authProcess(){
    postAction();
    if(gAuth.fail()){
        accessDeny();
        return;
    }


    if(AuthOne.empty()){
        AuthOne = gAuth;
        gAuth.clear();
        printScreen(F("    2FA Auth   "),F(" Verify Auth... "));
        return;
    }else{

    }

    if(AuthOne.getAuthMethod() == gAuth.getAuthMethod()){
        gAuth.clear();
        printScreen(F(" Duplicate Auth "),F(" Verify Auth... "));
        return;
    }

    if(gAuth.success() && AuthOne.success()){
        printScreen(F("  Auth Success  "),F(" Access Granted "));
        myservo.write(SERVO_UNLOCK);
    }else{
        accessDeny();
        gAuth.clear();
        AuthOne.clear();
        return;
    }
    gAuth.clear();
    AuthOne.clear();
}

void keypadEvent(KeypadEvent key){
    switch (keypad.getState()){
        case PRESSED:
        #ifdef DEBUG
            Serial.println(key);
        #endif

            if((key >= '0' && key <= '9') && state <= 1){
                keypad_code += key;
                displayCode();
                if(!timeStart){
                    timeStart = true;
                    authTime = millis();
                }
                state = 1;
                return;
            }

            if(key == '*' && state <= 1){
                keypad_code.remove(keypad_code.length()-1);
                displayCode();
                return;
            }


            if(key == '#' && (state <= 1 || state == 5)){
                #ifdef DEBUG
                    Serial.println(keypad_code);
                #endif

                lcd.clear();
                getKEYAuth();
                authProcess();

                keypad_code ="";
                state = 0;

                return;
            }

            if(key == 'A' && state == 0){
                state = 2;

                #ifdef DEBUG
                    Serial.print(F("NFC MODE"));
                #endif

                printScreen(F("--- TAP RFID ---"),F(""));

                getNFCAuth();
                authProcess();

                state = 0;
                return;
            }

            if(key == 'B' && state == 0){
                state = 3;

                printScreen(F("-- TAP FINGER --"),F(""));

                getFingerprintID();
                authProcess();
                state = 0;
                return;
            }

            if(key == 'C' && state == 0){
                state = 4;
                Serial.println(F("Init etc"));
                return;
            }

            if (key == 'D') {
                state = 0;
                Serial.println(F("Abort"));
                lock();
                keypad_code = "";
                AuthAbort = true;
                return;
            }
        break;
    }
}

void printScreen(const String &str1, const String &str2){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(str1);
    lcd.setCursor(0,1);
    lcd.print(str2);
}

void mainScreen(){
    printScreen(F("  Just A Vault  "),F("----------------"));
    keypad.setDebounceTime(85);
}

// ======================================== MAIN FUNCTION ======================================== //
void setup(void) {
    #ifdef DEBUG
        Serial.begin(115200);
        Serial.println(F("Hello!"));
    #endif

    myservo.attach(LOCK_PIN);
    myservo.write(SERVO_LOCK);
    ESP_Ser.begin(ESP_BAUD);
    Wire.begin();

    lcd.init();
    lcd.backlight();

    printScreen("  Just A Vault  ", " Mk.1 - ENSC100 ");
    delay(1000);
    printScreen("  Just A Vault  ", "  Softw V1.2.2  ");
    delay(1000);
    
    waitNet();

    renewTime();
    finger.begin(57600);

    nfc.begin();
    if (!nfc.getFirmwareVersion()) {
        #ifdef DEBUG
            Serial.print("Didn't find PN53x board");
        #endif
        while (1);
    }

    keypad.addEventListener(keypadEvent); // Add an event listener for this keypad

    mainScreen();
}

void loop(void) {
    keypad.getKey();
    if((millis() - authTime > AuthTimeout*1000 ) && timeStart){
        timeoutFunc();
    }
}