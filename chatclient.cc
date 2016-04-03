#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include "chatclient.h"

#define BROADCAST_PORT 9091
#define MAGIC_CODE "##404##"
//#define BROADCAST_ADDR "255.255.255.255"

int main(int argc, char **argv)
{
  int opt;
  int server_port = -1;

  char *username = NULL;
  char *server_ip = NULL;

  bool do_broadcast = false;
  if(argc<3)
  {
    display_usage();
    exit(EXIT_FAILURE);
  }
  while ((opt = getopt(argc, argv, "ha:u:p:")) != -1)
  {
    switch(opt){
      case 'p':
    server_port = atoi(optarg);
        if (server_port<1024)
        {
          printf("Please use a port > 1024\n");
          exit(EXIT_FAILURE);
        }
    break;
      case 'u':
    username = strdup(optarg);
    break;
      case 'a':
    server_ip = strdup(optarg);
    break;
      default:
    display_usage();
    exit(EXIT_FAILURE);
    }
  }

  if (username==NULL)
  {
    display_usage();
    exit(EXIT_FAILURE);
  }
  int k=0;
  while (username[k]!='\0')
  {
    if ( ( username[k] >='A' && username[k]<='Z')  ||( username[k] >='a' && username[k]<='z')  || ( username[k]>='0' && username[k]<='9') )
    {
      k++;
      if (k >10)
      {
        printf("The username must have at most 10 chars\n");
        exit(EXIT_FAILURE);
      }
    }
    else
    {
      printf("The username must contain only letters and characters\n");
          exit(EXIT_FAILURE);
    }
  }
  if (server_port==-1 || server_ip == NULL)
  {
    server_port = -1;
    server_ip = (char*)"";
    do_broadcast = true;
  }
  chatclient cl(server_ip,server_port,do_broadcast,username);
  if(do_broadcast) cl.search_server();
  int sfd = cl.create_tcp_client_socket();
  cl.spawn_daemon();
  system("clear");
  cl.prompt();
  return 1;
}

chatclient::chatclient(std::string server_ip,unsigned int port,bool automatic_discovery,std::string username)
{
  d_server_ip = server_ip;
  d_server_port = port;
  d_use_automatic_server_disc = automatic_discovery;
  d_username = std::string(username);
}

void chatclient::search_server()
{
  pthread_t visual;
  pthread_create(&visual, NULL, loading, (void*) &d_server_port);

  int sfd, ret;
  char msg[512];
  char *pch;
  struct sockaddr_in sinfo;
  int socket_reuseaddr = 1;
  struct ifreq ifrb;
  char *br_ip;
  char interface[] = "eth0";

  socklen_t len;

  sfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sfd<0)
  {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  ifrb.ifr_addr.sa_family = AF_INET;
  strncpy(ifrb.ifr_name, interface, IFNAMSIZ-1);
  ioctl(sfd,SIOCGIFBRDADDR,&ifrb);
  br_ip = strdup(inet_ntoa(((struct sockaddr_in*)&ifrb.ifr_broadaddr)->sin_addr));

  sinfo.sin_family = AF_INET;
  //sinfo.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);
  sinfo.sin_addr.s_addr = inet_addr(br_ip);
  sinfo.sin_port = htons(BROADCAST_PORT);

  ret = setsockopt(sfd,SOL_SOCKET,SO_REUSEADDR,&socket_reuseaddr,sizeof(socket_reuseaddr));
  if(ret<0)
  {
    perror("socket options");
    exit(EXIT_FAILURE);
  }
  ret = bind(sfd,(struct sockaddr *) &sinfo, sizeof(sinfo));
  len = sizeof(sinfo);

  while(1)
  {
    recvfrom(sfd, msg, sizeof(msg), 0, (struct sockaddr *) &sinfo, &len);
    pch = strtok(msg,"|");

    if(!strcmp(pch,MAGIC_CODE))
    {
      pch = strtok(NULL,"|");
      printf("%s\n",pch);
      d_server_port = atoi(pch);
      pch = strtok(NULL,"|");
      d_server_ip = strdup(pch);
      system("clear");
      printf("Server found: %s:%d\n\n",d_server_ip.c_str(),d_server_port);
      sleep(1.5);
      fprintf(stderr,"\nConnecting now...\n");
      sleep(1.5);
      break;
    }
  }
  pthread_join(visual, NULL);
  return;
}

void chatclient::prompt()
{
  char *choice =(char*) malloc(512*sizeof(char));
  printf("  *** HY335 CHAT CLIENT ***\n");
  printf("      -- Main Menu --\n\n\n");
  printf("Type login to login\n");
  printf("Type logout to logout\n");
  printf("Type sendto:'user to send' 'text' to text somebody\n");
  printf("Type users to view the list of  users\n");
  printf("Type clear to clear screen\n\n");
  while(1)
  {
    fprintf(stderr,"<< ");
    fflush(stdout);
    gets(choice);
    if(!strcmp(choice,"login"))
    {
      login();
    }
    else if(!strcmp(choice,"logout"))
    {
      logout();
      shutdown(d_socket,2);
      printf("<< Exiting\n");
      exit(EXIT_SUCCESS);
    }
    else if(!strncmp (choice,"sendto:",7))
    {
      send_msg(choice);
    }
    else if(!strcmp(choice,"users"))
    {
      list();
    }
    else if(!strcmp(choice,"clear"))
    {
      system("clear");
    }
    else
    {
      printf("<< Unknown command\n");
    }
  }
}

