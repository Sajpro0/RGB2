#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

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
        uint8_t extended = entry.type >> 7 & 1;
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
  static char str[9];
  for (int i = 0; i < 8; i++){
    str[i] = (v >> (7 - i) & 1) ? '1' : '0';
  }
  str[8] = 0;
  return String(str);
}

Datastore Store;

#include "entries.h"
#include "preset.h"

#define WIFI_SSID "SSID HERE"
#define WIFI_PASS "PASS HERE"

#include "my_settings.h"

// enumerates the available datastores and print when buf is 0, otherwise do not print and format the entries into ENTRY_ENTRYARRAY format, always returns the amount of bytes written
// (counts how many bytes we need to actually write if buf is set to 0)
int enum_entires(uint8_t* buf){
  int bytesWritten = 0;
  for (int i = 1; i <= 253; i++){
    DataEntry * entry = Store.get((uint8_t)i);
    if (entry){
      bytesWritten++;
      if (buf){
        *buf = (uint8_t)i;
        buf++;
      }
      else{
        Serial.println("at " + String(i) + ": 0b" + bits(entry->type));
      }
    }
      
  }
  bool foundExtended = false;
  for (int i = 1; i <= 0b1111111111111111; i++){
    DataEntry * entry = Store.get((uint16_t)i);
    if (entry){ // entry is found
      
      if (buf){ // buf is set
        if (!foundExtended){ // first ext
          bytesWritten++;
          foundExtended = true;
          *buf = 254;
          buf++;
        }
        bytesWritten += 2;
        *buf = ((uint16_t)i) >> 8 & 0xFF;
        buf++;
        *buf = ((uint16_t)i) & 0xFF;
        buf++;
      }
      else{ // buf not set
        if (!foundExtended){ // first ext
          bytesWritten++;
          foundExtended = true;
          Serial.println("ext:");
        }
        bytesWritten += 2;
        Serial.println("at " + String(i) + ": 0b" + bits(entry->type));
      }
    }
      
  }
  if (!buf)
    Serial.println("Enumeration done");
  return bytesWritten;
}

// load values into the datastore either default or perhaps from external storage
void setup_datastore(){
  Serial.println("Setting up datastore...");
  constexpr int count = 8;
  DataEntry * entries = new DataEntry[count];
  Store.entries = entries;
  Store.entryCnt = count;
  entries[0] = DataEntry(ENTRY_SSID, DataEntry::typeString | DataEntry::typeRW, strdup((String("0") + WIFI_SSID).c_str()));
  entries[1] = DataEntry(ENTRY_SSIDPASS, DataEntry::typeString | DataEntry::typeRW, strdup((String("0") + WIFI_PASS).c_str()));
  entries[2] = DataEntry(ENTRY_DEVID, DataEntry::typePerm | DataEntry::typeString, create_id());
  entries[3] = DataEntry(ENTRY_DEVNAME, DataEntry::typeRW | DataEntry::typeString, strdup(entries[2].data.str));

  entries[count - 2] = DataEntry(ENTRY_FEATURES, DataEntry::typePerm | DataEntry::typeString, (char*)"wifi");

  int len = enum_entires(0);
  uint8_t * buf = new uint8_t[len];
  enum_entires(buf);
  entries[count - 1] = DataEntry(ENTRY_ENTRYARRAY, DataEntry::typePerm | DataEntry::typeBuf, buf);
  Serial.println();
  Serial.println("Enumeration run 2");
  enum_entires(0);
}

void init_loop();

uint8_t local_ip[4];

// setup the network connection (like connection to a wifi AP)
void setup_network(){
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
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
  uint32_t ipv4ip = WiFi.localIP().v4();
  memcpy(local_ip, (uint8_t*)&ipv4ip, 4);
  Serial.println(String("ip: ") + local_ip[0] + "." + local_ip[1] + "." + local_ip[2] + "." + local_ip[3]);
  Serial.flush();
}

class Command {
  public:
    uint8_t  
    A, // the "A" bit, toggles between acks
    ack, // ack bit, signals a manual ack was set
    cmd, // cmd bits, signals the issued command
    len; // len of buf
    uint8_t * buf; // entire packet without proto sig

