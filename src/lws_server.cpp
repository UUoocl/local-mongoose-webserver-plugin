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
#include <unordered_map>
#include <algorithm>
#include <obs.h>
#include <util/platform.h>

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
// Unified Server State
// -----------------------------------------------------------------------------

static struct mg_mgr g_mgr;
static std::thread g_thread;
static std::atomic<bool> g_quit{false};
static std::atomic<int> g_http_port{0};
static std::atomic<int> g_ws_port{0};
static std::atomic<bool> g_http_failed{false};
static std::atomic<bool> g_ws_failed{false};
static std::string g_doc_root;

static std::mutex g_state_mutex;
static std::map<std::string, std::map<std::string, std::string>> g_topic_state_cache;

struct ConnContext {
	std::string topic;
	bool is_binary = false;
};

static std::mutex g_registry_mutex;
static std::unordered_map<std::string, std::vector<mg_connection *>> g_registry;

struct BroadcastPayload {
	std::string topic;
	std::string message;
	bool is_binary;
};
static std::mutex g_broadcast_mutex;
static std::vector<BroadcastPayload> g_broadcast_queue;

// Forward declarations
static void unified_fn(struct mg_connection *c, int ev, void *ev_data);
static void handle_http_request(struct mg_connection *c, struct mg_http_message *hm);
static void handle_ws_upgrade(struct mg_connection *c, struct mg_http_message *hm);

static void ensure_server_started()
{
    if (g_thread.joinable()) return;
    
    mg_mgr_init(&g_mgr);
    mg_wakeup_init(&g_mgr);
    g_quit = false;
    
    g_thread = std::thread([]() {
        LOGI("Unified Server: Thread started");
        while (!g_quit.load()) {
            mg_mgr_poll(&g_mgr, 100);
        }
        mg_mgr_free(&g_mgr);
        LOGI("Unified Server: Thread stopped");
    });
}

static void add_to_registry(const std::string &topic, struct mg_connection *c)
{
	std::lock_guard<std::mutex> lock(g_registry_mutex);
	g_registry[topic].push_back(c);
	LOGD("Registry: Added connection %p to topic '%s' (Total: %zu)", (void *)c, topic.c_str(), g_registry[topic].size());
}

static void remove_from_registry(struct mg_connection *c)
{
	if (!*(void **)c->data)
		return;
	ConnContext *ctx = *(ConnContext **)c->data;
	std::lock_guard<std::mutex> lock(g_registry_mutex);
	if (g_registry.count(ctx->topic)) {
		auto &vec = g_registry[ctx->topic];
		vec.erase(std::remove(vec.begin(), vec.end(), c), vec.end());
		LOGD("Registry: Removed connection %p from topic '%s' (Remaining: %zu)", (void *)c, ctx->topic.c_str(), vec.size());
		if (vec.empty())
			g_registry.erase(ctx->topic);
	}
}

// -----------------------------------------------------------------------------
// HTTP Logic
// -----------------------------------------------------------------------------

int lws_http_server_start(const QString &doc_root, int preferred_port)
{
	if (g_http_port.load() != 0)
		return g_http_port.load();

    g_doc_root = doc_root.toStdString();
    ensure_server_started();

	int port = 0;
	if (g_ws_port.load() == preferred_port) {
		port = preferred_port;
	} else {
		for (int p = preferred_port; p < preferred_port + 10; ++p) {
			char url[64];
			snprintf(url, sizeof(url), "http://127.0.0.1:%d", p);
			if (mg_http_listen(&g_mgr, url, unified_fn, NULL)) {
				port = p;
				break;
			}
		}
	}

	if (!port) {
		LOGW("HTTP: Failed to bind near port %d", preferred_port);
		g_http_failed = true;
		return 0;
	}

	g_http_port = port;
	g_http_failed = false;
    LOGI("HTTP: Listening on port %d", port);
	return port;
}

void lws_http_server_stop() { g_http_port = 0; }
int lws_http_server_port() { return g_http_port.load(); }
bool lws_http_server_is_running() { return g_http_port.load() != 0; }
bool lws_http_server_failed() { return g_http_failed.load(); }

void lws_server_shutdown()
{
    g_quit = true;
    mg_wakeup(&g_mgr, 0, NULL, 0); // Interrupt poll
    if (g_thread.joinable()) {
        g_thread.join();
    }
}

