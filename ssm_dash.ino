// #include <Wire.h>

/*
2004 ADM Subaru Liberty 3.0r ECU variables:
  Manifold Pressure: 0x72810
  AFR sensor: 0x020952,0x020953
  Coolant temperature: 0x728F0
  Intake air temperature: 0x729D8

Each 2 characters after '0x' represent a byte of the hex string. 
Each byte must be transmitted seperately to the ECU.
For hex strings with uneven character lengths, a zero should be added to the FRONT of the string (i.e. the left side).
  0x72810 == 0x072810 but != 0x728100
*/

/* Message Format:
    {
      header (128),
      to (16 - ECU), 
      from (240 - Diagnostics tool), 
      no. bytes,
      read type flag (purpose unknown),
      continuous response request flag,
      Address byte 1 (e.g. 0x07),
      Address byte 2, (e.g. 0x28),
      Address byte 3, (e.g. 0x10)
      ...,
      checksum (sum of bits excluding header, to, from, no. bytes, and checksum)}
*/

struct req_struct
{
  String title;
  String unit;
  byte *request;
  int req_length;
};

struct req_struct requests[4];
// Manifold Pressure: 0x07, 0x28, 0x10
byte pressure_req[10] = {128, 16, 240, 3, 168, 0, 0x07, 0x28, 0x10, 0x05};
req_struct pressure = {String("Manifold Pressure"), String("kPa"), pressure_req, 10}; // 1 byte response

// AFR sensor :0x02, 0x09, 0x52 && 0x02, 0x09, 0x53
byte afr_req[13] = {128, 16, 240, 6, 168, 0, 0x02, 0x09, 0x52, 0x02, 0x09, 0x53, 0x08};
req_struct afr = {String("Air/Fuel Ratio"), String(""), afr_req, 13}; // 2 byte repsonse

// Coolant temperature: 0x07, 0x28, 0xF0
byte coolant_req[10] = {128, 16, 240, 3, 168, 0, 0x07, 0x28, 0xF0, 0x05};
req_struct coolant_temp = {String("Coolant Temperature"), String("C"), coolant_req, 10}; // 1 byte response

// Intake air temperature: 0x07, 0x29, 0xD8
byte intake_req[10] = {128, 16, 240, 3, 168, 0, 0x07, 0x29, 0xd8, 0x05};
req_struct intake_temp = {String("Intake Temperature"), String("C"), coolant_req, 10}; // 1 byte response

uint8_t ECUResponseBuffer[2] = {0, 0};
uint8_t param_index = 0;
// Update when new requests are added
const int num_requests = 4;

const int display_btn = 53;
const int bright_btn = 51;
const int set_btn = 49;
const int plus_btn = 47;
const int minus_btn = 45;

void setup()
{
  // fill requests array
  requests[0] = pressure;
  requests[1] = afr;
  requests[2] = coolant_temp;
  requests[3] = intake_temp;

  // Initialize control buttons
  pinMode(display_btn, INPUT_PULLUP);
  pinMode(bright_btn, INPUT_PULLUP);
  pinMode(set_btn, INPUT_PULLUP);
  pinMode(plus_btn, INPUT_PULLUP);
  pinMode(minus_btn, INPUT_PULLUP);

  // Initialize serial ports
  Serial.begin(9600);
  Serial3.begin(4800);
  // TODO display startup logo
  // TODO initalize display
}

void loop()
{
  switchHandler();

  if (serialCallSSM(requests[param_index]))
  {
    double value = 0;
    switch (param_index)
    {
    case 0:
      // TODO Manifold Pressure Calculation
      break;
    case 1:
      // TODO AFR Caclculation
      break;
    case 2:
      // TODO Coolant temp calculation
      break;
    case 3:
      // TODO Intake temp calculation
      break;
    }
    Serial.println(requests[param_index].title);
    Serial.println(value);
    // TODO Print to screen
  }
}

void switchHandler()
{
  int display_val = digitalRead(display_btn);
  int bright_val = digitalRead(bright_btn);
  int set_val = digitalRead(set_btn);
  int plus_val = digitalRead(plus_btn);
  int minus_val = digitalRead(minus_btn);

  if (display_val == LOW)
  {
    Serial.println(F("DISPLAY"));
    if (param_index == (num_requests - 1))
    {
      param_index = 0;
    }
    else
    {
      param_index++;
    }
  }

  if (bright_val == LOW)
  {
    Serial.println(F("BRIGHT"));
    // TODO send brightness signal
  }

  if (set_val == LOW)
  {
    Serial.println(F("UPDATED"));
  }

  if (plus_val == LOW)
  {
    Serial.println(F("+"));
  }

  if (minus_val == LOW)
  {
    Serial.println(F("-"));
  }
}

bool readSSM()
{
  switchHandler();
  uint16_t timeout = 100;
  uint32_t timerstart = millis();
  boolean notFinished = true;
  boolean checksumSuccess = false;
  uint8_t dataSize = 0;
  uint8_t checksumCalc = 0;
  uint8_t packetIndex = 0;

  bool data_valid = true;

  Serial.print(F("Read Packet: [ "));
  while (millis() - timerstart <= timeout && notFinished)
  {
    if (Serial3.available())
    {
      uint8_t data = Serial3.read();
      // Serial.print(data);

      if (packetIndex == 0 && data == 128)
      {
        //0x80 or 128 marks the beginning of a packet
        packetIndex += 1;
        checksumCalc += data;
      }
      else if (packetIndex == 1 && data == 240)
      {
        //this byte indicates that the message recipient is the 'Diagnostic tool'
        packetIndex += 1;
        checksumCalc += data;
      }
      else if (packetIndex == 2 && data == 16)
      {
        //this byte indicates that the message sender is the ECU
        packetIndex += 1;
        checksumCalc += data;
      }
      else if (packetIndex == 3)
      {
        //this byte indicates the number of data bytes which follow
        dataSize = data;
        packetIndex += 1;
        checksumCalc += data;
      }
      else if (packetIndex == 4)
      {
        //I don't know what this byte is for
        packetIndex += 1;
        checksumCalc += data;
      }
      else if (packetIndex > 4 && packetIndex < (dataSize + 4))
      {
        //this byte is data
        if (packetIndex - 5 < 2) //make sure it fits into the buffer
        {
          ECUResponseBuffer[packetIndex - 5] = data;
        }
        packetIndex += 1;
        checksumCalc += data;
      }
      else if (packetIndex == dataSize + 4)
      {
        //this is the checksum byte
        if (data == checksumCalc)
        {
          //checksum was successful
          checksumSuccess = true;
        }
        else
        {
          Serial.print(F("Checksum fail "));
        }
        notFinished = false; //we're done now
        break;
      }
      timerstart = millis(); //reset the timeout if we're not finished
    }
  }
  Serial.println(F("]"));
  if (notFinished)
  {
    Serial.println(F("Comm. timeout"));
  }
  return checksumSuccess;
}

//writes data over the software serial port
void writeSSM(req_struct request)
{
  switchHandler();
  Serial.print(F("Sending packet: [ "));
  for (uint8_t i = 0; i < request.req_length; i++)
  {
    Serial3.write(request.request[i]);
    Serial.print(request.request[i]);
    Serial.print(F(" "));
  }
  Serial.print(F("] "));
  Serial.println(request.title);
}

bool serialCallSSM(req_struct request)
{
  bool success = false;
  int attempts = 0;
  while (attempts < 10 && !success)
  {
    switchHandler();
    writeSSM(request);
    success = readSSM();
    attempts++;
  }
  return success;
}