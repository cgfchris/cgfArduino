// web_server.h
#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h> 

void initialize_web_server();
void handle_web_server_clients();
void notify_web_server_wifi_status(bool connected);

#endif // WEB_SERVER_H
