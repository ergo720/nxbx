// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <QtCore/QString>


namespace QtHost {
	/// Default theme name for the platform.
	const char* GetDefaultThemeName();

	/// Sets application theme according to settings.
	void UpdateApplicationTheme();

	/// Returns true if the application theme is using dark colours.
	bool IsDarkApplicationTheme();

	/// Sets the icon theme, based on the current style (light/dark).
	void SetIconThemeFromStyle();
}
