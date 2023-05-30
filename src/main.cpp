
// #include <BLEDevice.h>
// #include <BLEServer.h>
// #include <BLEUtils.h>
// #include <BLE2902.h>



#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLE2904.h>

// For seeed studio ESP32C3
#include <HardwareSerial.h>
#include <EEPROM.h>


// UUID for the custom service and characteristics
#define SERVICE_UUID "ffe0"
#define CHARACTERISTIC_UUID "ffe1"

// Set this to 1 to enable advertising at startup, or 0 if it will remain off until commanded on by the UART interface (with AT+DTY0)
#define DEFAULT_ADVERTISING 1
#define DEFAULT_DEVICE_NAME "PKL24-ESP"
#define DEVICE_NAME_ADDRESS 0x00
#define DEVICE_NAME_LENGTH 32


/* Some global variables and objects */

char deviceName[32] = DEFAULT_DEVICE_NAME;

// Characteristic value to be updated periodically
uint16_t characteristicValue = 0;

// Data structure to store client handles
std::vector<uint16_t> clientIDs;

// Create a server instance
NimBLEServer *pServer;

// Create a custom service
NimBLEService *pService;

// Create a custom characteristic
NimBLECharacteristic *pCharacteristic;

// Advertising object
NimBLEAdvertising *pAdvertising;

bool deviceConnected = false;  // Reflects whether there is a device connected or not
bool advertising = false;  // Reflects whether we are advertising or not
bool oldDeviceConnected = false;
char txValue = 0;


/* Function prototypes */
void setup_ble_peripheral();
void send_ble_data_str (const char* str);
// void update_ble();
int disconnect_all_clients();
bool disable_advertising();
void parseFunctionBLE (const char* data);
void parseFunctionUART (String data);
bool stringNotEmpty (String mystr);
bool write_device_name_to_eeprom();
void attempt_restore_device_name();
String getParameter(String inputString);


/* U2UXD  GPIO on ESP32dev board */
// const int readPin = 16; 
// const int writePin = 17; // Use GPIO number. See ESP32 board pinouts

HardwareSerial MySerial(0);  // For seeed studio ESP32C3


// /* Callbacks for client disconnect & connect events.  */
// class MyServerCallbacks : public NimBLEServerCallbacks {
//     void onConnect(NimBLEServer *pServer, ble_gap_conn_desc connection_desc) 
//     {
//         // Print the client's BLE address to the console
//         deviceConnected = true;
//         MySerial.println("OK+CONN");
//         Serial.print("Client connected. Address: ");
//         // Serial.println(clientAddressString.c_str());
//         Serial.println(NimBLEAddress(connection_desc.peer_ota_addr).toString().c_str());

//     };

//     void onDisconnect(NimBLEServer *pServer, ble_gap_conn_desc connection_desc) 
//     {
//         deviceConnected = false;
//         // Print the client's BLE address to the console
//         MySerial.println("OK+LOST");
//         Serial.print("Client disconnected. Address: ");
//         // Serial.println(clientAddressString.c_str());
//         Serial.println(NimBLEAddress(connection_desc.peer_ota_addr).toString().c_str());        
        
//     };
    
// };


/* Unfortunately, none of this stuff actually works.  The Server callbacks are never called. I wasn't able to figure out
 * the cause of this, so I worked around it
 */
class MyServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
        Serial.print("Client address: ");
        Serial.println (connInfo.getAddress().toString().c_str());
        
        /** We can use the connection handle here to ask for different connection parameters.
         *  Args: connection handle, min connection interval, max connection interval
         *  latency, supervision timeout.
         *  Units; Min/Max Intervals: 1.25 millisecond increments.
         *  Latency: number of intervals allowed to skip.
         *  Timeout: 10 millisecond increments, try for 3x interval time for best results.  
         */
        // pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 18);
    };
    
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
        printf("Client disconnected - start advertising\n");
        if (DEFAULT_ADVERTISING) {NimBLEDevice::startAdvertising();}
    };

    // void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) {
    //     printf("MTU updated: %u for connection ID: %u\n", MTU, connInfo.getConnHandle());
    //     pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 60);
    // };
};


