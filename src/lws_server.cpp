#include "lws_server.hpp"
#include "config.hpp"
#define LOG_TAG "[" PLUGIN_NAME "][server]"
#include "log.hpp"
#include "lws_obs_helpers.hpp"

#include <thread>
#include <atomic>
#include <string>
#include <cstring>
#include <vector>
#include <map>
#include <mutex>
#include <obs.h>

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

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif

// -----------------------------------------------------------------------------
// Common Helpers
// -----------------------------------------------------------------------------

static void set_tcp_nodelay(struct mg_connection *c)
{
	int one = 1;
#ifdef _WIN32
	setsockopt((SOCKET)(intptr_t)c->fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
#else
	setsockopt((int)(intptr_t)c->fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#endif
}

// -----------------------------------------------------------------------------
// HTTP Server
// -----------------------------------------------------------------------------

static struct mg_mgr g_http_mgr;
static std::thread g_http_thread;
static std::atomic<int> g_http_port{0};
static std::atomic<bool> g_http_quit{false};
static std::atomic<bool> g_http_failed{false};
static std::string g_doc_root;

static void http_fn(struct mg_connection *c, int ev, void *ev_data)
{
	if (ev == MG_EV_HTTP_MSG) {
		struct mg_http_message *hm = (struct mg_http_message *)ev_data;
		if (mg_match(hm->uri, mg_str("/__lws/health"), NULL)) {
			mg_http_reply(c, 200, "Content-Type: text/plain\r\nAccess-Control-Allow-Origin: *\r\n", "ok");
		} else if (mg_match(hm->uri, mg_str("/__lws/ws_port"), NULL)) {
			int ws_port = lws_ws_server_port();
			char buf[32];
			snprintf(buf, sizeof(buf), "%d", ws_port);
			mg_http_reply(c, 200, "Content-Type: text/plain\r\nAccess-Control-Allow-Origin: *\r\n", buf);
		} else if (mg_match(hm->uri, mg_str("/callback"), NULL)) {
            // Received a shortcut callback!
            if (hm->body.len > 0) {
                std::string body(hm->body.buf, hm->body.len);
                LOGI("HTTP: Received shortcut callback: %s", body.c_str());
                
                // Broadcast to all connected WebSocket clients
                lws_ws_server_broadcast(body.c_str());
            }
            mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "{\"status\":\"ok\"}");
		} else {
			struct mg_http_serve_opts opts = {};
			opts.root_dir = g_doc_root.c_str();

			auto ends_with = [](struct mg_str s, const char *ext) {
				size_t ext_len = strlen(ext);
				if (s.len < ext_len)
					return false;
				return memcmp(s.buf + s.len - ext_len, ext, ext_len) == 0;
			};

			std::string headers = "Access-Control-Allow-Origin: *\r\n";
			if (ends_with(hm->uri, ".js") || ends_with(hm->uri, ".json") || ends_with(hm->uri, ".wasm")) {
				headers += "Cache-Control: no-store, must-revalidate\r\n";
			} else {
				headers += "Cache-Control: no-cache\r\n";
			}
            
            // Explicitly handle WASM and TASK files to fix MIME type issues
            std::string uri_str(hm->uri.buf, hm->uri.len);
            LOGI("HTTP Request: %s", uri_str.c_str());

            bool is_wasm = (uri_str.find(".wasm") != std::string::npos);
            bool is_task = (uri_str.find(".task") != std::string::npos);

            if (is_wasm || is_task) {
                std::string full_path = g_doc_root + uri_str;
                struct mg_str file_data = mg_file_read(&mg_fs_posix, full_path.c_str());
                if (file_data.buf != NULL) {
                    const char *mime = is_wasm ? "application/wasm" : "application/octet-stream";
                    LOGI("Serving binary file with override: %s as %s (%zu bytes)", uri_str.c_str(), mime, file_data.len);
                    mg_printf(c, "HTTP/1.1 200 OK\r\n"
                                 "Content-Type: %s\r\n"
                                 "Content-Length: %zu\r\n"
                                 "Access-Control-Allow-Origin: *\r\n"
                                 "Cache-Control: no-cache\r\n"
                                 "Connection: close\r\n\r\n", mime, file_data.len);
                    mg_send(c, file_data.buf, file_data.len);
                    free((void*)file_data.buf);
                    return; // EXIT EARLY
                } else {
                    LOGW("Binary file NOT found at: %s", full_path.c_str());
                }
            }
            
            opts.extra_headers = headers.c_str();
            opts.mime_types = "js=text/javascript,wasm=application/wasm,task=application/octet-stream";
            mg_http_serve_dir(c, hm, &opts);
		}
	}
}

int lws_http_server_start(const QString &doc_root_q, int preferred_port)
{
	if (g_http_port.load() != 0)
		return g_http_port.load();

	g_doc_root = QFileInfo(doc_root_q).absoluteFilePath().toStdString();
	if (!QDir(doc_root_q).exists()) {
		LOGW("HTTP: Document root does not exist: %s", g_doc_root.c_str());
		g_http_failed = true;
		return 0;
	}

	mg_mgr_init(&g_http_mgr);
	mg_wakeup_init(&g_http_mgr);

	int port = 0;
	struct mg_connection *lsn = nullptr;
	for (int p = preferred_port; p < preferred_port + 10; ++p) {
		char url[64];
		snprintf(url, sizeof(url), "http://127.0.0.1:%d", p);
		lsn = mg_http_listen(&g_http_mgr, url, http_fn, NULL);
		if (lsn != NULL) {
			port = p;
			break;
		}
	}

	if (!port) {
		LOGW("HTTP: Failed to bind near port %d", preferred_port);
		mg_mgr_free(&g_http_mgr);
		g_http_failed = true;
		return 0;
	}

	g_http_port = port;
	g_http_quit = false;
	g_http_failed = false;

	g_http_thread = std::thread([]() {
		LOGI("HTTP: Started on http://127.0.0.1:%d", g_http_port.load());
		while (!g_http_quit.load()) {
			mg_mgr_poll(&g_http_mgr, 100);
		}
		mg_mgr_free(&g_http_mgr);
		LOGI("HTTP: Stopped");
	});

	return port;
}

void lws_http_server_stop()
{
	if (g_http_port.load() == 0)
		return;
	g_http_quit = true;
	if (g_http_thread.joinable())
		g_http_thread.join();
	g_http_port = 0;
}

int lws_http_server_port() { return g_http_port.load(); }
bool lws_http_server_is_running() { return g_http_port.load() != 0; }
bool lws_http_server_failed() { return g_http_failed.load(); }

// -----------------------------------------------------------------------------
// WebSocket Server
// -----------------------------------------------------------------------------

static struct mg_mgr g_ws_mgr;
static std::thread g_ws_thread;
static std::atomic<int> g_ws_port{0};
static std::atomic<bool> g_ws_quit{false};
static std::atomic<bool> g_ws_failed{false};
static std::map<std::string, std::string> g_state_cache;
static std::mutex g_state_mutex;

static void ws_fn(struct mg_connection *c, int ev, void *ev_data)
{
    // LOGD("WS Event: %d on conn %lu", ev, c->id); // Too noisy, but good to know
	if (ev == MG_EV_ACCEPT) {
		set_tcp_nodelay(c);
	} else if (ev == MG_EV_WS_OPEN) {
		// Send snapshot to new client
		std::lock_guard<std::mutex> lock(g_state_mutex);
		for (auto const& [addr, json] : g_state_cache) {
			mg_ws_send(c, json.c_str(), json.length(), WEBSOCKET_OP_TEXT);
		}
		LOGI("WS: Sent state snapshot (%zu items) to new client", g_state_cache.size());

	} else if (ev == MG_EV_HTTP_MSG) {
		struct mg_http_message *hm = (struct mg_http_message *)ev_data;

		// 1. Validate Origin
		struct mg_str *origin = mg_http_get_header(hm, "Origin");
		bool is_valid = false;

		if (!origin) {
			is_valid = true; // Allow clients that don't send Origin (like some CLI tools)
		} else {
			if (mg_match(*origin, mg_str("http://127.0.0.1:*"), NULL) ||
			    mg_match(*origin, mg_str("http://localhost:*"), NULL) ||
			    mg_match(*origin, mg_str("null"), NULL)) {
				is_valid = true;
			}
		}

		if (!is_valid) {
			LOGW("WS: Rejected connection. Origin header: %.*s", (int)(origin ? origin->len : 4), origin ? origin->buf : "MISSING");
			mg_http_reply(c, 403, "", "Security: Unauthorized Origin\n");
			return;
		}
		
		if (origin) {
			LOGI("WS: Accepted connection from origin: %.*s", (int)origin->len, origin->buf);
		} else {
			LOGI("WS: Accepted connection (no Origin header)");
		}

		// 2. Upgrade to WS if URI matches
		if (mg_match(hm->uri, mg_str("/ws"), NULL)) {
			mg_ws_upgrade(c, hm, NULL);
		} else {
			mg_http_reply(c, 404, "", "Not Found\n");
		}

	} else if (ev == MG_EV_WS_MSG) {
		struct mg_ws_message *wm = (struct mg_ws_message *)ev_data;

		// Command: "get_obs_credentials"
		if (wm->data.len > 0 && strstr((const char *)wm->data.buf, "get_obs_credentials")) {
			LOGI("WS: Received credential request. Emitting via OBS API...");
			lws_emit_credentials_to_tagged_sources();
		} else if (wm->data.len > 0) {
			// 1. Relay to OBS signals
			signal_handler_t *sh = obs_get_signal_handler();
			if (sh) {
				calldata_t cd = {0};
				calldata_set_string(&cd, "json_str", (const char *)wm->data.buf);
				signal_handler_signal(sh, "media_warp_receive", &cd);
				calldata_free(&cd);
			}

            // 2. Broadcast to all OTHER connected clients
            std::string msg((const char*)wm->data.buf, wm->data.len);
            for (struct mg_connection *conn = c->mgr->conns; conn != NULL; conn = conn->next) {
                if (conn->is_websocket && conn != c) {
                    mg_ws_send(conn, msg.c_str(), msg.length(), WEBSOCKET_OP_TEXT);
                }
            }
		}
	} else if (ev == MG_EV_WAKEUP) {
		struct mg_str *s = (struct mg_str *)ev_data;
		if (c && c->mgr) {
			for (struct mg_connection *conn = c->mgr->conns; conn != NULL; conn = conn->next) {
				if (conn->is_websocket) {
					mg_ws_send(conn, s->buf, s->len, WEBSOCKET_OP_TEXT);
				}
			}
		}
	}
}

static std::atomic<unsigned long> g_ws_listener_id{0};

int lws_ws_server_start(int preferred_port)
{
	if (g_ws_port.load() != 0)
		return g_ws_port.load();

	mg_mgr_init(&g_ws_mgr);
	mg_wakeup_init(&g_ws_mgr);

	int port = 0;
	struct mg_connection *lsn = nullptr;
	for (int p = preferred_port; p < preferred_port + 10; ++p) {
		char url[64];
		snprintf(url, sizeof(url), "http://127.0.0.1:%d", p);
		lsn = mg_http_listen(&g_ws_mgr, url, ws_fn, NULL);
		if (lsn != NULL) {
			port = p;
			g_ws_listener_id = lsn->id;
			break;
		}
	}

	if (!port) {
		LOGW("WS: Failed to bind near port %d", preferred_port);
		mg_mgr_free(&g_ws_mgr);
		g_ws_failed = true;
		return 0;
	}

	g_ws_port = port;
	g_ws_quit = false;
	g_ws_failed = false;

	g_ws_thread = std::thread([]() {
		LOGI("WS: Started on ws://127.0.0.1:%d/ws", g_ws_port.load());
		while (!g_ws_quit.load()) {
			mg_mgr_poll(&g_ws_mgr, 100);
		}
		mg_mgr_free(&g_ws_mgr);
		LOGI("WS: Stopped");
	});

	return port;
}

void lws_ws_server_stop()
{
	if (g_ws_port.load() == 0)
		return;
	g_ws_quit = true;
	if (g_ws_thread.joinable())
		g_ws_thread.join();
	g_ws_port = 0;
	g_ws_listener_id = 0;
}

int lws_ws_server_port() { return g_ws_port.load(); }
bool lws_ws_server_is_running() { return g_ws_port.load() != 0; }
bool lws_ws_server_failed() { return g_ws_failed.load(); }

void lws_ws_server_broadcast(const char *message)
{
	if (g_ws_port.load() == 0 || g_ws_listener_id.load() == 0)
		return;
	mg_wakeup(&g_ws_mgr, g_ws_listener_id.load(), message, strlen(message));
}

void lws_ws_server_broadcast_state(const char *addr, const char *json_str)
{
	if (!addr || !json_str) return;

	// Update Snapshot Cache
	{
		std::lock_guard<std::mutex> lock(g_state_mutex);
		g_state_cache[addr] = json_str;
	}

	// Broadcast to network
	lws_ws_server_broadcast(json_str);
}

// -----------------------------------------------------------------------------
// Shared
// -----------------------------------------------------------------------------

QString lws_server_doc_root()
{
	return QString::fromStdString(g_doc_root);
}
