#include "lws_paths.hpp"
#include "config.hpp"
#define LOG_TAG "[" PLUGIN_NAME "][paths]"
#include "log.hpp"

#include <QDir>
#include <QStandardPaths>
#include <QSettings>

static inline QString org_name() { return QStringLiteral("MMLTech"); }
static inline QString app_name() { return QStringLiteral("local-mongoose-webserver"); }
static inline QString key_data() { return QStringLiteral("paths/data_root"); }

QString lws_default_data_root()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty())
        base = QDir::home().filePath(QStringLiteral(".local-mongoose-webserver"));
    else
        base = QDir(base).filePath(QStringLiteral("local-mongoose-webserver"));

    QDir d(base);
    if (!d.exists())
        d.mkpath(QStringLiteral("."));
    return d.absolutePath();
}

QString lws_get_data_root_no_ui()
{
    QSettings s(org_name(), app_name());
    QString p = s.value(key_data()).toString();
    if (p.isEmpty())
        return QString();

    QDir d(p);
    if (!d.exists())
        d.mkpath(QStringLiteral("."));
    return d.absolutePath();
}

void lws_set_data_root(const QString &path)
{
    if (path.isEmpty())
        return;

    QDir d(path);
    if (!d.exists())
        d.mkpath(QStringLiteral("."));

    const QString abs = d.absolutePath();
    QSettings s(org_name(), app_name());
    s.setValue(key_data(), abs);
    s.sync();

    LOGI("Data root set to: %s", abs.toUtf8().constData());
}

QString lws_get_data_root(QWidget *)
{
    QString existing = lws_get_data_root_no_ui();
    if (!existing.isEmpty())
        return existing;

    const QString def = lws_default_data_root();
    lws_set_data_root(def);
    return lws_get_data_root_no_ui();
}
