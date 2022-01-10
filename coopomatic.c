//motorshield library
#include <Adafruit-MotorShield-V2.h>
//I2C library
#include <Wire.h>



Adafruit_MotorShield AFMS = Adafruit_MotorShield();
Adafruit_DCMotor *myMotor = AFMS.getMotor(1);

// define some variables.

double lightHours = 13.5; 
int Reed1 = D5;
int Reed2 = D3;
int light = A1;
int photoresistor = A2;
int lightValue;
int openTime = 0;
int darkTime = 0;
int closeDelay = 45; //in minutes
int a;
int b;
bool powerSaving = false;
//normally 25
int brightOpen = 10;
//7 was too early; doesn't seem to be much difference until you get real low
int brightClose = 3;

double tempLight = 3.5; //in hours
int secondDelay = 15;
bool lightOn = false;
bool open = false;
bool doorFail = false;
bool debug = false;
bool manualMode = false;
String eventName = "twilio_sms";
float batterySoc;
float battWarning = 20.0;
bool battWarned = false;
bool powerSavingSwitch = true;

int heatLampOn = 0;
int lightPublishConfirm = 0;

float getVCell();
FuelGauge fuel;

//define functions
int doorControl(String command);
int lightControl(String command);
int debugControl(String command);
int lightHoursControl(String command);
int delayControl(String command);
int powerSavingControl(String command);
int brightCloseControl(String command);
int brightOpenControl(String command);
int secondDelayControl(String command);
int battWarningControl(String command);

SystemSleepConfiguration sleepConfig;
SystemSleepConfiguration longSleepConfig;



void setup() {


    
	if (creekGarden){
	    AFMS = Adafruit_MotorShield(0x61);
	    myMotor = AFMS.getMotor(1);
	    lightHours = 0;
	    tempLight = 0;
	    powerSaving = true;
	} 
	
    
    //declare functions
    Particle.function( "doorControl", doorControl );
    Particle.function( "lightControl", lightControl );
    Particle.function( "debugControl", debugControl );
    Particle.function( "lightHoursControl", lightHoursControl );
    Particle.function( "delayControl", delayControl );
    Particle.function( "powerSavingControl", powerSavingControl );
    Particle.function( "brightCloseControl", brightCloseControl);
    Particle.function( "brightOpenControl", brightOpenControl);
    Particle.function( "secondDelayControl", secondDelayControl);
    Particle.function( "battWarningControl", battWarningControl);
    
    Particle.subscribe("confirmLightPublish",confirmLightPublishHandler, MY_DEVICES);
    Particle.subscribe("heatLamp", heatLampHandler, MY_DEVICES);
    
  // declare ins and outs
  
  pinMode(photoresistor, INPUT);
  pinMode(Reed1, INPUT);
  pinMode(Reed2, INPUT);
  pinMode(light, OUTPUT);
  
  //Set time zone
  Time.zone(-8);


  //start talking to the motorshield
  AFMS.begin();
  
  
  // Set the speed to start, from 0 (off) to 255 (max speed)
  //was 150...maybe this will be too fast?
  myMotor->setSpeed(255);
    
  //set sleep mode so it wakes if door is moved
  sleepConfig.mode(SystemSleepMode::ULTRA_LOW_POWER).duration(20min).gpio(D3, CHANGE).gpio(D5, CHANGE);
  batterySoc = fuel.getSoC();
  if (hasPower()){
    sendEvent(eventName, Time.format(Time.now(),"System rebooted at %I:%M%p."));
  } else {
    sendEvent(eventName, Time.format(Time.now(),"System rebooted at %I:%M%p."+ String::format(" Batt %.1fV",fuel.getVCell())+String::format(", %.1f",batterySoc))+"%");
  }

}

