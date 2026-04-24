#include "config.hpp"
#define LOG_TAG "[" PLUGIN_NAME "][plugin]"
#include "log.hpp"

#include <obs-module.h>
#include <obs.h>

#ifdef ENABLE_FRONTEND_API
#include <obs-frontend-api.h>
#endif

#include <QAction>
#include <QPointer>
#include <QObject>
#include <QWidget>

#include "lws_settings.hpp"
#include "lws_server.hpp"
#include "lws_dialog.hpp"
#include "lws_obs_helpers.hpp"

OBS_DECLARE_MODULE();
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static QPointer<LwsDialog> g_dialog;
static QAction *g_menu_action = nullptr;

static std::string get_topic_for_packet(obs_data_t *packet)
{
	const char *t = obs_data_get_string(packet, "t");
	const char *a = obs_data_get_string(packet, "a");

	if (t && *t)
		return t;

	return a ? a : "default";
}

// Signal handling for media_warp

static void on_media_warp_transmit(void *data, calldata_t *cd)
{
	(void)data;
	obs_data_t *packet = (obs_data_t*)calldata_ptr(cd, "packet");
	if (!packet) return;

	const char *json_str = obs_data_get_json(packet);
	const char *addr = obs_data_get_string(packet, "a");
	std::string topic = get_topic_for_packet(packet);

	if (json_str && addr) {
		LOGD("Signal Received: media_warp_transmit topic=%s addr=%s", topic.c_str(), addr);
		lws_ws_server_broadcast_topic_state(topic.c_str(), addr, json_str,
						    topic.find("frame/") == 0);
	}
}

static void on_media_warp_transmit_binary(void *data, calldata_t *cd)
{
	(void)data;
	void *buf = calldata_ptr(cd, "data");
	long long size = calldata_int(cd, "size");
	const char *topic_label = calldata_string(cd, "topic");

	if (buf && size > 0 && topic_label) {
		lws_ws_server_broadcast_topic_raw(topic_label, buf, (size_t)size, true);
	}
}

static void on_media_warp_transmit_topic(void *data, calldata_t *cd)
{
	(void)data;
	const char *packet = calldata_string(cd, "data");
	const char *topic_label = calldata_string(cd, "topic");

	if (packet && topic_label) {
		lws_ws_server_broadcast_topic(topic_label, packet);
	}
}

static void on_media_warp_receive(void *data, calldata_t *cd)
{
	(void)data;
	const char *json_str = calldata_string(cd, "json_str");
	if (json_str) {
		// Broadcast to all "default" or interested routes?
		// Usually receive is from a generic client, we broadcast to "default" 
		// so everyone on the generic route gets it.
		lws_ws_server_broadcast_topic("default", json_str, false);
	}
}

MODULE_EXPORT const char *obs_module_name(void) { return PLUGIN_NAME; }
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Local Webserver and WebSocket Bridge plugin for OBS Studio.";
}

#ifdef ENABLE_FRONTEND_API
static void open_dialog()
{
	if (g_dialog) {
		g_dialog->raise();
		g_dialog->activateWindow();
		return;
	}

	void *mw = obs_frontend_get_main_window();
	auto *parent = reinterpret_cast<QWidget *>(mw);

	g_dialog = new LwsDialog(parent);
	g_dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	g_dialog->show();

	QObject::connect(g_dialog, &QObject::destroyed, []() {
		g_dialog = nullptr;
	});
}
#endif

bool obs_module_load(void)
{
	LOGI("Loaded v%s", PLUGIN_VERSION);

	LwsSettings s = lws_settings_load();
	
	// Start HTTP Server
	if (s.http_enabled) {
		int bound = lws_http_server_start(s.doc_root, s.http_port);
		if (!bound) LOGW("HTTP Server failed to start");
		else LOGI("HTTP Server listening on :%d", bound);
	}

	// Start WS Server
	if (s.ws_enabled) {
		int bound = lws_ws_server_start(s.ws_port);
		if (!bound) LOGW("WS Server failed to start");
		else LOGI("WS Server listening on :%d", bound);
	}

	// Register Signals
	signal_handler_t *sh = obs_get_signal_handler();
	if (sh) {
		signal_handler_add(sh, "void media_warp_transmit(ptr packet)");
		signal_handler_add(sh, "void media_warp_transmit_topic(string data, string topic)");
		signal_handler_add(sh, "void media_warp_transmit_binary(ptr data, int size, string topic)");
		signal_handler_add(sh, "void media_warp_auth(int port, string password)");
		signal_handler_add(sh, "void media_warp_receive(string json_str)");
		
		signal_handler_connect(sh, "media_warp_transmit", on_media_warp_transmit, nullptr);
		signal_handler_connect(sh, "media_warp_transmit_topic", on_media_warp_transmit_topic, nullptr);
		signal_handler_connect(sh, "media_warp_transmit_binary", on_media_warp_transmit_binary, nullptr);
		signal_handler_connect(sh, "media_warp_receive", on_media_warp_receive, nullptr);
	}

	// Bootstrap Auth for existing sources
	lws_emit_credentials_to_tagged_sources();

#ifdef ENABLE_FRONTEND_API
	void *act_void = obs_frontend_add_tools_menu_qaction("Web Server Settings");
	g_menu_action = reinterpret_cast<QAction *>(act_void);

	if (g_menu_action) {
		QObject::connect(g_menu_action, &QAction::triggered, []() {
			open_dialog();
		});
	}
#endif

	return true;
}

void obs_module_unload(void)
{
	LOGI("Unloading...");

	signal_handler_t *sh = obs_get_signal_handler();
	signal_handler_disconnect(sh, "media_warp_transmit", on_media_warp_transmit, nullptr);
	signal_handler_disconnect(sh, "media_warp_transmit_binary", on_media_warp_transmit_binary, nullptr);
	signal_handler_disconnect(sh, "media_warp_receive", on_media_warp_receive, nullptr);

	lws_http_server_stop();
	lws_ws_server_stop();
	lws_server_shutdown();

#ifdef ENABLE_FRONTEND_API
	g_menu_action = nullptr;
#endif
}
