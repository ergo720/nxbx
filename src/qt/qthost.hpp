// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <QtCore/QString>


class QWidget;
class QUrl;

namespace QtHost
{
	/// Sets application theme according to settings.
	void UpdateApplicationTheme();

	/// Returns true if the application theme is using dark colors.
	bool IsDarkApplicationTheme();

	/// Sets the icon theme, based on the current style (light/dark).
	void SetIconThemeFromStyle();

	/// Opens a URL with the default handler.
	void OpenURL(QWidget* parent, const QUrl& qurl);

	/// Opens a URL string with the default handler.
	void OpenURL(QWidget* parent, const char* url);

	/// Opens a URL string with the default handler.
	void OpenURL(QWidget* parent, const QString& url);
}
