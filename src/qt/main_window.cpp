// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "urls.hpp"
#include "host.hpp"
#include "main_window.hpp"
#include "isettings.hpp"
#include "qthost.hpp"
#include "console.hpp"
#include "paths.hpp"
#include <assert.h>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>


const char* MainWindow::OPEN_FILE_FILTER =
QT_TRANSLATE_NOOP("MainWindow", "All File Types (*.xbe *.iso);;"
	"Xbox Executable (*.xbe);;"
	"Xbox Game Disc Image (*.iso)");

MainWindow* g_main_window = nullptr;

static bool s_valid_machine = false;

// TODO: Figure out how to set this in the .ui file
/// Marks the icons for all actions in the given menu as mask icons
/// This means macOS's menubar renderer will ignore color values and use only the alpha in the image.
/// The color value will instead be taken from the system theme.
/// Since the menubar follows the OS's dark/light mode and not our current theme's, this prevents problems where a theme mismatch puts white icons in light mode or dark icons in dark mode.
static void makeIconsMasks(QWidget* menu)
{
	for (QAction* action : menu->actions())
	{
		if (!action->icon().isNull())
		{
			QIcon icon = action->icon();
			icon.setIsMask(true);
			action->setIcon(icon);
		}
		if (action->menu())
			makeIconsMasks(action->menu());
	}
}

MainWindow::MainWindow()
{
	assert(!g_main_window);
	g_main_window = this;
}

MainWindow::~MainWindow()
{
	if (g_main_window == this) {
		g_main_window = nullptr;
	}
}

void MainWindow::initialize()
{
	m_ui.setupUi(this);
	setupAdditionalUi();
	connectSignals();
}

bool MainWindow::startFile(const QString &path)
{
	doStartFile(path);
	return true;
}

void MainWindow::onMachineStarted()
{
	s_valid_machine = true;
	updateEmulationActions(true, true, false);
}

bool MainWindow::requestShutdown(bool allow_confirm, bool allow_save_to_state, bool default_save_to_state)
{
	if (!s_valid_machine) {
		return true;
	}

	if (isHidden()) {
		updateWindowState(true);
	}

	if (s_valid_machine) {
		s_valid_machine = false;
		updateEmulationActions(false, false, true);
	}

	g_console->exit();
	return true;
}

void MainWindow::onMachineStopped()
{
	s_valid_machine = false;
	updateEmulationActions(false, false, false);
	if (Host::InNoGUIMode()) {
		QGuiApplication::quit();
	}
}

void MainWindow::doStartFile(const QString& path)
{
	if (const auto exp = Host::validate_input_file(path.toStdString()); exp) {
		emu_path::update_after_reboot(exp.value(), path.toStdString());
		boot_params params = g_console->get_boot_params();
		s_valid_machine = false;
		g_console->exit(true);
		delete g_console;
		g_console = new console(params);
		if (g_console->get_state() == console_state::shut_down) {
			delete g_console;
			g_console = nullptr;
			QMessageBox::critical(this, tr("Error"), tr("Failed to create machine instance while launching file"));
			QGuiApplication::quit();
			g_main_window = nullptr;
		}
		g_console->start();
	}
	else {
		QMessageBox::critical(this, tr("Error"), tr(exp.error().c_str()));
	}
}

void MainWindow::setupAdditionalUi()
{
	makeIconsMasks(menuBar());

	const bool toolbar_visible = get_settings()->get_bool_value("ui", "show_toolbar", true);
	m_ui.actionViewToolbar->setChecked(toolbar_visible);
	m_ui.toolBar->setVisible(toolbar_visible);

	updateEmulationActions(false, false, false);
}

void MainWindow::connectSignals()
{
	connect(m_ui.actionStartFile, &QAction::triggered, this, &MainWindow::onStartFileActionTriggered);
	connect(m_ui.actionPowerOff, &QAction::triggered, this, [this]() { requestShutdown(true, true, true); });
	connect(m_ui.actionToolbarStartFile, &QAction::triggered, this, &MainWindow::onStartFileActionTriggered);
	connect(m_ui.actionToolbarPowerOff, &QAction::triggered, this, [this]() { requestShutdown(true, true, true); });
	connect(m_ui.actionExit, &QAction::triggered, this, &MainWindow::close);
	connect(m_ui.actionViewToolbar, &QAction::toggled, this, &MainWindow::onViewToolbarActionToggled);
	connect(m_ui.actionGitHubRepository, &QAction::triggered, this, &MainWindow::onGitHubRepositoryActionTriggered);
	connect(m_ui.actionAboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::updateEmulationActions(bool starting, bool running, bool stopping)
{
	const bool starting_or_running_or_stopping = starting || running || stopping;

	m_ui.actionStartFile->setDisabled(starting_or_running_or_stopping);

	m_ui.actionPowerOff->setEnabled(running);
	m_ui.actionToolbarPowerOff->setEnabled(running);
}

void MainWindow::updateWindowState(bool force_visible)
{
	const bool hide_window = Host::InNoGUIMode();
	const bool visible = force_visible && !hide_window;
	if (isVisible() != visible) {
		setVisible(visible);
	}
}

void MainWindow::onStartFileActionTriggered()
{
	const QString path(QFileDialog::getOpenFileName(this, tr("Start File"), QString(), tr(OPEN_FILE_FILTER), nullptr));
	if (path.isEmpty()) {
		return;
	}

	doStartFile(path);
}

void MainWindow::onViewToolbarActionToggled(bool checked)
{
	get_settings()->set_bool_value("ui", "show_toolbar", checked);
	m_ui.toolBar->setVisible(checked);
}

void MainWindow::onGitHubRepositoryActionTriggered()
{
	QtHost::OpenURL(this, QString::fromUtf8(NXBX_GITHUB_URL));
}