    Command(uint8_t * buf, uint8_t len){
      this->len = len;
      this->buf = buf;
      A = *buf & 0b10000000;
      ack = *buf & 0b01000000;
      cmd = *buf & 0b00001111;
    }

};

class Reply {
  public:
    uint8_t len; // the reply buffer's length
    uint8_t * buf; // the reply buffer

    Reply(uint8_t * buf, uint8_t len){
      this->len = len;
      this->buf = buf;
    }

    // set or clear the A bit
    void setA(bool v){
      if (v)
        *buf |= 0b10000000;
      else
        *buf &= ~0b10000000;
    }

    // get the current value of A bit in this reply
    uint8_t getA(){
      return *buf & 0b10000000;
    }
};

constexpr uint8_t replMaxLen = 128; // max size of the reply buffer of the replies
class Connection {
  public:
    uint32_t ctlId; // a unique identifier that matches this connection's controller
    uint8_t A; // current value of the "A" bit, incoming ack-able packets with the same value will be treated as a retransmitment, different value buffers a reply and toggles the bit
    Reply * repl; // a pointer to this connection's current acknowledgement reply
    //Reply * tempRepl; // a pointer to the temporary reply used
    IPAddress replyIP; // the ip address to reply to
    uint16_t replyPort; // the port to reply to
    bool hasValidRepl; // indicating if the repl pointer is a valid currently stored retransmittment

    unsigned long priority; // indicates the priority of this connection, lower means higher chance for replacement

    Connection(){
      reset();
    }

    // issue a reset on this connection
    void reset(){
      ctlId = 0;
      A = 0b10000000;
      hasValidRepl = false;
      repl->len = replMaxLen;
      //tempRepl->len = replMaxLen;
      priority = millis();
    }

    // returns false if the acked command should be ignored as it is a retransmission
    bool respond_to(Command& cmd){
      if (cmd.A && A || !cmd.A && !A){
        // retransmission
        return false;
      }
      return true;
    }
};

WiFiUDP udp;

constexpr uint8_t connectionCount = 4;
Connection conn[connectionCount];
// setup the connection to the mcu
void setup_connection(){
  udp.begin(UDP_LISTEN_PORT);
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
  setup_connection();
  init_loop();
}

// currently does nothing as WiFiUDP discards of the packet automatically
// intended for the future when ethernet is used (which is better as there is less delay)
inline void discard_packet(WiFiUDP& c, int len){
  constexpr int chunk = 128;
  
}

void do_v(uint8_t vol){

}
void do_rgb(uint8_t rgb[3]){

}
void do_rgbv(uint8_t rgbv[4]){
  do_rgb(rgbv);
  do_v(rgbv[3]);
}

#include "replies.h"
#include "errors.h"
#include "commands.h"

// acknowledge the receivement of the command
void do_ack(Command& cmd, Reply& repl){
  repl.setA(cmd.A);
  repl.buf[0] = REPLY_RESPONSE;
  repl.len = 1;
}


// process the incoming packet
// returns true if a new reply was forged
bool process_packet(Command& cmd, Reply& repl, Connection& con){
  uint8_t payloadLen = cmd.len - 1;
  switch (cmd.cmd){
    case COMMAND_PING: {
      constexpr uint32_t ping_const = 0x413A0D53;
      if (cmd.len == 5){
        if (con.respond_to(cmd)){ // check for retransmittment
          if (cmd.ack){ // forced ack, simply acknowledge the command
            do_ack(cmd, repl);
            return true;
          }
          else{
            uint32_t r_val = 0;
            memcpy((uint8_t*)&r_val, cmd.buf + 1, 4);
            r_val ^= ping_const;
            repl.setA(cmd.A);
            repl.buf[0] = REPLY_RESPONSE;
            memcpy(repl.buf + 1, (uint8_t*)&r_val, 4);
            repl.len = 5;
            return true;
          }
        }
      }
    }
    break;

    case COMMAND_RGBV: {
      if (payloadLen == 1) {
        do_v(cmd.buf[1]);
      }
      else if (payloadLen == 3) {
        do_rgb(cmd.buf + 1);
      }
      else if (payloadLen == 4) {
        do_rgbv(cmd.buf + 1);
      }
      else {
        if (cmd.ack && con.respond_to(cmd)){
          repl.setA(cmd.A);
          repl.buf[0] = REPLY_ERROR;
          repl.buf[1] = ERROR_COMMANDINVALID;
          repl.len = 2;
          return true;
        }
      }
      if (cmd.ack && con.respond_to(cmd)){
        do_ack(cmd, repl);
        return true;
      }
    }
    break;

    default: {
      if (cmd.ack){
        if (con.respond_to(cmd)){
          repl.setA(cmd.A);
          repl.buf[0] = REPLY_ERROR;
          repl.buf[1] = ERROR_NOTIMPLEMENTED_COMMAND;
          repl.len = 2;
          return true;
        }
      }
    }
    break;
  }
  return false;
}

