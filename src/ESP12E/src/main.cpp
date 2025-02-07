#include <Arduino.h>
#include <ESP8266WiFi.h>

#pragma region SETTINGS

#define DEBUG_EN

#pragma endregion

class DataEntry;

class RPCBinder{
  public:
    void* (*get) (DataEntry&);
    uint8_t (*set) (DataEntry&, void *);
};

class DataEntry{
  public:
    union idUni{
      uint8_t u8;
      uint16_t u16 = 0;
    };

    idUni id;

    DataEntry(){

    }

    DataEntry(uint16_t id, int type, void * value){
      this->id.u16 = id;
      this->type = (uint8_t)type | typeExtended;
      data.any = value;
    }

    DataEntry(uint8_t id, int type, void * value){
      this->id.u8 = id;
      this->type = (uint8_t)type;
      data.any = value;
    }

    static const uint8_t 
    typePerm = 0, // the saved value will not be attempted to be read
    typeRW = 1, // the saved value will get changed when set is called
    typeTemp = 2, // the saved value will get treated the same as RW, but can be identified as temp thus skipped when data is being saved
    typeRPC = 3, // the saved value will get it's set and get methods called when requested
    typeNone = 0, // initial value
    typeString = 1 << 2, // type.str will be chosen
    typeBuf = 2 << 2, // type.buf will be chosen
    typeNumber8 = 3 << 2, // type.b8 will be chosen
    typeNumber16 = 4 << 2, // type.b16 will be chosen
    typeNumber32 = 5 << 2, // type.b32 will be chosen
    typeRegular = 0, // type is stored in the regular datastore
    typeExtended = 1 << 7 // type is stored in the extended datastore
    ;

    static const uint8_t DEFTYPE = DataEntry::typePerm | DataEntry::typeNone;
    
    uint8_t type = DEFTYPE;

    uint8_t storeType(){
      return type & 0b00000011;
    }
    uint8_t dataType(){
      return type & 0b01111100;
    }

    bool extended(){
      return type & 0b10000000;
    }

    void * get(){
      switch (dataType()){
        case typeRPC:
          {
            RPCBinder& rpc = data.rpc;
            return rpc.get(*this);
          }
        break;

        case typeNone:
          return (void*)0;

        default:
          return data.any;
      }
    }

    uint8_t set(void * v){
      uint8_t storeType = this->storeType();
      if (storeType == typeRPC){ // do not delete memory, it is handled by the rpc
        return data.rpc.set(*this, v);
      }
      else if (storeType == typePerm){ // we cannot set perm memory
        return 1;
      }
      else{ // for rw or temp memory
        uint8_t type = dataType();
        if (type == typeString || type == typeBuf){ // delete an array
          delete[] data.any;
        }
        else if (type == typeNumber32){ // delete an object
          delete data.any;
        }
        else{
          // do not delete memory, we do not know how or it is not needed
        }
        data.any = v;
      }
      return 0;
    }

    union dataTypes{
      RPCBinder rpc;
      char * str;
      uint8_t * buf;
      uint8_t b8;
      uint16_t b16;
      uint32_t * b32;
      void * any;
    };

    dataTypes data;

};


class Datastore{
  public:
    DataEntry * entries = 0;
    int entryCnt = 0;
    // gets the pointer to the Entry assigned to this id
    // returns a null pointer when the value wasn't found
    DataEntry * get(uint8_t id){
      for (int i = 0; i < entryCnt; i++){
        DataEntry& entry = entries[i];
        uint8_t extended = entry.type >> 7;
        if (extended || entry.id.u8 != id) // this is not an extended query
          continue;
        return &entry;
      }
      return 0;
    }

    // gets the pointer to the value assigned this id from the EXTENDED datastore
    // returns a null pointer when the value wasn't found
    DataEntry * get(uint16_t id){
      for (int i = 0; i < entryCnt; i++){
        DataEntry& entry = entries[i];
        uint8_t extended = entry.type >> 7;
        if (!extended || entry.id.u16 != id) // this is not a regular query
          continue;
        return &entry;
      }
      return 0;
    }



};