void loop() {
    
    
    //get value of photoresistor
    lightValue = analogRead(photoresistor);
    
    //reset counts
    a=0;
    b=0;
    
    
    //release the motor just in case
    myMotor->run(RELEASE);
    
    if (digitalRead(Reed1)==HIGH){
        open = true;
    }
    if (digitalRead(Reed2)==HIGH){
        open = false;
    }
    
    //if the amount of required light has been reached and the light is on
    if (lightHours*60*60 - (Time.now() - openTime) < 0 && lightOn && heatLampOn == 0){
        //turn light off
        analogWrite(light,0);
        lightOn = false;
    }
    
    //if darkTime is closeDelay ago and door is up, close door
    if (darkTime != 0 && darkTime - Time.now() + closeDelay*60 < 0 && digitalRead(Reed2)==LOW && !manualMode){
        
        //close door
        doorControl("close");
        
        //if connected to internet
        if (Particle.connected()){
            //get system time
            Particle.syncTime();
        }
        //if openTime is set and the difference in time is less than lightHours
        if (openTime == 0){
            openTime = Time.now() - lightHours*60*60 + tempLight*60*60;
            delay(5000);
        }
        if (digitalRead(Reed2)==LOW && !doorFail){
            sendEvent(eventName, Time.format(Time.now(),"Door FAILED to close!"));
            doorFail = true;
        }
        if (Time.now() - openTime < lightHours*60*60 && digitalRead(Reed2)==HIGH){
            analogWrite(light,255);
            lightOn = true;
            sendEvent(eventName, Time.format(Time.now(),"Door closed at %I:%M%p.")+Time.format(openTime + lightHours*60*60," Light on until %I:%M%p."));
            doorFail = false;
        } 
        if (Time.now() - openTime > lightHours*60*60 && digitalRead(Reed2)==HIGH){
        //publish time of closed door
            if (hasPower()){
                sendEvent(eventName, Time.format(Time.now(),"Door closed at %I:%M%p."));
            } else {
                sendEvent(eventName, Time.format(Time.now(),"Door closed at %I:%M%p." + String::format(" Batt %.1fV",fuel.getVCell())+String::format(", %.1f",batterySoc))+"%");
            }
            doorFail = false;
        }
        //if door closed, reset darktime
        if (doorFail == false){
            darkTime = 0;
            if (powerSaving){
               long_sleep();
            }
        }
    }
    
    //if door up
    if(darkTime == 0 && digitalRead(Reed2)==LOW && !manualMode){
        //while dark
        while(lightValue < brightClose){
            //reset counter
            b=0;
            //read the light level
            lightValue = analogRead(photoresistor);
            //wait a sec
            delay(1000);
            //if a set number of seconds pass while dark
            if (a==secondDelay){
                //record the time
                darkTime = Time.now();
                
                //publish time door closed.     
                sendEvent("darkTime", Time.format(darkTime,"Dark at %I:%M%p."));
                
                
                
                //exit the while loop so it can start over
                break;
            }
            //increase the a count until secondDelay or brightness is too high
            a++;
        }
    }
    //release the motor just in case
    myMotor->run(RELEASE);
    //if door closed
    if(digitalRead(Reed1)==LOW && !manualMode){
        //while light
        while(lightValue > brightOpen){
            //reset counter
            a=0;
            //read the light level
            lightValue = analogRead(photoresistor);
            //wait a sec
            delay(1000);
            //if secondDelay seconds pass while light
            if (b==secondDelay){
                doorControl("open");
                //record openTime
                openTime = Time.now();
                darkTime = 0;
                //text time the door opened
                if (digitalRead(Reed1) == LOW){
                    sendEvent(eventName, Time.format(openTime,"Door FAILED to open!"));
                } else {
                    
                    if (hasPower()){
                        sendEvent(eventName, Time.format(openTime,"Door opened at %I:%M%p."));
                    } else {
                        sendEvent(eventName, Time.format(openTime,"Door opened at %I:%M%p." + String::format(" Batt %.1fV",fuel.getVCell())+String::format(", %.1f",batterySoc))+"%");
                    }
                }
                
                
                if (powerSaving){
                    long_sleep();
                }
                //exit the while loop so it can start over
                break;
            }
            //increase the b count until secondDelay or brightness is too high
            b++;
        }
    }
    //release the motor just in case
    myMotor->run(RELEASE);
    
    
   if (!hasPower()){
        //batterySoc = System.batteryCharge();
        batterySoc = fuel.getSoC();
        if (batterySoc < battWarning && !battWarned){
            sendEvent(eventName, Time.format(openTime,"Battery low."));
            battWarned = true;
        }
        if (batterySoc > battWarning+5 && battWarned){
            battWarned = false;
        }
    }
    
    //for debug
    if (debug){
        sendDebug();
    }
    
    
    
    
    
    //if you want it to sleep between checking
    delay(60000);
    
    if (hasPower() || !powerSavingSwitch){
        powerSaving = false;
    } else {
        powerSaving = true;
    }
    
    if (powerSaving){
        int preSleep = Time.now();
        System.sleep(sleepConfig);
        delay(15000);
        if (Time.now()-preSleep < 20*60){
            sendEvent(eventName, Time.format(openTime,"Something moved the door."));
        }
    }
    
    
}

