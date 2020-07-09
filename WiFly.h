/*

  (c) A-Vision Software

*/
#ifndef __WIFLY_H__
#define __WIFLY_H__

#define EOL 13

// SC16IS750 Register definitions
#define THR 0x00 << 3
#define RHR 0x00 << 3
#define IER 0x01 << 3
#define FCR 0x02 << 3
#define IIR 0x02 << 3
#define LCR 0x03 << 3
#define MCR 0x04 << 3
#define LSR 0x05 << 3
#define MSR 0x06 << 3
#define SPR 0x07 << 3
#define TXFIFO 0x08 << 3
#define RXFIFO 0x09 << 3
#define DLAB 0x80 << 3
#define IODIR 0x0A << 3
#define IOSTATE 0x0B << 3
#define IOINTMSK 0x0C << 3
#define IOCTRL 0x0E << 3
#define EFCR 0x0F << 3

#define DLL 0x00 << 3
#define DLM 0x01 << 3
#define EFR 0x02 << 3
#define XON1 0x04 << 3
#define XON2 0x05 << 3
#define XOFF1 0x06 << 3
#define XOFF2 0x07 << 3

// SPI Pin definitions
#define CS 10
#define MOSI 11
#define MISO 12
#define SCK 13

#define REBOOT_TIMEOUT 3000
#define ASSOCIATE_TIMEOUT 5000

#define EFR_ENABLE_CTS 0x01 << 7
#define EFR_ENABLE_RTS 0x01 << 6
#define EFR_ENABLE_ENHANCED_FUNCTIONS 0x01 << 4

struct SPI_UART_cfg
{
  char DivL, DivM, DataFormat, Flow;
};

class WiFlyDevice
{

public:
  WiFlyDevice();
  bool connect(String ssid, String pass);
  bool listen(String port);

  String read(String  until);
  String read();
  String readln();
  bool write(String  data);

  bool monitor();
  void onError(void (*listener)(String msg));
  void onWifi(void (*listener)(String address, String port));
  void onConnect(void (*listener)());
  void onDisconnect(void (*listener)());
  void onData(void (*listener)(String data));

private:
  // Status flags
  bool initialised = false;
  bool network_available = false;
  bool connected = false;

  // Event listeners
  void (*onErrorListener)(String msg) = NULL;
  void (*onWifiListener)(String address, String port) = NULL;
  void (*onConnectListener)() = NULL;
  void (*onDisconnectListener)() = NULL;
  void (*onDataListener)(String data) = NULL;

  String local_port = "";
  char TX_Fifo_Address = THR;

  struct SPI_UART_cfg SPI_Uart_config = {
      0x60, 0x00, 0x03, (char)(EFR_ENABLE_CTS | EFR_ENABLE_RTS | EFR_ENABLE_ENHANCED_FUNCTIONS)};

  void select(void);
  void deselect(void);

  bool data_available();
  bool wait_for_reponse(String  find, int timeout);

  void Flush_RX();
  void SPI_Uart_WriteArray(String data, long int NumBytes);
  void SPI_Uart_WriteByte(char address, char data);
  char SPI_Uart_ReadByte(char address);
  bool SPI_Uart_Init(void);
  long int SPI_Write(const char *srcptr, long int length);

  void SPI_Uart_println(String data);
  void SPI_Uart_print(String data);
  char spi_transfer(volatile char data);

  void debug(String msg);
  void debug(String msg, bool newline);
  void error(String msg);

  void init();
};

#endif