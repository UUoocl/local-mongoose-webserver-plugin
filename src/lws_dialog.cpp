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

static void styleDot(QLabel *dot, bool ok)
{
	dot->setFixedSize(12, 12);
	dot->setStyleSheet(QString("border-radius:6px;"
				   "background-color:%1;"
				   "border:1px solid rgba(0,0,0,0.25);")
				   .arg(ok ? "#39d353" : "#ff6b6b"));
}

static constexpr const char *kLwsBrowserSourceName = "Local Webserver";

LwsDialog::LwsDialog(QWidget *parent) : QDialog(parent)
{
	setObjectName(QStringLiteral("LwsDialog"));
	setWindowTitle(QStringLiteral("Local Webserver"));
	setModal(false);
	setSizeGripEnabled(false);
	setMinimumWidth(520);

	settings_ = lws_settings_load();

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(14, 14, 14, 14);
	root->setSpacing(10);

	auto *hint = new QLabel(QStringLiteral(
		"Start a local HTTP server that serves files from a folder.\n"
		"Then create a Browser Source in your current scene pointing to it."),
		this);
	hint->setWordWrap(true);
	root->addWidget(hint);

	// -----------------------------------------------------------------
	// Server status group
	// -----------------------------------------------------------------
	auto *gbServer = new QGroupBox(QStringLiteral("Server status"), this);
	auto *serverLayout = new QVBoxLayout(gbServer);

	auto *statusRow = new QWidget(gbServer);
	auto *statusLay = new QHBoxLayout(statusRow);
	statusLay->setContentsMargins(0, 0, 0, 0);

	statusDot_ = new QLabel(statusRow);
	statusText_ = new QLabel(QStringLiteral("Stopped"), statusRow);

	statusLay->addWidget(statusDot_);
	statusLay->addWidget(statusText_);
	statusLay->addStretch(1);
	statusRow->setLayout(statusLay);

	serverLayout->addWidget(statusRow);
	gbServer->setLayout(serverLayout);
	root->addWidget(gbServer);

	// -----------------------------------------------------------------
	// Doc root group
	// -----------------------------------------------------------------
	auto *gbDoc = new QGroupBox(QStringLiteral("Document root"), this);
	auto *docLayout = new QVBoxLayout(gbDoc);

	auto *docHint = new QLabel(QStringLiteral(
		"Files from this folder will be available at the server root (/)."),
		gbDoc);
	docHint->setWordWrap(true);
	docLayout->addWidget(docHint);

	docRootEdit_ = new QLineEdit(gbDoc);
	docRootEdit_->setReadOnly(true);
	docRootEdit_->setText(settings_.doc_root);
	docLayout->addWidget(docRootEdit_);

	auto *docButtonsRow = new QWidget(gbDoc);
	auto *docButtonsLay = new QHBoxLayout(docButtonsRow);
	docButtonsLay->setContentsMargins(0, 0, 0, 0);

	browseBtn_ = new QPushButton(QStringLiteral("Browse…"), docButtonsRow);
	openBtn_   = new QPushButton(QStringLiteral("Open folder"), docButtonsRow);

	browseBtn_->setCursor(Qt::PointingHandCursor);
	openBtn_->setCursor(Qt::PointingHandCursor);

	docButtonsLay->addStretch(1);
	docButtonsLay->addWidget(browseBtn_);
	docButtonsLay->addWidget(openBtn_);
	docButtonsRow->setLayout(docButtonsLay);

	docLayout->addWidget(docButtonsRow);
	gbDoc->setLayout(docLayout);
	root->addWidget(gbDoc);

	// -----------------------------------------------------------------
	// Port group
	// -----------------------------------------------------------------
	auto *gbPort = new QGroupBox(QStringLiteral("Port"), this);
	auto *portLayout = new QHBoxLayout(gbPort);

	portSpin_ = new QSpinBox(gbPort);
	portSpin_->setRange(1024, 65535);
	portSpin_->setValue(settings_.port);
	portSpin_->setFixedWidth(110);

	restartBtn_ = new QPushButton(QStringLiteral("Start / Restart"), gbPort);
	stopBtn_    = new QPushButton(QStringLiteral("Stop"), gbPort);

	restartBtn_->setCursor(Qt::PointingHandCursor);
	stopBtn_->setCursor(Qt::PointingHandCursor);

	portLayout->addWidget(new QLabel(QStringLiteral("Port:"), gbPort));
	portLayout->addWidget(portSpin_);
	portLayout->addStretch(1);
	portLayout->addWidget(restartBtn_);
	portLayout->addWidget(stopBtn_);

	gbPort->setLayout(portLayout);
	root->addWidget(gbPort);

	// -----------------------------------------------------------------
	// Browser source buttons
	// -----------------------------------------------------------------
	auto *browserRow = new QHBoxLayout();
	browserRow->setSpacing(8);
	browserRow->setContentsMargins(0, 0, 0, 0);

	createBrowserBtn_ = new QPushButton(QStringLiteral("Create Browser Source in current scene"), this);
	createBrowserBtn_->setCursor(Qt::PointingHandCursor);

	refreshBrowserBtn_ = new QPushButton(QStringLiteral("Refresh Browser Source"), this);
	refreshBrowserBtn_->setCursor(Qt::PointingHandCursor);
	refreshBrowserBtn_->setToolTip(
		QStringLiteral("Refresh the '%1' browser source (cache bust).").arg(kLwsBrowserSourceName));

	browserRow->addWidget(createBrowserBtn_, 1);
	browserRow->addWidget(refreshBrowserBtn_, 0);

	root->addLayout(browserRow);

	// -----------------------------------------------------------------
	// Close
	// -----------------------------------------------------------------
	auto *closeRow = new QHBoxLayout();
	closeRow->addStretch(1);
	auto *closeBtn = new QPushButton(QStringLiteral("Close"), this);
	closeBtn->setCursor(Qt::PointingHandCursor);
	closeBtn->setDefault(true);
	closeRow->addWidget(closeBtn);
	root->addLayout(closeRow);

	setLayout(root);

	// -----------------------------------------------------------------
	// Poll + signals
	// -----------------------------------------------------------------
	statusTimer_ = new QTimer(this);
	connect(statusTimer_, &QTimer::timeout, this, &LwsDialog::onPollHealth);
	statusTimer_->start(1500);

	connect(browseBtn_, &QPushButton::clicked, this, &LwsDialog::onBrowseDocRoot);
	connect(openBtn_,   &QPushButton::clicked, this, &LwsDialog::onOpenDocRoot);
	connect(restartBtn_, &QPushButton::clicked, this, &LwsDialog::onRestartServer);
	connect(stopBtn_, &QPushButton::clicked, this, &LwsDialog::onStopServer);

	connect(createBrowserBtn_, &QPushButton::clicked, this, &LwsDialog::onCreateBrowserSource);
	connect(refreshBrowserBtn_, &QPushButton::clicked, this, &LwsDialog::onRefreshBrowserSource);

	connect(closeBtn, &QPushButton::clicked, this, &LwsDialog::accept);

	onPollHealth();
}