void sendDebug() {
    sendEvent("brightness", String::format("%i",analogRead(photoresistor)));
    sendEvent("brightClose", String::format("%i",brightClose));
    sendEvent("brightOpen", String::format("%i",brightOpen));
    sendEvent("darkTime", Time.format(darkTime,"%I:%M%p.")+String::format(", %i",darkTime));
    sendEvent("openTime", Time.format(openTime,"%I:%M%p.")+String::format(", %i",openTime));
    sendEvent("lightHours", String::format("%.2f",lightHours));
    sendEvent("tempLight", String::format("%.2f",tempLight));
    sendEvent("closeDelay", String::format("%i",closeDelay));
    sendEvent("secondDelay", String::format("%i",secondDelay));
    sendEvent("heatLampOn", String::format("%i",heatLampOn));
    if (open){
        sendEvent("open", "true");
    } else {
        sendEvent("open", "false");
    }
    if (lightOn){
        sendEvent("lightOn", "true");
    } else {
        sendEvent("lightOn", "false");
    }
    if (manualMode){
        sendEvent("manualMode", "true");
    } else {
        sendEvent("manualMode", "false");
    }
    if (powerSaving){
        sendEvent("powerSaving", "true");
    } else {
        sendEvent("powerSaving", "false");
    }
    sendEvent("battery", String::format("%.1fV",fuel.getVCell())+String::format(", %.1f",batterySoc)+"%");
    sendEvent("battWarning", String::format("%.1f",battWarning));
}

void confirmLightPublishHandler(const char *event, const char *data){
    char dataCopy[strlen(data)+1];
    strncpy(dataCopy, data, sizeof(dataCopy));
    
    if (strncmp(dataCopy, "light",256) == 0){
        lightPublishConfirm = 1;
        sendEvent("confirmedLightPublish", "Confirmed light");
    }
    if (strncmp(dataCopy, "dark",256) == 0){
        lightPublishConfirm = 0;
        sendEvent("confirmedLightPublish", "Confirmed dark");
    }
}

void heatLampHandler(const char *event, const char *data){
    char dataCopy[strlen(data)+1];
    strncpy(dataCopy, data, sizeof(dataCopy));
    
    if (strncmp(dataCopy, "on",256) == 0){
        /*
        analogWrite(light,255);
        lightOn = true;
        heatLampOn = 1;
        */
        sendEvent("confirmHeatLamp", "on");
    }
    if (strncmp(dataCopy, "off",256) == 0){
        /*
        if (lightHours*60*60 - (Time.now() - openTime) > 0 && lightOn){
            analogWrite(light,0);
            lightOn = false;
        }
        heatLampOn = 0;
        */
        sendEvent("confirmHeatLamp", "off");
    }
}

void long_sleep(){
    int preSleep = Time.now();
    longSleepConfig.mode(SystemSleepMode::ULTRA_LOW_POWER).duration(480min).gpio(Reed1, CHANGE).gpio(Reed2, CHANGE);
    System.sleep(longSleepConfig);
    delay(15000);
    if (Time.now()-preSleep < 480*60){
        sendEvent(eventName, Time.format(openTime,"Something moved the door."));
    }
}

bool is_number(String line)
{
    char* p;
    strtol(line.c_str(), &p, 10);
    return *p == 0;
}

int doorControl(String command){
    if (command == "close"){
        if (digitalRead(Reed2)==LOW){
            //lower door
            myMotor->run(FORWARD);
            //wait for reed sensor to clear
            delay(1000);
            //while both reed sensors read low (in case the door gets stuck and it goes back up again)
            int start = Time.now();
            while(digitalRead(Reed2)==LOW && digitalRead(Reed1)==LOW && Time.now() - start < 100){
                myMotor->run(FORWARD);
            }
        
            //if door open reed switch is triggered again (the door didn't close all the way and rewound itself)
            if(digitalRead(Reed1)==HIGH){
                //unwind the motor
                myMotor->run(BACKWARD);
                delay(1000);
                //run the motor until another switch is triggered
                //if it gets stuck again and goes all the way through the rotation, it will try again after secondDelay seconds
                while(digitalRead(Reed2)==LOW && digitalRead(Reed1)==LOW){
                    myMotor->run(BACKWARD);
                }
            }
            //stop closing door
            myMotor->run(RELEASE);
            if (digitalRead(Reed2)==LOW){
                //failed to close
                sendEvent("doorControl", "Door FAILED to close!");
                return 3;
            } else {
                //closed
                sendEvent("doorControl", "Door closed.");
                return 2;
            }
        } else {
            //already closed
            sendEvent("doorControl", "Door already closed");
            return 0;
        }
    } else if (command == "open"){
        if (digitalRead(Reed1)==LOW){
            //if door open reed switch reads low
            int start = Time.now();
            while(digitalRead(Reed1)==LOW && Time.now() - start < 100){
                //open the door
                myMotor->run(BACKWARD);
            }
            myMotor->run(RELEASE);
            //opened
            if (digitalRead(Reed1)==HIGH){
                sendEvent("doorControl", "Door opened.");
                return 1;
            } else {
                sendEvent("doorControl", "Door FAILED to open!");
                return 6;
            }
        } else {
            //already open
            sendEvent("doorControl", "Door already open.");
            return 0;
        }
    } else if (command == "manual"){
        manualMode = true;   
        sendEvent("doorControl", "Manual mode on.");
        return 4;
    } else if (command == "auto"){
        manualMode = false;   
        sendEvent("doorControl", "Manual mode off.");
        return 5;
    } else{
        sendEvent("doorControl", "Invalid argument.");
        return -1;
    }
}