/* Callback function for characteristic write events, calls the parseFunc on new data */
class MyCallbacks : public NimBLECharacteristicCallbacks 
{
    public: 
    // Constructor that takes a pointer to the parse function
    MyCallbacks(void (*parseFunc)(const char* data)) : m_parseFunc(parseFunc) {}

    void onWrite(NimBLECharacteristic *pCharacteristic) {
        // Retrieve the value sent by the client
        std::string value = pCharacteristic->getValue();

        // Print the value to the console
        // Serial.print("Characteristic value changed: ");
        // Serial.println(value.c_str());
        m_parseFunc(value.c_str());
    }

    void onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* connection_desc, uint16_t subValue) {
        Serial.println("Client subscribed to characteristic");
        MySerial.println("OK+CONN");
    };

    private:
        // Member variable that stores the pointer to the parse function
        void (*m_parseFunc)(const char* data);
};



void setup() {
    /* Monitor UART0 for debugging */
    Serial.begin(115200);

    // Wait for serial monitor to open
    while(!Serial);    
    delay (1000);

    /* UART2 is used as a bridge to send/receive data over BLE */
    MySerial.begin(9600, SERIAL_8N1, RX, TX);   // AKA GPIO D7 and D6 on the Seeed Studio ESP32C3

    EEPROM.begin(512);  // Initialize EEPROM

    // Attempt to restore the saved deviceName from EEPROM.  If it doesn't exist, use the default
    attempt_restore_device_name();

    setup_ble_peripheral();
    Serial.println("Waiting for client connection...");
}



void loop() {

    String data;  // A buffer to collect data from the UART before it is sent out to BLE
    String str;
    String chunk;

    // Workaround to handle device connections and disconnections because callbacks don't work
    deviceConnected = (pServer->getConnectedCount() > 0);
    if (oldDeviceConnected )
    if (deviceConnected != oldDeviceConnected) {
        if (deviceConnected) {
            Serial.println("Client connected");
            // We don't send a message here because the onSubscribe BLE callback will do it
        }
        else {
            Serial.println("Client disconnected");
            MySerial.println("OK+LOST");
        }
        oldDeviceConnected = deviceConnected;
    }

    /* Get data from the UART, parse it, and send it to BLE   */
    // while (MySerial.available()) {
    // Apparently MySerial.read() can be told to return only one character at time 
    // based on the type of the variable it is assigned to, who knew?
    // char c = MySerial.read();
    // read from port 1, send to port 0:
    if (MySerial.available()) {
    str.concat(MySerial.readStringUntil('\r'));
    // At this point we will have a complete line, so remove any newline or carriage returns
    str.replace("\r", "");  
    str.replace("\n", "");
    // Serial.println("Got:" + c);  // Echo the character to the console (for debugging
    Serial.println("Got:" + str);  // Echo the character to the console (for debugging
    // if (c == '\n') {  // If we have a complete line, let's look at it
    //   if (data.length() > 0) {
        // Serial.println("Read from UART: " + data);
        // parseFunctionUART(data.c_str());  // Parse the data and (possibly) send it to BLE        
        parseFunctionUART(str.c_str());  // Parse the data and (possibly) send it to BLE
        // data = "";  // Clear the buffer
        str = "";  // Clear the buffer

        delay(10); // Just a safeguard, as bluetooth stack could go into congestion, if too many packets are sent
    }
      
//       data = "";  // Nothing of interest, clear the buffer and move on
//     } else {
//       data += c;  // Append this character to the buffer string
//     }
//   }
      
    
	// }

    // else {
    //     Serial.println("No device connected");
    //     Serial.print ("getConnectedCount(): ");
    //     Serial.println (pServer->getConnectedCount());

    //     delay(1000);
    // }

}






