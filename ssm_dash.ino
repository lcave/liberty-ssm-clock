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

void setup()
{
  requests[0] = pressure;
  requests[1] = afr;
  requests[2] = coolant_temp;
  requests[3] = intake_temp;

  Serial.begin(9600);
  Serial3.begin(4800);
  // TODO display startup logo
  // TODO initalize display
}

void loop()
{
  if (serialCallSSM(requests[param_index].request))
  {
    double value = 0;
    switch (param_index)
    {
    case 0:
      // Manifold Pressure Calculation
      //the pressure calc looks like: x*0.1333224 for units of kPa absolute, or (x-760)*0.001333224 for units of Bar relative to sea level
      //uint16_t combBytes = 256 * ECUResponseBuffer[0] + ECUResponseBuffer[1]; //we must combine the ECU response bytes into one 16-bit word - the response bytes are big endian
      //value = combBytes * 0.1333224;
      break;
    case 1:
      // AFR Caclculation TODO
      break;
    case 2:
      // Coolant temp calculation TODO
      break;
    case 3:
      // Intake temp calculation TODO
      break;
    }
    Serial.println(requests[param_index].title);
    Serial.println(value);
    // Print to screen TODO
  }
}

bool readSSM()
{
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
      Serial.print(data);

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
void writeSSM(byte *dataBuffer)
{
  Serial.print(F("Sending packet: [ "));
  for (uint8_t i = 0; i < sizeof(dataBuffer); i++)
  {
    Serial3.write(dataBuffer[i]);
    Serial.print(dataBuffer[i]);
    Serial.print(F(" "));
  }
  Serial.println(F("]"));
}

bool serialCallSSM(byte *sendDataBuffer)
{
  bool success = false;
  int attempts = 0;
  while (attempts < 10 && !success)
  {
    writeSSM(sendDataBuffer);
    success = readSSM();
    attempts++;
  }
  return success;
}