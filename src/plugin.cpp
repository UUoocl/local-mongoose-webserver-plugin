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

OBS_DECLARE_MODULE();
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static QPointer<LwsDialog> g_dialog;
static QAction *g_menu_action = nullptr;

MODULE_EXPORT const char *obs_module_name(void) { return PLUGIN_NAME; }
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Local Webserver plugin for OBS Studio.";
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
	int bound = lws_server_start(s.doc_root, s.port);

	if (!bound)
		LOGW("Server didn't bind on load");
	else
		LOGI("Server listening on :%d (docRoot=%s)",
		     bound, s.doc_root.toUtf8().constData());

#ifdef ENABLE_FRONTEND_API
	void *act_void = obs_frontend_add_tools_menu_qaction("Web Server Settings");
	g_menu_action = reinterpret_cast<QAction *>(act_void);

	if (g_menu_action) {
		QObject::connect(g_menu_action, &QAction::triggered, []() {
			open_dialog();
		});
	} else {
		LOGW("Failed to add Tools menu action");
	}
#endif

	return true;
}

void obs_module_unload(void)
{
	LOGI("Unloading...");

#ifdef ENABLE_FRONTEND_API
	if (g_menu_action) {
		delete g_menu_action;
		g_menu_action = nullptr;
	}
#endif

	lws_server_stop();
}
