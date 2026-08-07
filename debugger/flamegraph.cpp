#include "stdafx.h"
#include "flamegraph.h"
#include "profiler.h"
#include "prefs.h"
#include <algorithm>
#include <cstdio>

IMPLEMENT_DYNAMIC(FlameGraphPanel, CWnd)

BEGIN_MESSAGE_MAP(FlameGraphPanel, CWnd)
	ON_WM_SIZE()
	ON_WM_SHOWWINDOW()
	ON_WM_PAINT()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSELEAVE()
END_MESSAGE_MAP()

FlameGraphPanel::FlameGraphPanel() :profiler(nullptr), totalSamples(0), maxDepth(0), hoveredNode(nullptr), lastBuildTime(0),
fontsCreated(false), backBufferW(0), backBufferH(0), backBufferDirty(true) {
	tooltipText.reserve(256);
}

FlameGraphPanel::~FlameGraphPanel() {
}

void FlameGraphPanel::ensureFonts() {
	if (fontsCreated) return;
	fontsCreated = true;

	LOGFONT lf = {};
	lf.lfHeight = -13;
	lf.lfWeight = FW_NORMAL;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
	lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lf.lfQuality = CLEARTYPE_QUALITY;
	lf.lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
	_tcscpy_s(lf.lfFaceName, _T("Segoe UI")); // sooooo sexy
	labelFont.CreateFontIndirect(&lf);

	LOGFONT lfHeader = lf;
	lfHeader.lfHeight = -14;
	lfHeader.lfWeight = 600; 
	headerFont.CreateFontIndirect(&lfHeader);
}

BOOL FlameGraphPanel::PreCreateWindow(CREATESTRUCT& cs) {
	static CString cls;
	if (cls.IsEmpty()) {
		cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, 0, (HBRUSH)(COLOR_WINDOW + 1), 0);
	}
	cs.lpszClass = cls;
	cs.style |= WS_CLIPCHILDREN;
	return CWnd::PreCreateWindow(cs);
}

void FlameGraphPanel::setProfiler(const Profiler* prof) {
	profiler = prof;
	lastBuildTime = 0;
	lastTreeSignature.clear();
	backBufferDirty = true;
	hoveredNode = nullptr;
	refresh();
}

void FlameGraphPanel::refresh() {
	if (!IsWindow(m_hWnd)) return;
	if (profiler) {
		DWORD now = GetTickCount();
		if (now - lastBuildTime > 500) {
			hoveredNode = nullptr;
			buildTree();
			lastBuildTime = now;
			std::string sig = computeTreeSignature();
			if (sig != lastTreeSignature) {
				lastTreeSignature = sig;
				backBufferDirty = true;
			}
		}
	}
	else {
		root.reset();
		totalSamples = 0;
		maxDepth = 0;
		hoveredNode = nullptr;
		if (!lastTreeSignature.empty()) {
			lastTreeSignature.clear();
			backBufferDirty = true;
		}
	}

	if (backBufferDirty || !tooltipText.empty()) {
		Invalidate(FALSE);
	}
}

void FlameGraphPanel::OnSize(UINT nType, int cx, int cy) {
	CWnd::OnSize(nType, cx, cy);
	GetClientRect(&clientRect);
	backBufferDirty = true;
	Invalidate(FALSE);
}

void FlameGraphPanel::OnShowWindow(BOOL bShow, UINT nStatus) {
	CWnd::OnShowWindow(bShow, nStatus);
	if (bShow) {
		lastBuildTime = 0;
		backBufferDirty = true;
		refresh();
	}
}

std::string FlameGraphPanel::computeTreeSignature() const {
	if (!root) return std::string();

	std::string sig;
	sig.reserve(1024);

	std::vector<const RectNode*> stack;
	stack.push_back(root.get());
	while (!stack.empty()) {
		const RectNode* n = stack.back();
		stack.pop_back();

		char buf[64];
		std::snprintf(buf, sizeof(buf), "|%d:", n->samples);
		sig += n->name;
		sig += buf;

		for (auto& c : n->children) stack.push_back(c.get());
	}
	return sig;
}

