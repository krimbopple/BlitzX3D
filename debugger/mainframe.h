#ifndef MAINFRAME_H
#define MAINFRAME_H

#include "tabber.h"
#include "debugger.h"
#include "sourcefile.h"
#include "debugtree.h"
#include "profiler.h"
#include "profilerview.h"
#include "flamegraph.h"

enum ELogSeverity
{
	LOG_INFO = 0,
	LOG_WARNING,
	LOG_ERROR
};

struct LogEntry {
	ELogSeverity severity;
	std::string text;
};

class MainFrame : public CFrameWnd, public Debugger {

	Tabber tabber;
	Tabber tabber2;
	CToolBar toolBar;
	SourceFile debug_log;
	ConstsTree consts_tree;
	GlobalsTree globals_tree;
	LocalsTree locals_tree;
	ProfilerPanel profiler_panel;
	Profiler profiler;
	FlameGraphPanel flame_graph_panel;
	std::map<const char*, int> file_tabs;
	std::map<const char*, SourceFile*> files;
	CComboBox m_filterCombo;
	std::vector<LogEntry> m_logEntries;
	int m_currentFilter;

	int state, step_level, cur_pos;
	const char* cur_file;
	std::vector<std::string> call_stack;   // func names for crash reports
	int last_obj_cnt, last_unrel_cnt, last_str_cnt;
	__int64 last_working_set_bytes;

	bool shouldRun()const { return step_level < locals_tree.size(); }
	std::string buildCrashReport(const char* msg)const;

public:
	MainFrame();
	~MainFrame();

	void debugRun();
	void debugStop();
	bool debugStmt(int srcpos, const char* file);
	void debugEnter(void* frame, void* env, const char* func);
	void debugLeave();
	void debugLog(const char* msg);
	void debugMsg(const char* msg, bool serious);
	void debugSys(void* msg);

	void showCurStmt();
	void setState(int n);
	void setRuntime(void* mod, void* env);
	SourceFile* sourceFile(const char* file);

	void AddLogEntry(ELogSeverity severity, const std::string& text);
	void RefreshLogDisplay();

	DECLARE_DYNAMIC(MainFrame)
	DECLARE_MESSAGE_MAP()

	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT type, int w, int h);
	afx_msg void OnClose();
	afx_msg void OnTimer(UINT_PTR id);
	afx_msg void OnFilterSelChange();
	afx_msg void OnDestroy();
	afx_msg void OnTabSelChange(NMHDR* pNMHDR, LRESULT* pResult);

	afx_msg void cmdStop();
	afx_msg void cmdRun();
	afx_msg void cmdStepOver();
	afx_msg void cmdStepInto();
	afx_msg void cmdStepOut();
	afx_msg void cmdEnd();
	afx_msg void cmdProfileToggle();
	afx_msg void cmdProfileReset();

	afx_msg void updateCmdUI(CCmdUI* ui);

	afx_msg void OnWindowPosChanging(WINDOWPOS* pos);
};

#endif