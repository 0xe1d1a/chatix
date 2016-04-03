#include <string.h>
#include <string>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include "chatserver.h"

#define BROADCAST_PORT 9091
#define MAGIC_CODE "##404##"
#define REFRESH_RATE 5
//#define BROADCAST_ADDR "255.255.255.255"

int main(int argc, char **argv)
{
  int opt;
  int listen_port = -1;

  bool do_broadcast = false;
  if (argc<3)
  {
    display_usage();
    exit(EXIT_FAILURE);
  }
  while ((opt = getopt(argc, argv, "hbp:")) != -1) {
    switch(opt){
      case 'p':
	listen_port = atoi(optarg);
	if (listen_port<1024)
	{
	  printf("Please use a port > 1024\n");
	  exit(EXIT_FAILURE);
	}
	break;

      case 'b':
	/* Automatic server discovery enabled */
	do_broadcast = true;
	break;
      default:
	display_usage();
	exit(EXIT_FAILURE);
    }
  }
  chatserver sv(listen_port,do_broadcast);
  if(do_broadcast) sv.start_broadcasting();
  int sfd = sv.create_tcp_server_socket();
  sv.handle_incoming_connections(sfd);
  return 0;
}


/********************* Implementation ***********************/


chatserver::chatserver(int port, bool automatic_server_discovery)
{
  d_port_number = port;
  automatic = automatic_server_discovery;
  d_connections_list = (connection_t*) malloc(sizeof(connection_t));
  d_connections_list->next = NULL;
  d_connections_list->previous = NULL;
  d_total_msgs_received = 0;
  d_total_bytes_received = 0;
  d_total_bytes_sent = 0;

  pthread_mutex_init(&d_counters_mutex, NULL);
}

void *chatserver::handle_client_connection_thread(void *thread_parameter)
{
  client_thread_params_t *tmp = (client_thread_params_t*) thread_parameter;
  client_thread_params_t tp = *tmp;
  connection_t *head = tmp->active_connections_list;
  int cfd = tp.client_connection->socket_discriptor;
  char msg[512];
  int len, bytes;
  while(recv(cfd,msg,sizeof(msg),0))
  {
    printf("Received: %s from %d\n",msg,cfd);
    pthread_mutex_lock(&d_counters_mutex);
    d_total_msgs_received++;
    d_total_bytes_received+=strlen(msg);
    pthread_mutex_unlock(&d_counters_mutex);
    msg_type_t c = get_message_type(msg,sizeof(msg));
    if(tp.client_connection->is_logged_in)
    {
      switch(c){
	case LOGIN_MSG:
	  char msg2[11];
          sprintf(msg2, "%X", USER_ALREADY_LOGGED);
	  send(cfd, msg2,sizeof(msg2),0);
          pthread_mutex_lock(&d_counters_mutex);
          d_total_bytes_sent+=strlen(msg2);
	  pthread_mutex_unlock(&d_counters_mutex);
          printf("User already logged in\n");
	  break;
	case USERS_LOGGED_REQUEST:
	  send_to_client_online_users(tp.client_connection);
	  break;
	case TEXT_MSG:
	  handle_text_message(tp.client_connection,msg,sizeof(msg));
	  break;
	case LOGOUT_MSG:
	  if(handle_logout_message(tp.client_connection))
	     return NULL;
      }
    }
    else
    {
      if(c==LOGIN_MSG)
      {
        handle_login_message(tp.client_connection,msg,sizeof(msg));
      }
      else
      {
        printf("Ignoring  message from %s (%d), user not logged in\n",tp.client_connection->ip,cfd);
        if(cfd==-1) exit(EXIT_FAILURE); //broken port fix
        //char msg[11];
        //sprintf(msg, "%X", USER_NOT_LOGGED);
        //send(cfd, msg,sizeof(msg),0);
      }
    }
  }
}

