#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <time.h>

/*for getting file size using stat()*/
#include<sys/stat.h>

/*for sendfile()*/
#include<sys/sendfile.h>

/*for O_RDONLY*/
#include<fcntl.h>
#include "ftpklient.h"

#define h_addr h_addr_list[0]

//Set FTP server to passive mode and resolve data ports
void *ftp_enter_pasv(void *in) { //listening
    DATA *data = (DATA*) in;
    pthread_mutex_lock(data->mutex);
    data->portpasv = 1;
    usleep(1000000);
    char *find;
    char buf[10000];
    int pa;
    char *mess = "EPSV\r\n";
    send(data->sock, mess, strlen(mess), 0); //send a message on a socket
    bzero(buf, 10000); //vynulovanie buffera
    recv(data->sock, buf, 10000, 0); // receive a message from a socket, used for connected sockets
    printf("%s\n", buf);
    find = strrchr(buf, '('); //search for files in a directory hierarchy // locate character in string
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

int main() {
    struct sockaddr_in server;
    struct hostent *srv;
    struct stat obj;
    int port;
    int sock, sockpasv;
    int choice;
    //char *mess;
    char buf[10000], filename[20], in[30], nazov[50], host[20], user[20];
    int k;
    int size;
    int filehandle;

    sock = socket(AF_INET, SOCK_STREAM, 0); //inicializácia socketu
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv); //nastavenie optimálnosti socketu

    printf("Zadajte server: ");
    bzero(host, 20);
    scanf("%s", host);
    printf("Zadajte port: ");
    scanf("%d", &port);
    server.sin_family = AF_INET; //IPv4
    server.sin_port = htons(port); // convert values between host and network byte order
    srv = gethostbyname(host); //get network host entry
    bcopy(
            (char *) srv->h_addr, //source
            (char *) &server.sin_addr.s_addr, //destination
            srv->h_length //length
    );
    k = connect(sock, (struct sockaddr *) &server, sizeof(server)); //int connect(int socket, const struct sockaddr *address, socklen_t address_len);
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
    //mess = "PWD\r\n";
    send(sock, /*mess*/ (void *)"PWD\r\n", 5, 0);
    usleep(500000);
    while (data.kod == 530) {
        bzero(user, 20);
        printf("Zadajte pouzivatelske meno: ");
        bzero(in, 30);
        strcpy(in, "USER ");
        scanf("%s", user);
        strcat(in, user);
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
    FILE *logger;
    logger = fopen("log.log", "a+"); // a+ (create + append) option will allow appending which is useful in a log file
    if (logger == NULL) {
        printf("Logger not works.");
    } else {
        time_t rawtime;
        struct tm * timeinfo;
        time ( &rawtime );
        timeinfo = localtime ( &rawtime );
        fprintf(logger, "HOST: %s, USER: %s, TIME: %s", host, user, asctime(timeinfo));
        fclose(logger);
    };

    while (1) {
        printf("PC/FTP:\n1- PC\n2- FTP\nZvolte cislo: ");
        scanf("%d", &choice);
        if (choice == 1) {
            char command[50];
            char prepinace[5];
            char cesta[100];
            printf("Moznosti:\n1- get\n2- put\n3- pwd\n4- ls\n5- cd\n6- dele\n7- rmd\n8- mkd\n9- touch\n10- quit\nZvolte cislo: ");
            scanf("%d", &choice);
            switch (choice) {
                case 1: //GET
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
                case 2: // PUT
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
                case 3: // PWD
                    bzero(cesta, 50);
                    system("pwd");
                    //printf("%s\n", getcwd(cesta, 100));
                    break; //pwd
                case 4: //LS -L
                    system("ls -l");
                    break; //ls
                case 5: //CD
                    printf("Enter your path for command 'cd': ");
                    scanf("%s", cesta);
                    chdir(cesta);
                    break; //cd

                case 6: //DELE
                    bzero(buf, 10000);
                    printf("Enter a name of file you want to delete: ");
                    scanf("%s", buf);
                    remove(buf);
                    break;

                case 7: //RMDIR
                    bzero(buf, 10000);
                    printf("Enter a name of directory you want to delete: ");
                    scanf("%s", buf);
                    rmdir(buf);
                    break;

                case 8: //MKDIR
                    bzero(nazov, 50);
                    printf("Enter a name of directory you want to create: ");
                    scanf("%s", nazov);
                    mkdir(nazov,  0777);
                    break;

                case 9:   //TOUCH
                    bzero(buf, 10000);
                    bzero(nazov, 50);
                    printf("Enter a name of file you want to create: ");
                    scanf("%s", nazov);
                    strcpy(buf, "touch ");
                    strcat(buf, nazov);
                    system(buf);
                    break;

                case 10: //QUIT
                    //mess = "QUIT\r\n";
                    send(sock, /*mess*/(void *)"QUIT\r\n", 6, 0);
                    usleep(500000);
                    data.sock = 0;
                    pthread_detach(prijmac);
                    pthread_detach(pasiver);
                    usleep(1000000);
                    exit(0);
                default:
                    printf("Zly vstup.");
            }
        } else if (choice == 2) {
            printf("Moznosti:\n1- get\n2- put\n3- pwd\n4- ls\n5- cd\n6- dele\n7- rmd\n8- mkd\n9- quit\nZvolte cislo: ");
            scanf("%d", &choice);
            switch (choice) {
                case 1: //GET
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
                case 2: //PUT
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
                    usleep(00000);
                    break; //put
                case 3: //PWD
                    //mess = "PWD\r\n";
                    send(sock, /*mess*/ (void*)"PWD\r\n", 5, 0);
                    usleep(500000);
                    break;
                case 4: //LS -L
                    pthread_create(&pasiver, NULL, &ftp_enter_pasv, &data);
                    usleep(2000000);

                    //mess = "LIST\r\n";
                    send(sock, "LIST\r\n", 6, 0);
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
                case 5: //CD
                    strcpy(in, "CWD ");
                    printf("Enter the path to change the remote directory: ");
                    bzero(buf, 10000);
                    scanf("%s", buf);
                    strcat(in, buf);
                    strcat(in, "\r\n");
                    send(sock, in, strlen(in), 0);
                    usleep(500000);
                    break;

                case 6: //DELE
                    strcpy(in, "DELE ");
                    printf("Enter the file you want to remotely delete: ");
                    bzero(buf, 10000);
                    scanf("%s", buf);
                    strcat(in, buf);
                    strcat(in, "\r\n");
                    send(sock, in, strlen(in), 0);
                    usleep(500000);
                    break;

                case 7: //RMDIR
                    bzero(in, 30);
                    strcpy(in, "RMD ");
                    printf("Enter the directory you want to remotely delete: ");
                    bzero(buf, 10000);
                    scanf("%s", buf);
                    strcat(in, buf);
                    strcat(in, "\r\n");
                    send(sock, in, strlen(in), 0);
                    usleep(500000);
                    break;

                case 8: //MKDIR
                    bzero(in, 30);
                    strcpy(in, "MKD ");
                    printf("Enter name of directory you want to create remotely: ");
                    bzero(buf, 10000);
                    scanf("%s", buf);
                    strcat(in, buf);
                    strcat(in, "\r\n");
                    send(sock, in, strlen(in), 0);
                    usleep(500000);
                    break;
                    //TODO
                    //Na touch som nic nenasla, to vraj ale ani nie je, ze to je nejaka specificka
                    //forma STOR a to sme uz urobili

                case 9: //QUIT
                    //mess = "QUIT\r\n";
                    send(sock, "QUIT\r\n", 6, 0);
                    usleep(500000);
                    data.sock = 0;
                    pthread_detach(prijmac);
                    pthread_detach(pasiver);
                    usleep(1000000);
                    exit(0);
                default:
                    printf("Zly vstup.");
            }
        }
    }
}