// returns the connection to be used for this packet id
// expected behaviour:
// >>>return in-use connection when found
// >>>force-create a new one if not, even if there are no free ones
Connection& get_connection(uint32_t ctlId){
  Connection& to_replace = conn[0];
  if (to_replace.ctlId == ctlId)
    return to_replace;
  for (int i = 1; i < connectionCount; i++){
    Connection& current = conn[i];
    if (current.ctlId == ctlId)
      return current; // found a used connection
    if (to_replace.priority > current.priority)
      to_replace = current;
  }
  to_replace.reset(); // use the lowest priority connection as a new one
  return to_replace;
}

bool loop_connection(){
  int bytes = udp.parsePacket();
  constexpr int maxBytes = 128;
  auto ctlId = udp.remoteIP().v4();
  if (bytes && ctlId){ // there is data and ip is non-zero (should never be zero, also would break get_connection()'s function)
    // whatever arrived, we are not eligible for sleep anymore, return false
    if (bytes > maxBytes){
      discard_packet(udp, bytes);
      return false; // garbage, but something, do not sleep
    }
    uint8_t b = udp.read();
    bytes--;
    if (b != PROTO_SIG){ // nonsense packet, throw away
      discard_packet(udp, bytes);
      return false;
    }
    else{
      if (bytes == 0)
        return false; // packet too short
      // packet okay
      uint8_t buf[bytes];
      udp.readBytes(buf, bytes);
      Command pkt(buf, bytes);
      Connection& c = get_connection(ctlId);
      if (process_packet(pkt, *c.repl, c)){
        c.A = !c.A;
      }
      if (pkt.ack){
        c.replyPort = udp.remotePort();
        if (udp.beginPacket(IPAddress(ctlId), c.replyPort)){
          udp.write(PROTO_SIG);
          udp.write(c.repl->buf,c.repl->len);
          udp.endPacket();
        }
      }
    }

    return false;
  }
  return true;
}

bool do_filter(){

  return true;
}

unsigned long lastFilterSleepTime = 0;
constexpr unsigned long filterInterval = 10;
bool loop_filters(){
  unsigned long time = millis();
  auto elapsed = time - lastFilterSleepTime;
  bool okayToSleep = true;
  while (elapsed >= filterInterval){
    if (!do_filter())
      okayToSleep = false;
    elapsed -= filterInterval;
  }

  lastFilterSleepTime += (time - lastFilterSleepTime) - elapsed; // remove the amount of time we processed (time elapsed - leftover time after stepping)

  return okayToSleep;
}



void sleep_enter(){
  Serial.println("Entering sleep...");
  Serial.flush();
  WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
}

void sleep_leave(){
  Serial.println("Leaving sleep...");
  Serial.flush();
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
}

void sleep_poll(){
  delay(1000);
}

unsigned long loopSleepTime = 0; // the time captured when the loop needed updating last
unsigned long loopSleepTimeout = 5000; // the time for the device to go into sleep
bool wasSleeping = false;
// all loop functions should return true if device is eligible for sleep
void loop() {
  unsigned long time = millis();
  if (loop_connection() && loop_filters()){
    if (millis() - loopSleepTime > loopSleepTimeout){
      if (!wasSleeping){
        sleep_enter();
        wasSleeping = true;
      }
      sleep_poll();
    }
  }
  else{
    if (wasSleeping){
      sleep_leave();
    }
    loopSleepTime = time;
  }
}

// prepare the loop for running (mainly millis-based things)
void init_loop(){
  auto time = millis();
  loopSleepTime = time;
  lastFilterSleepTime = time;
  Serial.println("Main loop running...");
  Serial.flush();
}