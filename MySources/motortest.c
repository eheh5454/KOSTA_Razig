
/**
 *  file name:  motor_dc.c
 *  author:     JungJaeJoon (rgbi3307@nate.com) on the www.kernel.bz
 *  comments:   DC Motor Control
  *
 *  Copyright(C) www.kernel.bz
 *  This code is licenced under the GPL.
  *
 *  Editted:
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <wiringSerial.h>
#include <wiringPi.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>

#include "../lib/wiringPi.h"
#include "../lib/softPwm.h"

//Motor Speed GPIO
#define MOTOR_P1    30
#define MOTOR_P2    31
#define MOTOR_P3    21
#define MOTOR_P4    26

//Motor Direction GPIO
#define MOTOR_D1     4
#define MOTOR_D2     5
#define MOTOR_D3    28
#define MOTOR_D4    29

#define MOTOR_D1_GO     0
#define MOTOR_D2_GO     0
#define MOTOR_D3_GO     1
#define MOTOR_D4_GO     1

#define MOTOR_D1_BACK   1
#define MOTOR_D2_BACK   1
#define MOTOR_D3_BACK   0
#define MOTOR_D4_BACK   0

#define  BUFF_SIZE   160

#define SPEED_FIRST     20
#define SPEED_GOOD      30
#define SPEED_MAX       90
#define SPEED_MAX2      80
#define SPEED_MIN       15
#define SPEED_MIN2       5
#define SPEED_STEP       5
#define SPEED_RANGE    100

#define PORT 9001
#define BUF_SIZE 1024

int MotorRunning = 0;
unsigned int MotorSpeed = SPEED_FIRST;   //fast=4 ~ slow=24
char device[]= "/dev/ttyUSB0";
int fd;
unsigned long baud = 9600;
char getChar[] = "";
int j = 0;
int ultval, ultval2, clrval;
void *UDP_Server(void *arg);
int gocheck = 0;
int valcheck = 0;
int _valcheck = 0;
int valcheck2 = 0;
int _valcheck2 = 0;
int avoidmode = 0;
int avoidmodeout = 0;
int endmode = 0;

int auto_move = 0; //0 == passive, 1 == auto



static void motor_set_dir(int d1, int d2, int d3, int d4)
{
    digitalWrite(MOTOR_D1, d1);     ///Right Front Wheel
    digitalWrite(MOTOR_D2, d2);     ///Left Front Wheel
    digitalWrite(MOTOR_D3, d3);     ///Left Rear Wheel
    digitalWrite(MOTOR_D4, d4);     ///Right Rear Wheel
}

static void motor_set_speed(int s1, int s2, int s3, int s4)
{
    softPwmWrite(MOTOR_P1, s1);     ///Right Front Wheel
    softPwmWrite(MOTOR_P2, s2);     ///Left Front Wheel
    softPwmWrite(MOTOR_P3, s3);     ///Left Rear Wheel
    softPwmWrite(MOTOR_P4, s4);     ///Right Rear Wheel
}

static void motor_set_dec(void)
{
    while (MotorSpeed > SPEED_MIN2) {
        MotorSpeed -= SPEED_STEP;
        motor_set_speed(MotorSpeed, MotorSpeed, MotorSpeed, MotorSpeed);
        usleep(200);
    }
}

static void motor_set_inc(void)
{
    while (MotorSpeed < SPEED_GOOD) {
        MotorSpeed += SPEED_STEP;
        motor_set_speed(MotorSpeed, MotorSpeed, MotorSpeed, MotorSpeed);
        usleep(200);
    }
}

/**
//speed
//Soft PWM test(pin, duty, interval(Hz=1/f))
//-------------------------------------------------
// 3:  6: 2092Hz
// 6: 12: 1065Hz
//12: 24:  560Hz
//25: 50:  240Hz
//50:100   120Hz
*/
static int motor_init(void)
{
    fflush(stdout);

  //get filedescriptor
    if ((fd = serialOpen (device, baud)) < 0){
        fprintf (stderr, "Unable to open serial device: %s\n", strerror (errno)) ;
        exit(1); //error
    }

  //setup GPIO in wiringPi mode
    if (wiringPiSetup () == -1){
        fprintf (stdout, "Unable to start wiringPi: %s\n", strerror (errno)) ;
        exit(1); //error
    }

    pinMode(MOTOR_D1, OUTPUT);
    pinMode(MOTOR_D2, OUTPUT);
    pinMode(MOTOR_D3, OUTPUT);
    pinMode(MOTOR_D4, OUTPUT);

    motor_set_dir(MOTOR_D1_GO, MOTOR_D2_GO, MOTOR_D3_GO, MOTOR_D4_GO);

    pinMode(MOTOR_P1, OUTPUT);
    pinMode(MOTOR_P2, OUTPUT);
    pinMode(MOTOR_P3, OUTPUT);
    pinMode(MOTOR_P4, OUTPUT);

    digitalWrite(MOTOR_P1, 0);
    digitalWrite(MOTOR_P2, 0);
    digitalWrite(MOTOR_P3, 0);
    digitalWrite(MOTOR_P4, 0);

    pinMode(MOTOR_P1, PWM_OUTPUT);
    pinMode(MOTOR_P2, PWM_OUTPUT);
    pinMode(MOTOR_P3, PWM_OUTPUT);
    pinMode(MOTOR_P4, PWM_OUTPUT);

    return 0;
}

