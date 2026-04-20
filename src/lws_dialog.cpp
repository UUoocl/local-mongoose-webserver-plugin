#include "lws_dialog.hpp"
#include "config.hpp"
#define LOG_TAG "[" PLUGIN_NAME "][dialog]"
#include "log.hpp"

#include "lws_paths.hpp"
#include "lws_settings.hpp"
#include "lws_server.hpp"
#include "lws_obs_helpers.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFileDialog>
#include <QLineEdit>
#include <QGroupBox>

enum class ServerStatus { Stopped, Running, Failed };

static void styleDot(QLabel *dot, ServerStatus status)
{
	dot->setFixedSize(12, 12);
	QString color;
	if (status == ServerStatus::Running)
		color = "#39d353"; // Green
	else if (status == ServerStatus::Failed)
		color = "#ff6b6b"; // Red
	else
		color = "#8b949e"; // Grey

	dot->setStyleSheet(QString("border-radius:6px;"
				   "background-color:%1;"
				   "border:1px solid rgba(0,0,0,0.25);")
				   .arg(color));
}

static constexpr const char *kLwsBrowserSourceName = "Media Warp - Manager";

LwsDialog::LwsDialog(QWidget *parent) : QDialog(parent)
{
	setObjectName(QStringLiteral("LwsDialog"));
	setWindowTitle(QStringLiteral("Local Webserver Settings"));
	setModal(false);
	setSizeGripEnabled(false);
	setMinimumWidth(550);

	settings_ = lws_settings_load();

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(14, 14, 14, 14);
	root->setSpacing(12);

	// -----------------------------------------------------------------
	// HTTP Server Card
	// -----------------------------------------------------------------
	auto *gbHttp = new QGroupBox(QStringLiteral("HTTP Server (File Server)"), this);
	auto *httpLayout = new QVBoxLayout(gbHttp);

	auto *httpStatusRow = new QHBoxLayout();
	httpStatusDot_ = new QLabel(gbHttp);
	httpStatusText_ = new QLabel(gbHttp);
	httpStatusRow->addWidget(httpStatusDot_);
	httpStatusRow->addWidget(httpStatusText_);
	httpStatusRow->addStretch(1);
	httpLayout->addLayout(httpStatusRow);

	auto *httpCtrlRow = new QHBoxLayout();
	httpPortSpin_ = new QSpinBox(gbHttp);
	httpPortSpin_->setRange(1024, 65535);
	httpPortSpin_->setValue(settings_.http_port);
	httpPortSpin_->setFixedWidth(90);
	
	httpStartBtn_ = new QPushButton(QStringLiteral("Start"), gbHttp);
	httpStopBtn_ = new QPushButton(QStringLiteral("Stop"), gbHttp);
	httpStartBtn_->setCursor(Qt::PointingHandCursor);
	httpStopBtn_->setCursor(Qt::PointingHandCursor);

	httpCtrlRow->addWidget(new QLabel(QStringLiteral("Port:"), gbHttp));
	httpCtrlRow->addWidget(httpPortSpin_);
	httpCtrlRow->addStretch(1);
	httpCtrlRow->addWidget(httpStartBtn_);
	httpCtrlRow->addWidget(httpStopBtn_);
	httpLayout->addLayout(httpCtrlRow);

	root->addWidget(gbHttp);

	// -----------------------------------------------------------------
	// WebSocket Server Card
	// -----------------------------------------------------------------
	auto *gbWs = new QGroupBox(QStringLiteral("WebSocket Server (Bridge)"), this);
	auto *wsLayout = new QVBoxLayout(gbWs);

	auto *wsStatusRow = new QHBoxLayout();
	wsStatusDot_ = new QLabel(gbWs);
	wsStatusText_ = new QLabel(gbWs);
	wsStatusRow->addWidget(wsStatusDot_);
	wsStatusRow->addWidget(wsStatusText_);
	wsStatusRow->addStretch(1);
	wsLayout->addLayout(wsStatusRow);

	auto *wsCtrlRow = new QHBoxLayout();
	wsPortSpin_ = new QSpinBox(gbWs);
	wsPortSpin_->setRange(1024, 65535);
	wsPortSpin_->setValue(settings_.ws_port);
	wsPortSpin_->setFixedWidth(90);

	wsStartBtn_ = new QPushButton(QStringLiteral("Start"), gbWs);
	wsStopBtn_ = new QPushButton(QStringLiteral("Stop"), gbWs);
	wsStartBtn_->setCursor(Qt::PointingHandCursor);
	wsStopBtn_->setCursor(Qt::PointingHandCursor);

	wsCtrlRow->addWidget(new QLabel(QStringLiteral("Port:"), gbWs));
	wsCtrlRow->addWidget(wsPortSpin_);
	wsCtrlRow->addStretch(1);
	wsCtrlRow->addWidget(wsStartBtn_);
	wsCtrlRow->addWidget(wsStopBtn_);
	wsLayout->addLayout(wsCtrlRow);

	root->addWidget(gbWs);
	
	// -----------------------------------------------------------------
	// OBS WebSocket Connect info
	// -----------------------------------------------------------------
	auto *gbObs = new QGroupBox(QStringLiteral("OBS Websocket Connect info"), this);
	auto *obsLayout = new QVBoxLayout(gbObs);
	
	auto *obsPortRow = new QHBoxLayout();
	obsPortSpin_ = new QSpinBox(gbObs);
	obsPortSpin_->setRange(1, 65535);
	obsPortSpin_->setValue(settings_.obs_port);
	obsPortSpin_->setFixedWidth(90);
	obsPortRow->addWidget(new QLabel(QStringLiteral("OBS Port:"), gbObs));
	obsPortRow->addWidget(obsPortSpin_);
	obsPortRow->addStretch(1);
	obsLayout->addLayout(obsPortRow);
	
	auto *obsPassRow = new QHBoxLayout();
	obsPasswordEdit_ = new QLineEdit(gbObs);
	obsPasswordEdit_->setEchoMode(QLineEdit::Password);
	obsPasswordEdit_->setText(settings_.obs_password);
	obsPassRow->addWidget(new QLabel(QStringLiteral("Password:"), gbObs));
	obsPassRow->addWidget(obsPasswordEdit_);
	obsLayout->addLayout(obsPassRow);
	
	root->addWidget(gbObs);

	// -----------------------------------------------------------------
	// Document Root
	// -----------------------------------------------------------------
	auto *gbDoc = new QGroupBox(QStringLiteral("Document Root"), this);
	auto *docLayout = new QVBoxLayout(gbDoc);

	docRootEdit_ = new QLineEdit(gbDoc);
	docRootEdit_->setReadOnly(true);
	docRootEdit_->setText(settings_.doc_root);
	docLayout->addWidget(docRootEdit_);

	auto *docBtns = new QHBoxLayout();
	browseBtn_ = new QPushButton(QStringLiteral("Browse…"), gbDoc);
	openBtn_ = new QPushButton(QStringLiteral("Open Folder"), gbDoc);
	browseBtn_->setCursor(Qt::PointingHandCursor);
	openBtn_->setCursor(Qt::PointingHandCursor);
	docBtns->addStretch(1);
	docBtns->addWidget(browseBtn_);
	docBtns->addWidget(openBtn_);
	docLayout->addLayout(docBtns);

	root->addWidget(gbDoc);

	// -----------------------------------------------------------------
	// Actions
	// -----------------------------------------------------------------
	auto *actionRow = new QHBoxLayout();
	createBrowserBtn_ = new QPushButton(QStringLiteral("Create Browser Source"), this);
	refreshBrowserBtn_ = new QPushButton(QStringLiteral("Refresh Source"), this);
	createBrowserBtn_->setCursor(Qt::PointingHandCursor);
	refreshBrowserBtn_->setCursor(Qt::PointingHandCursor);
	actionRow->addWidget(createBrowserBtn_);
	actionRow->addWidget(refreshBrowserBtn_);
	root->addLayout(actionRow);

	// -----------------------------------------------------------------
	// Footer
	// -----------------------------------------------------------------
	auto *footer = new QHBoxLayout();
	footer->addStretch(1);
	auto *closeBtn = new QPushButton(QStringLiteral("Close"), this);
	closeBtn->setDefault(true);
	footer->addWidget(closeBtn);
	root->addLayout(footer);

	// -----------------------------------------------------------------
	// Signals
	// -----------------------------------------------------------------
	statusTimer_ = new QTimer(this);
	connect(statusTimer_, &QTimer::timeout, this, &LwsDialog::onPollHealth);
	statusTimer_->start(1000);

	connect(httpStartBtn_, &QPushButton::clicked, this, &LwsDialog::onStartHttp);
	connect(httpStopBtn_, &QPushButton::clicked, this, &LwsDialog::onStopHttp);
	connect(wsStartBtn_, &QPushButton::clicked, this, &LwsDialog::onStartWs);
	connect(wsStopBtn_, &QPushButton::clicked, this, &LwsDialog::onStopWs);

	connect(browseBtn_, &QPushButton::clicked, this, &LwsDialog::onBrowseDocRoot);
	connect(openBtn_, &QPushButton::clicked, this, &LwsDialog::onOpenDocRoot);

	connect(createBrowserBtn_, &QPushButton::clicked, this, &LwsDialog::onCreateBrowserSource);
	connect(refreshBrowserBtn_, &QPushButton::clicked, this, &LwsDialog::onRefreshBrowserSource);
	
	// Save when values change
	connect(obsPortSpin_, QOverload<int>::of(&QSpinBox::valueChanged), [this](int val) {
		settings_.obs_port = val;
		lws_settings_save(settings_);
	});
	connect(obsPasswordEdit_, &QLineEdit::textChanged, [this](const QString &text) {
		settings_.obs_password = text;
		lws_settings_save(settings_);
	});

	connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

	onPollHealth();
}

