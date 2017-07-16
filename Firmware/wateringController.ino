/*
 Code for the Particle Photon board.
 
 Released under GPL v2 license.

 Operates an array of pins that are intended to operate a watering system.
 
 Code is built around a job scheduler concept: When watering is required, first time is queued against the relevant solenoid. Then the queue monitoring loop picks
   up the queued time when capacity is available, starts the pin, and sets a switch-off time. A switch-off monitoring loop stops solenoids when their time has expired.
*/

//Number of watering channels.
const byte SOLENOID_COUNT = 5;

//Limits the number of concurrently active channels, to manage strain on small power supplies.
const byte MAX_RUNNING_SOLENOIDS = 1;

//Do not schedule any channel for more than on hour.
const int MAX_WATERING_TIME = 3600;

int _solenoidPins[SOLENOID_COUNT] = {D2, D3, D4, D5, D6};

//Serves as a queue for solenoids that need to be run. Value in this list is number of seconds the solenoid needs to run.
int _solenoidWaitingSeconds[SOLENOID_COUNT];

//Holds the time that each currently running solenoid should be turned off
int _solenoidOffTime[SOLENOID_COUNT];

byte _currentlyRunningSolenoids = 0;

void setup() {
    //Uncomment if you have installed an external antenna - which can be handy for outdoor mounting.
    //WiFi.selectAntenna(ANT_EXTERNAL);
    
    for(int i =0; i < SOLENOID_COUNT; i++)
    {
        pinMode(_solenoidPins[i], OUTPUT);
        _solenoidWaitingSeconds[i] = 0;
        _solenoidOffTime[i] = 0;
    }
    
    pinMode(D7, OUTPUT);
    
    Particle.function("queueWater", beginSolenoidSession);
    Particle.function("stopAll", stopAllSessions);
    Particle.function("getTimeOnCh", getSolenoidTimeToRun);
    Particle.function("getChCount", getSolenoidCount);
    Particle.function("isChActive", isSolenoidActive);
}

void loop() {
    //Check for solenoids to turn off
    int curTime = Time.now();
    byte runningSolenoids = 0;
    for(int i = 0; i < SOLENOID_COUNT; i++) {
        if (_solenoidOffTime[i] < curTime) {
            stopSolenoid(i);
        } else {
            runningSolenoids++;
        }
    }
    
    _currentlyRunningSolenoids = runningSolenoids;

    //Check for solenoids to turn on
    if(_currentlyRunningSolenoids < MAX_RUNNING_SOLENOIDS)
    {
        for(int i = 0; i < SOLENOID_COUNT; i++) {
            if(_solenoidWaitingSeconds[i] > 0) {
                startSolenoid(i);
                break;
            }
        }
    }
    
    
}



void startSolenoid(int solenoidId) {
    int offTime = Time.now() + _solenoidWaitingSeconds[solenoidId];
    _solenoidWaitingSeconds[solenoidId] = 0;
    _solenoidOffTime[solenoidId] = offTime;
    digitalWrite(_solenoidPins[solenoidId], HIGH);
    
    //Send notification that run has started
    if(!Particle.publish("started",String(solenoidId),300,PRIVATE)) {
        //Publish didn't work - queue message
    }
    
    //Provide time for events to be sent
    Particle.process();
    delay(1000);
}

void stopSolenoid(int solenoidId) {
    digitalWrite(_solenoidPins[solenoidId], LOW);
    
    if(_solenoidOffTime[solenoidId] != 0) {
        //Send notification that run has ended
        if(!Particle.publish("stopped",String(solenoidId),300,PRIVATE)) {
            //Publish didn't work - queue message
        }
        
        _solenoidOffTime[solenoidId] = 0;
        
        //Provide time for events to be sent
        Particle.process();
        delay(1000);
    }
}

//Cloud function handler to start a solenoid up
//Expects a string CCSSSS , where CC is the channel number and S is a 1 to 4 digit number indicating how long to turn the channel on for.
int beginSolenoidSession(String params) {
    int minutes = 0, solenoid = 0;
    
    if(params.length() < 3) return -3;
    
    solenoid = params.substring(0,2).toInt();
    minutes =  params.substring(2).toInt();
    
    if(solenoid >= 0 && solenoid < SOLENOID_COUNT) {
        if(minutes > 0 && minutes + _solenoidWaitingSeconds[solenoid] < MAX_WATERING_TIME) {
            _solenoidWaitingSeconds[solenoid] += minutes;
            if(_currentlyRunningSolenoids < MAX_RUNNING_SOLENOIDS) {
                return 0;
            } else {
                return 1;   
            }
        } else {
            return -2;
        }
    } else {
        return -1;
        
    }
}

//Cancels all current runs and empties the waiting list
int stopAllSessions(String params) {
    int rc = 0;
    
    for(int i = 0; i < SOLENOID_COUNT; i++) {
        _solenoidWaitingSeconds[i] = 0;
        if(_solenoidOffTime[i] > 0) {
            _solenoidOffTime[i] = Time.now() + 1;
            rc = 1;
        } else {
            _solenoidOffTime[i] = 0;
        }
    }
    return rc;
}

//Returns the sum of any current session and queued time for the target solenoid
int getSolenoidTimeToRun(String params) {
    int secondsToRun = 0;
    int solenoid;
    
    if(params.length() < 1) return -3;
    solenoid = params.toInt();
    
    if(solenoid >= 0 && solenoid < SOLENOID_COUNT) {
        if(_solenoidOffTime[solenoid] > 0) {
            secondsToRun = _solenoidOffTime[solenoid] - Time.now();
        }
        secondsToRun += _solenoidWaitingSeconds[solenoid];
        return secondsToRun;
    } else {
        return -1;
    }
}

//Indicates whether a solenoid channel is currently on
int isSolenoidActive(String params) {
    int solenoid;
    
    if(params.length() < 1) return -3;
    solenoid = params.toInt();
    
    if(solenoid >= 0 && solenoid < SOLENOID_COUNT) {
        if(_solenoidOffTime[solenoid] > 0) {
            return 1;
        }
        return 0;
    } else {
        return -1;
    }
}

//Returns the number of solenoid channels available
int getSolenoidCount(String params) {
    return SOLENOID_COUNT;
}
