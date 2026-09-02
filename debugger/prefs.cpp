#include "stdafx.h"
#include <winreg.h>
#include <iomanip>
#include <filesystem>
#include "resource.h"
#include "prefs.h"
#include "../inipp/inipp.h"
#include "../MultiLang/MultiLang.h"

#pragma warning(disable: 6031)

Prefs prefs;

void Prefs::open() {
	homeDir = getenv("blitzpath");

	setDefault();

	PWSTR appdataDir = NULL;
	if(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appdataDir) != S_OK) {
		AfxMessageBox(MultiLang::debugger_cant_access_appdata, MB_TOPMOST | MB_SETFOREGROUND | MB_ICONERROR);
		return;
	}

	std::wstringstream wss;
	wss << appdataDir << "/Blitz3D TSS/";
	if(!std::filesystem::exists(wss.str())) {
		if(!CreateDirectoryW(wss.str().c_str(), 0)) {
			AfxMessageBox(MultiLang::debugger_cant_create_folder, MB_TOPMOST | MB_SETFOREGROUND | MB_ICONERROR);
			return;
		}
	}

	wss << "blitzide.ini";

	std::ifstream in(wss.str().c_str(), std::ios::in);
	if(!in.good()) return;

	in.seekg(0, std::ios::end);
	if(in.tellg() == 0) {
		AfxMessageBox(MultiLang::debugger_empty_ini, MB_TOPMOST | MB_SETFOREGROUND | MB_ICONERROR);
		return;
	}
	in.seekg(0, std::ios::beg);

	inipp::Ini<char> ini;
	ini.parse(in);

	inipp::get_value(ini.sections["COMPILER"], "Debug", prg_debug);
	inipp::get_value(ini.sections["COMPILER"], "NoLAA", prg_nolaa);
	inipp::get_value(ini.sections["COMPILER"], "NoAutoDecl", prg_noautodecl);
	inipp::get_value(ini.sections["COMPILER"], "LastBuild", prg_lastbuild);
	inipp::get_value(ini.sections["COMPILER"], "CommandLine", cmd_line);

	std::string unprocessedRect;

	inipp::get_value(ini.sections["WINDOW"], "Maximized", win_maximized);
	inipp::get_value(ini.sections["WINDOW"], "NoToolbar", win_notoolbar);
	inipp::get_value(ini.sections["WINDOW"], "WindowRect", unprocessedRect);

	sscanf(unprocessedRect.c_str(), "%ld %ld %ld %ld", &win_rect.left, &win_rect.top, &win_rect.right, &win_rect.bottom);

	inipp::get_value(ini.sections["FONTS"], "EditorFont", font_editor);
	inipp::get_value(ini.sections["FONTS"], "EditorFontSize", font_editor_height);
	inipp::get_value(ini.sections["FONTS"], "TabsFont", font_tabs);
	inipp::get_value(ini.sections["FONTS"], "TabsFontSize", font_tabs_height);
	inipp::get_value(ini.sections["FONTS"], "DebugFont", font_debug);
	inipp::get_value(ini.sections["FONTS"], "DebugFontSize", font_debug_height);

	std::string unprocessedBackground, unprocessedString, unprocessedIdent, unprocessedKeyword, unprocessedComment, unprocessedDigit, unprocessedDefault;

	inipp::get_value(ini.sections["COLORS"], "Background", unprocessedBackground);
	inipp::get_value(ini.sections["COLORS"], "String", unprocessedString);
	inipp::get_value(ini.sections["COLORS"], "Ident", unprocessedIdent);
	inipp::get_value(ini.sections["COLORS"], "Keyword", unprocessedKeyword);
	inipp::get_value(ini.sections["COLORS"], "Comment", unprocessedComment);
	inipp::get_value(ini.sections["COLORS"], "Digit", unprocessedDigit);
	inipp::get_value(ini.sections["COLORS"], "Default", unprocessedDefault);

	int r = 0, g = 0, b = 0;

	sscanf(unprocessedBackground.c_str(), "%d %d %d", &r, &g, &b);
	rgb_bkgrnd = RGB(r, g, b);
	sscanf(unprocessedString.c_str(), "%d %d %d", &r, &g, &b);
	rgb_string = RGB(r, g, b);
	sscanf(unprocessedIdent.c_str(), "%d %d %d", &r, &g, &b);
	rgb_ident = RGB(r, g, b);
	sscanf(unprocessedKeyword.c_str(), "%d %d %d", &r, &g, &b);
	rgb_keyword = RGB(r, g, b);
	sscanf(unprocessedComment.c_str(), "%d %d %d", &r, &g, &b);
	rgb_comment = RGB(r, g, b);
	sscanf(unprocessedDigit.c_str(), "%d %d %d", &r, &g, &b);
	rgb_digit = RGB(r, g, b);
	sscanf(unprocessedDefault.c_str(), "%d %d %d", &r, &g, &b);
	rgb_default = RGB(r, g, b);

	inipp::get_value(ini.sections["EDITOR"], "TabSpaces", edit_tabs);
	inipp::get_value(ini.sections["EDITOR"], "BackupCount", edit_backup);
	inipp::get_value(ini.sections["EDITOR"], "ToolbarImage", img_toolbar);
	inipp::get_value(ini.sections["EDITOR"], "SampleWindowSeconds", sampleWindowSeconds);

	std::string recentFile;
	for(int i = 1; i < 11; i++) {
		inipp::get_value(ini.sections["RECENT_FILES"], "File" + itoa(i), recentFile);
		if(recentFile.size() == 0) continue;
		recentFiles.push_back(recentFile);
	}
	createFonts();
	in.close();
	CoTaskMemFree(static_cast<void*>(appdataDir));
}

void Prefs::setDefault() {
	prg_debug = true;
	prg_nolaa = false;
	prg_noautodecl = false;

	win_rect.left = win_rect.top = 0;
	win_rect.right = 800; win_rect.bottom = 600;
	win_maximized = false;
	win_notoolbar = false;

	font_editor = "consolas";
	font_editor_height = 14;
	font_debug = "consolas";
	font_debug_height = 14;

	rgb_bkgrnd = RGB(34, 85, 136);
	rgb_string = RGB(0, 255, 102);
	rgb_ident = RGB(255, 255, 255);
	rgb_keyword = RGB(170, 255, 255);
	rgb_comment = RGB(255, 238, 0);
	rgb_digit = RGB(51, 255, 221);
	rgb_default = RGB(238, 238, 238);

	edit_tabs = 4;
	edit_backup = 2;
	sampleWindowSeconds = 30;

	img_toolbar = "toolbar.bmp";

	recentFiles.clear();

	createFonts();
}

void Prefs::createFonts() {
	editFont.Detach();
	tabsFont.Detach();
	debugFont.Detach();
	conFont.Detach();

	editFont.CreatePointFont(font_editor_height * 10, font_editor.c_str());
	tabsFont.CreatePointFont(font_tabs_height * 10, font_tabs.c_str());
	debugFont.CreatePointFont(font_debug_height * 10, font_debug.c_str());
	conFont.CreatePointFont(80, "courier");
}

std::string Prefs::boolToString(bool value) {
	return value ? "true" : "false";
}