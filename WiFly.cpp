/*

  A-Vision Software

*/
#include <string.h>
#include <Arduino.h>
#include <SPI.h>

#include "WiFly.h"

#define DEBUG false
#define BEEP 8 // Portnumber of the buzzer (set to false to disable)

void WiFlyDevice::error(const char *msg)
{
  Serial.println(msg);
  #ifdef BEEP
  tone(BEEP, 2000, 1000);
  delay(1500);
  tone(BEEP, 2000, 1000);
  delay(1500);
  tone(BEEP, 2000, 1000);
  #endif
  while (true);
}
void WiFlyDevice::debug(const char *msg)
{
  debug(msg, true);
}
void WiFlyDevice::debug(const char *msg, bool newline)
{
  #if DEBUG 
  if (newline) {
    Serial.println(msg);
  }
  else
  {
    Serial.print(msg);
  }
  #endif
}

void WiFlyDevice::init()
{
  char clr = 0;

  #if BEEP
  tone(BEEP, 2000, 125);
  delay(200);
  tone(BEEP, 2000, 125);
  #endif

  // Initialize SPI pins
  pinMode(MOSI, OUTPUT);
  pinMode(MISO, INPUT);
  pinMode(SCK, OUTPUT);
  pinMode(CS, OUTPUT);
  digitalWrite(CS, HIGH); //disable device

  SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);
  clr = SPSR;
  clr = SPDR;
  delay(10);

  debug("WiFly network connection");
  // Initialize and test SC16IS750
  if (SPI_Uart_Init())
  {
    debug("Bridge initialized successfully!");
  }
  else
  {
    error("Could not initialize bridge, locking up.");
  }
  initialised = true;
}

void WiFlyDevice::select(void)
{
  digitalWrite(CS, LOW);
}

void WiFlyDevice::deselect(void)
{
  digitalWrite(CS, HIGH);
}

void WiFlyDevice::SPI_Uart_WriteByte(char address, char data)
// Write <data> byte to SC16IS750 register <address>
{
  long int length;
  char senddata[2];
  senddata[0] = address;
  senddata[1] = data;

  select();
  length = SPI_Write(senddata, 2);
  deselect();
}

long int WiFlyDevice::SPI_Write(const char *srcptr, long int length)
// Write string to SC16IS750
{
  for (long int i = 0; i < length; i++)
  {
    spi_transfer(srcptr[i]);
  }
  return length;
}

void WiFlyDevice::SPI_Uart_WriteArray(const char *data, long int NumBytes)
// Write string to SC16IS750 THR
{
  long int length;
  select();
  length = SPI_Write(&TX_Fifo_Address, 1);

  while (NumBytes > 16)
  {
    length = SPI_Write(data, 16);
    NumBytes -= 16;
    data += 16;
  }
  length = SPI_Write(data, NumBytes);

  deselect();
}

char WiFlyDevice::SPI_Uart_ReadByte(char address)
// Read byte from SC16IS750 register at <address>
{
  char data;

  address = (address | 0x80);

  select();
  spi_transfer(address);
  data = spi_transfer(0xFF);
  deselect();
  return data;
}

bool WiFlyDevice::SPI_Uart_Init(void) // Initialize and test SC16IS750
{
  char data = 0;

  SPI_Uart_WriteByte(LCR, 0x80);                 // 0x80 to program baudrate
  SPI_Uart_WriteByte(DLL, SPI_Uart_config.DivL); //0x50 = 9600 with Xtal = 12.288MHz
  SPI_Uart_WriteByte(DLM, SPI_Uart_config.DivM);

  SPI_Uart_WriteByte(LCR, 0xBF);                       // access EFR register
  SPI_Uart_WriteByte(EFR, SPI_Uart_config.Flow);       // enable enhanced registers
  SPI_Uart_WriteByte(LCR, SPI_Uart_config.DataFormat); // 8 data bit, 1 stop bit, no parity
  SPI_Uart_WriteByte(FCR, 0x06);                       // reset TXFIFO, reset RXFIFO, non FIFO mode
  SPI_Uart_WriteByte(FCR, 0x01);                       // enable FIFO mode

  // Perform read/write test to check if UART is working
  SPI_Uart_WriteByte(SPR, 'H');
  data = SPI_Uart_ReadByte(SPR);

  if (data == 'H')
  {
    return true;
  }
  return false;
}

void WiFlyDevice::SPI_Uart_println(const char *data)
// Write array to SC16IS750 followed by a carriage return
{
  debug(data);
  SPI_Uart_WriteArray(data, strlen(data));
  SPI_Uart_WriteByte(THR, 0x0d);
  delay(250);
}

void WiFlyDevice::SPI_Uart_print(const char *data)
// Routine to write array to SC16IS750 using strlen instead of hardcoded length
{
  debug("send: ", false);
  debug(data, false);
  SPI_Uart_WriteArray(data, strlen(data));
  delay(250);
}

char WiFlyDevice::spi_transfer(volatile char data)
{
  SPDR = data;                  // Start the transmission
  while (!(SPSR & (1 << SPIF))) // Wait for the end of the transmission
  {
  };
  return SPDR; // return the received byte
}

void WiFlyDevice::Flush_RX(void)
// Flush characters from SC16IS750
{
  char incoming_data;
  char data[] = " ";
  int j = 0;
  while (j < 1000)
  {
    if ((SPI_Uart_ReadByte(LSR) & 0x01))
    {
      incoming_data = SPI_Uart_ReadByte(RHR);
      if (incoming_data < 0)
      {
        return;
      }
      data[0] = incoming_data;
      debug(data, false);
    }
    else
    {
      j++;
    }
  }
  debug("");
  delay(250);
}