static void motor_setup(void)
{
    MotorSpeed = SPEED_FIRST;

    ///thread create
    softPwmCreate (MOTOR_P1, MotorSpeed, SPEED_RANGE, MODE_DC_MOTOR);
    softPwmCreate (MOTOR_P2, MotorSpeed, SPEED_RANGE, MODE_DC_MOTOR);
    softPwmCreate (MOTOR_P3, MotorSpeed, SPEED_RANGE, MODE_DC_MOTOR);
    softPwmCreate (MOTOR_P4, MotorSpeed, SPEED_RANGE, MODE_DC_MOTOR);

    MotorRunning = 1;
}

static void motor_close(void)
{
    ///thread cancel
    softPwmStop(MOTOR_P1);
    softPwmStop(MOTOR_P2);
    softPwmStop(MOTOR_P3);
    softPwmStop(MOTOR_P4);

    MotorRunning = 0;
}

static void motor_stop(void)
{
    motor_set_dec();

    if (MotorRunning) motor_close();
}

static void motor_go(void)
{
    if (!MotorRunning) motor_setup();

    motor_set_dec();

    motor_set_dir(MOTOR_D1_GO, MOTOR_D2_GO, MOTOR_D3_GO, MOTOR_D4_GO);

    ///speed up to SPEED_GOOD
    while (MotorSpeed < SPEED_GOOD) {
        MotorSpeed += SPEED_STEP;
        motor_set_speed(MotorSpeed, MotorSpeed, MotorSpeed, MotorSpeed);
        usleep(400);
    }
}

static void motor_back(void)
{
    if (!MotorRunning) motor_setup();

    motor_set_dec();

    motor_set_dir(MOTOR_D1_BACK, MOTOR_D2_BACK, MOTOR_D3_BACK, MOTOR_D4_BACK);

    ///speed up to SPEED_GOOD
    while (MotorSpeed < SPEED_GOOD) {
        MotorSpeed += SPEED_STEP;
        motor_set_speed(MotorSpeed, MotorSpeed, MotorSpeed, MotorSpeed);
        usleep(400);
    }
}

static void motor_left(void)
{
    if (!MotorRunning) motor_setup();

    motor_set_dec();
    motor_set_dir(MOTOR_D1_GO, MOTOR_D2_BACK, MOTOR_D3_BACK, MOTOR_D4_GO);

    ///speed up to SPEED_MAX2
    while (MotorSpeed < SPEED_MAX2) {
        MotorSpeed += SPEED_STEP;
        motor_set_speed(MotorSpeed, MotorSpeed, MotorSpeed, MotorSpeed);
        usleep(200);
    }
}