/* This is the main entry point for this bluetooth implementation.  Call this from your code to start it up */
void setup_ble_peripheral()
{
    Serial.println("Setting up BLE Peripheral...");
    
    // Initialize the BLE stack
    NimBLEDevice::init("");

    /* set the transmit power, default is 3db */
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */

    // Create the server instance
    pServer = NimBLEDevice::createServer();

    // Instantiate Server Callbacks and tell the server instance to use them
    pServer->setCallbacks(new MyServerCallbacks());

    // Create the custom service
    pService = pServer->createService(SERVICE_UUID);

    
    // Create the custom characteristic
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY
    );

    // Set the characteristic callbacks
    pCharacteristic->setCallbacks(new MyCallbacks(parseFunctionBLE));
    // Start the service
    pService->start();

    // Setup advertising the service
    // NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setName(deviceName);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    // This is no longer necessarily started by default, it can be enabled with AT+ADTY0
    if (DEFAULT_ADVERTISING) {
        advertising = NimBLEDevice::startAdvertising();
    }


}

/* Parses and acts on any in-band AT commands received at the UART.  
 * Otherwise, sends any actual data on to BLE 
 * Please ensure strings passed are already free of any newline or carriage returns
*/

void parseFunctionUART (String data)
{
    Serial.print("parseFunctionUART() Received: ");
    Serial.println(data);

    if (!stringNotEmpty(data)) {
        // Serial.println("parseFunctionUART() Received empty string");
        return;
    }

    
    /* See if this is an AT command */    
    // if (strncmp(data, "AT", 2) == 0) {
    if (data.startsWith("AT+")) {
                
        // if (strncmp(data, "AT+ADTY0", 8) == 0) {
        if (data == "AT+ADTY0") {
            
            Serial.println("Request advertise start");
            advertising = NimBLEDevice::startAdvertising();
            send_ble_data_str("OK+Set:0");
        }

        // if (strncmp(data, "AT+ADTY", 7) == 0) {
        if (data == "AT+ADTY") {
            Serial.println("Get advertising state");
            advertising = pAdvertising->isAdvertising();
            send_ble_data_str("OK+Get:" + (int) advertising);
        }


        // if (strncmp(data, "AT+ADTY1", 8) == 0) {
        if (data == "AT+ADTY1") {            
            Serial.println("Request advertise stop");
            advertising = NimBLEDevice::stopAdvertising();
            send_ble_data_str("OK+Set:1");
        }

        // if (strncmp(data, "AT+ADDR?", 8) == 0) {
        if (data == "AT+ADDR?") {            
            Serial.println("Request server address");
            String buffer = "OK+ADDR:";
            buffer.concat(NimBLEDevice::getAddress().toString().c_str());
            send_ble_data_str(buffer.c_str());
        }

        // If the string  ends with a question mark, then it's a get command
        if (data == ("AT+NAME?")) {
            Serial.println("Request server name");
            String buffer = "OK+NAME:";
            buffer.concat(deviceName);
            send_ble_data_str(buffer.c_str());
        }

        // Otherwise it is a set command, such as AT+NAMENewName
        else if (data.startsWith("AT+NAME")) {

            strcpy (deviceName, getParameter(data).c_str());
            Serial.print("Request set server name to: ");
            Serial.println (deviceName);

            // Write this to EEPROM
            if (write_device_name_to_eeprom()) {
            
                // Send ack
                String ackstring = "OK+Set:";
                ackstring.concat(deviceName);
                send_ble_data_str(ackstring.c_str());
            
                // May need to reboot here
                ESP.restart();
            }
            else {
                send_ble_data_str("ERROR+Set:Name");
            }
        }

        // If the string is just AT by itself, this is the disconnect all command
        else if (data == "AT") {
            // Disconnect command
            deviceConnected = disconnect_all_clients();
        }
    }

    else {
        /* Not an AT command, pass this data through to the BLE TX */
        send_ble_data_str(data.c_str());
    }
}

/* Receive data from the BLE client and pass it through to the UART TX */
void parseFunctionBLE (const char* data)
{
    Serial.print("parseFunctionBLE() Received: ");
    Serial.println(data);

    /* Pass this data through to the UART TX */
    MySerial.print(data);
}


