// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include "ui_main_window.h"


class MainWindow final : public QMainWindow
{
	Q_OBJECT

public:
	/// Default filter for opening a file.
	static const char* OPEN_FILE_FILTER;

	MainWindow();
	~MainWindow();

	void initialize();

	/// Start a file from a user action (e.g. dragging a file onto the main window or with macOS open with)
	bool startFile(const QString& path);

private Q_SLOTS:
	void onStartFileActionTriggered();
	void onViewToolbarActionToggled(bool checked);
	void onGitHubRepositoryActionTriggered();

	void onMachineStarted();
	void onMachineStopped();
	bool requestShutdown(bool allow_confirm = true, bool allow_save_to_state = true, bool default_save_to_state = true);

private:
	void doStartFile(const QString& path);
	void setupAdditionalUi();
	void connectSignals();
	void updateEmulationActions(bool starting, bool running, bool stopping);
	void updateWindowState(bool force_visible = false);

	Ui::MainWindow m_ui;
};

extern MainWindow* g_main_window;
