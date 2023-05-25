
// #include <BLEDevice.h>
// #include <BLEServer.h>
// #include <BLEUtils.h>
// #include <BLE2902.h>



#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLE2904.h>


// UUID for the custom service and characteristics
#define SERVICE_UUID "ffe0"
#define CHARACTERISTIC_UUID "ffe1"

// Bluetooth advertised name
// #define DEVICE_NAME "PKL24-ESP"

char deviceName[32] = "PKL24-ESP";

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


void setup_ble_peripheral();
void send_ble_data_str (const char* str);
// void update_ble();
int disconnect_all_clients();
bool disable_advertising();
void parseFunctionBLE (const char* data);
void parseFunctionUART (const char* data);


bool deviceConnected = false;  // Reflects whether there is a device connected or not
bool advertising = false;  // Reflects whether we are advertising or not
bool oldDeviceConnected = false;
char txValue = 0;

/* U2UXD */
const int readPin = 16; // Use GPIO number. See ESP32 board pinouts
const int writePin = 17; // Use GPIO number. See ESP32 board pinouts




// /* Callbacks for client disconnect & connect events.  */
// class MyServerCallbacks : public NimBLEServerCallbacks {
//     void onConnect(NimBLEServer *pServer, ble_gap_conn_desc connection_desc) 
//     {
//         // Print the client's BLE address to the console
//         deviceConnected = true;
//         Serial2.println("OK+CONN");
//         Serial.print("Client connected. Address: ");
//         // Serial.println(clientAddressString.c_str());
//         Serial.println(NimBLEAddress(connection_desc.peer_ota_addr).toString().c_str());

//     };

//     void onDisconnect(NimBLEServer *pServer, ble_gap_conn_desc connection_desc) 
//     {
//         deviceConnected = false;
//         // Print the client's BLE address to the console
//         Serial2.println("OK+LOST");
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
        NimBLEDevice::startAdvertising();
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
        Serial2.println("OK+CONN");
    };

    private:
        // Member variable that stores the pointer to the parse function
        void (*m_parseFunc)(const char* data);
};



void setup() {
  /* Monitor UART0 for debugging */
  Serial.begin(115200);


  /* UART2 is used as a bridge to send/receive data over BLE */
  Serial2.begin(9600, SERIAL_8N1, readPin, writePin);

  setup_ble_peripheral();
  Serial.println("Waiting for client connection...");
}

void loop() {

    String data;

    // Workaround to handle device connections and disconnections because callbacks don't work
    deviceConnected = (pServer->getConnectedCount() > 0);
    if (oldDeviceConnected )
    if (deviceConnected != oldDeviceConnected) {
        if (deviceConnected) {
            Serial.println("Client connected");

        }
        else {
            Serial.println("Client disconnected");
            Serial2.println("OK+LOST");
        }
        oldDeviceConnected = deviceConnected;
    }

    if (deviceConnected) {
        /* Read the value of the readPin and send this to the characteristic */
        while (Serial2.available()) {

            txValue = Serial2.read();
            if (txValue == '\n' || txValue == '\r') {
                if (data.length() > 0) {
                    // Serial.println("Read from UART: " + data);
                    parseFunctionUART(data.c_str());  // Deal with the incoming data                    
                    data = "";
                }
            } else {
                data += txValue;
            }
        }
      
       delay(10); // bluetooth stack will go into congestion, if too many packets are sent
	}

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

    // Start advertising the service
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setName(deviceName);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    advertising = NimBLEDevice::startAdvertising();


}

void parseFunctionUART (const char* data)
{
    Serial.print("parseFunctionUART() Received: ");
    Serial.println(data);

    
    /* See if this is an AT command */    
    if (strncmp(data, "AT", 2) == 0) {
                
        if (strncmp(data, "AT+ADTY0", 8) == 0) {
            
            Serial.println("Request advertise start");
            advertising = NimBLEDevice::startAdvertising();
            send_ble_data_str("OK+Set:0");
        }

        if (strncmp(data, "AT+ADTY1", 8) == 0) {
            
            Serial.println("Request advertise stop");
            advertising = NimBLEDevice::stopAdvertising();
            send_ble_data_str("OK+Set:1");
        }

        if (strncmp(data, "AT+ADDR?", 8) == 0) {
            
            Serial.println("Request server address");
            String buffer = "OK+ADDR:";
            buffer.concat(NimBLEDevice::getAddress().toString().c_str());
            send_ble_data_str(buffer.c_str());
        }

        if (strncmp(data, "AT+NAME?", 8) == 0) {
            
            Serial.println("Request server name");
            String buffer = "OK+NAME:";
            buffer.concat(deviceName);
            send_ble_data_str(buffer.c_str());
        }

        else {
            // Disconnect command
            deviceConnected = disconnect_all_clients();
        }
    }

    else {
        /* Not an AT command, pass this data through to the BLE TX */
        send_ble_data_str(data);
    }
}

/* Receive data from the BLE client and pass it through to the UART TX */
void parseFunctionBLE (const char* data)
{
    Serial.print("parseFunctionBLE() Received: ");
    Serial.println(data);

    /* Pass this data through to the UART TX */
    Serial2.print(data);
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