#ifndef MESSAGE_H
#define MESSAGE_H

#include <mysql.h>

void message_box_menu(MYSQL *conn, const char *logged_id);
void view_received_messages(MYSQL *conn, const char *logged_id);
void view_sent_messages(MYSQL *conn, const char *logged_id);
void send_message(MYSQL *conn, const char *sender_id);
void manage_join_requests(MYSQL *conn, const char *logged_id);
int is_club_leader(MYSQL *conn, const char *logged_id);
void send_system_message(MYSQL *conn, const char *receiver_id, const char *title, const char *content);

#endif