void setup_debug(){

}

char * create_id(){
  const char* prefix = "ESP-12E_RGB2_";
  const char* chars = "0123456789ABCDEF";
  int preflen = strlen(prefix);
  char * buf = new char[preflen + 2 * 3 + 1];
  memcpy(buf,prefix,preflen);
  uint8_t macBuf[6];
  WiFi.macAddress(macBuf);
  char * bufPtr = buf + preflen;
  uint8_t * macPtr = macBuf + 3;
  for (int i = 0; i < 3; i++){
    *bufPtr = chars[*macPtr >> 4 & 0xF];
    bufPtr++;
    *bufPtr = chars[*macPtr & 0xF];
    bufPtr++;
    macPtr++;
  }
  *bufPtr = 0; // null terminate the string
  return buf;
}

String bits(uint8_t v){
  char str[9];
  for (int i = 0; i < 8; i++){
    str[i] = (i >> (7 - i) & 1) ? '1' : '0';
  }
  str[8] = 0;
  return String(str);
}

Datastore Store;

#define ENTRY_SSID (uint8_t)32
#define ENTRY_SSIDPASS (uint8_t)33
#define ENTRY_DEVID (uint8_t)1
#define ENTRY_DEVNAME (uint8_t)2

#define WIFI_SSID "SSID HERE"
#define WIFI_PASS "PASS HERE"

#include "my_settings.h"

void setup_datastore(){
  Serial.println("Setting up datastore...");
  constexpr int count = 8;
  DataEntry * entries = new DataEntry[count];
  Store.entries = entries;
  Store.entryCnt = count;
  entries[0] = DataEntry(ENTRY_SSID, DataEntry::typeString | DataEntry::typeRW, strdup((String("0") + WIFI_SSID).c_str()));
  entries[1] = DataEntry(ENTRY_SSIDPASS, DataEntry::typeString | DataEntry::typeRW, strdup((String("0") + WIFI_PASS).c_str()));
  entries[2] = DataEntry(ENTRY_DEVID,DataEntry::typePerm | DataEntry::typeString, create_id());
  entries[3] = DataEntry(ENTRY_DEVNAME,DataEntry::typeRW | DataEntry::typeString, strdup(entries[2].data.str));
  for (int i = 1; i <= 253; i++){
    DataEntry * entry = Store.get((uint8_t)i);
    if (entry)
      Serial.println("at " + String(i) + ": 0b" + bits(entry->type));
  }
  for (int i = 1; i <= 0b1111111111111111; i++){
    DataEntry * entry = Store.get((uint16_t)i);
    if (entry)
      Serial.println("at e" + String(i) + ": 0b" + bits(entry->type));
  }
  Serial.println("Enumeration done");
}

// setup the network connection (like connection to a wifi AP)
void setup_network(){
  WiFi.mode(WIFI_STA);
  char * ssid = (char*)Store.get(ENTRY_SSID)->get();
  char * pass = (char*)Store.get(ENTRY_SSIDPASS)->get();
  char * host = (char*)Store.get(ENTRY_DEVNAME)->get();
  Serial.println("WiFi...");
  Serial.println("host: " + String(host));
  Serial.println("ssid: " + String(ssid + 1));
  Serial.print("pass: ");
  int passlen = strlen(pass);
  for (int i = 1; i < passlen; i++){
    Serial.print('*');
  }
  Serial.println();
  Serial.flush();
  WiFi.setHostname(host);
  WiFi.begin(ssid + 1,pass + 1);
  Serial.print("      ...");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print('.');
    delay(2000);
  }
  Serial.println("Connected");
  Serial.flush();
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();
  #ifdef DEBUG_EN
    setup_debug();
  #endif
  setup_datastore();
  setup_network();
}

void loop() {
  // put your main code here, to run repeatedly:
}