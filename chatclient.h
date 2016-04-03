#ifndef CHATCLIENT_H
#define CHATCLIENT_H

/**
 * @file chatclient.h
 * @author surligas@csd.uoc.gr
 *
 * @brief The header file of the chatclient
 *
 * This header file contains several structures
 * that are vital, in order to implement your server.
 *
 * Also contains the declarations of several functions.
 * All the functions below MUST be implemented, WITHOUT
 * changing its parameters or their return types.
 *
 * Although you are welcome to add your own.
 *
 * Note that in your program you should use, all of them,
 * having in mind that some of them are called inside some other.
 *
 * For questions, problems, or bugs of this header file just email me.
 *
 * May the code be with you... :)
 */

#include <stddef.h>
#include <string>



/**
 * The different types of messges.
 * The message type is the first
 * byte of every message from the
 * client to the server and vice versa.
 *
 * As the message type is only one byte
 * we do not care about endianess problems.
 * r
 * NOTE 1: Each message number is in hexademical
 * form.
 * NOTE 2: Enum in C are at least two bytes,
 * so make sure that you take the right byte
 * when you send it. A simple cast to unsigned
 * char would be enough. (Or not? :P )
 * NOTE 3: Fill free to add your own message types
 * However, they must be unique, one byte long, and
 * be carefull to be the same at the server and at the client.
 */
typedef enum {
  LOGIN_MSG = 0x1,
  LOGOUT_MSG = 0x2,
  TEXT_MSG = 0x3,
  USERNAME_NOT_EXIST = 0x4,
  USER_ALREADY_LOGGED = 0x5,
  USERS_LOGGED_REQUEST = 0x6,
  USERS_LOGGED_RESPONSE = 0x7,
  BROADCAST_MSG = 0x8,
  UKNOWN_MSG_TYPE = 0xFE,
  GENERAL_ERROR = 0xFF
} msg_type_t;


class chatclient{
private:
  unsigned int d_server_port;
  int d_socket;

  std::string d_server_ip;
  std::string d_username;

  bool d_use_automatic_server_disc;

public:
  /**
   * The contructor of the chatclient.
   *
   * @param server_ip The ip of the server
   * @param port The port that the server waits for
   * incoming connections.
   * @param automatic_discovery if true, the automatic server
   * discovery is used to find out the server address and port.
   * Also if this option is true server_ip and port parameters
   * are ignored.
   * @param username the username with which the client is going to
   * perform a login at the sevrer.
   */
  chatclient(std::string server_ip,
         unsigned int port,
         bool automatic_discovery,
         std::string username);

  /**
    * Function that finds the type of the received message.
    *
    * @param[in] buffer The buffer containing the data received
    * from the server.
    * @param[in] buf_len The length of the buffer.
    *
    * @returns the message type. If the type could NOT be found
    * UKNOWN_MSG_TYPE should be returned.
    */
    msg_type_t get_message_type(char *buffer, size_t buf_len);

    /*
     * Fill your methods here!
     */

    void search_server();
    int create_tcp_client_socket();
    void spawn_daemon();
    void prompt();
    int login();
    int logout();
    int list();
    int send_msg(char *choice);
};
  msg_type_t get_message_type(char *buffer, size_t buf_len);
  void display_usage();
  void *daemon(void *data);
  void *loading(void *found);


#endif


