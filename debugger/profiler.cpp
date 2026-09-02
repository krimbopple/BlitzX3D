#include "stdafx.h"
#include "profiler.h"
#include <psapi.h>

Profiler::Profiler() :freq(0), startTicks(0), lastMemSampleMs(0), lastWorkingSetBytes(0), enabled(true), sampleWindowSeconds(30) {
	LARGE_INTEGER f;
	QueryPerformanceFrequency(&f);
	freq = f.QuadPart;
}

__int64 Profiler::nowTicks()const {
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	return t.QuadPart;
}

void Profiler::reset() {
	stats.clear();
	stack.clear();
	memHistory.clear();
	lastMemSampleMs = 0;
	lastWorkingSetBytes = 0;
	startTicks = nowTicks();
	clearSamples();
}

void Profiler::enter(const std::string& func) {
	if(!enabled) return;
	stack.push_back(ProfileFrame(func, nowTicks()));
	sampleStack();
}

void Profiler::leave() {
	if(!enabled || stack.empty()) return;

	ProfileFrame f = stack.back();
	stack.pop_back();

	__int64 now = nowTicks();
	__int64 total = now - f.enterTicks;
	__int64 self = total - f.childTicks;
	if(self < 0) self = 0;

	if(!stack.empty()) stack.back().childTicks += total;

	ProfileStats& s = stats[f.func];
	s.totalTicks += total;
	s.selfTicks += self;
	++s.callCount;
	if(total > s.maxTicks) s.maxTicks = total;

	sampleStack();
}

void Profiler::resyncStack() {
	stack.clear();
}

void Profiler::sampleMemory(int objCnt, int unrelObjCnt, int stringCnt, __int64 workingSetBytes) {
	lastWorkingSetBytes = workingSetBytes;

	if(!enabled) return;

	int ms = (int)ticksToMs(nowTicks() - startTicks);

	if(ms - lastMemSampleMs < 50 && !memHistory.empty()) return;
	lastMemSampleMs = ms;

	MemSample s;
	s.msecs = ms;
	s.objCnt = objCnt;
	s.unrelObjCnt = unrelObjCnt;
	s.stringCnt = stringCnt;
	s.workingSetBytes = workingSetBytes;
	memHistory.push_back(s);
}

void Profiler::sampleStack() {
	if (!enabled) return;
	if (stack.empty()) return;

	std::vector<std::string> sample;
	sample.reserve(stack.size());
	for (const auto& f : stack) {
		sample.push_back(f.func);
	}
	__int64 now = nowTicks();
	__int64 sampleTimeMs = ticksToMs(now - startTicks);
	stackSamples.push_back(std::make_pair(sampleTimeMs, std::move(sample)));

	const size_t maxStackSamples = 20000;
	if (stackSamples.size() > maxStackSamples) {
		stackSamples.pop_front();
	}
}

void Profiler::clearSamples() {
	stackSamples.clear();
}
