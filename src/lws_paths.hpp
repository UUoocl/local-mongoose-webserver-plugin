#pragma once
#include <QString>

class QWidget;

QString lws_default_data_root();
QString lws_get_data_root_no_ui();
void    lws_set_data_root(const QString &path);
QString lws_get_data_root(QWidget *parentForDialogs = nullptr);
