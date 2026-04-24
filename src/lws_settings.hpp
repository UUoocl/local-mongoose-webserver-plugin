#pragma once
#include <QString>

struct LwsSettings {
    QString doc_root;
    int http_port = 4466;
    bool http_enabled = true;
    int ws_port = 4466;
    bool ws_enabled = true;

    // Manual OBS connection info
    int obs_port = 4455;
    QString obs_password;
};

LwsSettings lws_settings_load();
void        lws_settings_save(const LwsSettings &s);
