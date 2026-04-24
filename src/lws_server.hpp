#pragma once
#include <QString>

// HTTP Server
int  lws_http_server_start(const QString &doc_root, int preferred_port);
void lws_http_server_stop();
int  lws_http_server_port();
bool lws_http_server_is_running();
bool lws_http_server_failed(); // New: true if start was attempted but failed

// WebSocket Server
int  lws_ws_server_start(int preferred_port);
void lws_ws_server_stop();
int  lws_ws_server_port();
bool lws_ws_server_is_running();
bool lws_ws_server_failed(); // New: true if start was attempted but failed
void lws_server_shutdown();
void lws_ws_server_broadcast(const char *message);
void lws_ws_server_broadcast_topic(const char *topic, const char *message, bool is_binary = false);
void lws_ws_server_broadcast_topic_raw(const char *topic, const void *data, size_t len, bool is_binary = false);
void lws_ws_server_broadcast_topic_state(const char *topic, const char *addr, const char *json_str, bool is_binary = false);
void lws_ws_server_broadcast_state(const char *addr, const char *json_str);

QString lws_server_doc_root();
