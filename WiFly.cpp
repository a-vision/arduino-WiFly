/*

  (c) A-Vision Software

*/
#include <string.h>
#include <Arduino.h>
#include <SPI.h>

#include "WiFly.h"

#define DEBUG false

#include "debug.h"

#define BEEP 8 // Portnumber of the buzzer (set to false to disable)

void WiFlyDevice::error(String msg)
{
  if (onErrorListener != NULL)
  {
    onErrorListener(msg);
  }
  Serial.println(msg);
  #if BEEP
  tone(BEEP, 2000, 1000);
  delay(1500);
  tone(BEEP, 2000, 1000);
  delay(1500);
  tone(BEEP, 2000, 1000);
  #endif
  while (true);
}
/*
void WiFlyDevice::DBG(String msg)
{
  DBG(msg, true);
}
void WiFlyDevice::DBG(String msg, bool newline)
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
*/

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

  DBG("WiFly network connection", true);
  // Initialize and test SC16IS750
  if (SPI_Uart_Init())
  {
    DBG("Bridge initialized successfully!", true);
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

void WiFlyDevice::SPI_Uart_WriteArray(String str, long int NumBytes)
// Write string to SC16IS750 THR
{
  const char *data = str.c_str();
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

void WiFlyDevice::SPI_Uart_println(String data)
// Write array to SC16IS750 followed by a carriage return
{
  DBG(data, true);
  SPI_Uart_WriteArray(data, data.length());
  SPI_Uart_WriteByte(THR, 0x0d);
  delay(250);
}

void WiFlyDevice::SPI_Uart_print(String data)
// Routine to write array to SC16IS750 using strlen instead of hardcoded length
{
  DBG("send: ", false);
  DBG(data, false);
  SPI_Uart_WriteArray(data, data.length());
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
    if (data_available())
    {
      incoming_data = SPI_Uart_ReadByte(RHR);
      if (incoming_data < 0)
      {
        return;
      }
      data[0] = incoming_data;
      DBG(data, false);
    }
    else
    {
      j++;
    }
  }
  DBG("", true);
  delay(250);
}

bool WiFlyDevice::wait_for_reponse(String find, int timeout = 10000)
{
  bool found = false;
  bool nodata = false;
  bool timedout = false;
  unsigned int timeOutTarget; // in milliseconds
  String response = "";

  char incoming_data;
  char data[] = " ";
  int j = 0;

  if (timeout > 100)
  {
    DBG("FIND:", false);
    DBG(find, false);
    DBG(" in ", false);
    DBG(String(timeout), false);
    DBG(" ms", true);
  }

  timeOutTarget = millis() + timeout;

  while (!found && !timedout)
  {
    if (data_available())
    {
      incoming_data = SPI_Uart_ReadByte(RHR);
      if (incoming_data < 0)
      {
        nodata = true;
      }
      else
      {
        data[0] = incoming_data;
        if (timeout > 100)
        {
          DBG(data, false);
        }
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
  if (timeout > 100)
  {
    DBG("", true);

    DBG("REPONSE:", false);
    DBG(response, true);
  }
  return found;
}

bool WiFlyDevice::connect(String ssid, String pass)
{
  char auth_level[] = "4";
  String ip_address = "0.0.0.0";
  int ip_port = local_port;

  network_available = false;

  if (!initialised)
  {
    init();
  }

  if (initialised) {

    // Exit command mode if active at all
    SPI_Uart_println("");
    SPI_Uart_println("exit");
    if (!wait_for_reponse("EXIT", 1000))
    {
      DBG("Uable to leave Command mode", true);
    }
    Flush_RX();

    // Enter command mode
    SPI_Uart_print("$$$");
    if (!wait_for_reponse("CMD"))
    {
      error("Uable to enter Command mode (initial)");
    }
    Flush_RX();

    DBG("User external antenna", true);
    SPI_Uart_println("set wlan ext_antenna 1");
    wait_for_reponse("AOK");
    Flush_RX();

    // Reboot to get device into known state
    DBG("Rebooting...", true);
    SPI_Uart_println("reboot");
    Flush_RX();
    wait_for_reponse("*READY*", REBOOT_TIMEOUT);

    // Enter command mode
    DBG("Entering command mode.", true);
    SPI_Uart_print("$$$");
    if (!wait_for_reponse("CMD"))
    {
      error("Uable to enter Command mode (after reboot)");
    }
    Flush_RX();

    // Set authorization level to <auth_level>
    SPI_Uart_print("set w a ");
    SPI_Uart_println(auth_level);
    Flush_RX();
    DBG("Set wlan to authorization level ", false);
    DBG(auth_level, true);

    // Set passphrase to <auth_phrase>
    SPI_Uart_print("set w p ");
    SPI_Uart_println(pass);
    Flush_RX();
    DBG("Set security phrase to ", false);
    DBG(pass, true);

    if (local_port) {
      listen(local_port);
    }

    // Join wifi network <ssid>
    DBG("Joining '", false);
    DBG(ssid, false);
    DBG("'", true);
    Flush_RX();
    SPI_Uart_print("join ");
    SPI_Uart_println(ssid);
    Flush_RX();

    network_available = wait_for_reponse("Associated!", ASSOCIATE_TIMEOUT);

    Flush_RX();

    if (network_available)
    {
      #if BEEP
      tone(BEEP, 2000, 500);
      delay(600);
      #endif

      if (onWifiListener != NULL)
      {
        DBG("WIFI CALLBACK", true);

        SPI_Uart_println("get ip a");
        delay(500);

        wait_for_reponse("IP=");
        ip_address = read(":");

        DBG("IP ADDRESS IS: ", false);
        DBG(ip_address, true);
        DBG("IP PORT IS: ", false);
        DBG(String(ip_port), true);

        Flush_RX();

        #if BEEP
        tone(BEEP, 2000, 50);
        #endif
        onWifiListener(ip_address, ip_port);
      }
    }

    SPI_Uart_println("set sys iofunc 0x40");
    Flush_RX();

    return network_available;
  }
  return false;
}

bool WiFlyDevice::listen(int port)
{
  if (!initialised) {
    local_port = port;
    return false;
  }
  bool success = false;
  String ip = "";
  String p = "";

  DBG("Listen on port '", false);
  DBG(String(port), false);
  DBG("'", true);

  Flush_RX();
  SPI_Uart_print("set ip localport ");
  SPI_Uart_println(String(port));
  if (wait_for_reponse("AOK")) {
    success = true;
  }
  Flush_RX();

/*
  SPI_Uart_println("get ip");

  wait_for_reponse("IP=");
  ip.concat(read(":"));
  p.concat(read("\r"));

  DBG("IP ADDRESS IS: ", false);
  DBG(&ip[0]);
  DBG("IP PORT IS: ", false);
  DBG(&p[0]);

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
String WiFlyDevice::read(String until = "")
// Read until no data or until is found
{
  bool found = false;
  bool timedout = false;
  unsigned int timeOutTarget; // in milliseconds
  String response = "";

  char incoming_data;
  char data[] = " ";

  timeOutTarget = millis() + 1000;
  while (!found && !timedout)
  {
    if (data_available()) {
      timeOutTarget = millis() + 1000;
      incoming_data = SPI_Uart_ReadByte(RHR);
      if (incoming_data < 0)
      {
      }
      else
      {
        data[0] = incoming_data;
        if (until != "")
        {
          DBG(data, false);
        }
        response.concat(incoming_data);

        if (until != "") {
          found = (response.indexOf(until) >= 0);
          if (found)
          {
            response = response.substring(0, response.length() - until.length());
          }
        }
      }
    } else {
      if (until != "") {
        timedout = (millis() > timeOutTarget);
      } else {
        timedout = true;
      }
    }
  }
  if (until != "" && response.length() > 0) {
    DBG("", true);
  }

  return response;
}

bool WiFlyDevice::write(String data)
{
  SPI_Uart_print(data);
  return true;
}

WiFlyDevice::WiFlyDevice()
{
  initialised = false;
  network_available = false;
  connected = false;
}

bool WiFlyDevice::monitor()
{
  // Constantly check for connection and data
  static String stream = "";

  stream.concat(read(""));

  int detectOPENpos = stream.lastIndexOf("*OPEN*");
  int detectCLOSEpos = stream.lastIndexOf("*CLOS*");
  int detectDATApos = stream.lastIndexOf("\r");

  stream.trim();
  if (!connected) {
    if (detectOPENpos >= 0)
    {
      DBG("CLIENT OPEN", true);
      connected = true;
      if (onConnectListener != NULL)
      {
        DBG("CONNECT CALLBACK", true);
        onConnectListener();
      }
      stream = "";
    }
  }
  else
  {
    if (detectCLOSEpos >= 0)
    {
      DBG("CLIENT CLOSED", true);
      connected = false;
      if (onDisconnectListener != NULL)
      {
        DBG("DISCONNECT CALLBACK", true);
        onDisconnectListener();
      }
      stream = "";
    }
    if (detectDATApos >= 0)
    {
      DBG("CLIENT DATA", true);
      if (onDataListener != NULL)
      {
        DBG("DATA CALLBACK", true);
        onDataListener(stream);
      }
      stream = "";
    }
  }

  return true;
}

void WiFlyDevice::onError(void (*listener)(String msg))
{
  onErrorListener = listener;
}
void WiFlyDevice::onWifi(void (*listener)(String address, int port))
{
  onWifiListener = listener;
}
void WiFlyDevice::onConnect(void (*listener)())
{
  onConnectListener = listener;
}
void WiFlyDevice::onDisconnect(void (*listener)())
{
  onDisconnectListener = listener;
}
void WiFlyDevice::onData(void (*listener)(String data))
{
  onDataListener = listener;
}

void WiFlyDevice::interactive()
{
  String cmd;
  Serial.print("WiFly interactive mode.");
  Serial.setTimeout(100);
  while (true) {
    while (data_available()) {
      Serial.print(SPI_Uart_ReadByte(RHR));
    }
    cmd = Serial.readString();
    if (cmd != "") {
      Serial.println("");
      write(cmd);
    }
  }
}
