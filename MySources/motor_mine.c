#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <dirent.h>

#define BUF_SIZE 100

#define PORT "9001"

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
#define SPEED_GOOD      50
#define SPEED_MAX       90
#define SPEED_MAX2      80
#define SPEED_MIN       15
#define SPEED_MIN2       5
#define SPEED_STEP       5
#define SPEED_RANGE    100


int MotorRunning = 0;
unsigned int MotorSpeed = SPEED_FIRST;   //fast=4 ~ slow=24

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
    if (wiringPiSetup () == -1) return -1;

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

    printf("motor go\n");

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
                    , "faster", "slower","stop" };
    void (*fn_action[])(void) = {
              motor_stop, motor_go, motor_back, motor_left, motor_right
            , motor_left_go, motor_left_back, motor_right_go, motor_right_back
            , motor_faster, motor_slower
            , motor_stop };

    for (i=0; i<11; i++) {
        if (!strcmp(mode, action[i]))
        {
            printf("action: %s",action[i]);
            break;
        }

    }


    fn_action[i]();
}

int main(void)
{
	int sock;
	char message[BUF_SIZE];
	int str_len;
	socklen_t adr_sz;
	socklen_t clnt_adr_sz;

	struct sockaddr_in serv_adr,clnt_adr;

	sock=socket(PF_INET, SOCK_DGRAM, 0);

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port=htons(atoi(PORT));

	if (motor_init() < 0) {
        printf("motor_init() error!\r\n");
        return -1;
	}

	int num = bind(sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr));
	//motor_close();


    int i = 0;
    //printf("\n");
    motor_action("stop");
//    while(1)
//    {
//        motor_action("go");
//        sleep(0.5);
//    }
//    motor_close();

    //motor_action("go");

//    while(1)
//    {
//        scanf("%s",message);
//        motor_action(message);
//
//    }
	//motor_action("right");

//	while(1)
//	{
//        printf("Waiting.......\n");
//        str_len = recvfrom(sock, message, BUF_SIZE-1, 0, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
//        message[str_len] = '\0';
//        printf("%s (%d)\n", message,strlen);
//       motor_action(message);
//
//	}
}