LwsDialog::~LwsDialog() = default;

void LwsDialog::onPollHealth()
{
	updateHttpUi();
	updateWsUi();
}

void LwsDialog::updateHttpUi()
{
	bool running = lws_http_server_is_running();
	bool failed = lws_http_server_failed();
	int port = lws_http_server_port();

	ServerStatus status = ServerStatus::Stopped;
	QString text = QStringLiteral("Stopped");

	if (running) {
		status = ServerStatus::Running;
		text = QStringLiteral("Running on :%1").arg(port);
	} else if (failed) {
		status = ServerStatus::Failed;
		text = QStringLiteral("Port not available");
	}

	styleDot(httpStatusDot_, status);
	httpStatusText_->setText(text);
	httpStartBtn_->setEnabled(!running);
	httpStopBtn_->setEnabled(running);
}

void LwsDialog::updateWsUi()
{
	bool running = lws_ws_server_is_running();
	bool failed = lws_ws_server_failed();
	int port = lws_ws_server_port();

	ServerStatus status = ServerStatus::Stopped;
	QString text = QStringLiteral("Stopped");

	if (running) {
		status = ServerStatus::Running;
		text = QStringLiteral("Running on :%1").arg(port);
	} else if (failed) {
		status = ServerStatus::Failed;
		text = QStringLiteral("Port not available");
	}

	styleDot(wsStatusDot_, status);
	wsStatusText_->setText(text);
	wsStartBtn_->setEnabled(!running);
	wsStopBtn_->setEnabled(running);
}