bool WiFlyDevice::wait_for_reponse(const char * find, int timeout = 10000)
{
  bool found = false;
  bool nodata = false;
  bool timedout = false;
  unsigned int timeOutTarget; // in milliseconds
  String response = "";

  char incoming_data;
  char data[] = " ";
  int j = 0;

  debug("FIND:", false);
  debug(find);

  timeOutTarget = millis() + timeout;

  while (!found && !timedout)
  {
    if ((SPI_Uart_ReadByte(LSR) & 0x01))
    {
      incoming_data = SPI_Uart_ReadByte(RHR);
      if (incoming_data < 0)
      {
        nodata = true;
      } 
      else 
      {
        data[0] = incoming_data;
        debug(data, false);
        response.concat(incoming_data);

        found = (response.indexOf(find) >= 0);
      }
    }
    else
    {
      j++;
    }
    timedout = (millis() > timeOutTarget);
  }
  debug("");

  debug("REPONSE:", false);
  debug(&response[0]);

  return found;
}

bool WiFlyDevice::connect(const char *ssid, const char *pass)
{
  char auth_level[] = "4";

  connected = false;

      if (!initialised)
  {
    init();
  }

  if (initialised) {

    // Exit command mode if active at all
    SPI_Uart_println("");
    SPI_Uart_println("exit");
    Flush_RX();

    // Enter command mode
    SPI_Uart_print("$$$");
    if (!wait_for_reponse("CMD")) 
    {
      error("Uable to enter Command mode");
    }
    Flush_RX();
    
    debug("User external antenna");
    SPI_Uart_println("set wlan ext_antenna 1");
    wait_for_reponse("AOK");
    Flush_RX();

    // Reboot to get device into known state
    debug("Rebooting...");
    SPI_Uart_println("reboot");
    Flush_RX();
    //delay(REBOOT_TIMEOUT);
    wait_for_reponse("*READY*", REBOOT_TIMEOUT);

    // Enter command mode
    debug("Entering command mode.");
    SPI_Uart_print("$$$");
    if (!wait_for_reponse("CMD"))
    {
      error("Uable to enter Command mode");
    }
    Flush_RX();

    // Set authorization level to <auth_level>
    SPI_Uart_print("set w a ");
    SPI_Uart_println(auth_level);
    Flush_RX();
    debug("Set wlan to authorization level ", false);
    debug(auth_level);

    // Set passphrase to <auth_phrase>
    SPI_Uart_print("set w p ");
    SPI_Uart_println(pass);
    Flush_RX();
    debug("Set security phrase to ", false);
    debug(pass);

    if (local_port != "") {
      listen(local_port);
    }

    // Join wifi network <ssid>
    debug("Joining '", false);
    debug(ssid, false);
    debug("'", true);
    Flush_RX();
    SPI_Uart_print("join ");
    SPI_Uart_println(ssid);
    Flush_RX();

    connected = wait_for_reponse("Associated!", ASSOCIATE_TIMEOUT);

    Flush_RX();

    if (connected) {
      #if BEEP
      tone(BEEP, 2000, 500);
      #endif
    }

    SPI_Uart_println("set sys iofunc 0x40");
    Flush_RX();    

    return connected;
  }
  return false;
}

char * WiFlyDevice::info()
{
  Flush_RX();
  SPI_Uart_println("get everything");

  wait_for_reponse("I have everything");
}

bool WiFlyDevice::listen(const char *port)
{
  if (!initialised) {
    local_port = port;
    return false;
  }
  bool success = false;
  String ip = "";
  String p = "";

  debug("Listen on port '", false);
  debug(port, false);
  debug("'", true);

  Flush_RX();
  SPI_Uart_print("set ip localport ");
  SPI_Uart_println(port);
  if (wait_for_reponse("AOK")) {
    success = true;
  }
  Flush_RX();

/*
  SPI_Uart_println("get ip");

  wait_for_reponse("IP=");
  ip.concat(read(":"));
  p.concat(read("\r"));

  debug("IP ADDRESS IS: ", false);
  debug(&ip[0]);
  debug("IP PORT IS: ", false);
  debug(&p[0]);
  
  Flush_RX();

  return (p == port);
*/
  return success;
}

bool WiFlyDevice::data_available()
{
  return (SPI_Uart_ReadByte(LSR) & 0x01) > 0;
}

String WiFlyDevice::readln()
// Read until EOL
{
  return read("\r");
}
String WiFlyDevice::read()
{
  return read("");
}
String WiFlyDevice::read(const char *until = "")
// Read until no data or until is found
{
  bool found = false;
  String response = "";

  char incoming_data;
  char data[] = " ";

  while (!found && data_available())
  {
    incoming_data = SPI_Uart_ReadByte(RHR);
    if (incoming_data < 0)
    {
    }
    else
    {
      data[0] = incoming_data;
      debug(data, false);
      response.concat(incoming_data);

      if (until != "") {
        found = (response.indexOf(until) >= 0);
        if (found)
        {
          response = response.substring(0, response.length() - strlen(until));
        }
      }
    }
  }
  debug("");

  return (response);
}

bool WiFlyDevice::write(char *data)
{
  return true;
}

WiFlyDevice::WiFlyDevice()
{
}
