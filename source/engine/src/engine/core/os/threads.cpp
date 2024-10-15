#include "threads.h"

#include <latch>
#include <thread>

#include "core/framework/asset_manager.h"
#include "core/io/print.h"
#include "core/systems/job_system.h"
#include "drivers/windows/win32_prerequisites.h"

namespace my::thread {

struct ThreadObject {
    const char* name;
    ThreadMainFunc thread_func;
    uint32_t id;
    std::thread thread;
};

static thread_local uint32_t g_thread_id;
static struct
{
    std::atomic_bool shutdown_requested;
    std::array<ThreadObject, THREAD_MAX> threads = {
        ThreadObject{ "main" },
        ThreadObject{ "render thread", []() {} },
        ThreadObject{ "asset thread 1", AssetManager::workerMain },
        // ThreadObject{ "asset thread 2", AssetManager::worker_main },
        ThreadObject{ "js worker 0", jobsystem::workerMain },
        ThreadObject{ "js worker 1", jobsystem::workerMain },
        ThreadObject{ "js worker 2", jobsystem::workerMain },
        ThreadObject{ "js worker 3", jobsystem::workerMain },
        ThreadObject{ "js worker 4", jobsystem::workerMain },
        ThreadObject{ "js worker 5", jobsystem::workerMain },
        ThreadObject{ "js worker 6", jobsystem::workerMain },
        ThreadObject{ "js worker 7", jobsystem::workerMain },
    };
} s_thread_glob;

bool initialize() {
    g_thread_id = THREAD_MAIN;

    std::latch latch{ THREAD_MAX - 1 };

    for (uint32_t id = THREAD_MAIN + 1; id < THREAD_MAX; ++id) {
        ThreadObject& thread = s_thread_glob.threads[id];
        thread.id = id;
        thread.thread = std::thread(
            [&](ThreadObject* object) {
                // set thread id
                g_thread_id = object->id;

                latch.count_down();
                LOG_VERBOSE("[threads] thread '{}'(id: {}) starts.", object->name, object->id);
                object->thread_func();
                LOG_VERBOSE("[threads] thread '{}'(id: {}) ends.", object->name, object->id);
            },
            &thread);

        HANDLE handle = (HANDLE)thread.thread.native_handle();

        // @TODO: set thread affinity
        // DWORD_PTR affinityMask = 1ull << threadID;
        // DWORD_PTR affinityResult = SetThreadAffinityMask(handle, affinityMask);

        std::string name = thread.name;
        std::wstring wname(name.begin(), name.end());
        HRESULT hr = SetThreadDescription(handle, wname.c_str());
        DEV_ASSERT(!FAILED(hr));
    }

    latch.wait();
    return true;
}

void finailize() {
    for (uint32_t id = THREAD_MAIN + 1; id < THREAD_MAX; ++id) {
        auto& thread = s_thread_glob.threads[id];
        thread.thread.join();
    }
}

bool shutdownRequested() {
    return s_thread_glob.shutdown_requested;
}

void requestShutdown() {
    s_thread_glob.shutdown_requested = true;
}

bool isMainThread() {
    return g_thread_id == THREAD_MAIN;
}

uint32_t getThreadId() {
    return g_thread_id;
}

}  // namespace my::thread