void chatserver::start_broadcasting()
{
  int sfd, ret;
  struct sockaddr_in sinfo;
  socklen_t len;
  int sfd_broadcast = 1;
  pthread_t caster;
  broadcast_invitation_thread_params_t *tinfo;
  struct ifreq ifr;
  struct ifreq ifrb;
  char interface[] = "eth0";
  char *ip, *br_ip;

  sfd = socket(AF_INET,SOCK_DGRAM,0);
  if(sfd<0)
  {
    perror("udp socket");
    exit(EXIT_FAILURE);
  }
  memset(&sinfo,'0',sizeof(sinfo));
  sinfo.sin_family = AF_INET;
  sinfo.sin_addr.s_addr = htonl(INADDR_ANY);
  sinfo.sin_port = htons(BROADCAST_PORT);

  len = sizeof(sinfo);
  ret = setsockopt(sfd,SOL_SOCKET,SO_BROADCAST,&sfd_broadcast,sizeof(sfd_broadcast));
  if(ret<0)
  {
    perror("socket options");
    exit(EXIT_FAILURE);
  }
  ret = bind(sfd,(struct sockaddr *) &sinfo,sizeof(sinfo));
  if(ret<0)
  {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);
  ioctl(sfd,SIOCGIFADDR, &ifr);
  ip = strdup(inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr));

  ifrb.ifr_addr.sa_family = AF_INET;
  strncpy(ifrb.ifr_name, interface, IFNAMSIZ-1);
  ioctl(sfd,SIOCGIFBRDADDR,&ifrb);
  br_ip = strdup(inet_ntoa(((struct sockaddr_in*)&ifrb.ifr_broadaddr)->sin_addr));

  tinfo = (broadcast_invitation_thread_params*) malloc(sizeof(broadcast_invitation_thread_params));
  tinfo->refresh_rate = REFRESH_RATE;
  tinfo->caller = this;
  tinfo->socket_discriptor = sfd;
  tinfo->server_ip = ip;
  tinfo->braddr = br_ip;
  pthread_create(&caster, NULL, callHandle2, (void*) tinfo);
  return;
}

void chatserver::handle_incoming_connections(int server_socket)
{
  struct sockaddr_in cinfo;
  socklen_t cinfo_len;
  int cfd, thread_count=0;
  pthread_t *thread_pool, printer;
  client_thread_params_t *thread_info;
  connection_t *con_info;

  pthread_create(&printer,NULL,callHandle3,(void*)this);

  thread_info = (client_thread_params_t*) malloc(sizeof(client_thread_params_t));
  cinfo_len  = sizeof(struct sockaddr_in);
  while(1)
  {
    cfd = accept(server_socket,(struct sockaddr*)&cinfo,&cinfo_len);
    printf("Client with ip %s just connected.\n",inet_ntoa(cinfo.sin_addr));
    con_info = new_connection(cfd, (char*)inet_ntoa(cinfo.sin_addr));
    thread_info->active_connections_list = d_connections_list;
    thread_info->client_connection = con_info;
    thread_info->caller = this;
    thread_info->thread_count = &thread_count;
    pthread_t *thread_pool = (pthread_t *)malloc(sizeof(pthread_t));
    pthread_create(thread_pool,NULL,callHandle,(void*)thread_info);
    //thread_count++;
  }
}

