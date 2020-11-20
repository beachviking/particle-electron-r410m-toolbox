/*
 * Project electron-r410m-setup
 * Description: Simple modem config app for Electron/Boron, LTE models. Loosley based on Rickkas7's work
 * Author: Gunnar L. Larsen
 * Date: 09/01/2020
 */

// includes
#include "SerialCommand.h"
#include "CellularHelper.h"

// put in your 3rd party apn info here
STARTUP(cellular_credentials_set("public", "", "", NULL));

SYSTEM_MODE(MANUAL);
SYSTEM_THREAD(ENABLED);

// 'Serial1' usage allows power measurements
//#define SerialCLI Serial1
#define SerialCLI Serial

Serial1LogHandler logHandler(9600,LOG_LEVEL_TRACE);
SerialCommand sCmd;

bool cellularOn = false;

const unsigned long MODEM_ON_WAIT_TIME_MS = 4000;
const unsigned long CONNECT_WAIT_TIME_MS = 40000;
const unsigned long AT_COMMAND_WAIT_TIME_MS = 10000;

void unrecognized();
void modem_register();
void modem_unregister();
void network_connect();
void network_disconnect();
void particle_connect();
void particle_disconnect();

void set_rat();
void get_rat();

void set_mno();
void get_mno();

void verify_lte_settings();

// setup() runs once, when the device is first turned on.
void setup() {
  // Put initialization like pinMode and begin functions here.
  delay(5000);

  // serial debug init
  SerialCLI.begin(19200);

  // Setup callbacks for SerialCommand commands
  sCmd.addCommand("modemreg", modem_register);
  sCmd.addCommand("modemunreg", modem_unregister);
  sCmd.addCommand("networkconn", network_connect);
  sCmd.addCommand("networkdisconn", network_disconnect);

  sCmd.addCommand("particleconn", particle_connect);
  sCmd.addCommand("particledisconn", particle_disconnect);

  sCmd.addCommand("setrat", set_rat);
  sCmd.addCommand("getrat", get_rat);

  sCmd.addCommand("getmno", get_mno);
  sCmd.addCommand("setmno", set_mno);

  sCmd.addCommand("setuplte", verify_lte_settings);

  sCmd.setDefaultHandler(unrecognized);      // Handler for command that isn't matched

  //while(!SerialCLI.isConnected()) Particle.process();

  Log.info("ready");

  sCmd.listCommands();
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
  // The core of your code will likely live here.
  sCmd.readSerial();     // We don't do much, just process serial commands
}

// This gets set as the default handler, and gets called when no other command matches.
void unrecognized(const char *) {
  Log.info("Unsupported command");
  Log.info("Supported commands are:");
  sCmd.listCommands();
}

void modem_register()
{
  unsigned long stateTime = 0;
  bool connected = false;
  stateTime = millis();

  Log.info("registering modem onto the cellular network...");
  Cellular.on();

  while (millis() - stateTime <= MODEM_ON_WAIT_TIME_MS) {}

  unsigned long elapsed = millis() - stateTime;
  Log.info("registered on the cellular network in %lu milliseconds", elapsed);

  Log.info("modem initialized");
  Log.info("manufacturer=%s", CellularHelper.getManufacturer().c_str());
  Log.info("model=%s", CellularHelper.getModel().c_str());
  Log.info("firmware version=%s", CellularHelper.getFirmwareVersion().c_str());
  Log.info("ordering code=%s", CellularHelper.getOrderingCode().c_str());
  Log.info("IMEI=%s", CellularHelper.getIMEI().c_str());
  Log.info("IMSI=%s", CellularHelper.getIMSI().c_str());
  Log.info("ICCID=%s", CellularHelper.getICCID().c_str());

  cellularOn = true;
}

void modem_unregister()
{
  unsigned long stateTime = 0;
  bool connected = false;
  stateTime = millis();

  Log.info("unregistering modem from the cellular network...");
  Cellular.off();

  while (millis() - stateTime <= MODEM_ON_WAIT_TIME_MS) {}
  unsigned long elapsed = millis() - stateTime;
  Log.info("unregistered from the cellular network in %lu milliseconds", elapsed);

  cellularOn = false;
}

