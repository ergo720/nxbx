// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "host.hpp"
#include "qthost.hpp"
#include "main_window.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QMessageBox>


void QtHost::OpenURL(QWidget* parent, const QUrl& qurl)
{
	if (!QDesktopServices::openUrl(qurl))
	{
		QMessageBox::critical(parent, QCoreApplication::translate("FileOperations", "Failed to open URL"),
			QCoreApplication::translate("FileOperations", "Failed to open URL.\n\nThe URL was: %1").arg(qurl.toString()));
	}
}

void QtHost::OpenURL(QWidget* parent, const char* url)
{
	return OpenURL(parent, QUrl::fromEncoded(QByteArray(url, static_cast<int>(std::strlen(url)))));
}

void QtHost::OpenURL(QWidget* parent, const QString& url)
{
	return OpenURL(parent, QUrl(url));
}

void Host::RequestShutdown(bool allow_confirm, bool allow_save_to_state, bool default_save_to_state)
{
	QMetaObject::invokeMethod(g_main_window, "requestShutdown", Qt::QueuedConnection, Q_ARG(bool, allow_confirm),
		Q_ARG(bool, allow_save_to_state), Q_ARG(bool, default_save_to_state));
}

void Host::SignalStartup()
{
	QMetaObject::invokeMethod(g_main_window, "onMachineStarted", Qt::QueuedConnection);
}

void Host::SignalStop()
{
	QMetaObject::invokeMethod(g_main_window, "onMachineStopped", Qt::QueuedConnection);
}
