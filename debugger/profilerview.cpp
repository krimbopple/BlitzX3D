#include "stdafx.h"
#include "profilerview.h"
#include "prefs.h"
#include "../MultiLang/MultiLang.h"

#include <algorithm>
#include <unordered_map>

#define SUMMARY_HEIGHT 22

static std::string formatBytes(__int64 bytes) {
	char buf[64];
	double mb = (double)bytes / (1024.0 * 1024.0);
	sprintf(buf, "%.2f MB", mb);
	return buf;
}

// ProfilerSummary

IMPLEMENT_DYNAMIC(ProfilerSummary, CStatic)

void ProfilerSummary::update(const Profiler& prof, int objCnt, int unrelObjCnt, int stringCnt, __int64 workingSetBytes) {
	__int64 total = 0;
	const std::map<std::string, ProfileStats>& results = prof.results();

	std::string topFunc;
	__int64 topSelf = 0;
	for(std::map<std::string, ProfileStats>::const_iterator it = results.begin(); it != results.end(); ++it) {
		total += it->second.selfTicks;
		if(it->second.selfTicks > topSelf) {
			topSelf = it->second.selfTicks;
			topFunc = it->first;
		}
	}

	std::string mem = formatBytes(workingSetBytes);

	// may god have mercy on my soul
	char buf[420];
	if(!topFunc.empty() && total > 0) {
		double topPct = 100.0 * (double)topSelf / (double)total;
		sprintf(buf, "  Top: %s (%.1f%%)    Active memory: %s    Objects: %d    Unreleased: %d    Strings: %d    |    Sampled CPU time: %.2f ms", topFunc.c_str(), topPct, mem.c_str(), objCnt, unrelObjCnt, stringCnt, prof.ticksToMs(total));
	}
	else {
		sprintf(buf, "  Active memory: %s    Objects: %d    Unreleased: %d    Strings: %d    |    Sampled CPU time: %.2f ms", mem.c_str(), objCnt, unrelObjCnt, stringCnt, prof.ticksToMs(total));
	}

	CString cur;
	GetWindowText(cur);
	if(cur != buf) SetWindowTextA(buf);
}

// ProfilerListCtrl

IMPLEMENT_DYNAMIC(ProfilerListCtrl, CListCtrl)
BEGIN_MESSAGE_MAP(ProfilerListCtrl, CListCtrl)
	ON_WM_CREATE()
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnColumnClick)
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnCustomDraw)
END_MESSAGE_MAP()

ProfilerListCtrl::ProfilerListCtrl() :sortCol(1), sortAsc(false), curProf(0), lastRowCount(-1), headerSubclassed(false) {
}

int ProfilerListCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct) {
	CListCtrl::OnCreate(lpCreateStruct);

	SetExtendedStyle(GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	SetBkColor(prefs.rgb_bkgrnd);
	SetTextColor(prefs.rgb_default);
	SetFont(&prefs.debugFont);

	InsertColumn(0, MultiLang::profiler_function, LVCFMT_LEFT, 190);
	InsertColumn(1, MultiLang::profiler_pct_cpu, LVCFMT_RIGHT, 65);
	InsertColumn(2, MultiLang::profiler_pct_mem, LVCFMT_RIGHT, 65);
	InsertColumn(3, MultiLang::profiler_calls, LVCFMT_RIGHT, 55);
	InsertColumn(4, MultiLang::profiler_self_ms, LVCFMT_RIGHT, 80);
	InsertColumn(5, MultiLang::profiler_total_ms, LVCFMT_RIGHT, 80);
	InsertColumn(6, MultiLang::profiler_max_ms, LVCFMT_RIGHT, 70);
	InsertColumn(7, MultiLang::profiler_net_objs, LVCFMT_RIGHT, 75);
	InsertColumn(8, MultiLang::profiler_net_strs, LVCFMT_RIGHT, 75);

	CWnd* hdrWnd = GetHeaderCtrl();
	if(hdrWnd && !headerSubclassed) {
		headerSubclassed = header.SubclassWindow(hdrWnd->GetSafeHwnd()) != FALSE;
		if(headerSubclassed) header.SetFont(&prefs.debugFont);
	}

	return 0;
}

void ProfilerListCtrl::clear() {
	rows.clear();
	lastRowCount = -1;
	DeleteAllItems();
}

void ProfilerListCtrl::sortRows() {
	std::sort(rows.begin(), rows.end(), [this](const Row& a, const Row& b) {
		bool less;
		switch(sortCol) {
			case 0: less = a.func < b.func; break;
			case 1: less = a.stats.selfTicks < b.stats.selfTicks; break;
			case 2: less = a.stats.netObjDelta < b.stats.netObjDelta; break;
			case 3: less = a.stats.callCount < b.stats.callCount; break;
			case 4: less = a.stats.selfTicks < b.stats.selfTicks; break;
			case 6: less = a.stats.maxTicks < b.stats.maxTicks; break;
			case 7: less = a.stats.netObjDelta < b.stats.netObjDelta; break;
			case 8: less = a.stats.netStrDelta < b.stats.netStrDelta; break;
			case 5:
			default: less = a.stats.totalTicks < b.stats.totalTicks; break;
		}
		return sortAsc ? less : !less;
	});
}

void ProfilerListCtrl::repaint() {
	if(!curProf) return;

	bool needRebuild = (int)rows.size() != lastRowCount;

	if(needRebuild) {
		DeleteAllItems();
		lastRowCount = (int)rows.size();
		for(Row& r : rows) {
			for(int c = 0; c < 9; ++c) r.textSet[c] = false;
		}
	}

	__int64 totalSelf = curProf->totalSelfTicks();
	int totalPosObjDelta = curProf->totalPositiveNetObjDelta();

	char buf[64];
	for(size_t i = 0; i < rows.size(); ++i) {
		Row& r = rows[i];
		int idx = (int)i;
		if(needRebuild) {
			InsertItem(idx, r.func.c_str());
			r.lastText[0] = r.func;
			r.textSet[0] = true;
		}
		else if(!r.textSet[0] || r.lastText[0] != r.func) {
			SetItemText(idx, 0, r.func.c_str());
			r.lastText[0] = r.func;
			r.textSet[0] = true;
		}

		double pctCpu = totalSelf ? (100.0 * (double)r.stats.selfTicks / (double)totalSelf) : 0.0;
		sprintf(buf, "%.1f%%", pctCpu);
		if(!r.textSet[1] || r.lastText[1] != buf) { SetItemText(idx, 1, buf); r.lastText[1] = buf; r.textSet[1] = true; }

		double pctMem = (totalPosObjDelta && r.stats.netObjDelta > 0) ? (100.0 * (double)r.stats.netObjDelta / (double)totalPosObjDelta) : 0.0;
		sprintf(buf, "%.1f%%", pctMem);
		if(!r.textSet[2] || r.lastText[2] != buf) { SetItemText(idx, 2, buf); r.lastText[2] = buf; r.textSet[2] = true; }

		sprintf(buf, "%d", r.stats.callCount);
		if(!r.textSet[3] || r.lastText[3] != buf) { SetItemText(idx, 3, buf); r.lastText[3] = buf; r.textSet[3] = true; }

		sprintf(buf, "%.3f", curProf->ticksToMs(r.stats.selfTicks));
		if(!r.textSet[4] || r.lastText[4] != buf) { SetItemText(idx, 4, buf); r.lastText[4] = buf; r.textSet[4] = true; }

		sprintf(buf, "%.3f", curProf->ticksToMs(r.stats.totalTicks));
		if(!r.textSet[5] || r.lastText[5] != buf) { SetItemText(idx, 5, buf); r.lastText[5] = buf; r.textSet[5] = true; }

		sprintf(buf, "%.3f", curProf->ticksToMs(r.stats.maxTicks));
		if(!r.textSet[6] || r.lastText[6] != buf) { SetItemText(idx, 6, buf); r.lastText[6] = buf; r.textSet[6] = true; }

		sprintf(buf, "%+d", r.stats.netObjDelta);
		if(!r.textSet[7] || r.lastText[7] != buf) { SetItemText(idx, 7, buf); r.lastText[7] = buf; r.textSet[7] = true; }

		sprintf(buf, "%+d", r.stats.netStrDelta);
		if(!r.textSet[8] || r.lastText[8] != buf) { SetItemText(idx, 8, buf); r.lastText[8] = buf; r.textSet[8] = true; }
	}
}

void ProfilerListCtrl::refresh(const Profiler& prof) {
	curProf = &prof;

	std::unordered_map<std::string, Row> previous;
	previous.reserve(rows.size());
	for(Row& r : rows) previous.emplace(r.func, std::move(r));
	rows.clear();

	const std::map<std::string, ProfileStats>& results = prof.results();
	rows.reserve(results.size());
	for(std::map<std::string, ProfileStats>::const_iterator it = results.begin(); it != results.end(); ++it) {
		auto found = previous.find(it->first);
		if(found != previous.end()) {
			Row r = std::move(found->second);
			r.stats = it->second;
			rows.push_back(std::move(r));
		}
		else {
			Row r;
			r.func = it->first;
			r.stats = it->second;
			rows.push_back(std::move(r));
		}
	}

	sortRows();
	repaint();
}

void ProfilerListCtrl::OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult) {
	LPNMLISTVIEW p = (LPNMLISTVIEW)pNMHDR;

	if(p->iSubItem == sortCol) sortAsc = !sortAsc;
	else { sortCol = p->iSubItem; sortAsc = false; }

	sortRows();
	lastRowCount = -1;
	repaint();

	*pResult = 0;
}