static void handle_http_request(struct mg_connection *c, struct mg_http_message *hm)
{
	if (mg_match(hm->uri, mg_str("/__lws/health"), NULL)) {
		mg_http_reply(c, 200, "Content-Type: text/plain\r\nAccess-Control-Allow-Origin: *\r\n", "ok");
	} else if (mg_match(hm->uri, mg_str("/callback"), NULL)) {
		if (hm->body.len > 0) {
			std::string body(hm->body.buf, hm->body.len);
			char topic_buf[64] = {0};
			mg_http_get_var(&hm->query, "topic", topic_buf, sizeof(topic_buf));
			std::string topic = (strlen(topic_buf) > 0) ? topic_buf : "default";
			
			LOGI("Bridge: Callback received for topic '%s' (size: %zu)", topic.c_str(), body.size());
			lws_ws_server_broadcast_topic(topic.c_str(), body.c_str(), false);
		}
		mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n", "{\"status\":\"ok\"}");
	} else {
		struct mg_http_serve_opts opts = {};
		opts.root_dir = g_doc_root.c_str();
        
        auto ends_with = [](struct mg_str s, const char *ext) {
            size_t ext_len = strlen(ext);
            if (s.len < ext_len) return false;
            return memcmp(s.buf + s.len - ext_len, ext, ext_len) == 0;
        };

        std::string headers = "Access-Control-Allow-Origin: *\r\n";
        if (ends_with(hm->uri, ".js") || ends_with(hm->uri, ".json") || ends_with(hm->uri, ".wasm")) {
            headers += "Cache-Control: no-store, must-revalidate\r\n";
        } else {
            headers += "Cache-Control: no-cache\r\n";
        }
        
        opts.extra_headers = headers.c_str();
        opts.mime_types = "js=text/javascript,wasm=application/wasm,task=application/octet-stream";
		mg_http_serve_dir(c, hm, &opts);
	}
}

// -----------------------------------------------------------------------------
// WebSocket Logic
// -----------------------------------------------------------------------------

int lws_ws_server_start(int preferred_port)
{
	if (g_ws_port.load() != 0)
		return g_ws_port.load();

	ensure_server_started();

	int port = 0;
	if (g_http_port.load() == preferred_port) {
		port = preferred_port;
	} else {
		for (int p = preferred_port; p < preferred_port + 10; ++p) {
			char url[64];
			snprintf(url, sizeof(url), "http://127.0.0.1:%d", p);
			if (mg_http_listen(&g_mgr, url, unified_fn, NULL)) {
				port = p;
				break;
			}
		}
	}

	if (!port) {
		LOGW("WS: Failed to bind near port %d", preferred_port);
		g_ws_failed = true;
		return 0;
	}

	g_ws_port = port;
	g_ws_failed = false;
    LOGI("WS: Listening on port %d", port);
	return port;
}

void lws_ws_server_stop() { g_ws_port = 0; }
int lws_ws_server_port() { return g_ws_port.load(); }
bool lws_ws_server_is_running() { return g_ws_port.load() != 0; }
bool lws_ws_server_failed() { return g_ws_failed.load(); }

static void handle_ws_upgrade(struct mg_connection *c, struct mg_http_message *hm)
{
	struct mg_str *origin = mg_http_get_header(hm, "Origin");
	bool is_valid = (!origin || mg_match(*origin, mg_str("http://127.0.0.1:*"), NULL) || mg_match(*origin, mg_str("null"), NULL));

	if (!is_valid) {
		mg_http_reply(c, 403, "", "Security: Unauthorized Origin\n");
		return;
	}
	
	std::string uri(hm->uri.buf, hm->uri.len);
	std::string topic = "default";
	if (uri.find("/ws/") == 0) topic = uri.substr(4);
	else if (uri == "/ws") topic = "default";

	ConnContext *ctx = new ConnContext();
	ctx->topic = topic;
	ctx->is_binary = false;

	*(void **)c->data = ctx;
	mg_ws_upgrade(c, hm, NULL);
	add_to_registry(topic, c);
	LOGD("Bridge: Client joined topic '%s'", topic.c_str());
}

// -----------------------------------------------------------------------------
// Unified Handler
// -----------------------------------------------------------------------------