LwsDialog::~LwsDialog() = default;

void LwsDialog::setStatusUi(bool ok, int port)
{
	styleDot(statusDot_, ok);
	statusText_->setText(ok
		? QStringLiteral("Running on :%1").arg(port)
		: QStringLiteral("Stopped"));
}

void LwsDialog::onPollHealth()
{
	setStatusUi(lws_server_is_running(), lws_server_port());
}

void LwsDialog::onBrowseDocRoot()
{
	QString current = settings_.doc_root;
	if (current.isEmpty())
		current = lws_default_data_root();

	QString picked = QFileDialog::getExistingDirectory(
		this, tr("Select document root"), current);

	if (picked.isEmpty())
		return;

	settings_.doc_root = picked;
	docRootEdit_->setText(picked);
	lws_settings_save(settings_);
}

void LwsDialog::onOpenDocRoot()
{
	QString root = settings_.doc_root;
	if (root.isEmpty())
		root = lws_get_data_root();

	QDesktopServices::openUrl(QUrl::fromLocalFile(root));
}

void LwsDialog::onRestartServer()
{
	settings_.port = portSpin_->value();
	if (settings_.doc_root.isEmpty())
		settings_.doc_root = lws_get_data_root();

	lws_settings_save(settings_);

	if (lws_server_is_running())
		lws_server_stop();

	int bound = lws_server_start(settings_.doc_root, settings_.port);
	if (!bound)
		LOGW("Failed to start webserver");
	else if (bound != settings_.port) {
		settings_.port = bound;
		portSpin_->setValue(bound);
		lws_settings_save(settings_);
	}

	onPollHealth();
}

void LwsDialog::onStopServer()
{
	lws_server_stop();
	onPollHealth();
}

void LwsDialog::onCreateBrowserSource()
{
	const int port = lws_server_port();
	if (!lws_server_is_running() || port <= 0) {
		LOGW("Server not running; start it first.");
		return;
	}

	const QString url = QStringLiteral("http://127.0.0.1:%1/").arg(port);
	lws_create_or_update_browser_source_in_current_scene(
		QString::fromUtf8(kLwsBrowserSourceName), url);

	LOGI("Browser source created/updated: %s", url.toUtf8().constData());
}

void LwsDialog::onRefreshBrowserSource()
{
	const bool ok = lws_refresh_browser_source_by_name(QString::fromUtf8(kLwsBrowserSourceName));
	if (!ok)
		LOGW("Refresh failed for Browser Source: %s", kLwsBrowserSourceName);
	else
		LOGI("Browser source refreshed: %s", kLwsBrowserSourceName);
}