void ProfilerListCtrl::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult) {
	LPNMLVCUSTOMDRAW cd = (LPNMLVCUSTOMDRAW)pNMHDR;

	switch(cd->nmcd.dwDrawStage) {
		case CDDS_PREPAINT:
			*pResult = CDRF_NOTIFYITEMDRAW;
			return;

		case CDDS_ITEMPREPAINT:
			*pResult = CDRF_NOTIFYSUBITEMDRAW;
			return;

		case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
			cd->clrText = prefs.rgb_default;
			cd->clrTextBk = prefs.rgb_bkgrnd;
			*pResult = CDRF_DODEFAULT;
			return;

		default:
			*pResult = CDRF_DODEFAULT;
			return;
	}
}

IMPLEMENT_DYNAMIC(ProfilerHeaderCtrl, CHeaderCtrl)
BEGIN_MESSAGE_MAP(ProfilerHeaderCtrl, CHeaderCtrl)
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnCustomDraw)
END_MESSAGE_MAP()

void ProfilerHeaderCtrl::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult) {
	LPNMCUSTOMDRAW cd = (LPNMCUSTOMDRAW)pNMHDR;

	switch(cd->dwDrawStage) {
		case CDDS_PREPAINT:
			*pResult = CDRF_NOTIFYITEMDRAW;
			return;

		case CDDS_ITEMPREPAINT: {
			CDC* dc = CDC::FromHandle(cd->hdc);
			CRect rc(cd->rc);

			dc->FillSolidRect(rc, prefs.rgb_bkgrnd);

			COLORREF oldColor = dc->SetTextColor(prefs.rgb_default);
			int oldMode = dc->SetBkMode(TRANSPARENT);
			CFont* oldFont = dc->SelectObject(&prefs.debugFont);

			char text[64];
			HDITEM hdi;
			hdi.mask = HDI_TEXT | HDI_FORMAT;
			hdi.pszText = text;
			hdi.cchTextMax = sizeof(text);
			GetItem((int)cd->dwItemSpec, &hdi);

			UINT align = (hdi.fmt & HDF_RIGHT) ? DT_RIGHT : DT_LEFT;
			if(align == DT_RIGHT) rc.right -= 6;
			else rc.left += 6;

			dc->DrawText(text, -1, rc, align | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

			dc->SelectObject(oldFont);
			dc->SetBkMode(oldMode);
			dc->SetTextColor(oldColor);

			*pResult = CDRF_SKIPDEFAULT;
			return;
		}

		default:
			*pResult = CDRF_DODEFAULT;
			return;
	}
}

// ProfilerPanel

IMPLEMENT_DYNAMIC(ProfilerPanel, CWnd)
BEGIN_MESSAGE_MAP(ProfilerPanel, CWnd)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

ProfilerPanel::ProfilerPanel() {
}

BOOL ProfilerPanel::PreCreateWindow(CREATESTRUCT& cs) {
	static CString cls;
	if(cls.IsEmpty()) {
		cls = AfxRegisterWndClass(
			CS_HREDRAW | CS_VREDRAW,
			0, (HBRUSH)(COLOR_WINDOW + 1), 0);
	}
	cs.lpszClass = cls;
	return CWnd::PreCreateWindow(cs);
}

int ProfilerPanel::OnCreate(LPCREATESTRUCT lpCreateStruct) {
	if(CWnd::OnCreate(lpCreateStruct) == -1) return -1;

	bkBrush.CreateSolidBrush(prefs.rgb_bkgrnd);

	summary.Create("", WS_VISIBLE | WS_CHILD | SS_LEFT | SS_CENTERIMAGE, CRect(0, 0, 0, 0), this, 100);
	summary.SetFont(&prefs.debugFont);

	list.Create(WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_SHOWSELALWAYS, CRect(0, 0, 0, 0), this, 101);

	return 0;
}

void ProfilerPanel::OnSize(UINT type, int w, int h) {
	CWnd::OnSize(type, w, h);
	summary.MoveWindow(0, 0, w, SUMMARY_HEIGHT);
	list.MoveWindow(0, SUMMARY_HEIGHT, w, h - SUMMARY_HEIGHT);
}

HBRUSH ProfilerPanel::OnCtlColor(CDC* dc, CWnd* wnd, UINT ctlType) {
	if(ctlType == CTLCOLOR_STATIC && wnd->GetSafeHwnd() == summary.GetSafeHwnd()) {
		dc->SetTextColor(prefs.rgb_default);
		dc->SetBkColor(prefs.rgb_bkgrnd);
		return (HBRUSH)bkBrush;
	}
	return (HBRUSH)::DefWindowProc(m_hWnd, WM_CTLCOLORSTATIC, (WPARAM)dc->GetSafeHdc(), (LPARAM)wnd->GetSafeHwnd());
}

void ProfilerPanel::refresh(const Profiler& prof, int objCnt, int unrelObjCnt, int stringCnt, __int64 workingSetBytes) {
	summary.update(prof, objCnt, unrelObjCnt, stringCnt, workingSetBytes);
	list.refresh(prof);
}

void ProfilerPanel::clear() {
	list.clear();
}