static void motor_right(void)
{
    if (!MotorRunning) motor_setup();

    motor_set_dec();
    motor_set_dir(MOTOR_D1_BACK, MOTOR_D2_GO, MOTOR_D3_GO, MOTOR_D4_BACK);

    ///speed up to SPEED_MAX2
    while (MotorSpeed < SPEED_MAX2) {
        MotorSpeed += SPEED_STEP;
        motor_set_speed(MotorSpeed, MotorSpeed, MotorSpeed, MotorSpeed);
        usleep(200);
    }
}

static void motor_left_go(void)
{
    if (!MotorRunning) motor_setup();

    motor_set_dec();
    motor_set_dir(MOTOR_D1_GO, MOTOR_D2_GO, MOTOR_D3_GO, MOTOR_D4_GO);
    motor_set_inc();

    MotorSpeed = SPEED_FIRST;
    motor_set_speed(SPEED_MAX2, MotorSpeed, MotorSpeed, SPEED_MAX2);
}

static void motor_right_go(void)
{
    if (!MotorRunning) motor_setup();

    motor_set_dec();
    motor_set_dir(MOTOR_D1_GO, MOTOR_D2_GO, MOTOR_D3_GO, MOTOR_D4_GO);
    motor_set_inc();

    MotorSpeed = SPEED_FIRST;
    motor_set_speed(MotorSpeed, SPEED_MAX2, SPEED_MAX2, MotorSpeed);
}

static void motor_left_back(void)
{
    if (!MotorRunning) motor_setup();

    motor_set_dec();
    motor_set_dir(MOTOR_D1_BACK, MOTOR_D2_BACK, MOTOR_D3_BACK, MOTOR_D4_BACK);
    motor_set_inc();

    MotorSpeed = SPEED_FIRST;
    motor_set_speed(SPEED_MAX2, MotorSpeed, MotorSpeed, SPEED_MAX2);
}

static void motor_right_back(void)
{
    if (!MotorRunning) motor_setup();

    motor_set_dec();
    motor_set_dir(MOTOR_D1_BACK, MOTOR_D2_BACK, MOTOR_D3_BACK, MOTOR_D4_BACK);
    motor_set_inc();

    MotorSpeed = SPEED_FIRST;
    motor_set_speed(MotorSpeed, SPEED_MAX2, SPEED_MAX2, MotorSpeed);
}

static void motor_faster(void)
{
    ///speed up to SPEED_MAX
    ///while (MotorSpeed < SPEED_MAX) {
        if (MotorSpeed >= SPEED_MAX) MotorSpeed = SPEED_MAX2;
        else MotorSpeed += SPEED_STEP;
        motor_set_speed(MotorSpeed, MotorSpeed, MotorSpeed, MotorSpeed);
        usleep(400);
    ///}
}

static void motor_slower(void)
{
    ///speed down to SPEED_MIN
    ///while (MotorSpeed > SPEED_MIN) {
        if (MotorSpeed <= SPEED_MIN) MotorSpeed = SPEED_MIN;
        else MotorSpeed -= SPEED_STEP;
        motor_set_speed(MotorSpeed, MotorSpeed, MotorSpeed, MotorSpeed);
        usleep(400);
    ///}
}


static void motor_action(char *mode)
{
    int i;
    char *action[] = { "stop", "go", "back", "left", "right"
                    , "left_go", "left_back", "right_go", "right_back"
                    , "faster", "slower" };
    void (*fn_action[])(void) = {
              motor_stop, motor_go, motor_back, motor_left, motor_right
            , motor_left_go, motor_left_back, motor_right_go, motor_right_back
            , motor_faster, motor_slower
            , motor_stop };

    for (i=0; i<11; i++) {
        if (!strcmp(mode, action[i])) break;
    }

    fn_action[i]();
}

//int rightcheck =0; // 4 -> 2sec 90deg
//int backcheck = 0;

void left_turn()
{
    motor_right_back();
    usleep(300000);
    motor_left();
    usleep(300000);
    motor_go();
}

