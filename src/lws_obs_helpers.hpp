#pragma once
#include <QString>

struct ObsWsAuth {
    int port;
    QString password;
};

ObsWsAuth lws_get_obs_ws_credentials();

bool lws_create_or_update_browser_source_in_current_scene(const QString &name,
                                                          const QString &url,
                                                          int width = 1920,
                                                          int height = 1080);

bool lws_refresh_browser_source_by_name(const QString &name);
void lws_refresh_all_tagged_browser_sources(int httpPort, int wsPort);
void lws_emit_credentials_to_tagged_sources();
