#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <pthread.h>

/**
 * @file chatserver.h
 * @author surligas@csd.uoc.gr
 *
 * @brief The header file of the chatserver
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
 * Eg handle_client_connection_thread() will definitly called from the
 * handle_incoming_connections() function.
 *
 * For questions, problems, or bugs of this header file just email me.
 *
 * May the code be with you... :)
 */

/**
 * This is your default backlog size.
 * Use it at the right function.
 */
#define BACKLOG_SIZE 1000

/**
 * The different types of messges.
 * The message type is the first
 * byte of every message from the
 * client to the server and vice versa.
 *
 * As the message type is only one byte
 * we do not care about endianess problems.
 *
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


/**
 * In this struct we keep information
 * for every active connection.
 *
 * The server keeps a list with all
 * the connections. If a new client connects
 * the server adds a node to the list. Until
 * the client performs a loggin, is_logged_in
 * remains FALSE, and username is NULL.
 *
 * NOTE: Fill free to add your own fields
 */
typedef struct connection{
  int socket_discriptor; /**< The socket discriptor of the connection with the client */
  char *username;        /**< The username of the client or NULL if it is not logged in */
  char *ip;              /**< The IP address in dot notation of the client */
  bool is_logged_in;     /**< TRUE if it is logged in. FALSE otherwise */
  struct connection *previous;
  struct connection *next;
} connection_t;

/**
 * At every client thread we pass this struct as parameter
 * that contains all the info that you need.
 */

class chatserver;
typedef struct client_thread_params {
  connection_t *active_connections_list; /**< The list with all active connections */
  connection_t *client_connection;       /**< The node of the list that corresponds to this client */
  chatserver *caller;
  int *thread_count;
} client_thread_params_t;

/**
 * Pass this struct as parameter to the thread
 * that broadcasts the invitation messages
 * for the automatic server discovery.
 */
typedef struct broadcast_invitation_thread_params {
  int port;                             /**< The port on which the server listens */
  char *server_ip;                      /**< The IP address of the server */
  int refresh_rate;                     /**< Every how many seconds a broadcast message is sent */
  chatserver *caller;
  int socket_discriptor;
  char *braddr;
} broadcast_invitation_thread_params_t;


class chatserver {
private:
  int d_port_number;
  bool automatic;
  connection_t *d_connections_list;
   /* 3 different global counters
    * NOTE; These counters are accessed
    * simutanously by all threads, so race conditions
    * may rise. Go to the threads lab and check
    * about race conditions and locking.
    *
    * Some setters and getters functions may
    * need to access those counters
    */

  unsigned long int d_total_msgs_received;
  unsigned long int d_total_bytes_sent;
  unsigned long int d_total_bytes_received;

   /*
    * Three different mutexes that you are going to use for
    * locking in your threads.
    */
  pthread_mutex_t d_print_mutex;
  pthread_mutex_t d_counters_mutex;
  pthread_mutex_t d_active_connections_mutex;

  /***** Additions to original header *****/

  connection_t *add_connection(connection_t *new_con);
  connection_t *new_connection(int cfd, char* ip);
  void handle_list_users(int cfd);

  /***** End of additions to original header *****/

public:
  void start_broadcasting();
  /**
   * The constructor of the class chatserver.
   *
   * @param port the port number on which the server
   * will listen for TCP connections
   * @param automatic_server_discovery true if the automatic
   * server discovery is enabled, false otherwise.
   */
  chatserver(int port, bool automatic_server_discovery);

 /**
  * This function creates a server socket that listens at a
  * specific port and is binded to all available network interfaces
  * of the host.
  *
  * @returns the socket descriptor or -1 if something went wrong. If
  * something failed, a descriptive error message should be printed
  * by using perror() inside this function.
  */
  int create_tcp_server_socket();

  /**
   * This function runs on a seperate thread and prints every
   * refresh_rate seconds the values of the three counters
   * of the server.
   *
   * If your are curious why the parameter is void *, go to the
   * thread Lab and study it AGAIN carefully.
   *
   * @param[in] refresh_rate The interval in seconds between
   * two prints of the counters
   */
  void *print_stats_counters_thread(void *refresh_rate);

