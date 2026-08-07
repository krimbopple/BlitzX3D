#ifndef PROFILERVIEW_H
#define PROFILERVIEW_H

#include "profiler.h"

class ProfilerSummary : public CStatic {
public:
	void update(const Profiler& prof, int objCnt, int unrelObjCnt, int stringCnt, __int64 workingSetBytes);
	DECLARE_DYNAMIC(ProfilerSummary)
};

class ProfilerHeaderCtrl : public CHeaderCtrl {
public:
	DECLARE_DYNAMIC(ProfilerHeaderCtrl)
	DECLARE_MESSAGE_MAP()
	afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
};

class ProfilerListCtrl : public CListCtrl {
	int sortCol;
	bool sortAsc;
	const Profiler* curProf;
	ProfilerHeaderCtrl header;
	bool headerSubclassed;

	struct Row {
		std::string func;
		ProfileStats stats;
		std::string lastText[9];
		bool textSet[9] = { false, false, false, false, false, false, false, false, false };
	};
	std::vector<Row> rows;
	int lastRowCount;

	void sortRows();
	void repaint();

public:
	ProfilerListCtrl();

	void refresh(const Profiler& prof);
	void clear();

	DECLARE_DYNAMIC(ProfilerListCtrl)
	DECLARE_MESSAGE_MAP()

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
};

class ProfilerPanel : public CWnd {
	ProfilerSummary summary;
	ProfilerListCtrl list;
	CBrush bkBrush;

public:
	ProfilerPanel();

	void refresh(const Profiler& prof, int objCnt, int unrelObjCnt, int stringCnt, __int64 workingSetBytes);
	void clear();

	DECLARE_DYNAMIC(ProfilerPanel)
	DECLARE_MESSAGE_MAP()

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT type, int w, int h);
	afx_msg HBRUSH OnCtlColor(CDC* dc, CWnd* wnd, UINT ctlType);
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
};

#endif