void FlameGraphPanel::renderToBackBuffer(CDC& refDC, const CRect& rect) {
	CDC memDC;
	memDC.CreateCompatibleDC(&refDC);

	if (backBufferW != rect.Width() || backBufferH != rect.Height() || !backBuffer.GetSafeHandle()) {
		if (backBuffer.GetSafeHandle()) backBuffer.DeleteObject();
		backBuffer.CreateCompatibleBitmap(&refDC, rect.Width(), rect.Height());
		backBufferW = rect.Width();
		backBufferH = rect.Height();
	}

	CBitmap* oldBmp = memDC.SelectObject(&backBuffer);

	COLORREF bg = prefs.rgb_bkgrnd;
	int bgR = GetRValue(bg), bgG = GetGValue(bg), bgB = GetBValue(bg);
	bool darkBg = (bgR * 299 + bgG * 587 + bgB * 114) / 1000 < 128;
	int washDelta = darkBg ? 6 : -6;
	const int bands = 8;
	for (int i = 0; i < bands; ++i) {
		int y0 = rect.top + (rect.Height() * i) / bands;
		int y1 = rect.top + (rect.Height() * (i + 1)) / bands;
		int shift = washDelta * i / bands;
		int r = std::clamp(bgR + shift, 0, 255);
		int g = std::clamp(bgG + shift, 0, 255);
		int b = std::clamp(bgB + shift, 0, 255);
		memDC.FillSolidRect(CRect(rect.left, y0, rect.right, y1), RGB(r, g, b));
	}

	if (!root || root->children.empty() || totalSamples <= 0) {
		CFont* oldFont = memDC.SelectObject(&headerFont);
		memDC.SetTextColor(prefs.rgb_default);
		memDC.SetBkMode(TRANSPARENT);
		memDC.DrawText(_T("No samples collected"), const_cast<CRect&>(rect), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		memDC.SelectObject(oldFont);
	}
	else {
		const int margin = 8;
		const int rowGap = 2;
		int yOffset = margin;
		int availableHeight = rect.Height() - yOffset - margin;
		int depthHeight = maxDepth > 0 ? availableHeight / (maxDepth + 1) : 24;
		if (depthHeight < 14) depthHeight = 14;

		int xPos = rect.left + margin;
		int totalWidth = rect.Width() - margin * 2;
		if (totalWidth < 1) totalWidth = 1;

		int remainingWidth = totalWidth;
		for (size_t i = 0; i < root->children.size(); ++i) {
			RectNode* child = root->children[i].get();
			int childWidth;
			if (i + 1 == root->children.size()) {
				childWidth = remainingWidth;
			}
			else {
				childWidth = (int)((double)child->samples / (double)totalSamples * totalWidth);
				if (childWidth < 1) childWidth = 1;
				if (childWidth > remainingWidth) childWidth = remainingWidth;
			}
			buildRectTree(child, 0, xPos, yOffset, childWidth, depthHeight - rowGap);
			xPos += childWidth;
			remainingWidth -= childWidth;
		}

		CFont* oldFont = memDC.SelectObject(&labelFont);
		for (auto& child : root->children) {
			drawNode(memDC, child.get(), 0);
		}
		memDC.SelectObject(oldFont);
	}

	memDC.SelectObject(oldBmp);
	backBufferDirty = false;
}

void FlameGraphPanel::OnPaint() {
	if (!IsWindow(m_hWnd)) return;
	ensureFonts();
	CPaintDC dc(this);
	CRect rect;
	GetClientRect(&rect);

	if (rect.Width() <= 0 || rect.Height() <= 0) return;

	if (backBufferDirty || backBufferW != rect.Width() || backBufferH != rect.Height()) {
		renderToBackBuffer(dc, rect);
	}

	CDC blitDC;
	blitDC.CreateCompatibleDC(&dc);
	CBitmap* oldBlitBmp = blitDC.SelectObject(&backBuffer);
	dc.BitBlt(0, 0, rect.Width(), rect.Height(), &blitDC, 0, 0, SRCCOPY);
	blitDC.SelectObject(oldBlitBmp);

	if (hoveredNode) {
		CPen pen(PS_SOLID, 2, RGB(255, 255, 255));
		CPen* oldPen = dc.SelectObject(&pen);
		CGdiObject* oldBrush = dc.SelectStockObject(NULL_BRUSH);
		dc.Rectangle(hoveredNode->rect.left, hoveredNode->rect.top, hoveredNode->rect.right, hoveredNode->rect.bottom);
		dc.SelectObject(oldBrush);
		dc.SelectObject(oldPen);
	}

	if (!tooltipText.empty()) {
		CRect ttRect(rect);
		CFont* oldFont = dc.SelectObject(&prefs.debugFont);
		dc.SetBkColor(prefs.rgb_bkgrnd);
		dc.SetTextColor(prefs.rgb_default);
		dc.SetBkMode(OPAQUE);
		dc.DrawText(tooltipText.c_str(), ttRect, DT_LEFT | DT_TOP | DT_NOPREFIX);
		dc.SelectObject(oldFont);
	}
}

void FlameGraphPanel::buildTree() {
	if (!profiler) {
		root.reset();
		totalSamples = 0;
		maxDepth = 0;
		return;
	}

	auto samples = profiler->getStackSamples(); // copy
	if (samples.empty()) {
		root.reset();
		totalSamples = 0;
		maxDepth = 0;
		return;
	}

	root.reset(new RectNode("root", 0));
	totalSamples = 0;
	maxDepth = 0;

	for (const auto& stack : samples) {
		RectNode* current = root.get();
		int depth = 0;
		for (const std::string& func : stack) {
			RectNode* child = nullptr;
			for (auto& c : current->children) {
				if (c->name == func) {
					child = c.get();
					break;
				}
			}
			if (!child) {
				std::unique_ptr<RectNode> newNode(new RectNode(func, 0));
				newNode->parent = current;
				child = newNode.get();
				current->children.push_back(std::move(newNode));
			}
			current = child;
			++depth;
		}
		if (depth > maxDepth) maxDepth = depth;
		++current->selfSamples;
		++totalSamples;
	}

	std::vector<RectNode*> postorder;
	std::vector<RectNode*> toVisit;
	toVisit.push_back(root.get());
	while (!toVisit.empty()) {
		RectNode* n = toVisit.back();
		toVisit.pop_back();
		postorder.push_back(n);
		for (auto& c : n->children) toVisit.push_back(c.get());
	}
	for (auto it = postorder.rbegin(); it != postorder.rend(); ++it) {
		RectNode* n = *it;
		int inclusive = n->selfSamples;
		for (auto& c : n->children) inclusive += c->samples;
		n->samples = inclusive;
	}

	std::vector<RectNode*> stack;
	stack.push_back(root.get());
	while (!stack.empty()) {
		RectNode* n = stack.back();
		stack.pop_back();
		std::sort(n->children.begin(), n->children.end(),
			[](const std::unique_ptr<RectNode>& a, const std::unique_ptr<RectNode>& b) {
				if (a->samples != b->samples) return a->samples > b->samples;
				return a->name < b->name;
			});
		for (auto& c : n->children) stack.push_back(c.get());
	}
}

void FlameGraphPanel::buildRectTree(RectNode* node, int depth, int& x, int y, int width, int rowHeight) {
	node->rect = CRect(x, y, x + width, y + rowHeight);
	if (node->children.empty()) return;

	int childTotal = 0;
	for (auto& c : node->children) childTotal += c->samples;
	if (childTotal <= 0) return;

	int childX = x;
	int remainingWidth = width;
	for (size_t i = 0; i < node->children.size(); ++i) {
		RectNode* child = node->children[i].get();
		int childWidth;
		if (i + 1 == node->children.size()) {
			childWidth = remainingWidth;
		}
		else {
			childWidth = (int)((double)child->samples / (double)childTotal * width);
			if (childWidth < 1) childWidth = 1;
			if (childWidth > remainingWidth) childWidth = remainingWidth;
		}
		buildRectTree(child, depth + 1, childX, y + rowHeight, childWidth, rowHeight);
		childX += childWidth;
		remainingWidth -= childWidth;
	}
}

COLORREF FlameGraphPanel::getColor(const std::string& name) {
	auto it = colorMap.find(name);
	if (it != colorMap.end()) return it->second;
	unsigned int hash = 2166136261u; 
	for (unsigned char c : name) {
		hash ^= c;
		hash *= 16777619u;
	}
	int hue = hash % 360;

	int r, g, b;
	double h = hue / 360.0;
	double s = 0.55, v = 0.85;
	int i = (int)(h * 6);
	double f = h * 6 - i;
	double p = v * (1 - s);
	double q = v * (1 - f * s);
	double t = v * (1 - (1 - f) * s);
	switch (i % 6) {
	case 0: r = (int)(v * 255); g = (int)(t * 255); b = (int)(p * 255); break;
	case 1: r = (int)(q * 255); g = (int)(v * 255); b = (int)(p * 255); break;
	case 2: r = (int)(p * 255); g = (int)(v * 255); b = (int)(t * 255); break;
	case 3: r = (int)(p * 255); g = (int)(q * 255); b = (int)(v * 255); break;
	case 4: r = (int)(t * 255); g = (int)(p * 255); b = (int)(v * 255); break;
	default: r = (int)(v * 255); g = (int)(p * 255); b = (int)(q * 255); break;
	}
	COLORREF color = RGB(r, g, b);
	colorMap[name] = color;
	return color;
}

void FlameGraphPanel::drawShadedRect(CDC& dc, const CRect& r, COLORREF base) {
	int br = GetRValue(base), bg = GetGValue(base), bb = GetBValue(base);
	auto lighten = [](int c, int amt) { return std::clamp(c + amt, 0, 255); };

	COLORREF top = RGB(lighten(br, 18), lighten(bg, 18), lighten(bb, 18));
	COLORREF bottom = RGB(lighten(br, -14), lighten(bg, -14), lighten(bb, -14));

	int h = r.Height();
	int thirdH = (std::max)(1, h / 3);
	dc.FillSolidRect(CRect(r.left, r.top, r.right, r.top + thirdH), top);
	dc.FillSolidRect(CRect(r.left, r.top + thirdH, r.right, r.bottom - thirdH), base);
	dc.FillSolidRect(CRect(r.left, r.bottom - thirdH, r.right, r.bottom), bottom);

	CPen pen(PS_SOLID, 1, RGB(lighten(br, -35), lighten(bg, -35), lighten(bb, -35)));
	CPen* oldPen = dc.SelectObject(&pen);
	CGdiObject* oldBrush = dc.SelectStockObject(NULL_BRUSH);
	dc.Rectangle(r.left, r.top, r.right, r.bottom);
	dc.SelectObject(oldBrush);
	dc.SelectObject(oldPen);
}

void FlameGraphPanel::drawNode(CDC& dc, RectNode* node, int depth) {
	CRect r = node->rect;
	if (r.Width() < 2) r.right = r.left + 2;
	if (r.Height() < 2) r.bottom = r.top + 2;

	COLORREF color = getColor(node->name);
	drawShadedRect(dc, r, color);

	if (r.Width() > 28 && r.Height() >= 12) {
		int luma = (GetRValue(color) * 299 + GetGValue(color) * 587 + GetBValue(color) * 114) / 1000;
		dc.SetTextColor(luma > 140 ? RGB(20, 20, 20) : RGB(245, 245, 245));
		dc.SetBkMode(TRANSPARENT);
		CRect textRect(r);
		textRect.DeflateRect(4, 1);

		char countBuf[32];
		std::snprintf(countBuf, sizeof(countBuf), " (%d)", node->samples);
		std::string withCount = node->name + countBuf;

		CSize fullSize = dc.GetTextExtent(CString(withCount.c_str()));
		CString label = (fullSize.cx <= textRect.Width()) ? CString(withCount.c_str()) : CString(node->name.c_str());

		dc.DrawText(label, textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
	}
	for (auto& child : node->children) {
		drawNode(dc, child.get(), depth + 1);
	}
}

void FlameGraphPanel::OnMouseMove(UINT nFlags, CPoint point) {
	CWnd::OnMouseMove(nFlags, point);
	hoverPos = point;
	showTooltip(point);

	TRACKMOUSEEVENT tme = { sizeof(tme) };
	tme.dwFlags = TME_LEAVE;
	tme.hwndTrack = m_hWnd;
	TrackMouseEvent(&tme);
}

void FlameGraphPanel::OnMouseLeave() {
	hideTooltip();
}

FlameGraphPanel::RectNode* FlameGraphPanel::hitTest(const CPoint& pt) const {
	if (!root) return nullptr;

	RectNode* current = root.get();
	RectNode* found = nullptr;
	while (current) {
		bool hitChild = false;
		for (auto& child : current->children) {
			if (child->rect.PtInRect(pt)) {
				found = child.get();
				current = child.get();
				hitChild = true;
				break;
			}
		}
		if (!hitChild) break;
	}
	return found;
}

void FlameGraphPanel::showTooltip(const CPoint& pt) {
	if (!root || totalSamples <= 0) return;

	RectNode* found = hitTest(pt);

	std::string newTooltip;
	if (found) {
		char buf[256];
		std::snprintf(buf, sizeof(buf), "%s: %d samples (%.1f%% total, %.1f%% self)", found->name.c_str(), found->samples, 100.0 * (double)found->samples / (double)totalSamples, 100.0 * (double)found->selfSamples / (double)totalSamples);
		newTooltip = buf;
	}

	bool needRepaint = false;
	if (found != hoveredNode) {
		hoveredNode = found;
		needRepaint = true;
	}
	if (newTooltip != tooltipText) {
		tooltipText = newTooltip;
		needRepaint = true;
	}
	if (needRepaint) {
		Invalidate(FALSE);
	}
}

void FlameGraphPanel::hideTooltip() {
	bool needRepaint = false;
	if (!tooltipText.empty()) {
		tooltipText.clear();
		needRepaint = true;
	}
	if (hoveredNode) {
		hoveredNode = nullptr;
		needRepaint = true;
	}
	if (needRepaint) {
		Invalidate(FALSE);
	}
}