void network_connect() {
  unsigned long stateTime = 0;
  stateTime = millis();

  Log.info("requesting a data connection on the cellular network...");

  Cellular.connect();

  while ((millis() - stateTime <= CONNECT_WAIT_TIME_MS) && !(Cellular.ready())) {}

  if (Cellular.ready()) {
    unsigned long elapsed = millis() - stateTime;

    Log.info("obtained a data connection in %lu milliseconds", elapsed);
    Log.info("operator name=%s", CellularHelper.getOperatorName().c_str());

  	CellularHelperRSSIQualResponse rssiQual = CellularHelper.getRSSIQual();
  	int bars = CellularHelperClass::rssiToBars(rssiQual.rssi);

  	Log.info("rssi=%d, qual=%d, bars=%d", rssiQual.rssi, rssiQual.qual, bars);

  	CellularHelperLocationResponse locResp = CellularHelper.getLocation();
  	Log.info(locResp.toString());

  	Log.info("ping 8.8.8.8=%d", CellularHelper.ping("8.8.8.8"));

  	Log.info("dns device.spark.io=%s", CellularHelper.dnsLookup("device.spark.io").toString().c_str());
  }
  else
  if (Cellular.listening()) {
    Log.info("entered listening mode (blinking dark blue) - probably no SIM installed");
  }
}

void network_disconnect() {
  unsigned long stateTime = 0;
  stateTime = millis();
  Log.info("disconnecting data connection...");
  Cellular.disconnect();
  unsigned long elapsed = millis() - stateTime;
  Log.info("disconnected data connection in %lu milliseconds", elapsed);
}

void particle_connect() {
  if (Particle.connected() == false) {
    Log.info("Connecting to the Particle cloud...");
    Particle.connect();
  }
}

void particle_disconnect() {
  if (Particle.connected() == true) {
    Log.info("Disconnecting from the Particle cloud...");
    Particle.disconnect();
  }
}

void get_rat() {
  Log.info("querying for RAT");
  Log.info("RAT=%s", CellularHelper.getRAT().c_str());
}

void set_rat() {
  char *arg;
  int primary = 0;
  int secondary = 0;

  arg = sCmd.next();
  if (arg != NULL) {
    primary = atoi(arg);    // Converts a char string to an integer
  }
  else {
    Log.info("no valid RAT value given");
    return;
  }

  arg = sCmd.next();
  if (arg != NULL) {
    secondary = atol(arg);
    CellularHelper.setRAT(primary, secondary);
  }
  else {
    // no second arg given
    CellularHelper.setRAT(primary);
  }
}

void get_mno() {
  Log.info("querying for MNO");
  Log.info("mno=%d", CellularHelper.getMNO());
}

// For EU, this needs to be set to 100!
void set_mno() {
  char *szIndex;
  int profile;
  int sel;
  int pref;

  szIndex = sCmd.next();    // Get the next argument from the SerialCommand object buffer

  sscanf(szIndex,"%d", &profile);
  CellularHelper.setMNO(profile);

}

void verify_lte_settings() {
	int args[2];
  String ratResult;
  int tokens;

  if (cellularOn == false)
  {
    Log.info("turn on modem first!");
    return;
  }

  Log.info("model=%s", CellularHelper.getModel().c_str());

  if(CellularHelper.isLTE())
  {
    // check MNO
    // Factory default is 0
    // Particle OS may set this to 1 if it sees the 0
    // For EU, must be set to 100

    if(CellularHelper.getMNO() != 100)
    {
      Log.error("MNO is NOT ok - correcting");
      CellularHelper.setMNO(100);
      delay(MODEM_ON_WAIT_TIME_MS);
      Cellular.off();
      delay(MODEM_ON_WAIT_TIME_MS);
      Cellular.on();
      delay(MODEM_ON_WAIT_TIME_MS);
    }

    Log.info("MNO is ok");

    // check RAT
    // Factory default is 7,8 (7=cat M1, 8=NB-IoT)
    // For our application, must be set to 7
    ratResult = CellularHelper.getRAT();
    tokens = sscanf((ratResult.c_str()), "%u,%u", &args[0], &args[1]);

    if (tokens != 1 || args[0] != 7) {
      Log.error("RAT is NOT ok - correcting");
      CellularHelper.setRAT(7);
      delay(MODEM_ON_WAIT_TIME_MS);
      Cellular.off();
      delay(MODEM_ON_WAIT_TIME_MS);
      Cellular.on();
      delay(MODEM_ON_WAIT_TIME_MS);
    }

    Log.info("RAT is ok");
    Log.info("LTE setup verified");
  } // isLTE
}
