#include "lws_obs_helpers.hpp"
#include "config.hpp"
#define LOG_TAG "[" PLUGIN_NAME "][obs]"
#include "log.hpp"

#include <obs.h>
#ifdef ENABLE_FRONTEND_API
#include <obs-frontend-api.h>
#endif

#include <cstring>

#ifdef ENABLE_FRONTEND_API
static obs_scene_t *get_current_scene()
{
    obs_source_t *cur = obs_frontend_get_current_scene();
    if (!cur)
        return nullptr;

    obs_scene_t *scn = obs_scene_from_source(cur);
    obs_source_release(cur);
    return scn;
}
#endif

bool lws_create_or_update_browser_source_in_current_scene(const QString &name,
                                                          const QString &url,
                                                          int width,
                                                          int height)
{
#ifdef ENABLE_FRONTEND_API
    obs_scene_t *scn = get_current_scene();
    if (!scn) {
        LOGW("No current scene");
        return false;
    }

    obs_data_t *settings = obs_data_create();
    obs_data_set_bool(settings, "is_local_file", false);
    obs_data_set_string(settings, "url", url.toUtf8().constData());
    obs_data_set_int(settings, "width", width);
    obs_data_set_int(settings, "height", height);
    obs_data_set_bool(settings, "shutdown", false);

    struct Ctx { obs_data_t *settings; bool updated; };
    Ctx ctx{settings, false};

    obs_scene_enum_items(
        scn,
        [](obs_scene_t *, obs_sceneitem_t *item, void *param)->bool {
            auto *ctx = static_cast<Ctx*>(param);
            obs_source_t *src = obs_sceneitem_get_source(item);
            const char *n = obs_source_get_name(src);
            if (n && std::strcmp(n, ctx->settings ? obs_data_get_string(ctx->settings, "name") : "") == 0)
                return true;
            if (n && std::strcmp(n, obs_data_get_string(ctx->settings, "browser_name")) == 0)
                return true;
            return true;
        },
        &ctx);

    obs_source_t *existing = obs_get_source_by_name(name.toUtf8().constData());
    if (existing) {
        const char *id = obs_source_get_id(existing);
        if (id && std::strcmp(id, "browser_source") == 0) {
            obs_source_update(existing, settings);
            obs_source_release(existing);
            obs_data_release(settings);
            LOGI("Updated browser source '%s'", name.toUtf8().constData());
            return true;
        }
        obs_source_release(existing);
    }

    obs_source_t *br = obs_source_create("browser_source",
                                         name.toUtf8().constData(),
                                         settings, nullptr);
    obs_data_release(settings);

    if (!br) {
        LOGW("Failed to create browser_source (is Browser plugin installed?)");
        return false;
    }

    obs_sceneitem_t *item = obs_scene_add(scn, br);
    if (!item) {
        LOGW("Failed to add browser source to scene");
        obs_source_release(br);
        return false;
    }

    vec2 pos = {40.0f, 40.0f};
    obs_sceneitem_set_pos(item, &pos);

    obs_source_release(br);

    LOGI("Created browser source '%s' -> %s",
         name.toUtf8().constData(), url.toUtf8().constData());
    return true;
#else
    Q_UNUSED(name); Q_UNUSED(url); Q_UNUSED(width); Q_UNUSED(height);
    LOGW("Frontend API disabled; can't create browser source.");
    return false;
#endif
}

bool lws_refresh_browser_source_by_name(const QString &name)
{
#ifdef ENABLE_FRONTEND_API
    if (name.isEmpty())
        return false;

    obs_source_t *src = obs_get_source_by_name(name.toUtf8().constData());
    if (!src) {
        LOGW("Browser source '%s' not found", name.toUtf8().constData());
        return false;
    }

    const char *id = obs_source_get_id(src);
    if (!id || std::strcmp(id, "browser_source") != 0) {
        LOGW("Source '%s' is not a browser_source", name.toUtf8().constData());
        obs_source_release(src);
        return false;
    }

    obs_data_t *s = obs_source_get_settings(src);
    obs_source_update(src, s);
    obs_data_release(s);

    obs_source_release(src);

    LOGI("Refreshed browser source '%s'", name.toUtf8().constData());
    return true;
#else
    Q_UNUSED(name);
    LOGW("Frontend API disabled; can't refresh browser source.");
    return false;
#endif
}