void *daemon(void *data)
{
  printf("Started daemon...\n");
  int sfd = (int) data;
  printf("%d\n",sfd);
  char msg[512];
  char body[400];
  char sender[11];
  char receiver[11];
  while(recv(sfd,msg,sizeof(msg),0))
  {
    msg_type_t c = get_message_type(msg,sizeof(msg));
    switch(c)
    {
      case TEXT_MSG:
        printf("You have a new message:\n");
        char *tmp;
        tmp = strtok(msg,"$");
        tmp = strtok(NULL,"$");
        strcpy(sender,tmp);
        tmp = strtok(NULL,"$");
        strcpy(receiver,tmp);
        tmp = strtok(NULL,"$");
        strcpy(body,tmp);
        printf("FROM: %s\n",sender);
        printf("%s\n",body);
        printf("<< ");
        fflush(stdout);
        break;
      case USERS_LOGGED_RESPONSE:
        printf(" --List of users--\n");
        printf("\n%s\n",msg+1);
        printf("<< ------------------\n");
        printf("<< ");
        fflush(stdout);
        break;
      case USER_ALREADY_LOGGED:
        printf("Server response: Username in use, please choose another username\n<< ");
        fflush(stdout);
        break;
      case USERNAME_NOT_EXIST:
        printf("Server response: Username not found\n<< ");
        fflush(stdout);
        break;
    }
  }
}

int chatclient::send_msg(char *choice)
{
  char sendto[11];
  char body[400];
  char *msg = (char*) malloc(512);
  char *pch = strtok(choice,":");
  char *str1 = strtok(NULL," ");
  char *str2 = strtok(NULL,"\n");
  if(str1!=NULL)strcpy(sendto,str1);
  else {printf("<< Specify receiver's username\n");return -1;}
  if(str2!=NULL)strcpy(body,str2);
  else {printf("<< You forgot to write your message!\n");return -1;}
  sprintf(msg,"%X", TEXT_MSG);
  strcat(msg,"$");
  strcat(msg,d_username.c_str());
  strcat(msg,"$");
  strcat(msg,sendto);
  strcat(msg,"$");
  strcat(msg,body);
  send(d_socket,msg,sizeof(msg),0);
}

int chatclient::login()
{
  char msg[11];
  sprintf(msg, "%X", LOGIN_MSG);
  strcat(msg,d_username.c_str());
  printf("<< Sending message to server: %s\n",msg);
  return send(d_socket,msg,sizeof(msg),0);
}

int chatclient::logout()
{
  char msg[2];
  sprintf(msg, "%X", LOGOUT_MSG);
  printf("<< Sending message to server: %s\n",msg);
  return send(d_socket,msg,sizeof(msg),0);
}

int chatclient::list()
{
  char msg[2];
  sprintf(msg, "%X", USERS_LOGGED_REQUEST);
  return send(d_socket,msg,sizeof(msg),0);
}

int chatclient::create_tcp_client_socket()
{
  int sfd;
  struct sockaddr_in sinfo;
  sfd = socket(AF_INET,SOCK_STREAM,0);
  if (sfd<0)
    perror("socket");
  memset(&sinfo,'0',sizeof(sinfo));
  sinfo.sin_family = AF_INET;
  sinfo.sin_port = htons(d_server_port);
  if(inet_pton(AF_INET,d_server_ip.c_str(),&sinfo.sin_addr)<=0)
  {
    perror("inet_pton");
    exit(EXIT_FAILURE);
  }
  if (connect(sfd,(struct sockaddr*)&sinfo,sizeof(sinfo))<0)
  {
    printf("Connection unsuccesfull :( Is server running?\n");
    perror("Details");
    exit(EXIT_FAILURE);
  }
  d_socket = sfd;
  return sfd;
}

msg_type_t chatclient::get_message_type(char *msg, size_t buf_len)
{
  char type[2];
  type[0] = msg[0];
  type[1] = '\0';
  msg_type_t c = (msg_type_t) strtol(type, NULL, 16);
  return c;
}

msg_type_t get_message_type(char *msg, size_t buf_len)
{
  char type[2];
  type[0] = msg[0];
  type[1] = '\0';
  msg_type_t c = (msg_type_t) strtol(type, NULL, 16);
  return c;
}

void chatclient::spawn_daemon()
{
  pthread_t thread;
  pthread_create(&thread,NULL,daemon,(void*)d_socket);
}

void display_usage()
{
  printf("Usage: chatclient [-a address -p port] -u username\n"
         "Options:\n"
         "   -p port             Specifies the listening port of the server\n"
         "   -a                  Specifies the address of the server\n"
         "   -u                  The username to be used\n"
         "   -h                  prints this help\n");
}

void *loading(void* d)
{
  int *port = (int*) d;
  int i;
  fprintf(stderr,"Searching for server\n");
  fprintf(stderr,"<               >");
  fprintf(stderr,"\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
  while(*port == -1)
  {
    for(i=0;i<14;i++)
    {
      if (*port!=-1) break;
      usleep(70000);
      fprintf(stderr," ");
    }
    for(i=0;i<14;i++)
    {
      if (*port!=-1) break;
      usleep(70000);
      fprintf(stderr,"\b");
    }
  }
}