  /**
  * This function runs until the user kills the server
  * and waits for incoming connections. When accept()
  * returns a new connection, adds it at the d_connections_list
  * and pass the node (that contains all the necessary that you need)
  * at a new thread that handles the client. The pthread_create() takes
  * as arguement the function handle_client_thread(void *connection_info)
  *
  * @param[in] server_socket a valid server socket discriptor
  */
  void handle_incoming_connections(int server_socket);

  /**
  * This function runs on a new thread that has been created by they
  * handle_incoming_connections() function, and is responsible for
  * sending and receiving message from the connected client.
  *
  * If your are curious why the parameter is void *, go to the
  * thread Lab and study it AGAIN carefully.
  *
  * The thread terminates, when the server is killed
  * by the user.
  *
  * @param[in] thread_parameter A struct that contains
  * all the appropriate info for the client thread.
  * Use the client_thread_params_t struct that
  * has been casted to void *, as we said at the
  * LAB.
  */
  void *handle_client_connection_thread(void *thread_parameter);

  /**
  * Function that finds the type of the received message.
  *
  * @param[in] buffer The buffer containing the data received
  * from the client.
  * @param[in] buf_len The length of the buffer.
  *
  * @returns the message type. If the type could NOT be found
  * UKNOWN_MSG_TYPE should be returned.
  */
  msg_type_t get_message_type(char *buffer, size_t buf_len);

  /**
  * Function that sends back to the client a list with all the online users.
  *
  * @param[in] client_connection the node of the connections_list, that corresponds
  * to the current client.
  *
  * @returns The total number of bytes that were sent, or -1 in case of error.
  */
  int send_to_client_online_users(connection_t *client_connection);

  /**
  * When a login message received, try to login the user. In any case of error
  * this function sends back to the client the appropriate error message. Also
  * if the user succesfully logged in set the username, at the appropriate field
  * of the client_connection node.
  *
  * @param[in] client_connection the node of the connections_list, that corresponds
  * to the current client.
  * @param[in] buffer the data as they received from the client
  * @param[in] buf_len the length of the buffer
  *
  * @returns TRUE if the user succesfully logged in FALSE otherwise.
  */
  bool handle_login_message(connection_t *client_connection,
			       char *buffer,
			       size_t buf_len);

  /**
  * When a logout message received, logout the client by deleting the node at the
  * connections_list list and closing the established connection.
  *
  * @param[in] client_connection the node of the connections_list, that corresponds
  * to the current client.
  * @returns TRUE if the user succesfully logged out, FALSE otherwise.
  */
  bool handle_logout_message( connection_t *client_connection);


  /**
  * Function responsible for propagating a message
  * received from a client, to the right receiver that must
  * be connected and logged in.
  *
  * @param[in] client_connection the node of the connections_list, that corresponds
  * to the current client.
  * @param[in] buffer the data as they received from the client
  * @param[in] buf_len the length of the buffer
  *
  * @returns the number of bytes sent, or -1 in case of error. Note that in case of error,
  * this function should send back an error message to the client.
  */
  int handle_text_message(connection_t *client_connection,
			  char *buffer,
			  size_t buf_len);

  /**
  * General function that sends back to the client,
  * a specified error message.
  *
  * @param[in] error_type the message type that describes
  * the desired error.
  * @param[in] msg_content the contents of the error message.
  * This can be a discriptive human readable text.
  * @param[in] msg_content_len the length in bytes, of the msg_content
  *
  * @returns the number of bytes sent, or -1 in case of error.
  */
  int send_back_error_message(connection_t *client_connection,
			      msg_type_t error_type,
			      char *msg_content,
			      size_t msg_content_len);

  /**
  * Thread that performs the broadcast for the
  * automatic server discovery.
  * @param[in] broadcast_params all the necessary parameters
  * for the automatic server discovery.
  */
  void *broadcast_invitation_thread(void *broadcast_params);

};

  /***** Additions to original header *****/
  void *callHandle(void *data);
  void *callHandle2(void *data);
  void *callHandle3(void *data);
  /** Visual aids **/
  void display_usage();
  void *display_loading(void* d);

  /***** End of additions to original header *****/
#endif