void right_turn()
{
    //printf("right in\n");
    motor_left_back();
    usleep(300000);
    motor_right();
    usleep(300000);
    motor_go();
    //printf("right out");
}
void serialclr(){
    while(serialDataAvail(fd)) {
    char readData = serialGetchar(fd);
    printf("readdata = %d\n", readData);
    }
}

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

        printf("Waiting.......\n");
        str_len = recvfrom(sock, buf, BUF_SIZE-1, 0, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
        buf[str_len] = '\0';
        printf("%s\n", buf);

        if(strcmp(buf,"auto") == 0)
        {
            auto_move = 1;
        }
        else if(strcmp(buf,"passive") == 0)
        {
            auto_move = 0;
            endmode = 0;
            motor_stop();
        }
        else
        {
             motor_action(buf);
        }

	}
}



void loop(void){

 if(serialDataAvail (fd)){
    //printf("check\n");
    char readData = serialGetchar (fd);
    if(readData == '%')
        {
            ultval = atoi(getChar);
            printf("ult1 = %d\n", ultval);
            memset(getChar, '\0', j);
            j = 0;
            valcheck++;
        }
    //else if(readData == '#')
   // {
     //       ultval2 = atoi(getChar);
       //     printf("ult2 = %d\n", ultval2);
         //   memset(getChar, '\0', j);
           // j = 0;
       // }
    else if(readData == '^')
    {
            clrval = atoi(getChar);
            printf("clr = %d\n", clrval);
            memset(getChar, '\0', j);
            j = 0;
            valcheck2++;
    }
    else
    {
            getChar[j++] = readData;
    }


    if(ultval < 15 && valcheck>_valcheck && valcheck != 0 && avoidmodeout == 0)
    {
        if(endmode < 2) {
        _valcheck = valcheck;
        avoidmode = 1;
        avoidmodeout = 1;
        motor_left();
        printf("left\n");
        usleep(700000);
        motor_go();
        printf("go\n");
        usleep(3000000);
        motor_right();
        printf("right\n");
        usleep(1300000);
        }

        avoidmode = 2;
        _valcheck2 = valcheck2;
        endmode += 1;
        printf("endmode = %d\n", endmode);

        serialFlush(fd);
        memset(getChar, '\0', j);
        serialclr();
    }



    if(valcheck2 > _valcheck2)
    {
        if(avoidmode == 2 && clrval < 140)
        {
        printf("avoidmode2 IN\n");
        motor_go();
        }
        else if(avoidmode == 2 && clrval >= 140)
        {
        printf("avoidmode2 else IN\n");
        usleep(1000000);
        motor_stop();
        usleep(200000);
        motor_left();
        usleep(1500000);
        avoidmode = 0;
        }
    }

    if(avoidmode == 0)
    {
        if(clrval >= 140 && valcheck2 != 0 )
        {
        motor_go();
        gocheck++;
        printf("%d\n",gocheck);
        avoidmodeout = 0;
        }
        else if(valcheck2 > 1 && valcheck2 != 0)
        {
            if(gocheck < 20)
            {
                while(clrval < 140 && _valcheck2 <valcheck2 )
                {
                _valcheck2 = valcheck2-1;
                right_turn();
                _valcheck2++;
                gocheck = 0;
                break;
                }
                avoidmodeout = 0;
            }
            else
            {
            while(clrval < 140 && _valcheck2 <valcheck2 )
                {
                _valcheck2 = valcheck2-1;
                left_turn();
                _valcheck2++;
                gocheck = 0;
                break;
                }
                avoidmodeout = 0;
            }
        }
    }
    fflush(stdout);
    }

  }


int main(void)
{
    pthread_t  t_id;
    int thread_param = 5;

    //start udp thread
    if(pthread_create(&t_id, NULL, UDP_Server, (void*)&thread_param)!=0)
    {
        puts("thread error()\n");
        return -1;
    }

	if (motor_init() < 0) {
        printf("motor_init() error!\r\n");
        return -1;
	}
    avoidmode = 0;

    while(1)
    {
        while(endmode < 3 && auto_move == 1)
        {
            loop();
        }
    }

    motor_stop();
    motor_close();

   	return 0;

	}



