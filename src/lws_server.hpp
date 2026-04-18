#pragma once
#include <QString>

int  lws_server_start(const QString &doc_root, int preferred_port);
void lws_server_stop();
int  lws_server_port();
bool lws_server_is_running();
QString lws_server_doc_root();