void LwsDialog::onStartHttp()
{
	settings_.http_port = httpPortSpin_->value();
	settings_.http_enabled = true;
	lws_settings_save(settings_);
	lws_http_server_start(settings_.doc_root, settings_.http_port);
	onPollHealth();
}

void LwsDialog::onStopHttp()
{
	settings_.http_enabled = false;
	lws_settings_save(settings_);
	lws_http_server_stop();
	onPollHealth();
}

void LwsDialog::onStartWs()
{
	settings_.ws_port = wsPortSpin_->value();
	settings_.ws_enabled = true;
	lws_settings_save(settings_);
	lws_ws_server_start(settings_.ws_port);
	onPollHealth();
}

void LwsDialog::onStopWs()
{
	settings_.ws_enabled = false;
	lws_settings_save(settings_);
	lws_ws_server_stop();
	onPollHealth();
}

void LwsDialog::onBrowseDocRoot()
{
	QString picked = QFileDialog::getExistingDirectory(this, tr("Select Document Root"), settings_.doc_root);
	if (!picked.isEmpty()) {
		settings_.doc_root = picked;
		docRootEdit_->setText(picked);
		lws_settings_save(settings_);
	}
}

void LwsDialog::onOpenDocRoot()
{
	QDesktopServices::openUrl(QUrl::fromLocalFile(settings_.doc_root));
}

void LwsDialog::onCreateBrowserSource()
{
	int port = lws_http_server_port();
	if (port <= 0) port = settings_.http_port;
	
	QString baseName = QString::fromUtf8(kLwsBrowserSourceName);
	QString finalName = baseName;
	int index = 1;

	// Check if source with this name already exists
	while (true) {
		obs_source_t *existing = obs_get_source_by_name(finalName.toUtf8().constData());
		if (!existing) break;
		
		obs_source_release(existing);
		finalName = QString("%1 %2").arg(baseName).arg(index++);
	}

	QString url = QStringLiteral("http://127.0.0.1:%1/").arg(port);
	lws_create_or_update_browser_source_in_current_scene(
		finalName, 
		url
	);
}

void LwsDialog::onRefreshBrowserSource()
{
	// 1. Refresh the primary browser source created by the plugin
	lws_refresh_browser_source_by_name(QStringLiteral("Media Warp - Manager"));

	// 2. Perform bulk refresh for all tagged sources
	lws_refresh_all_tagged_browser_sources(settings_.http_port, settings_.ws_port);
}
