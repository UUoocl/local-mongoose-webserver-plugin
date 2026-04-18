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
	void onRestartServer();
	void onStopServer();
	void onCreateBrowserSource();
	void onRefreshBrowserSource();

private:
	void setStatusUi(bool ok, int port);

private:
	LwsSettings settings_;

	QLabel *statusDot_ = nullptr;
	QLabel *statusText_ = nullptr;
	QSpinBox *portSpin_ = nullptr;
	QLineEdit *docRootEdit_ = nullptr;

	QPushButton *restartBtn_ = nullptr;
	QPushButton *stopBtn_ = nullptr;
	QPushButton *browseBtn_ = nullptr;
	QPushButton *openBtn_ = nullptr;

	QPushButton *createBrowserBtn_ = nullptr;
	QPushButton *refreshBrowserBtn_ = nullptr; // NEW

	QTimer *statusTimer_ = nullptr;
};