void *chatserver::broadcast_invitation_thread(void *data)
{
  broadcast_invitation_thread_params_t *tmp = (broadcast_invitation_thread_params_t*) data;
  broadcast_invitation_thread_params_t tp = *tmp;
  int sfd = tp.socket_discriptor;
  struct sockaddr_in binfo;
  memset(&binfo,'0',sizeof(binfo));
  binfo.sin_family = AF_INET;
  binfo.sin_addr.s_addr = inet_addr(tp.braddr);
  //binfo.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);
  binfo.sin_port = htons(BROADCAST_PORT);
  char msg[100] = MAGIC_CODE;
  char port[10];
  sprintf(port,"%d",d_port_number);
  //sprintf(msg, "%X", BROADCAST_MSG);
  //strcat(msg,MAGIC_CODE);
  strcat(msg,"|");
  strcat(msg,port);
  strcat(msg,"|");
  strcat(msg,tp.server_ip);
  printf("Broadcasting ip:port\n");
  while(1)
  {
    sendto(sfd,msg,sizeof(msg),0,(struct sockaddr *) &binfo, sizeof(binfo));
    pthread_mutex_lock(&d_counters_mutex);
    d_total_bytes_sent+=strlen(msg);
    pthread_mutex_unlock(&d_counters_mutex);
    sleep(REFRESH_RATE);
  }
}


connection_t *chatserver::new_connection(int cfd, char* ip)
{
  connection_t *new_con  = (connection_t*) malloc(sizeof(connection_t));
  new_con->ip = strdup(ip);
  new_con->socket_discriptor = cfd;
  new_con->is_logged_in = false;
  return new_con;
}

connection_t *chatserver::add_connection(connection_t *new_con)
{
  connection_t *tmp = d_connections_list;
  connection_t *tmp2 = d_connections_list;
  tmp2 = tmp2->next;
  while(tmp2!=NULL)
  {
    if(!strcmp(tmp2->username, new_con->username))
    {
      return NULL;
    }
    tmp2=tmp2->next;
  }
  while(tmp->next!=NULL)
  {
    tmp=tmp->next;
  }
  new_con->next = NULL;
  new_con->previous = tmp;
  tmp->next = new_con;
  return new_con;
}

void *chatserver::print_stats_counters_thread(void *refresh_rate)
{
  while(1)
  {
    printf("Total messages: %d | ",d_total_msgs_received);
    printf("Total bytes received: %d | ",d_total_bytes_received);
    printf("Total bytes sent: %d\n",d_total_bytes_sent);
    sleep(REFRESH_RATE);
  }
}

bool chatserver::handle_login_message(connection_t *client_connection,char *msg,size_t buf_len)
{
  client_connection->username = strdup(msg+1);
  connection_t *tmp = add_connection(client_connection);
  if (tmp == NULL)
  {
    printf("User %s already logged in\n",client_connection->username);
    char msg[11];
    sprintf(msg, "%X", USER_ALREADY_LOGGED);
    send(client_connection->socket_discriptor,msg,sizeof(msg),0);
    pthread_mutex_lock(&d_counters_mutex);
    d_total_bytes_sent+=strlen(msg);
    pthread_mutex_unlock(&d_counters_mutex);
    return false;
  }
  else
  {
    client_connection->is_logged_in = true;
    printf("%s just logged in\n",client_connection->username);
    return true;
  }
}

int chatserver::handle_text_message(connection_t *client_connection,char *buffer,size_t buf_len)
{
  connection_t *iterator = d_connections_list->next;
  char *tmp;
  char msg[512];
  char body[400];
  char sender[11];
  char receiver[11];
  strcpy(msg,buffer);
  tmp = strtok(buffer,"$");
  tmp = strtok(NULL,"$");
  strcpy(sender,tmp);
  tmp = strtok(NULL,"$");
  strcpy(receiver,tmp);
  tmp = strtok(NULL,"$");
  strcpy(body,tmp);
  while(iterator!=NULL)
  {
    if(!strcmp(iterator->username,receiver))
    {
      int receiver_sfd = iterator->socket_discriptor;
      send(receiver_sfd,msg,sizeof(msg),0);
      pthread_mutex_lock(&d_counters_mutex);
      d_total_bytes_sent+=strlen(msg);
      pthread_mutex_unlock(&d_counters_mutex);
      return 1;
    }
    iterator=iterator->next;
  }
  char msg2[11];
  sprintf(msg2, "%X", USERNAME_NOT_EXIST);
  send(client_connection->socket_discriptor,msg2,sizeof(msg2),0);
  pthread_mutex_lock(&d_counters_mutex);
  d_total_bytes_sent+=strlen(msg2);
  pthread_mutex_unlock(&d_counters_mutex);
  return -1;
}

