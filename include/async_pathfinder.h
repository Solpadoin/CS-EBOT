#ifndef ASYNC_PATHFINDER_H
#define ASYNC_PATHFINDER_H

#include "core.h"
#include "tinythread.h"

struct ClientCache
{
	bool active{false};
	bool alive{false};
	int team{-1};
	Vector origin{0.0f, 0.0f, 0.0f};
};

struct BotCache
{
	bool active{false};
	bool alive{false};
	int team{-1};
	bool isZombie{false};
	int personality{0};
	float gravity{1.0f};
	Vector origin{0.0f, 0.0f, 0.0f};
};

struct WaypointCache
{
	bool fallCheckPassed{true};
	bool visibleConnection[8]{true, true, true, true, true, true, true, true};
};

// Thread-safe replicated cache
extern ClientCache g_clientCache[33];
extern BotCache g_botCache[33];
extern WaypointCache g_waypointCache[Const_MaxWaypoints];

// Job structures
struct PathJob
{
	int botIndex;
	int16_t srcIndex;
	int16_t destIndex;
	bool isZombie;
	int personality;
	float gravity;
	int team;
	bool isHumanCampPath;
	int16_t lastDeclineWaypoint;
	bool forceShortest;

	// Output path
	CArray<int16_t> pathResult;
	bool completed{false};
	bool canceled{false};
};

class AsyncPathfinder
{
private:
	tthread::mutex m_mutex;
	tthread::condition_variable m_cv;
	tthread::thread* m_thread{nullptr};
	CArray<PathJob*> m_queue;
	PathJob* m_activeJobs[33]{nullptr}; // Active job per bot
	bool m_running{false};

	static void ThreadFunc(void* arg);

public:
	AsyncPathfinder(void);
	~AsyncPathfinder(void);

	void Start(void);
	void Stop(void);

	void RequestPath(int botIndex, int16_t srcIndex, int16_t destIndex, bool isZombie, int personality, float gravity, int team, bool isHumanCampPath, int16_t lastDeclineWaypoint, bool forceShortest);
	bool IsPathReady(int botIndex);
	CArray<int16_t> GetPath(int botIndex);

	PathJob* PopJob(void);
};

extern AsyncPathfinder g_asyncPathfinder;

#endif
