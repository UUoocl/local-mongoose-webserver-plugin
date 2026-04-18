#pragma once
#include <QString>

bool lws_create_or_update_browser_source_in_current_scene(const QString &name,
                                                          const QString &url,
                                                          int width = 1920,
                                                          int height = 1080);

bool lws_refresh_browser_source_by_name(const QString &name);
