/*
 * tpm2.net implementation, Copyright (C) 2015 Sebastian Förster
 * 
 */


#include <SPI.h>         
#include <Ethernet.h>
#include <EthernetUdp.h>  

#define LED RED_LED

//define some tpm constants
#define TPM2NET_LISTENING_PORT 65506

// set down if arduino with lesser UDP_PACKET_SIZE + PIXEL_DATA bytes of RAM is used
// keep track of your arduino board ram !!!
#define UDP_PACKET_SIZE   1600
#define PIXEL_DATA        (2048*3) /// 2048 PIXEL!

// buffers for receiving and sending data
// buffer to hold incoming LED Data... should be able to hold all the pixel data
// example 288 LEDs -> 288 * 3 = 864 byte minimum!
// keep track of your arduino board ram !!!
uint8_t packetBuffer[UDP_PACKET_SIZE]; 

byte mac[] = { 
  0xBE, 0x00, 0xBE, 0x00, 0xBE, 0x01 };
IPAddress ip(192, 168, 0, 30);

EthernetUDP Udp;


void setup() {  
  Serial.begin(115200);
  Serial.println("Init tmp2.net recv'er!");

  // start the Ethernet (dhcp) and UDP:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    Ethernet.begin(mac,ip);
  }

  printIPAddress();

  Udp.begin(TPM2NET_LISTENING_PORT);

  //setup (Tiva) SPI
  SPI.setModule(0); // SPI0/SSI0 -> PA2 = Clock, PA4 = Data
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  //tiva Launchpad runs SPI with 2 MHz Clock! -> SPI_CLOCK_DIV64
  SPI.setClockDivider(SPI_CLOCK_DIV64);
  SPI.begin();

  pinMode(LED, OUTPUT); 

  Serial.println("Setup done");
}


void loop() {

  switch (Ethernet.maintain())
  {
  case 1:
    //renewed fail
    Serial.println("Error: renewed fail");
    break;

  case 2:
    //renewed success
    Serial.println("Renewed success");

    //print your local IP address:
    printIPAddress();

    break;

  case 3:
    //rebind fail
    Serial.println("Error: rebind fail");
    break;

  case 4:
    //rebind success
    Serial.println("Rebind success");

    //print your local IP address:
    printIPAddress();
    break;

  default:
    {
      // if there's data available, read a packet
      int packetSize = Udp.parsePacket();
      // read the packet into packetBufffer
      int rx_data_count = Udp.read(packetBuffer, UDP_PACKET_SIZE);
      if(rx_data_count > 0) {
        tpm2_net_to_ws2801(packetBuffer,rx_data_count);
        //init for debugging
        //Serial.print("Received data of size ");
        //Serial.println(rx_data_count);
      }
    }
    break;

  }



}

void printIPAddress()
{
  Serial.print("My IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print(".");
  }

  Serial.println();
}


//////////////////// TPM2.net Handling /////////////////////////////////////////////
enum tpm2_net_states { //tpm2 states
  IDLE, 
  STARTB, 
  STARTB2,
  STARTB3,
  STARTB4,
  STARTB5,
  STARTB6,
  ENDB
};			

tpm2_net_states tpm2state = IDLE; //state maschine for protocol recv'ing

//variables used for counting packeges and bytes recv'ed
static uint16_t SpiCount = 0;
static uint16_t Framesize = 0;
static uint16_t data_counter = 0;
static uint8_t packet_counter = 1;
static uint8_t send_data_to_spi = false;

#define TPM2_BLOCK_START			0xC9
#define TPM2_BLOCK_DATAFRAME		        0xDA
#define TPM2_BLOCK_END				0x36

#define TPM2_ACK				0xAC
#define TPM2_NAK				0xFC

#define TPM2_NET_BLOCK_START		        0x9C

#define TPM2_BLOCK_COMMAND			0xC0

uint8_t tx_buffer_1[PIXEL_DATA];

void tpm2_net_to_ws2801(uint8_t  *buf,uint16_t len)
{
  for(uint16_t i = 0;i<len;i++) {
    if(tpm2state == STARTB6) {
      if(data_counter < Framesize) {
        if(SpiCount < PIXEL_DATA) {
          tx_buffer_1[SpiCount]=buf[i];
          SpiCount++;
          data_counter++;
          continue;
        } else {
          tpm2_reset_statemaschine();
        }
      }
      else
      {
        tpm2state = ENDB;
      }
    }

    //check end byte
    if(tpm2state == ENDB) {
      if(buf[i] == TPM2_BLOCK_END) {
        //send data to spi port
        if(send_data_to_spi) {
          //send frames here
          for(uint16_t a = 0; a < SpiCount; a++) {
            SPI.transfer(tx_buffer_1[a]);
          }
          //delay for ws2801 is set automatic by time to parse new frame...
          digitalWrite(LED, !digitalRead(LED));
          SpiCount = 0;
          send_data_to_spi = false;
          packet_counter = 1;
        } 
        else {
          packet_counter++;
        }
      }

      tpm2state = IDLE;
    }

    //check for start- and sizebyte
    else if(tpm2state == IDLE && buf[i] == TPM2_NET_BLOCK_START) {
      tpm2state = STARTB;
      continue;
    }

    else if(tpm2state == STARTB && buf[i] == TPM2_BLOCK_DATAFRAME) {
      tpm2state = STARTB2;
      continue;
    }

    else if(tpm2state == STARTB2)
    {
      Framesize = (uint16_t)buf[i]<<8;
      tpm2state = STARTB3;
      continue;
    }

    else if(tpm2state == STARTB3)
    {
      Framesize |= (uint16_t)buf[i];
      data_counter = 0;
      if(Framesize <= PIXEL_DATA) {
        tpm2state = STARTB4;
      } 
      else tpm2state = IDLE;

      continue;
    }
    else if(tpm2state == STARTB4) {
      if(packet_counter==buf[i]) {
        tpm2state = STARTB5;
      } 
      else {
        tpm2state = IDLE;
        tpm2_reset_statemaschine();

      }

      continue;
    }
    else if(tpm2state == STARTB5) {
      if(packet_counter==buf[i]) {
        packet_counter=1;
        send_data_to_spi = true;
      }



      tpm2state = STARTB6;

      continue;
    }
  }
}

void tpm2_reset_statemaschine(void)
{
  tpm2state = IDLE;
  SpiCount = 0;
  Framesize = 0;
  packet_counter = 1;
  send_data_to_spi = false;
}



