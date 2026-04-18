#include "lws_settings.hpp"
#include "lws_paths.hpp"
#include "config.hpp"
#define LOG_TAG "[" PLUGIN_NAME "][settings]"
#include "log.hpp"

#include <QSettings>

static inline QString org_name() { return QStringLiteral("MMLTech"); }
static inline QString app_name() { return QStringLiteral("local-webserver"); }
static inline QString key_port() { return QStringLiteral("server/port"); }

LwsSettings lws_settings_load()
{
    LwsSettings s;
    s.doc_root = lws_get_data_root_no_ui();
    if (s.doc_root.isEmpty())
        s.doc_root = lws_get_data_root();

    QSettings qs(org_name(), app_name());
    s.port = qs.value(key_port(), 8089).toInt();
    if (s.port < 1024 || s.port > 65535)
        s.port = 8089;

    return s;
}

void lws_settings_save(const LwsSettings &s)
{
    if (!s.doc_root.isEmpty())
        lws_set_data_root(s.doc_root);

    QSettings qs(org_name(), app_name());
    qs.setValue(key_port(), s.port);
    qs.sync();

    LOGI("Settings saved: port=%d doc_root=%s",
         s.port, s.doc_root.toUtf8().constData());
}