int lightControl(String command){
    if (command == "on"){
        analogWrite(light,255);
        lightOn = true;
        sendEvent("lightControl", "Light on.");
        return 1;
    } else if (command == "off"){
        analogWrite(light,0);
        lightOn = false;
        sendEvent("lightControl", "Light off.");
        return 0;
    } else {
        sendEvent("lightControl", "Invalid argument.");
        return -1;
    }
}

int debugControl(String command){
    if (command == "on"){
        debug = true;
        sendEvent("debugControl", "Debug on.");
        return 1;
    } else if (command == "off"){
        sendEvent("debugControl", "Debug off.");
        debug = false;
        return 0;
    } else if (command == "1"){
        sendDebug();
        return 2;
    } else {
        sendEvent("debugControl", "Invalid argument.");
        return -1;
    }
}

int lightHoursControl(String command){
    if (is_number( command)){
        lightHours = atof( command.c_str() );
        sendEvent("lightHoursControl", String::format("Light hours set to %.2f.",lightHours));
        return 1;
    } else {
        sendEvent("lightHoursControl", "Invalid input");
        return -1;
    }
    //tempLight
}

int delayControl(String command){
    if (is_number( command)){
        closeDelay = atoi( command.c_str() );
        sendEvent("delayControl", String::format("Close delay set to %i.",closeDelay));
        return 1;
    } else {
        return -1;
    }
}

int powerSavingControl(String command){
    if (command == "on"){
        powerSavingSwitch = true;
        sendEvent("powerSavingControl", "Power saving on.");
        return 1;
    } else if (command == "off"){
        sendEvent("powerSavingControl", "Power saving off.");
        powerSavingSwitch = false;
        return 0;
    } else {
        sendEvent("powerSavingControl", "Invalid argument.");
        return -1;
    }
}

int brightCloseControl(String command){
    if (is_number( command)){
        brightClose = atoi( command.c_str() );
        sendEvent("brightCloseControl", String::format("Bright close set to %i.",brightClose));
        return 1;
    } else {
        return -1;
    }
}

int brightOpenControl(String command){
    if (is_number( command)){
        brightOpen = atoi( command.c_str() );
        sendEvent("brightOpenControl", String::format("Bright open set to %i.",brightOpen));
        return 1;
    } else {
        return -1;
    }
}

int secondDelayControl(String command){
    if (is_number( command)){
        secondDelay = atoi( command.c_str() );
        sendEvent("secondDelayControl", String::format("Second delay set to %i.",secondDelay));
        return 1;
    } else {
        return -1;
    }
}

int battWarningControl(String command){
    if (is_number( command)){
        battWarning = atoi( command.c_str() );
        sendEvent("battWarningControl", String::format("Battery warning set to %i.",battWarning));
        return 1;
    } else {
        return -1;
    }
}

void sendEvent(String name, String data){
    if (Particle.connected()){
        Particle.publish(name, data, PRIVATE);
    }
    delay(1000);
}

bool hasPower() {
	// Bit 2 (mask 0x4) == PG_STAT. If non-zero, power is good
	// This means we're powered off USB or VIN, so we don't know for sure if there's a battery
	/*
	PMIC pmic;
	byte systemStatus = pmic.getSystemStatus();
	return ((systemStatus & 0x04) != 0);
	*/
	if (fuel.getVCell() == -1){
	return true;
	} else {
	    return false;
	}
}

