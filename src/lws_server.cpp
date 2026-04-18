#include "lws_server.hpp"
#include "config.hpp"
#define LOG_TAG "[" PLUGIN_NAME "][server]"
#include "log.hpp"

#include <thread>
#include <atomic>
#include <string>
#include <cstring>

#include <QString>
#include <QFileInfo>
#include <QDir>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomma"
#endif
#include "mongoose.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif

static struct mg_mgr g_mgr;
static std::thread g_thread;
static std::atomic<int> g_port{0};
static std::atomic<bool> g_quit{false};
static std::string g_doc_root;

static void fn(struct mg_connection *c, int ev, void *ev_data)
{
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        if (mg_match(hm->uri, mg_str("/__lws/health"), NULL)) {
            mg_http_reply(c, 200, "Content-Type: text/plain\r\n", "ok");
        } else {
            struct mg_http_serve_opts opts = {};
            opts.root_dir = g_doc_root.c_str();

            auto ends_with = [](struct mg_str s, const char *ext) {
                size_t ext_len = strlen(ext);
                if (s.len < ext_len) return false;
                return memcmp(s.buf + s.len - ext_len, ext, ext_len) == 0;
            };

            if (ends_with(hm->uri, ".js") || ends_with(hm->uri, ".json")) {
                opts.extra_headers = "Cache-Control: no-store, must-revalidate\r\n";
            } else {
                opts.extra_headers = "Cache-Control: no-cache\r\n";
            }
            mg_http_serve_dir(c, hm, &opts);
        }
    }
}

int lws_server_start(const QString &doc_root_q, int preferred_port)
{
    if (g_port.load() != 0)
        return g_port.load();

    g_doc_root = QFileInfo(doc_root_q).absoluteFilePath().toStdString();
    LOGI("docRoot: %s", g_doc_root.c_str());

    if (!QDir(doc_root_q).exists()) {
        LOGW("Document root does not exist: %s", g_doc_root.c_str());
        return 0;
    }

    mg_mgr_init(&g_mgr);

    int port = 0;
    for (int p = preferred_port; p < preferred_port + 10; ++p) {
        char url[64];
        snprintf(url, sizeof(url), "http://127.0.0.1:%d", p);
        if (mg_http_listen(&g_mgr, url, fn, NULL) != NULL) {
            port = p;
            break;
        }
    }

    if (!port) {
        LOGW("Failed to bind near port %d", preferred_port);
        mg_mgr_free(&g_mgr);
        return 0;
    }

    g_port = port;
    g_quit = false;

    g_thread = std::thread([]() {
        LOGI("START http://127.0.0.1:%d (docRoot=%s)", g_port.load(), g_doc_root.c_str());
        while (!g_quit.load()) {
            mg_mgr_poll(&g_mgr, 100);
        }
        mg_mgr_free(&g_mgr);
        LOGI("LOOP ended (port %d)", g_port.load());
    });

    return port;
}

void lws_server_stop()
{
    if (g_port.load() == 0)
        return;

    LOGI("STOP requested (port %d)", g_port.load());
    g_quit = true;
    if (g_thread.joinable())
        g_thread.join();
    g_port = 0;
}

int lws_server_port() { return g_port.load(); }
bool lws_server_is_running() { return g_port.load() != 0; }

QString lws_server_doc_root()
{
    return QString::fromStdString(g_doc_root);
}
