#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>


#define BUF_SIZE 1024
#define PORT 9000
#define IP "192.168.0.109"

int main()
{
    int sock;
    socklen_t adr_sz;
    struct sockaddr_in serv_adr;

    sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family=AF_INET;
    serv_adr.sin_addr.s_addr = inet_addr(IP);
    serv_adr.sin_port=htons(PORT);

    connect(sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr));

    char filename[256];
    char filebuf[BUF_SIZE];
    char sizebuf[BUF_SIZE];
    int numread = 0;
    int numtotal = 0;

    FILE *fp;

    //system("raspistill -o capture.jpg");

    fp = fopen("capture.jpg", "rb");

    fseek(fp,0,SEEK_END);

    int totalbyte = ftell(fp);
    sprintf(sizebuf,"%d",totalbyte);
    write(sock,sizebuf,sizeof(sizebuf));

    rewind(fp);

    while(feof(fp)==0)
    {
        numread = fread(filebuf, sizeof(char), BUF_SIZE, fp);
        write(sock,filebuf,numread);
        numtotal += numread;
        printf("%d\n",numtotal);
        sleep(0.2);
        if(numtotal == totalbyte)
            break;
    }
    fclose(fp);
    close(sock);
    printf("total:%d", totalbyte);

    return 0;
}
