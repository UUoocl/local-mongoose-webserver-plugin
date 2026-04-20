#include "lws_settings.hpp"
#include "lws_paths.hpp"
#include "config.hpp"
#define LOG_TAG "[" PLUGIN_NAME "][settings]"
#include "log.hpp"

#include <QSettings>

static inline QString org_name() { return QStringLiteral("MMLTech"); }
static inline QString app_name() { return QStringLiteral("local-mongoose-webserver"); }
static inline QString key_http_port() { return QStringLiteral("server/http_port"); }
static inline QString key_http_enabled() { return QStringLiteral("server/http_enabled"); }
static inline QString key_ws_port() { return QStringLiteral("server/ws_port"); }
static inline QString key_ws_enabled() { return QStringLiteral("server/ws_enabled"); }
static inline QString key_obs_port() { return QStringLiteral("obs/port"); }
static inline QString key_obs_password() { return QStringLiteral("obs/password"); }

LwsSettings lws_settings_load()
{
    LwsSettings s;
    s.doc_root = lws_get_data_root_no_ui();
    if (s.doc_root.isEmpty())
        s.doc_root = lws_get_data_root();

    QSettings qs(org_name(), app_name());
    
    // Migration: if old "server/port" exists, use it for http_port
    if (qs.contains(QStringLiteral("server/port"))) {
        s.http_port = qs.value(QStringLiteral("server/port"), 4466).toInt();
        qs.remove(QStringLiteral("server/port"));
    } else {
        s.http_port = qs.value(key_http_port(), 4466).toInt();
    }
    
    if (s.http_port < 1024 || s.http_port > 65535)
        s.http_port = 4466;

    s.http_enabled = qs.value(key_http_enabled(), true).toBool();
    s.ws_port = qs.value(key_ws_port(), 4477).toInt();
    if (s.ws_port < 1024 || s.ws_port > 65535)
        s.ws_port = 4477;
    s.ws_enabled = qs.value(key_ws_enabled(), true).toBool();
    s.obs_port = qs.value(key_obs_port(), 4455).toInt();
    s.obs_password = qs.value(key_obs_password(), QString()).toString();

    return s;
}

void lws_settings_save(const LwsSettings &s)
{
    if (!s.doc_root.isEmpty())
        lws_set_data_root(s.doc_root);

    QSettings qs(org_name(), app_name());
    qs.setValue(key_http_port(), s.http_port);
    qs.setValue(key_http_enabled(), s.http_enabled);
    qs.setValue(key_ws_port(), s.ws_port);
    qs.setValue(key_ws_enabled(), s.ws_enabled);
    qs.setValue(key_obs_port(), s.obs_port);
    qs.setValue(key_obs_password(), s.obs_password);
    qs.sync();

    LOGI("Settings saved: http=%d(%s) ws=%d(%s) doc_root=%s",
         s.http_port, s.http_enabled ? "on" : "off",
         s.ws_port, s.ws_enabled ? "on" : "off",
         s.doc_root.toUtf8().constData());
}
