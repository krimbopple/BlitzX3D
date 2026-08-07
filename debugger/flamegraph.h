#ifndef FLAMEGRAPH_H
#define FLAMEGRAPH_H

#include "stdafx.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

class Profiler;

class FlameGraphPanel : public CWnd {
	DECLARE_DYNAMIC(FlameGraphPanel)
public:
	FlameGraphPanel();
	virtual ~FlameGraphPanel();

	void setProfiler(const Profiler* prof);
	void refresh();

protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnPaint();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnMouseLeave();

	DECLARE_MESSAGE_MAP()

private:
	struct RectNode {
		std::string name;
		int samples;
		int selfSamples;
		CRect rect;
		std::vector<std::unique_ptr<RectNode>> children;
		RectNode* parent;
		RectNode(const std::string& n, int s) : name(n), samples(s), selfSamples(0), parent(nullptr) {}
	};

	const Profiler* profiler;
	std::unique_ptr<RectNode> root;
	CRect clientRect;
	int totalSamples;
	int maxDepth;
	std::unordered_map<std::string, COLORREF> colorMap;
	CPoint hoverPos;
	RectNode* hoveredNode;
	std::string tooltipText;
	DWORD lastBuildTime;
	CFont labelFont;
	CFont headerFont;
	bool fontsCreated;

	CBitmap backBuffer;
	int backBufferW, backBufferH;
	bool backBufferDirty;
	std::string lastTreeSignature;

	void buildTree();
	void buildRectTree(RectNode* node, int depth, int& x, int y, int width, int rowHeight);
	COLORREF getColor(const std::string& name);
	void drawNode(CDC& dc, RectNode* node, int depth);
	void showTooltip(const CPoint& pt);
	void hideTooltip();
	void renderToBackBuffer(CDC& refDC, const CRect& rect);
	std::string computeTreeSignature() const;
	RectNode* hitTest(const CPoint& pt) const;
	void ensureFonts();
	void drawShadedRect(CDC& dc, const CRect& r, COLORREF base);
};

#endif
