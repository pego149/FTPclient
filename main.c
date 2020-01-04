
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>

/*for getting file size using stat()*/
#include<sys/stat.h>

/*for sendfile()*/
#include<sys/sendfile.h>

/*for O_RDONLY*/
#include<fcntl.h>
#include "ftpklient.h"

//Set FTP server to passive mode and resolve data ports
void *ftp_enter_pasv(void *in) {
    DATA *data = (DATA*) in;
    pthread_mutex_lock(data->mutex);
    data->portpasv = 1;
    usleep(1000000);
    char *find;
    int a, b, c, d;
    char buf[10000];
    int pa, pb;
    char *mess = "EPSV\r\n";
    send(data->sock, mess, strlen(mess), 0);
    bzero(buf, 10000);
    recv(data->sock, buf, 10000, 0);
    printf("%s\n", buf);
    find = strrchr(buf, '(');
    sscanf(find, "(|||%d", &pa);
    data->portpasv = pa;
    pthread_mutex_unlock(data->mutex);
    return NULL;
}

void *recv_ftp(void *in) {
    DATA *data = (DATA*) in;
    while(data->sock > 0) {
        if(data->portpasv == 1) {
            sleep(3);
            data->portpasv = 2;
        }
        char buf[10000];
        bzero(buf, 10000);
        recv(data->sock, buf, 10000, 0);
        printf("%s", buf);
        sscanf(buf, "%d", &data->kod);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    struct sockaddr_in server;
    struct hostent *srv;
    struct stat obj;
    int port;
    int sock, sockpasv;
    int choice, true;
    char *mess;
    char buf[10000], filename[20], in[30];
    char *f;
    int k;
    int size;
    int status;
    int filehandle;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    printf("Zadajte server: ");
    bzero(buf, 10000);
    scanf("%s", buf);
    printf("Zadajte port: ");
    scanf("%d", &port);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    srv = gethostbyname(buf);
    bcopy(
            (char *) srv->h_addr,
            (char *) &server.sin_addr.s_addr,
            srv->h_length
    );
    k = connect(sock, (struct sockaddr *) &server, sizeof(server));
    if (k != 0) {
        printf("Connect Error");
        exit(1);
    }

    pthread_mutex_t mutexik;
    pthread_mutex_init(&mutexik, NULL);
    pthread_t prijmac;
    pthread_t pasiver;
    DATA data = {sock, 0, 0,&mutexik};
    pthread_create(&prijmac, NULL, &recv_ftp, &data);

    usleep(500000);
    mess = "PWD\r\n";
    send(sock, mess, strlen(mess), 0);
    usleep(500000);
    while (data.kod == 530) {
        bzero(filename, 20);
        printf("Zadajte pouzivatelske meno: ");
        bzero(in, 30);
        strcpy(in, "USER ");
        scanf("%s", buf);
        strcat(in, buf);
        strcat(in, "\r\n");
        send(sock, in, strlen(in), 0);
        usleep(500000);
        printf("Zadajte heslo: ");
        strcpy(in, "PASS ");
        bzero(buf, 10000);
        scanf("%s", buf);
        strcat(in, buf);
        strcat(in, "\r\n");
        send(sock, in, strlen(in), 0);
        usleep(1000000);
    }
    usleep(500000);

    int i = 1;
    while (1) {
        printf("PC/FTP:\n1- PC\n2- FTP\nZvolte cislo: ");
        scanf("%d", &choice);
        if (choice == 1) {
            char command[50];
            char prepinace[5];
            char cesta[100];
            printf("Moznosti:\n1- get\n2- put\n3- pwd\n4- ls\n5- cd\n6- quit\nZvolte cislo: ");
            scanf("%d", &choice);
            switch (choice) {
                case 1:
                    printf("Enter filename to get from server: ");
                    scanf("%s", filename);

                    pthread_create(&pasiver, NULL, &ftp_enter_pasv, &data);
                    usleep(2000000);

                    bzero(buf, 10000);
                    strcpy(buf, "RETR ");
                    strcat(buf, filename);
                    strcat(buf, "\r\n");
                    send(sock, buf, 100, 0);
                    usleep(500000);
                    if(data.kod == 550) {
                        printf("%s\n", buf);
                        break;
                    }

                    server.sin_port = htons(data.portpasv);
                    sockpasv = socket(AF_INET, SOCK_STREAM, 0);
                    connect(sockpasv, (struct sockaddr *) &server, sizeof(server));
                    usleep(500000);

                    bzero(buf, 10000);
                    recv(sockpasv, buf, 10000, 0);
                    filehandle = open(filename, O_CREAT | O_EXCL | O_WRONLY, 0666);
                    write(filehandle, buf, strlen(buf));
                    close(sockpasv);
                    close(filehandle);
                    usleep(500000);
                    break; //get
                case 2:
                    printf("Enter filename to put to server: ");
                    scanf("%s", filename);
                    filehandle = open(filename, O_RDONLY);
                    if (filehandle == -1) {
                        printf("Subor nenajdeny\n\n");
                        break;
                    }
                    pthread_create(&pasiver, NULL, &ftp_enter_pasv, &data);
                    usleep(2000000);

                    bzero(buf, 10000);
                    strcpy(buf, "STOR ");
                    strcat(buf, filename);
                    strcat(buf, "\r\n");
                    send(sock, buf, 100, 0);
                    usleep(500000);
                    if(data.kod == 550) {
                        break;
                    }

                    server.sin_port = htons(data.portpasv);
                    sockpasv = socket(AF_INET, SOCK_STREAM, 0);
                    connect(sockpasv, (struct sockaddr *) &server, sizeof(server));
                    usleep(300000);
                    stat(filename, &obj);
                    size = obj.st_size;
                    sendfile(sockpasv, filehandle, NULL, size);
                    close(sockpasv);
                    close(filehandle);
                    usleep(1000000);
                    break; //put
                case 3:
                    bzero(cesta, 50);
                    printf("%s\n", getcwd(cesta, 100));
                    break; //pwd
                case 4:
                    system("ls -l");
                    break; //ls
                case 5:
                    printf("Enter your path for command 'cd': ");
                    scanf("%s", cesta);
                    chdir(cesta);
                    break; //cd
                case 6:
                    mess = "QUIT\r\n";
                    send(sock, mess, strlen(mess), 0);
                    usleep(500000);
                    pthread_exit(0);
                    exit(0);
                default:
                    printf("Zly vstup.");
            }
        } else if (choice == 2) {
            printf("Moznosti:\n1- get\n2- put\n3- pwd\n4- ls\n5- cd\n6- quit\nZvolte cislo: ");
            scanf("%d", &choice);
            switch (choice) {
                case 1:

                    break;
                case 2:

                    break;
                case 3:
                    mess = "PWD\r\n";
                    send(sock, mess, strlen(mess), 0);
                    usleep(500000);
                    break;
                case 4:
                    pthread_create(&pasiver, NULL, &ftp_enter_pasv, &data);
                    usleep(2000000);

                    mess = "LIST\r\n";
                    send(sock, mess, strlen(mess), 0);
                    usleep(500000);
                    server.sin_port = htons(data.portpasv);
                    sockpasv = socket(AF_INET, SOCK_STREAM, 0);
                    connect(sockpasv, (struct sockaddr *) &server, sizeof(server));
                    usleep(500000);
                    bzero(buf, 10000);
                    recv(sockpasv, buf, 10000, 0);
                    printf("Obsah priecinka:\n%s\n", buf);
                    close(sockpasv);
                    usleep(1000000);
                    break;
                case 5:
                    strcpy(in, "CWD ");
                    printf("Enter the path to change the remote directory: ");
                    bzero(buf, 10000);
                    scanf("%s", buf);
                    strcat(in, buf);
                    strcat(in, "\r\n");
                    send(sock, in, strlen(in), 0);
                    usleep(500000);
                    break;
                case 6:
                    mess = "QUIT\r\n";
                    send(sock, mess, strlen(mess), 0);
                    usleep(500000);
                    pthread_exit(0);
                    exit(0);
                default:
                    printf("Zly vstup.");
            }
        }
    }
}