static void unified_fn(struct mg_connection *c, int ev, void *ev_data)
{
	if (ev == MG_EV_ACCEPT) {
		set_tcp_nodelay(c);
		memset(c->data, 0, sizeof(c->data));
	} else if (ev == MG_EV_HTTP_MSG) {
		struct mg_http_message *hm = (struct mg_http_message *)ev_data;
		if (mg_match(hm->uri, mg_str("/ws/#"), NULL) || mg_match(hm->uri, mg_str("/ws"), NULL)) {
			handle_ws_upgrade(c, hm);
		} else {
			handle_http_request(c, hm);
		}
	} else if (ev == MG_EV_WS_OPEN) {
		if (*(void **)c->data) {
			ConnContext *ctx = *(ConnContext **)c->data;
			std::lock_guard<std::mutex> lock(g_state_mutex);
			if (g_topic_state_cache.count(ctx->topic)) {
				for (auto const &[addr, json] : g_topic_state_cache[ctx->topic]) {
					mg_ws_send(c, json.c_str(), json.length(), ctx->is_binary ? WEBSOCKET_OP_BINARY : WEBSOCKET_OP_TEXT);
				}
			}
		}
	} else if (ev == MG_EV_WS_MSG) {
		struct mg_ws_message *wm = (struct mg_ws_message *)ev_data;
        if (wm->data.len > 0 && strstr((const char *)wm->data.buf, "get_obs_credentials")) {
            lws_emit_credentials_to_tagged_sources();
        } else {
            std::string msg((const char*)wm->data.buf, wm->data.len);
            lws_ws_server_broadcast(msg.c_str());
            
            // Also relay to OBS
            signal_handler_t *sh = obs_get_signal_handler();
            if (sh) {
                calldata_t cd = {0};
                calldata_set_string(&cd, "json_str", msg.c_str());
                signal_handler_signal(sh, "media_warp_receive", &cd);
                calldata_free(&cd);
            }
        }
	} else if (ev == MG_EV_CLOSE) {
		remove_from_registry(c);
		if (*(void **)c->data) {
			delete *(ConnContext **)c->data;
			*(void **)c->data = NULL;
		}
	} else if (ev == MG_EV_WAKEUP || ev == MG_EV_POLL) {
		std::vector<BroadcastPayload> queue;
		{
			std::lock_guard<std::mutex> lock(g_broadcast_mutex);
			if (g_broadcast_queue.empty()) return;
			queue.swap(g_broadcast_queue);
		}
		
		if (ev == MG_EV_WAKEUP) {
			LOGD("Event Loop Woken Up (MG_EV_WAKEUP)");
		}

		for (const auto &payload : queue) {
			std::lock_guard<std::mutex> lock(g_registry_mutex);
			if (g_registry.count(payload.topic)) {
				auto &conns = g_registry[payload.topic];
				for (auto *conn : conns) {
					if (payload.is_binary && conn->send.len > 1024 * 1024) continue;
					mg_ws_send(conn, payload.message.data(), payload.message.size(), payload.is_binary ? WEBSOCKET_OP_BINARY : WEBSOCKET_OP_TEXT);
				}
			}
		}
	}
}

// -----------------------------------------------------------------------------
// Public Broadcast API
// -----------------------------------------------------------------------------

void lws_ws_server_broadcast(const char *message)
{
	lws_ws_server_broadcast_topic("default", message, false);
}

void lws_ws_server_broadcast_topic(const char *topic, const char *message, bool is_binary)
{
	lws_ws_server_broadcast_topic_raw(topic, message, message ? strlen(message) : 0, is_binary);
}

void lws_ws_server_broadcast_topic_raw(const char *topic, const void *data, size_t len, bool is_binary)
{
	if (g_ws_port.load() == 0) return;
	{
		std::lock_guard<std::mutex> lock(g_broadcast_mutex);
		g_broadcast_queue.push_back({topic ? topic : "default", std::string((const char *)data, len), is_binary});
	}
	mg_wakeup(&g_mgr, 0, NULL, 0);
}

void lws_ws_server_broadcast_topic_state(const char *topic, const char *addr, const char *json_str, bool is_binary)
{
	if (!topic || !addr || !json_str) return;
	{
		std::lock_guard<std::mutex> lock(g_state_mutex);
		g_topic_state_cache[topic][addr] = json_str;
	}
	lws_ws_server_broadcast_topic(topic, json_str, is_binary);
}

void lws_ws_server_broadcast_state(const char *addr, const char *json_str)
{
	if (!addr || !json_str) return;
	{
		std::lock_guard<std::mutex> lock(g_state_mutex);
		g_topic_state_cache["default"][addr] = json_str;
	}
	lws_ws_server_broadcast_topic("default", json_str, false);
}

QString lws_server_doc_root() { return QString::fromStdString(g_doc_root); }
