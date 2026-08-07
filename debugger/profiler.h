#ifndef PROFILER_H
#define PROFILER_H

#include <map>
#include <string>
#include <vector>

struct ProfileStats {
	__int64 totalTicks;   // inclusive
	__int64 selfTicks;    // exclusive
	int     callCount;
	__int64 maxTicks;
	int     netObjDelta;
	int     netStrDelta;
	int     maxObjDelta;   // biggest single call to spot spikes
	ProfileStats() :totalTicks(0), selfTicks(0), callCount(0), maxTicks(0), netObjDelta(0), netStrDelta(0), maxObjDelta(0) {}
};

struct ProfileFrame {
	std::string func;
	__int64 enterTicks;
	__int64 childTicks;
	int enterObjCnt, enterStrCnt;
	ProfileFrame(const std::string& f, __int64 t) :func(f), enterTicks(t), childTicks(0), enterObjCnt(0), enterStrCnt(0) {}
};

struct MemSample {
	int msecs;
	int objCnt;
	int unrelObjCnt;
	int stringCnt;
	__int64 workingSetBytes;
};

class Profiler {
	std::map<std::string, ProfileStats> stats;
	std::vector<ProfileFrame> stack;
	__int64 freq;
	__int64 startTicks;
	std::vector<MemSample> memHistory;
	int lastMemSampleMs;
	__int64 lastWorkingSetBytes;
	std::vector<std::vector<std::string>> stackSamples;
	int sampleIntervalMs;

public:
	Profiler();

	bool enabled;

	void reset();
	void enter(const std::string& func);
	void leave();

	void noteEnterAlloc(int objCnt, int stringCnt);
	void noteLeaveAlloc(int objCnt, int stringCnt);

	void resyncStack();

	void sampleMemory(int objCnt, int unrelObjCnt, int stringCnt, __int64 workingSetBytes);

	__int64 nowTicks()const;
	double ticksToMs(__int64 ticks)const { return freq ? (double)ticks * 1000.0 / (double)freq : 0.0; }

	const std::map<std::string, ProfileStats>& results()const { return stats; }
	const std::vector<MemSample>& memoryHistory()const { return memHistory; }
	__int64 currentWorkingSetBytes()const { return lastWorkingSetBytes; }
	int currentDepth()const { return (int)stack.size(); }

	__int64 totalSelfTicks()const {
		__int64 sum = 0;
		for(std::map<std::string, ProfileStats>::const_iterator it = stats.begin(); it != stats.end(); ++it) {
			sum += it->second.selfTicks;
		}
		return sum;
	}

	int totalPositiveNetObjDelta()const {
		int sum = 0;
		for(std::map<std::string, ProfileStats>::const_iterator it = stats.begin(); it != stats.end(); ++it) {
			if(it->second.netObjDelta > 0) sum += it->second.netObjDelta;
		}
		return sum;
	}

	void sampleStack();
	void clearSamples();
	const std::vector<std::vector<std::string>>& getStackSamples() const { return stackSamples; }
};

#endif
