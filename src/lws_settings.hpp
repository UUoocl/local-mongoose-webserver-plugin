#pragma once
#include <QString>

struct LwsSettings {
    QString doc_root;
    int port = 8089;
};

LwsSettings lws_settings_load();
void        lws_settings_save(const LwsSettings &s);
