#pragma once
#include <QDialog>
#include "lws_settings.hpp"

class QLabel;
class QSpinBox;
class QPushButton;
class QTimer;
class QLineEdit;

class LwsDialog : public QDialog {
	Q_OBJECT
public:
	explicit LwsDialog(QWidget *parent = nullptr);
	~LwsDialog() override;

private slots:
	void onPollHealth();
	void onBrowseDocRoot();
	void onOpenDocRoot();
	
	// HTTP
	void onStartHttp();
	void onStopHttp();
	
	// WS
	void onStartWs();
	void onStopWs();

	void onCreateBrowserSource();
	void onRefreshBrowserSource();

private:
	void updateHttpUi();
	void updateWsUi();

private:
	LwsSettings settings_;

	// HTTP Card
	QLabel *httpStatusDot_ = nullptr;
	QLabel *httpStatusText_ = nullptr;
	QSpinBox *httpPortSpin_ = nullptr;
	QPushButton *httpStartBtn_ = nullptr;
	QPushButton *httpStopBtn_ = nullptr;

	// WS Card
	QLabel *wsStatusDot_ = nullptr;
	QLabel *wsStatusText_ = nullptr;
	QSpinBox *wsPortSpin_ = nullptr;
	QPushButton *wsStartBtn_ = nullptr;
	QPushButton *wsStopBtn_ = nullptr;
	
	// OBS Card
	QSpinBox *obsPortSpin_ = nullptr;
	QLineEdit *obsPasswordEdit_ = nullptr;

	// Doc Root
	QLineEdit *docRootEdit_ = nullptr;
	QPushButton *browseBtn_ = nullptr;
	QPushButton *openBtn_ = nullptr;

	// Actions
	QPushButton *createBrowserBtn_ = nullptr;
	QPushButton *refreshBrowserBtn_ = nullptr;

	QTimer *statusTimer_ = nullptr;
};
