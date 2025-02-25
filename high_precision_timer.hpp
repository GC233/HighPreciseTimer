#include <iostream>
#include <windows.h>
#include <mmsystem.h>
#include <functional>
#include <vector>
#pragma comment(lib,"winmm.lib")

// 定义 1ms 和 2s 时钟间隔，以 ms 为单位
#define ONE_MILLI_SECOND 1

// 定义时钟分辨率，以 ms 为单位
#define TIMER_ACCURACY 1

// 高精度定时器类
class HighPrecisionTimer {
public:
    // 构造函数
    HighPrecisionTimer() : m_mmTimerId(0), m_wAccuracy(0), m_isRunning(false), m_generalCallback(nullptr) {}

    // 析构函数，确保资源释放
    ~HighPrecisionTimer() {
        FreeTimer();
    }

    // 注册普通函数，注意，std::apply需要c++17
    template<typename... Args>
    bool RegisterFunction(void(*func)(Args...), UINT interval, Args... args) {
        if (m_isRunning) {
            std::cout << "Timer is already running. Cannot register new function." << std::endl;
            return false;
        }
        auto argsTuple = std::make_tuple(args...);
        m_generalCallback = [func, argsTuple]() {
            std::apply(func, argsTuple); //将argsTuple打包交给func执行
            };
        m_callback = &HighPrecisionTimer::TimerCallback;
        m_interval = interval;
        return true;
    }

    // 注册类成员函数，这个没必要加入参数
    template<typename T>
    bool RegisterMemberFunction(T& obj, void (T::* func)(), UINT interval) {
        if (m_isRunning) {
            std::cout << "Timer is already running. Cannot register new function." << std::endl;
            return false;
        }
        m_generalCallback = [&obj, func]() {
            (obj.*func)();
            };
        m_callback = &HighPrecisionTimer::TimerCallback;
        m_interval = interval;
        return true;
    }

    // 初始化并启动定时器
    bool Start() {
        if (m_isRunning) {
            std::cout << "Timer is already running." << std::endl;
            return false;
        }

        TIMECAPS tc;
        // 获取性能计数器的频率
        QueryPerformanceFrequency(&m_xliPerfFreq);
        // 记录性能计数器的起始时间
        QueryPerformanceCounter(&m_xliPerfStart);
        m_lastTriggerTime = m_xliPerfStart;

        // 记录当前的时间戳
        m_nTicket = GetTickCount();

        // 利用函数 timeGetDevCaps 取出系统分辨率的取值范围，如果无错则继续
        if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) == TIMERR_NOERROR) {
            // 分辨率的值不能超出系统的取值范围，取合适的精度
            m_wAccuracy = min(max(tc.wPeriodMin, TIMER_ACCURACY), tc.wPeriodMax);

            // 调用 timeBeginPeriod 函数设置定时器的分辨率
            timeBeginPeriod(m_wAccuracy);

            // 设定定时器，使用 TIME_PERIODIC 表示周期性触发
            m_mmTimerId = timeSetEvent(m_interval, 0, m_callback, reinterpret_cast<DWORD_PTR>(this), TIME_PERIODIC);
            // 如果定时器设置失败
            if (m_mmTimerId == 0) {
                // 输出错误信息
                std::cout << "timeSetEvent failed: " << GetLastError() << std::endl;
                return false;
            }

            m_isRunning = true;
            return true;
        }

        return false;
    }

    // 停止定时器任务
    void Stop() {
        if (m_mmTimerId != 0) {
            timeKillEvent(m_mmTimerId);
            m_mmTimerId = 0;
            m_isRunning = false;
        }
    }

    // 释放定时器资源
    void FreeTimer() {
        Stop();
        if (m_wAccuracy != 0) {
            timeEndPeriod(m_wAccuracy);
            m_wAccuracy = 0;
        }
    }

private:
    static void CALLBACK TimerCallback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
        HighPrecisionTimer* timer = reinterpret_cast<HighPrecisionTimer*>(dwUser);
        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);
        double elapsedTime = (currentTime.QuadPart - timer->m_lastTriggerTime.QuadPart) * 1000.0 / timer->m_xliPerfFreq.QuadPart;
        std::cout << "Time interval since last trigger: " << elapsedTime << " ms" << std::endl;
        timer->m_lastTriggerTime = currentTime;
        timer->m_generalCallback();
    }

    MMRESULT m_mmTimerId;  // 存储定时器的 ID
    UINT m_wAccuracy;      // 存储定时器的精度
    LARGE_INTEGER m_xliPerfFreq = { 0 };  // 存储性能计数器的频率
    LARGE_INTEGER m_xliPerfStart = { 0 }; // 存储性能计数器的起始时间
    LARGE_INTEGER m_lastTriggerTime = { 0 }; // 存储上一次触发的时间
    DWORD m_nTicket = 0;     // 记录上次计时的起始时间戳
    LPTIMECALLBACK m_callback; // 回调函数指针
    std::function<void()> m_generalCallback; // 通用回调函数
    UINT m_interval;         // 定时器间隔
    bool m_isRunning;        // 定时器是否正在运行
};

// 普通函数示例
void TestFunction(int a) {
    std::cout << "TestFunction is called." << a << std::endl;
}

// 测试类
class TestClass {
public:
    void TestMemberFunction() {
        std::cout << "TestMemberFunction is called." << std::endl;
    }
};

int main() {
    HighPrecisionTimer timer;

    // 显式构造 std::function 对象
    //std::function<void(int)> func = TestFunction;
    // 注册普通函数
    timer.RegisterFunction(TestFunction, 10, 100);
    // 启动定时器
    if (timer.Start()) {
        std::cout << "Timer started successfully." << std::endl;
        // 让主线程等待一段时间，确保定时器事件有足够的时间执行
        Sleep(50);
        // 停止定时器
        timer.Stop();
        std::cout << "Timer stopped." << std::endl;
    }
    else {
        std::cout << "Timer start failed." << std::endl;
    }

    // 注册类成员函数
    TestClass testObj;
    timer.RegisterMemberFunction(testObj, &TestClass::TestMemberFunction, 5); //带参数绑定
    // 再次启动定时器
    if (timer.Start()) {
        std::cout << "Timer started successfully." << std::endl;
        // 让主线程等待一段时间，确保定时器事件有足够的时间执行
        Sleep(50);
        // 释放定时器资源
        timer.FreeTimer();
        std::cout << "Timer resources freed." << std::endl;
    }
    else {
        std::cout << "Timer start failed." << std::endl;
    }

    return 0;
}