/* Use this to send data back to the BLE client */
void send_ble_data_str (const char* str)
{
    Serial.print("send_ble_data() Sending: ");
    Serial.println(str);    
    uint8_t* data = (uint8_t*) str;
    int len = strlen(str);
    pCharacteristic->setValue(data, len);
    pCharacteristic->notify();
    
}


// void send_ble_data_byte (uint8_t* byte)
// {
//     Serial.print("send_ble_data() Sending: ");
//     Serial.println(byte);    
//     // uint8_t* data = (uint8_t*) str;
//     // int len = strlen(str);
//     pCharacteristic->setValue(byte, 1);
//     pCharacteristic->notify();
    
// }


/* Gets a list of all clients currently connected and disconnects them. Returns number of any still-connected clients after this attempt */
int disconnect_all_clients()
{

    // Get a vector of all connected client IDs
      clientIDs = pServer->getPeerDevices();

      if (!clientIDs.empty()) {
          
          for (uint16_t clientID : clientIDs) {
              pServer->disconnect(clientID);      
              //NimBLEDevice::getServer()->disconnect(clientHandle);
          }            
          
          // Remove the client ID from the data structure after disconnection          
          clientIDs.erase(clientIDs.begin());
      }
      return pServer->getConnectedCount(); 
}

/* Kills BLE advertising but everything else should stay running */
bool disable_advertising()
{
    return pServer->stopAdvertising();
}


/* Helper function to determine if a String contains any alphanumeric characters */
bool stringNotEmpty (String mystr)
{
    // Iterate through the string and return true if any character is alphanumeric 
    for (int i = 0; i < mystr.length(); i++) {
        if (isAlphaNumeric(mystr.charAt(i))) {
            return true;
        }
    }
  return false;
}


/* Helper function that returns the parameter of a command */
String getParameter(String inputString) {
    String outputString = inputString.substring(7);

  Serial.println ("getParameter() Returning: " + outputString);
  return outputString;
}

bool write_device_name_to_eeprom() {
    Serial.println("write_device_name_to_eeprom() Writing device name to EEPROM");
    for (int i = 0; i < strlen(deviceName); i++) {
      EEPROM.write(DEVICE_NAME_ADDRESS + i, deviceName[i]);
    }
    // Terminate the string
    EEPROM.write (DEVICE_NAME_ADDRESS + strlen(deviceName), 0);
    EEPROM.commit();

    // Verify the write was successful by reading it back and comparing
    for (int i = 0; i < strlen(deviceName); i++) {
      if (EEPROM.read(DEVICE_NAME_ADDRESS + i) != deviceName[i]) {
        Serial.println("write_device_name_to_eeprom() Error writing to EEPROM");
        break;
        return false;
      }
    }

    return true;

}

/* attempts to restore device name from EEPROM. */
void attempt_restore_device_name() {

    Serial.println("attempt_restore_device_name() Restoring device name from EEPROM");

    // Read it in and if there are invalid characters, then use the default name 
    for (int i = DEVICE_NAME_ADDRESS; i < DEVICE_NAME_LENGTH; i++) {
        deviceName[i] = EEPROM.read(i);

        // Stop if we've reached the end of the string
        if (deviceName[i] == 0) {            
            break;
        }

        // Stop and use default name if we find an invalid character
        if (!isalnum(deviceName[i]) && deviceName[i] != '-' && deviceName[i] != '_' && deviceName[i] != '+') {
            Serial.print("attempt_restore_device_name() Invalid character found in EEPROM:");
            Serial.print(deviceName[i]);
            Serial.println(" so using default name");
            strcpy(deviceName, DEFAULT_DEVICE_NAME);
            break;
        }
    }

    // If the length is zero, the restore was not successful.. use the default name
    if (strlen(deviceName) == 0) {
        Serial.println("attempt_restore_device_name() No name found in EEPROM, using default name");
        strcpy(deviceName, DEFAULT_DEVICE_NAME);
    }
}