int chatserver::send_to_client_online_users(connection_t *client_connection)
{
  int cfd = client_connection->socket_discriptor;
  char msg[512];
  sprintf(msg, "%X", USERS_LOGGED_RESPONSE);
  connection_t *tmp;
  tmp = d_connections_list->next;
  while(tmp!=NULL)
  {
    if(tmp->is_logged_in)
    {
      strcat(msg,tmp->username);
      strcat(msg,"\n");
    }
    tmp=tmp->next;
  }
  pthread_mutex_lock(&d_counters_mutex);
  d_total_bytes_sent+=strlen(msg);
  pthread_mutex_unlock(&d_counters_mutex);
  return send(cfd,msg,sizeof(msg),0);
}

int chatserver::create_tcp_server_socket()
{
  struct sockaddr_in sinfo;
  struct hostent *host;
  int sfd;
  sfd = socket(AF_INET,SOCK_STREAM,0);
  if (sfd<0)
    perror("socket");
  memset(&sinfo, '0', sizeof(sinfo));
  sinfo.sin_family = AF_INET;
  sinfo.sin_addr.s_addr = htonl(INADDR_ANY);
  sinfo.sin_port = htons(d_port_number);
  if (bind(sfd,(struct sockaddr *)&sinfo,sizeof(sinfo))<0)
  {
    perror("bind");
    exit(EXIT_FAILURE);
  }
  if (listen(sfd,5)<0)
  {
    perror("listen");
    exit(EXIT_FAILURE);
  }
  printf("Server created on port %d\n",d_port_number);
  return sfd;
}

msg_type_t chatserver::get_message_type(char *msg, size_t buf_len)
{
  char type[2];
  type[0] = msg[0];
  type[1] = '\0';
  msg_type_t c = (msg_type_t) strtol(type, NULL, 16);
  return c;
}

void *callHandle(void *data)
{
  client_thread_params_t *tmp = (client_thread_params_t*) data;
  client_thread_params_t tp = *tmp;
  tp.caller->handle_client_connection_thread(data);
}

void *callHandle2(void *data)
{
  broadcast_invitation_thread_params_t *tmp = (broadcast_invitation_thread_params_t*) data;
  broadcast_invitation_thread_params_t tp = *tmp;
  tp.caller->broadcast_invitation_thread(data);
}

void *callHandle3(void *data)
{
  chatserver *caller = (chatserver*) data;
  caller->print_stats_counters_thread(NULL);
}

bool chatserver::handle_logout_message( connection_t *client_connection)
{
  connection_t *tmp = d_connections_list;
  int sfd;
  char *usr;
  while (tmp!=NULL)
  {
    if (tmp == client_connection)
    {
      sfd = tmp->socket_discriptor;
      usr = strdup(tmp->username);
      if (tmp->previous==NULL)
      {
        tmp->next->previous=d_connections_list;
        free(tmp);
      }
      else if (tmp->next==NULL)
      {
        tmp->previous->next=NULL;
	free(tmp);
      }
      else
      {
	tmp->previous->next=tmp->next;
	tmp->next->previous=tmp->previous;
	free(tmp);
      }
      printf("%s logged out\n",usr);
      shutdown(sfd,2);
      return true;
    }
    tmp=tmp->next;
  }
  printf("Error username not found smth wrong, i hope this doesnt happen in exam\n");
  return false;
}

void display_usage()
{
  printf("Usage: chatserver -p port [-b] \n"
         "Options:\n"
         "   -p port             Specifies the listening port of the server\n"
         "   -b                  Enables the automatic server discovery\n"
         "   -h                  prints this help\n");
}


