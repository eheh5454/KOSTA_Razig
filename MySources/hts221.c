#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <pthread.h>

#define BUF_SIZE 30
//#define IP "192.168.0.109"
#define PORT 9002
void* UDP_Server(void *arg);

char ip[BUF_SIZE];
char port[BUF_SIZE];

int check_con = 0;



int main(int argc, char *argv[])
{
	char tempchar[BUF_SIZE];
	char humichar[BUF_SIZE];


	//Using Thread
	pthread_t  t_id;
	int thread_param = 5;

	if(pthread_create(&t_id, NULL, UDP_Server, (void*)&thread_param)!=0)
	{
        puts("thread error()\n");
        return -1;
    }

   // Create I2C bus
   while(1) {


   int file;
   char *bus = "/dev/i2c-1";
   if((file = open(bus, O_RDWR)) < 0)
   {
      printf("Failed to open the bus. \n");
      exit(1);
   }
   // Get I2C device, HTS221 I2C address is 0x5F(95)
   ioctl(file, I2C_SLAVE, 0x5F);

   // Select average configuration register(0x10)
   // Temperature average samples = 16, humidity average samples = 32(0x1B)
   char config[2] = {0};
   config[0] = 0x10;
   config[1] = 0x1B;
   write(file, config, 2);
   sleep(1);
   // Select control register1(0x20)
   // Power on, block data update, data rate o/p = 1 Hz(0x85)
   config[0] = 0x20;
   config[1] = 0x85;
   write(file, config, 2);
   sleep(1);

   // Read Calibration values from the non-volatile memory of the device
   // Humidity Calibration values
   //Read 1 byte of data from address(0x30)
   char reg[1] = {0x30};
   write(file, reg, 1);
   char data[1] = {0};
   if(read(file, data, 1) != 1)
   {
      printf("Erorr : Input/output Erorr \n");
      exit(1);
   }
   char data_0 = data[0];
   // Read 1 byte of data from address(0x31)
   reg[0] = 0x31;
   write(file, reg, 1);
   read(file, data, 1);
   char data_1 = data[0];

   int H0 = data_0 / 2;
   int H1 = data_1 / 2;

   //Read 1 byte of data from address(0x36)
   reg[0] = 0x36;
   write(file, reg, 1);
   if(read(file, data, 1) != 1)
   {
      printf("Erorr : Input/output Erorr \n");
      exit(1);
   }
   data_0 = data[0];
   // Read 1 byte of data from address(0x37)
   reg[0] = 0x37;
   write(file, reg, 1);
   read(file, data, 1);
   data_1 = data[0];

   int H2 = (data_1 * 256) + data_0;

   //Read 1 byte of data from address(0x3A)
   reg[0] = 0x3A;
   write(file, reg, 1);
   if(read(file, data, 1) != 1)
   {
      printf("Erorr : Input/output Erorr \n");
      exit(1);
   }
   data_0 = data[0];
   // Read 1 byte of data from address(0x3B)
   reg[0] = 0x3B;
   write(file, reg, 1);
   read(file, data, 1);
   data_1 = data[0];

   int H3 = (data_1 * 256) + data_0;

   // Temperature Calibration values
   // Read 1 byte of data from address(0x32)
   reg[0] = 0x32;
   write(file, reg, 1);
   read(file, data, 1);

   int T0 = data[0];

   // Read 1 byte of data from address(0x33)
   reg[0] = 0x33;
   write(file, reg, 1);
   read(file, data, 1);

   int T1 = data[0];

   // Read 1 byte of data from address(0x35)
   reg[0] = 0x35;
   write(file, reg, 1);
   read(file, data, 1);

   int raw = data[0];

   // Convert the temperature Calibration values to 10-bits
   T0 = ((raw & 0x03) * 256) + T0;
   T1 = ((raw & 0x0C) * 64) + T1;

   //Read 1 byte of data from address(0x3C)
   reg[0] = 0x3C;
   write(file, reg, 1);
   if(read(file, data, 1) != 1)
   {
      printf("Erorr : Input/output Erorr \n");
      exit(1);
   }
   data_0 = data[0];
   // Read 1 byte of data from address(0x3D)
   reg[0] = 0x3D;
   write(file, reg, 1);
   read(file, data, 1);
   data_1 = data[0];

   int T2 = (data_1 * 256) + data_0;

   //Read 1 byte of data from address(0x3E)
   reg[0] = 0x3E;
   write(file, reg, 1);
   if(read(file, data, 1) != 1)
   {
      printf("Erorr : Input/output Erorr \n");
      exit(1);
   }
   data_0 = data[0];
   // Read 1 byte of data from address(0x3F)
   reg[0] = 0x3F;
   write(file, reg, 1);
   read(file, data, 1);
   data_1 = data[0];

   int T3 = (data_1 * 256) + data_0;

   // Read 4 bytes of data(0x28 | 0x80)
   // hum msb, hum lsb, temp msb, temp lsb
   reg[0] = 0x28 | 0x80;
   write(file, reg, 1);
   //char data[4] = {0};
   if(read(file, data, 4) != 4)
   {
      printf("Erorr : Input/output Erorr \n");
      exit(1);
   }
   else
   {
      // Convert the data
      int hum = (data[1] * 256) + data[0];
      int temp = (data[3] * 256) + data[2];

      if(temp > 32767)
      {
         temp -= 65536;
      }

      float humidity = ((1.0 * H1) - (1.0 * H0)) * (1.0 * hum - 1.0 * H2) / (1.0 * H3 - 1.0 * H2) + (1.0 * H0);
      float cTemp = ((T1 - T0) / 8.0) * (temp - T2) / (T3 - T2) + (T0 / 8.0) - 15;
      float fTemp = (cTemp * 1.8 ) + 32;



      if(check_con == 1)
      {
          // Output data to screen
          printf("Relative humidity : %.2f %RH \n", humidity);
          printf("Temperature in Celsius : %.2f C \n", cTemp);
          printf("Temperature in Fahrenheit : %.2f F \n", fTemp);

         //send tmep,hum
          sprintf(tempchar, "%0.2f", cTemp);
          sprintf(humichar, "%0.2f", humidity);
          char buff[BUF_SIZE]  = "TS";
          strcat(buff,tempchar);
          strcat(buff, humichar);

          int sock;
          //struct sockaddr_in serv_adr, from_adr;
          struct sockaddr_in serv_adr,from_adr;
          socklen_t adr_sz;

	      sock=socket(PF_INET, SOCK_DGRAM, 0);

          memset(&serv_adr, 0, sizeof(serv_adr));
          serv_adr.sin_family = AF_INET;
          serv_adr.sin_addr.s_addr = inet_addr(ip);
          serv_adr.sin_port=htons(atoi(port));

          printf("%s %s \n", ip, port);

          sendto(sock,buff,strlen(buff), 0, (struct sockaddr*)&serv_adr, sizeof(serv_adr));
          printf("send!\n");
          sleep(3);

      }

   }
}
}

//UDP Server Open
//get send target ip, port
void* UDP_Server(void *arg)
{
    //UDP Socket Setting
    int sock;
	int str_len;
	socklen_t adr_sz;
	socklen_t clnt_adr_sz;

	struct sockaddr_in serv_adr,clnt_adr;

	sock=socket(PF_INET, SOCK_DGRAM, 0);

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port=htons(PORT);

	int num = bind(sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr));

	while(1)
	{
        //for check cmd
        char buf[BUF_SIZE];
        char cmd[30];
        char local_ip[30];
        char local_port[30];

        printf("Waiting.......\n");
        str_len = recvfrom(sock, buf, BUF_SIZE-1, 0, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
        buf[str_len] = '\0';

        //get IP, PORT string
        strncpy(local_ip,buf+3,13);
        strncpy(local_port,buf+strlen(local_ip)+3,4);

        printf("%s %s\n",local_ip, local_port);

        //copy to global ip, port
        strcpy(ip,local_ip);
        strcpy(port,local_port);

        //change check
        check_con = 1;
	}

}
