#include <graphics.h>
#include <conio.h>
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <locale>
#include <fstream>
#include <functional>
#include <mmsystem.h> 
#pragma comment(lib, "Winmm.lib")  // 链接Winmm.lib库

using namespace std;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//结构体发动机
enum class EnginePhase { STOPPED, STARTING, STABLE, STOPPING };
struct EngineData {
    double N, N1, T, C, V, runTime;
    bool sensorN1Valid, sensorN2Valid, sensorEGTValid, sensorEGT2Valid, sensorFuelValid;
    bool dualN1Failure, dualEGTFailure;
    EngineData() : N(0.0), N1(0.0), T(20.0), C(20000.0), V(0.0), runTime(0.0),
        sensorN1Valid(true), sensorN2Valid(true), sensorEGTValid(true),
        sensorEGT2Valid(true), sensorFuelValid(true),
        dualN1Failure(false), dualEGTFailure(false) {
    }
    void reset() { *this = EngineData(); }
    //计算N1
    void calculateN1() {
        N1 = (N / 40000.0) * 100.0;
        if (N1 < 0.0) N1 = 0.0;
        if (N1 > 125.0) N1 = 125.0;
    }
};

//结构体异常
struct AnomalyInfo {
    wstring message;
    int level, anomalyId;
    bool requiresShutdown;
    double displayTime;
    AnomalyInfo(const wstring& msg = L"", int lvl = 0, bool shutdown = false,
        int id = 0, double time = 0.0) :
        message(msg), level(lvl), requiresShutdown(shutdown),
        anomalyId(id), displayTime(time) {
    }
};
namespace EngineConstants {
    const double RATED_SPEED = 40000.0;
    const double MAX_N1 = 125.0;
    const double STABLE_N1_THRESHOLD = 95.0;
    const double MIN_TEMP = -5.0;
    const double MAX_TEMP = 1200.0;
    const double INITIAL_TEMP = 20.0;
    const double MAX_FUEL = 20000.0;
    const double MAX_FUEL_FLOW = 55.0;
    const double LOW_FUEL_THRESHOLD = 1000.0;
    const double OVER_SPEED_1 = 105.0;
    const double OVER_SPEED_2 = 120.0;
    const double OVER_TEMP_START_1 = 850.0;
    const double OVER_TEMP_START_2 = 1000.0;
    const double OVER_TEMP_STABLE_1 = 950.0;
    const double OVER_TEMP_STABLE_2 = 1100.0;
    const double START_LINEAR_TIME = 2.0;
    const double STOP_DURATION = 10.0; 
}
enum class ColorLevel { NORMAL, WARNING, DANGER, INVALID };

// 工具函数
class MathUtils {
public:
    static void initRandom() { srand(static_cast<unsigned int>(time(nullptr))); }
    static double random(double min, double max) {
        return min + (max - min) * (rand() / static_cast<double>(RAND_MAX));
    }
    static double addNoise(double v, double pct) {
        if (v == 0.0) return 0.0;
        return v + v * pct * (2.0 * rand() / static_cast<double>(RAND_MAX) - 1.0);
    }
    static double smooth(double cur, double target, double a = 0.1) {
        return cur + (target - cur) * a;
    }
    static double clamp(double v, double min, double max) {
        if (v < min) return min;
        if (v > max) return max;
        return v;
    }
    static double log10Growth(double coeff, double t, double offset) {
        if (t <= offset) return 0.0;
        return coeff * log10(t - offset);
    }
    static double expDecay(double init, double decayRate, double t) {
        if (decayRate <= 0.0) return init;
        return init * exp(-decayRate * t);
    }
};

//基类图形
class Gauge {
protected:
    int cx, cy, radius;
    static double degToRad(double deg) { return deg * M_PI / 180.0; }
public:
    Gauge(int x, int y, int r) : cx(x), cy(y), radius(r) {}
    virtual void draw(double value, double minV, double maxV,
        ColorLevel level, const wchar_t* label,
        const wchar_t* unit = L"") = 0;
};
class SectorGauge : public Gauge {
    double startAng, totalAng, ptrStartAng;
    COLORREF getColor(ColorLevel l) {
        switch (l) {
        case ColorLevel::NORMAL: return RGB(255, 255, 255);
        case ColorLevel::WARNING: return RGB(255, 191, 0);
        case ColorLevel::DANGER: return RGB(255, 0, 0);
        default: return RGB(128, 128, 128); // INVALID为灰色
        }
    }
public:
    SectorGauge(int x, int y, int r, double sa = 135.0, double ta = 210.0, double psa = 135.0)
        : Gauge(x, y, r), startAng(sa), totalAng(ta), ptrStartAng(psa) {
    }
    void draw(double value, double minV, double maxV, ColorLevel level,
        const wchar_t* label, const wchar_t* unit = L"") override {
        double ratio = (value - minV) / (maxV - minV);
        if (ratio < 0.0) ratio = 0.0;
        if (ratio > 1.0) ratio = 1.0;
        double ptrAngle = ptrStartAng - ratio * 190.0;
        COLORREF color = getColor(level);
        // 绘制表盘
        setlinecolor(RGB(60, 60, 70));
        setlinestyle(PS_SOLID, 3);
        circle(cx, cy, radius);
        for (int i = 0; i <= 21; i++) {
            double ang = ptrStartAng - 10.0 * i;
            double rad = degToRad(ang);
            int x1 = cx + static_cast<int>((radius - 15) * cos(rad));
            int y1 = cy - static_cast<int>((radius - 15) * sin(rad));
            int x2 = cx + static_cast<int>((radius - 5) * cos(rad));
            int y2 = cy - static_cast<int>((radius - 5) * sin(rad));
            setlinecolor(RGB(220, 220, 230));
            setlinestyle(PS_SOLID, 2);
            line(x1, y1, x2, y2);
        }
        if (level != ColorLevel::INVALID) {
            double rad = degToRad(ptrAngle);
            int px = cx + static_cast<int>((radius - 12) * cos(rad));
            int py = cy - static_cast<int>((radius - 12) * sin(rad));
            setlinecolor(color);
            setlinestyle(PS_SOLID, 3);
            line(cx, cy, px, py);
            setfillcolor(color);
            setlinecolor(color);
            fillcircle(px, py, 3);
        }
        setfillcolor(RGB(180, 180, 190));
        setlinecolor(RGB(100, 100, 110));
        fillcircle(cx, cy, 4);
        settextcolor(RGB(200, 200, 210));
        settextstyle(20, 0, L"宋体");
        int lw = textwidth(label);
        outtextxy(cx - lw / 2, cy - radius - 30, label);
        wchar_t valStr[32];
        if (level == ColorLevel::INVALID) {
            swprintf_s(valStr, 32, L"--"); // 传感器失效时显示"--"
        }
        else {
            swprintf_s(valStr, 32, L"%.1f", value);
        }
        settextcolor(color);
        settextstyle(28, 0, L"Arial");
        int vw = textwidth(valStr);
        outtextxy(cx - vw / 2, cy - 10, valStr);
        if (unit && unit[0] != L'\0') {
            settextstyle(14, 0, L"宋体");
            outtextxy(cx + vw / 2 + 5, cy + 5, unit);
        }
    }
};

//按钮基类
class Button {
    int x, y, w, h;
    wstring text;
    COLORREF bg, txtColor, hoverColor;
    bool hovered;
public:
    Button(int xp, int yp, int wd, int ht, const wchar_t* txt,
        COLORREF bgc = RGB(70, 130, 180),
        COLORREF txtc = RGB(255, 255, 255),
        COLORREF hov = RGB(100, 149, 237)) :
        x(xp), y(yp), w(wd), h(ht), text(txt), bg(bgc), txtColor(txtc),
        hoverColor(hov), hovered(false) {
    }
    void draw() {
        COLORREF curBg = hovered ? hoverColor : bg;
        setfillcolor(RGB(20, 20, 25));
        setlinecolor(RGB(0, 0, 0));
        setlinestyle(PS_SOLID, 1);
        solidrectangle(x + 2, y + 2, x + w + 2, y + h + 2);
        setfillcolor(curBg);
        solidrectangle(x, y, x + w, y + h);
        setlinecolor(RGB(
            min(GetRValue(curBg) + 80, 255),
            min(GetGValue(curBg) + 80, 255),
            min(GetBValue(curBg) + 80, 255)
        ));
        setlinestyle(PS_SOLID, 2);
        line(x, y, x + w, y);
        line(x, y, x, y + h);
        setlinecolor(RGB(
            max(GetRValue(curBg) - 60, 0),
            max(GetGValue(curBg) - 60, 0),
            max(GetBValue(curBg) - 60, 0)
        ));
        line(x + w, y, x + w, y + h);
        line(x, y + h, x + w, y + h);
        settextcolor(txtColor);
        settextstyle(14, 0, L"Arial");
        setbkmode(TRANSPARENT);
        int tw = textwidth(text.c_str());
        int th = textheight(text.c_str());
        int tx = x + (w - tw) / 2;
        int ty = y + (h - th) / 2;
        settextcolor(RGB(50, 50, 50));
        outtextxy(tx + 1, ty + 1, text.c_str());
        settextcolor(txtColor);
        outtextxy(tx, ty, text.c_str());
    }
    bool inside(int mx, int my) const {
        return mx >= x && mx <= x + w && my >= y && my <= y + h;
    }
    void setHover(bool h) { hovered = h; }
    void setBg(COLORREF c) { bg = c; }
    void setText(const wchar_t* t) { text = t; }
};

//系统管理器
class PhaseManager {
    EnginePhase curPhase, prevPhase;
public:
    PhaseManager() : curPhase(EnginePhase::STOPPED),
        prevPhase(EnginePhase::STOPPED) {
    }
    EnginePhase getPhase() const { return curPhase; }
    void setPhase(EnginePhase p) {
        if (p != curPhase) {
            prevPhase = curPhase;
            curPhase = p;
        }
    }
    void update(const EngineData& data) {
        if (curPhase == EnginePhase::STARTING && data.N >= 38000.0) {
            setPhase(EnginePhase::STABLE);
        }
    }
    bool isStable() const { return curPhase == EnginePhase::STABLE; }
    bool isStopped() const { return curPhase == EnginePhase::STOPPED; }
    bool isStopping() const { return curPhase == EnginePhase::STOPPING; }
    bool isStarting() const { return curPhase == EnginePhase::STARTING; }
    void reset() {
        prevPhase = curPhase;
        curPhase = EnginePhase::STOPPED;
    }
};

// 异常检测器
class AnomalyDetector {
    map<int, double> lastLogTime;
    bool shouldLog(int id, double time) {
        auto it = lastLogTime.find(id);
        if (it == lastLogTime.end() || time - it->second >= 5.0) {
            lastLogTime[id] = time;
            return true;
        }
        return false;
    }
public:
    vector<AnomalyInfo> detect(const EngineData& data,
        EnginePhase phase, double time) {
        vector<AnomalyInfo> anomalies;

        // 传感器故障检测
        if (!data.sensorN1Valid && !data.sensorN2Valid && data.dualN1Failure) {
            if (shouldLog(3, time)) {
                anomalies.push_back(AnomalyInfo(L"DUAL N1 SENSOR FAIL - SHUTDOWN", 2, true, 3));
            }
        }
        else if (!data.sensorN1Valid && !data.sensorN2Valid) {
            anomalies.push_back(AnomalyInfo(L"DUAL N1 SENSOR FAULT", 1, false, 2));
        }
        else if (!data.sensorN1Valid) {
            anomalies.push_back(AnomalyInfo(L"N1 SENSOR 1 FAULT", 0, false, 1));
        }
        else if (!data.sensorN2Valid) {
            anomalies.push_back(AnomalyInfo(L"N1 SENSOR 2 FAULT", 0, false, 1));
        }

        // EGT传感器故障检测
        if (!data.sensorEGTValid && !data.sensorEGT2Valid && data.dualEGTFailure) {
            if (shouldLog(6, time)) {
                anomalies.push_back(AnomalyInfo(L"DUAL EGT SENSOR FAIL - SHUTDOWN", 2, true, 6));
            }
        }
        else if (!data.sensorEGTValid && !data.sensorEGT2Valid) {
            anomalies.push_back(AnomalyInfo(L"DUAL EGT SENSOR FAULT", 1, false, 5));
        }
        else if (!data.sensorEGTValid) {
            anomalies.push_back(AnomalyInfo(L"EGT SENSOR 1 FAULT", 0, false, 4));
        }
        else if (!data.sensorEGT2Valid) {
            anomalies.push_back(AnomalyInfo(L"EGT SENSOR 2 FAULT", 0, false, 4));
        }

        // 超速检测
        if (data.N1 > EngineConstants::OVER_SPEED_2) {
            if (shouldLog(11, time)) {
                anomalies.push_back(AnomalyInfo(L"OVERSPEED 2 - EMERGENCY SHUTDOWN", 2, true, 11));
            }
        }
        else if (data.N1 > EngineConstants::OVER_SPEED_1) {
            if (shouldLog(10, time)) {
                anomalies.push_back(AnomalyInfo(L"OVERSPEED 1", 1, false, 10));
            }
        }

        // 超温检测
        if (phase == EnginePhase::STARTING) {
            if (data.T > EngineConstants::OVER_TEMP_START_2) {
                if (shouldLog(13, time)) {
                    anomalies.push_back(AnomalyInfo(L"OVERTEMP 2 - SHUTDOWN", 2, true, 13));
                }
            }
            else if (data.T > EngineConstants::OVER_TEMP_START_1) {
                if (shouldLog(12, time)) {
                    anomalies.push_back(AnomalyInfo(L"OVERTEMP 1 (START)", 1, false, 12));
                }
            }
        }
        else if (phase == EnginePhase::STABLE) {
            if (data.T > EngineConstants::OVER_TEMP_STABLE_2) {
                if (shouldLog(15, time)) {
                    anomalies.push_back(AnomalyInfo(L"OVERTEMP 4 - SHUTDOWN", 2, true, 15));
                }
            }
            else if (data.T > EngineConstants::OVER_TEMP_STABLE_1) {
                if (shouldLog(14, time)) {
                    anomalies.push_back(AnomalyInfo(L"OVERTEMP 3 (STABLE)", 1, false, 14));
                }
            }
        }

        // 燃油系统
        if (!data.sensorFuelValid) {
            if (shouldLog(8, time)) {
                anomalies.push_back(AnomalyInfo(L"FUEL SENSOR FAIL", 2, false, 8));
            }
        }
        else if (data.C < EngineConstants::LOW_FUEL_THRESHOLD) {
            if (shouldLog(7, time)) {
                anomalies.push_back(AnomalyInfo(L"LOW FUEL", 1, false, 7));
            }
        }
        if (data.V > EngineConstants::MAX_FUEL_FLOW) {
            anomalies.push_back(AnomalyInfo(L"HIGH FUEL FLOW", 1, false, 9));
            if (shouldLog(9, time)) {
            }
        }
        return anomalies;
    }
    void reset() { lastLogTime.clear(); }
};

class AlarmManager {
    vector<AnomalyInfo> activeAlarms;
    const double ALARM_DURATION = 5.0;
    void removeExpired(double curTime) {
        auto it = remove_if(activeAlarms.begin(), activeAlarms.end(),
            [curTime](const AnomalyInfo& a) {
                return (curTime - a.displayTime) >= 5.0;
            });
        activeAlarms.erase(it, activeAlarms.end());
    }
    bool exists(const AnomalyInfo& anomaly) {
        for (auto& a : activeAlarms) {
            if (a.anomalyId == anomaly.anomalyId) return true;
        }
        return false;
    }
public:
    void update(const vector<AnomalyInfo>& newAnomalies, double time) {
        removeExpired(time);
        for (auto& a : newAnomalies) {
            if (!exists(a)) {
                AnomalyInfo newAlarm = a;
                newAlarm.displayTime = time;
                activeAlarms.push_back(newAlarm);
            }
        }
    }
    const vector<AnomalyInfo>& getAlarms() const { return activeAlarms; }
    void clear() { activeAlarms.clear(); }
    bool hasEmergency() const {
        for (auto& a : activeAlarms) {
            if (a.requiresShutdown) return true;
        }
        return false;
    }
};

// 数据模拟器
class DataSimulator {
    PhaseManager* phaseMgr;
    EngineData curData;
    double phaseStartTime;
    double thrustAdj;
    double smoothedN, smoothedT, smoothedV;
    void updateFuel(double dt) {
        // 燃油余量
        curData.C -= curData.V * dt;
        if (curData.C < 0.0) {
            curData.C = 0.0;
            if (!phaseMgr->isStopped() && !phaseMgr->isStopping()) {
                phaseMgr->setPhase(EnginePhase::STOPPING);
            }
        }
        if (curData.C > EngineConstants::MAX_FUEL) {
            curData.C = EngineConstants::MAX_FUEL;
        }
    }
    EngineData genStartData(double dt) {
        double phaseTime = curData.runTime - phaseStartTime;
        if (phaseTime <= EngineConstants::START_LINEAR_TIME) {
            curData.N = 10000.0 * phaseTime;
            curData.V = 5.0 * phaseTime;
            curData.T = 20.0 + 40.0 * phaseTime;
        }
        else {
            curData.V = 42.0 * log10(phaseTime - 1.0) + 10.0;
            curData.N = 23000.0 * log10(phaseTime - 1.0) + 20000.0;
            curData.T = 900.0 * log10(phaseTime - 1.0) + 20.0;
        }
        // 添加噪声
        smoothedN = MathUtils::smooth(smoothedN, MathUtils::addNoise(curData.N, 0.002), 0.2);
        smoothedT = MathUtils::smooth(smoothedT, MathUtils::addNoise(curData.T, 0.002), 0.2);
        smoothedV = MathUtils::smooth(smoothedV, MathUtils::addNoise(curData.V, 0.003), 0.2);
        curData.N = smoothedN;
        curData.T = smoothedT;
        curData.V = smoothedV;
        curData.N = MathUtils::clamp(curData.N, 0.0, 50000.0);
        curData.T = MathUtils::clamp(curData.T, -5.0, 1200.0);
        curData.V = MathUtils::clamp(curData.V, 0.0, EngineConstants::MAX_FUEL_FLOW);
        updateFuel(dt);
        return curData;
    }
    EngineData genStableData(double dt) {
        static double baseN = 38000.0;
        static double baseV = 42.0 * log10(EngineConstants::START_LINEAR_TIME) + 10.0; // t=3时的近似值
        static double baseT = 900.0 * log10(EngineConstants::START_LINEAR_TIME) + 20.0;
        double targetN = baseN * (1.0 + thrustAdj * 0.01);
        double targetV = baseV * (1.0 + thrustAdj * 0.01);
        double targetT = baseT * (1.0 + thrustAdj * 0.01);
        if (curData.N < targetN) {
            curData.N += 2000.0 * dt;
            if (curData.N > targetN) curData.N = targetN;
        }
        else if (curData.N > targetN) {
            curData.N -= 2000.0 * dt;
            if (curData.N < targetN) curData.N = targetN;
        }
        curData.N = MathUtils::addNoise(curData.N, 0.003);
        curData.T = MathUtils::addNoise(curData.T, 0.003);
        curData.V = MathUtils::addNoise(curData.V, 0.003);
        curData.N = MathUtils::clamp(curData.N, 0.0, 50000.0);
        curData.T = MathUtils::clamp(curData.T, -5.0, 1200.0);
        curData.V = MathUtils::clamp(curData.V, 0.0, EngineConstants::MAX_FUEL_FLOW);
        updateFuel(dt);
        return curData;
    }
    EngineData genStopData(double dt) {
        double phaseTime = curData.runTime - phaseStartTime;
        curData.V = 0.0;
        if (phaseTime >= EngineConstants::STOP_DURATION) {
            curData.N = 0.0;
            curData.T = EngineConstants::INITIAL_TEMP;
            smoothedN = 0.0;
            smoothedT = EngineConstants::INITIAL_TEMP;
            smoothedV = 0.0;
            phaseMgr->setPhase(EnginePhase::STOPPED);
            return curData;
        }
        double decayFactor = pow(0.5, phaseTime);
        static double initialN = curData.N;
        static double initialT = curData.T;
        if (phaseTime <= 0.005) {
            initialN = curData.N;
            initialT = curData.T;
        }
        // 应用对数衰减
        curData.N = initialN * decayFactor;
        if (initialT > 20.0) {
            curData.T = 20.0 + (initialT - 20.0) * decayFactor;
        }
        else {
            curData.T = initialT * decayFactor;
        }
        // 添加噪声
        if (curData.N > 1000.0) {
            curData.N = MathUtils::addNoise(curData.N, 0.002);
            curData.T = MathUtils::addNoise(curData.T, 0.002);
        }
        curData.N = MathUtils::clamp(curData.N, 0.0, 50000.0);
        curData.T = MathUtils::clamp(curData.T, -5.0, 1200.0);
        smoothedN = curData.N;
        smoothedT = curData.T;
        smoothedV = curData.V;
        return curData;
    }
public:
    DataSimulator(PhaseManager* pm) : phaseMgr(pm), phaseStartTime(0.0),
        thrustAdj(0.0), smoothedN(0.0), smoothedT(20.0), smoothedV(0.0) {
        curData.reset();
    }
    EngineData update(double dt) {
        if (!phaseMgr->isStopped()) {
            curData.runTime += dt;
            if (curData.runTime > 3600.0) {
                curData.runTime = 3600.0;
            }
        }

        switch (phaseMgr->getPhase()) {
        case EnginePhase::STARTING:
            curData = genStartData(dt);
            break;
        case EnginePhase::STABLE:
            curData = genStableData(dt);
            break;
        case EnginePhase::STOPPING:
            curData = genStopData(dt);
            break;
        case EnginePhase::STOPPED:
            // 停止状态不更新数据，但保持时间更新
            break;
        }
        curData.calculateN1();
        phaseMgr->update(curData);
        return curData;
    }

    void start() {
        curData.reset();
        phaseStartTime = curData.runTime;
        thrustAdj = 0.0;
        phaseMgr->setPhase(EnginePhase::STARTING);
        smoothedN = 0.0;
        smoothedT = 20.0;
        smoothedV = 0.0;

        // 重置所有传感器状态
        curData.sensorN1Valid = true;
        curData.sensorN2Valid = true;
        curData.sensorEGTValid = true;
        curData.sensorEGT2Valid = true;
        curData.sensorFuelValid = true;
        curData.dualN1Failure = false;
        curData.dualEGTFailure = false;
    }

    void stop() {
        if (!phaseMgr->isStopped() && !phaseMgr->isStopping()) {
            phaseStartTime = curData.runTime; // 记录停车开始时间
            phaseMgr->setPhase(EnginePhase::STOPPING);
        }
    }

    void incThrust() {
        if (phaseMgr->isStable()) {
            thrustAdj += 1.0;
            if (thrustAdj > 10.0) thrustAdj = 10.0;
        }
    }

    void decThrust() {
        if (phaseMgr->isStable()) {
            thrustAdj -= 1.0;
            if (thrustAdj < -10.0) thrustAdj = -10.0;
        }
    }

    const EngineData& getCurrentData() const { return curData; }

    void triggerAnomaly(int id) {
        switch (id) {
        case 1: curData.sensorN1Valid = false; break;
        case 2: curData.sensorN2Valid = false; break;
        case 3:
            curData.sensorN1Valid = false;
            curData.sensorN2Valid = false;
            curData.dualN1Failure = true;
            break;
        case 4: curData.sensorEGTValid = false; break;
        case 5: curData.sensorEGT2Valid = false; break;
        case 6:
            curData.sensorEGTValid = false;
            curData.sensorEGT2Valid = false;
            curData.dualEGTFailure = true;
            break;
        case 7: curData.C = 1000.0; break;
        case 8: curData.sensorFuelValid = false; break;
        case 9:
            curData.V = 58.0; 
            break;
        case 10:
            curData.N = 44000.0;
            curData.calculateN1();
            break;
        case 11:
            curData.N = 50000.0;
            curData.calculateN1();
            break;
        case 12: curData.T = 900.0; break;
        case 13: curData.T = 1050.0; break;
        case 14: curData.T = 1000.0; break;
        case 15: curData.T = 1150.0; break;
        default: break;
        }
    }
};

//用户界面
class Dashboard {
    unique_ptr<SectorGauge> n1Gauge, n2Gauge, egtGauge, egt2Gauge;
    unique_ptr<Button> startBtn, stopBtn, incBtn, decBtn;
    vector<unique_ptr<Button>> anomalyBtns;
    vector<bool> anomalyActive;
    int mouseX, mouseY, winW, winH;
    bool inRunState;

    // 单独判断每个表盘的颜色级别，传感器失效时显示灰色"--"
    ColorLevel getN1S1Color(const EngineData& data, const vector<AnomalyInfo>& alarms) {
        // N1S1传感器失效
        if (!data.sensorN1Valid) {
            return ColorLevel::INVALID;
        }

        // 超速检查
        if (data.N1 > EngineConstants::OVER_SPEED_2) {
            return ColorLevel::DANGER;
        }
        else if (data.N1 > EngineConstants::OVER_SPEED_1) {
            return ColorLevel::WARNING;
        }

        return ColorLevel::NORMAL;
    }

    ColorLevel getN1S2Color(const EngineData& data, const vector<AnomalyInfo>& alarms) {
        // N1S2传感器失效
        if (!data.sensorN2Valid) {
            return ColorLevel::INVALID;
        }

        // 超速检查
        if (data.N1 > EngineConstants::OVER_SPEED_2) {
            return ColorLevel::DANGER;
        }
        else if (data.N1 > EngineConstants::OVER_SPEED_1) {
            return ColorLevel::WARNING;
        }

        return ColorLevel::NORMAL;
    }

    ColorLevel getEGT1Color(const EngineData& data, const vector<AnomalyInfo>& alarms) {
        // EGT1传感器失效
        if (!data.sensorEGTValid) {
            return ColorLevel::INVALID;
        }

        // 超温检查
        EnginePhase phase = EnginePhase::STABLE; // 简化处理，实际应从phaseManager获取
        if (phase == EnginePhase::STARTING) {
            if (data.T > EngineConstants::OVER_TEMP_START_2) {
                return ColorLevel::DANGER;
            }
            else if (data.T > EngineConstants::OVER_TEMP_START_1) {
                return ColorLevel::WARNING;
            }
        }
        else if (phase == EnginePhase::STABLE) {
            if (data.T > EngineConstants::OVER_TEMP_STABLE_2) {
                return ColorLevel::DANGER;
            }
            else if (data.T > EngineConstants::OVER_TEMP_STABLE_1) {
                return ColorLevel::WARNING;
            }
        }

        return ColorLevel::NORMAL;
    }

    ColorLevel getEGT2Color(const EngineData& data, const vector<AnomalyInfo>& alarms) {
        // EGT2传感器失效
        if (!data.sensorEGT2Valid) {
            return ColorLevel::INVALID;
        }

        // 超温检查
        EnginePhase phase = EnginePhase::STABLE; // 简化处理，实际应从phaseManager获取
        if (phase == EnginePhase::STARTING) {
            if (data.T > EngineConstants::OVER_TEMP_START_2) {
                return ColorLevel::DANGER;
            }
            else if (data.T > EngineConstants::OVER_TEMP_START_1) {
                return ColorLevel::WARNING;
            }
        }
        else if (phase == EnginePhase::STABLE) {
            if (data.T > EngineConstants::OVER_TEMP_STABLE_2) {
                return ColorLevel::DANGER;
            }
            else if (data.T > EngineConstants::OVER_TEMP_STABLE_1) {
                return ColorLevel::WARNING;
            }
        }

        return ColorLevel::NORMAL;
    }

    void drawFuelBar(const EngineData& data, const vector<AnomalyInfo>& alarms) {
        int x = 60, y = 520, w = 200, h = 30;

        // 确定颜色级别
        ColorLevel level = ColorLevel::NORMAL;
        if (!data.sensorFuelValid) {
            level = ColorLevel::INVALID;
        }
        else if (data.C < EngineConstants::LOW_FUEL_THRESHOLD) {
            level = ColorLevel::WARNING;
        }

        // 检查是否有燃油相关的警报
        for (const auto& alarm : alarms) {
            if (alarm.message.find(L"FUEL") != wstring::npos) {
                if (alarm.level == 2) level = ColorLevel::DANGER;
                else if (alarm.level == 1 && level != ColorLevel::DANGER) level = ColorLevel::WARNING;
            }
        }

        // 根据级别选择颜色
        COLORREF barColor;
        switch (level) {
        case ColorLevel::NORMAL: barColor = RGB(255, 255, 255); break;
        case ColorLevel::WARNING: barColor = RGB(255, 191, 0); break;
        case ColorLevel::DANGER: barColor = RGB(255, 0, 0); break;
        case ColorLevel::INVALID: barColor = RGB(128, 128, 128); break;
        default: barColor = RGB(255, 255, 255); break;
        }

        // 计算燃油比例
        double ratio = 0.0;
        if (data.sensorFuelValid && data.C >= 0.0) {
            ratio = data.C / EngineConstants::MAX_FUEL;
            if (ratio > 1.0) ratio = 1.0;
            if (ratio < 0.0) ratio = 0.0;
        }

        // 绘制标签
        settextcolor(RGB(255, 255, 255));
        settextstyle(14, 0, L"Arial");
        outtextxy(x, y - 18, L"FUEL");

        // 绘制背景框
        setfillcolor(RGB(40, 40, 40));
        setlinecolor(RGB(100, 100, 100));
        setlinestyle(PS_SOLID, 1);
        solidrectangle(x, y, x + w, y + h);
        rectangle(x, y, x + w, y + h);

        // 绘制燃油条
        if (ratio > 0.0 && data.sensorFuelValid) {
            int fillWidth = static_cast<int>(w * ratio);
            setfillcolor(barColor);
            setlinecolor(barColor);
            solidrectangle(x, y, x + fillWidth, y + h);
        }

        // 绘制数值
        settextstyle(16, 0, L"Arial");
        wchar_t valueStr[32];
        if (!data.sensorFuelValid) {
            swprintf_s(valueStr, 32, L"--");
            settextcolor(RGB(128, 128, 128));
        }
        else {
            swprintf_s(valueStr, 32, L"%.0f", data.C);
            settextcolor(barColor);
        }

        outtextxy(x + w + 10, y + 6, valueStr);

        // 绘制单位
        settextstyle(12, 0, L"Arial");
        settextcolor(RGB(200, 200, 200));
        outtextxy(x + w + 60, y + 8, L"kg");
    }

    void drawFuelFlow(const EngineData& data, const vector<AnomalyInfo>& alarms) {
        int x = 580, y = 50, w = 70, h = 28;

        ColorLevel level = ColorLevel::NORMAL;
        if (!data.sensorFuelValid) {
            level = ColorLevel::INVALID;
        }
        else if (data.V > EngineConstants::MAX_FUEL_FLOW) {
            level = ColorLevel::WARNING;
        }

        // 检查是否有燃油流量高的警报
        for (const auto& alarm : alarms) {
            if (alarm.message.find(L"HIGH FUEL FLOW") != wstring::npos) {
                level = ColorLevel::WARNING;
                break;
            }
        }

        COLORREF boxColor;
        switch (level) {
        case ColorLevel::NORMAL: boxColor = RGB(255, 255, 255); break;
        case ColorLevel::WARNING: boxColor = RGB(255, 191, 0); break;
        case ColorLevel::DANGER: boxColor = RGB(255, 0, 0); break;
        case ColorLevel::INVALID: boxColor = RGB(128, 128, 128); break;
        default: boxColor = RGB(255, 255, 255); break;
        }

        // 绘制背景
        setfillcolor(RGB(30, 30, 30));
        solidrectangle(x, y, x + w, y + h);

        // 绘制边框
        setlinecolor(boxColor);
        setlinestyle(PS_SOLID, 2);
        rectangle(x, y, x + w, y + h);

        // 绘制标签
        settextcolor(RGB(255, 255, 255));
        settextstyle(12, 0, L"Arial");
        outtextxy(x + 3, y + 5, L"FF:");

        // 绘制数值
        settextcolor(boxColor);
        settextstyle(16, 0, L"Arial");

        wchar_t valueStr[32];
        if (level == ColorLevel::INVALID) {
            swprintf_s(valueStr, 32, L"--");
        }
        else {
            swprintf_s(valueStr, 32, L"%.0f", data.V);
        }

        outtextxy(x + 25, y + 5, valueStr);

        // 如果燃油流量高，显示警告符号
        if (level == ColorLevel::WARNING && data.V > EngineConstants::MAX_FUEL_FLOW) {
            settextcolor(RGB(255, 191, 0));
            settextstyle(12, 0, L"Arial");
            outtextxy(x + w - 15, y + 5, L"!");
        }
    }

    void drawAlertBox(const vector<AnomalyInfo>& alarms) {
        int x = 520, y = 265, w = 260, h = 160;

        // 绘制阴影
        setfillcolor(RGB(10, 10, 15));
        setlinecolor(RGB(0, 0, 0));
        setlinestyle(PS_SOLID, 1);
        solidrectangle(x + 3, y + 3, x + w + 3, y + h + 3);

        // 绘制背景
        setfillcolor(RGB(18, 18, 25));
        solidrectangle(x, y, x + w, y + h);

        // 绘制边框
        setlinecolor(RGB(70, 70, 90));
        setlinestyle(PS_SOLID, 2);
        line(x, y, x + w, y);
        line(x, y, x, y + h);

        setlinecolor(RGB(30, 30, 45));
        line(x + w, y, x + w, y + h);
        line(x, y + h, x + w, y + h);

        setlinecolor(RGB(90, 90, 110));
        setlinestyle(PS_SOLID, 1);
        rectangle(x, y, x + w, y + h);

        // 绘制标题栏
        int titleHeight = 25;
        setfillcolor(RGB(40, 40, 55));
        solidrectangle(x + 1, y + 1, x + w + 1, y + titleHeight + 1);

        // 标题栏渐变
        setfillcolor(RGB(55, 55, 75));
        solidrectangle(x, y, x + w, y + titleHeight / 3);
        setfillcolor(RGB(50, 50, 70));
        solidrectangle(x, y + titleHeight / 3, x + w, y + titleHeight * 2 / 3);
        setfillcolor(RGB(45, 45, 65));
        solidrectangle(x, y + titleHeight * 2 / 3, x + w, y + titleHeight);

        // 标题文本（阴影效果）
        settextcolor(RGB(30, 30, 40));
        settextstyle(16, 0, L"Arial");
        outtextxy(x + 9, y + 5, L"ALERTS");
        settextcolor(RGB(220, 220, 240));
        outtextxy(x + 8, y + 4, L"ALERTS");

        // 警报列表区域
        int startY = y + titleHeight + 5;
        int lineHeight = 18;
        int textX = x + 8;

        // 显示最多8条警报
        size_t displayCount = min(alarms.size(), static_cast<size_t>(8));

        for (size_t i = 0; i < displayCount; i++) {
            const AnomalyInfo& alert = alarms[i];

            COLORREF textColor;
            switch (alert.level) {
            case 1:
                textColor = RGB(255, 200, 50);
                break;
            case 2:
                textColor = RGB(255, 60, 60);
                break;
            default:
                textColor = RGB(200, 200, 220);
                break;
            }

            // 交替行背景色
            if (i % 2 == 0) {
                setfillcolor(RGB(25, 25, 35));
            }
            else {
                setfillcolor(RGB(22, 22, 30));
            }
            solidrectangle(textX - 4, startY + static_cast<int>(i) * lineHeight - 2,
                x + w - 18, startY + static_cast<int>(i) * lineHeight + lineHeight - 2);

            // 行号
            wchar_t lineNum[8];
            swprintf_s(lineNum, 8, L"%d.", static_cast<int>(i + 1));
            settextcolor(RGB(60, 60, 70));
            outtextxy(textX + 1, startY + static_cast<int>(i) * lineHeight + 1, lineNum);
            settextcolor(RGB(150, 150, 160));
            outtextxy(textX, startY + static_cast<int>(i) * lineHeight, lineNum);

            // 警报文本（截断过长的文本）
            wstring msg = alert.message;
            int maxTextWidth = w - 35;
            if (textwidth(msg.c_str()) > maxTextWidth) {
                while (textwidth(msg.c_str()) > maxTextWidth && !msg.empty()) {
                    msg = msg.substr(0, msg.length() - 1);
                }
                msg += L"...";
            }

            // 警报文本（阴影效果）
            settextcolor(RGB(30, 30, 40));
            outtextxy(textX + 26, startY + static_cast<int>(i) * lineHeight + 1, msg.c_str());
            settextcolor(textColor);
            outtextxy(textX + 25, startY + static_cast<int>(i) * lineHeight, msg.c_str());
        }

        // 如果没有警报
        if (alarms.empty()) {
            settextcolor(RGB(60, 60, 70));
            settextstyle(14, 0, L"Arial");
            outtextxy(textX + 1, startY + 11, L"NO ALERTS");
            settextcolor(RGB(100, 110, 120));
            outtextxy(textX, startY + 10, L"NO ALERTS");
        }

        // 滚动条指示器（如果有更多警报）
        if (alarms.size() > 8) {
            wchar_t stats[32];
            swprintf_s(stats, 32, L"[%d/%d]", static_cast<int>(alarms.size()), 8);
            settextcolor(RGB(40, 40, 50));
            settextstyle(12, 0, L"Arial");
            outtextxy(x + w - textwidth(stats) - 7, y + 7, stats);
            settextcolor(RGB(140, 150, 160));
            outtextxy(x + w - textwidth(stats) - 8, y + 6, stats);
        }
    }

    void drawStatusBoxes(const EngineData& data, const PhaseManager* phaseManager) {
        // 启动状态灯
        int startX = 380, startY = 520, startW = 70, startH = 25;
        bool startLit = phaseManager->isStarting();

        // 运行状态灯
        int runX = 470, runY = 520, runW = 70, runH = 25;
        bool runLit = phaseManager->isStable();

        // 绘制启动状态灯
        COLORREF startColor = startLit ? RGB(0, 255, 0) : RGB(50, 50, 50);
        setfillcolor(startColor);
        solidrectangle(startX, startY, startX + startW, startY + startH);

        settextcolor(startLit ? RGB(0, 0, 0) : RGB(120, 130, 140));
        settextstyle(16, 0, L"Arial");
        int startTextW = textwidth(L"START");
        outtextxy(startX + (startW - startTextW) / 2, startY + 4, L"START");

        // 绘制运行状态灯
        COLORREF runColor = runLit ? RGB(0, 255, 0) : RGB(50, 50, 50);
        setfillcolor(runColor);
        solidrectangle(runX, runY, runX + runW, runY + runH);

        settextcolor(runLit ? RGB(0, 0, 0) : RGB(120, 130, 140));
        int runTextW = textwidth(L"RUN");
        outtextxy(runX + (runW - runTextW) / 2, runY + 4, L"RUN");
    }

public:
    Dashboard(int width = 800, int height = 600) :
        winW(width), winH(height), mouseX(0), mouseY(0), inRunState(false) {

        // 创建仪表
        n1Gauge = make_unique<SectorGauge>(170, 180, 90);
        n2Gauge = make_unique<SectorGauge>(380, 180, 90);
        egtGauge = make_unique<SectorGauge>(180, 400, 90);
        egt2Gauge = make_unique<SectorGauge>(380, 400, 90);

        // 创建控制按钮
        startBtn = make_unique<Button>(250, 10, 70, 35, L"START");
        stopBtn = make_unique<Button>(330, 10, 70, 35, L"STOP", RGB(220, 20, 60));
        incBtn = make_unique<Button>(410, 10, 70, 35, L"THRUST+", RGB(60, 179, 113));
        decBtn = make_unique<Button>(490, 10, 70, 35, L"THRUST-", RGB(60, 179, 113));

        // 创建异常测试按钮
        const wchar_t* anomalyNames[15] = {
            L"N1S1Fail", L"N1S2Fail", L"DualN1Fail",
            L"EGTS1Fail", L"EGTS2Fail", L"DualEGTFail",
            L"LowFuel", L"FuelSFail", L"FuelFlowHigh",
            L"OverSpd1", L"OverSpd2",
            L"OverTemp1", L"OverTemp2", L"OverTemp3", L"OverTemp4"
        };

        for (int i = 0; i < 15; i++) {
            int row = i / 3;
            int col = i % 3;
            anomalyBtns.push_back(make_unique<Button>(
                520 + col * 90, 100 + row * 31, 85, 26, anomalyNames[i], RGB(80, 80, 80)));
        }

        anomalyActive.resize(15, false);
    }

    void render(const EngineData& data, const vector<AnomalyInfo>& alarms, const PhaseManager* phaseManager) {
        cleardevice();

        // 绘制背景
        setfillcolor(RGB(0, 0, 0));
        solidrectangle(0, 0, winW, winH);

        // 绘制网格背景
        setlinecolor(RGB(30, 30, 30));
        setlinestyle(PS_SOLID, 1);
        for (int i = 0; i < winW; i += 50) {
            line(i, 0, i, winH);
        }
        for (int i = 0; i < winH; i += 50) {
            line(0, i, winW, i);
        }

        // 获取当前阶段用于温度显示
        EnginePhase currentPhase = phaseManager->getPhase();

        // 绘制仪表
        n1Gauge->draw(data.N1, 0.0, 100.0, getN1S1Color(data, alarms), L"N1S1", L"%");
        n2Gauge->draw(data.N1, 0.0, 100.0, getN1S2Color(data, alarms), L"N1S2", L"%");

        // EGT仪表需要考虑当前阶段
        ColorLevel egt1Level = getEGT1Color(data, alarms);
        ColorLevel egt2Level = getEGT2Color(data, alarms);

        // 如果当前阶段是STARTING，应用启动阶段的温度阈值
        if (currentPhase == EnginePhase::STARTING) {
            if (data.T > EngineConstants::OVER_TEMP_START_2) {
                egt1Level = ColorLevel::DANGER;
                egt2Level = ColorLevel::DANGER;
            }
            else if (data.T > EngineConstants::OVER_TEMP_START_1) {
                egt1Level = ColorLevel::WARNING;
                egt2Level = ColorLevel::WARNING;
            }
        }
        else if (currentPhase == EnginePhase::STABLE) {
            if (data.T > EngineConstants::OVER_TEMP_STABLE_2) {
                egt1Level = ColorLevel::DANGER;
                egt2Level = ColorLevel::DANGER;
            }
            else if (data.T > EngineConstants::OVER_TEMP_STABLE_1) {
                egt1Level = ColorLevel::WARNING;
                egt2Level = ColorLevel::WARNING;
            }
        }

        egtGauge->draw(data.T, -5.0, 1080.0, egt1Level, L"EGT1", L"C");
        egt2Gauge->draw(data.T, -5.0, 1080.0, egt2Level, L"EGT2", L"C");

        // 绘制燃油流量显示
        drawFuelFlow(data, alarms);

        // 绘制燃油余量条
        drawFuelBar(data, alarms);

        // 更新按钮悬停状态
        startBtn->setHover(startBtn->inside(mouseX, mouseY));
        stopBtn->setHover(stopBtn->inside(mouseX, mouseY));
        incBtn->setHover(incBtn->inside(mouseX, mouseY));
        decBtn->setHover(decBtn->inside(mouseX, mouseY));

        // 绘制控制按钮
        startBtn->draw();
        stopBtn->draw();
        incBtn->draw();
        decBtn->draw();

        // 更新并绘制异常按钮
        for (size_t i = 0; i < anomalyBtns.size(); i++) {
            bool hover = anomalyBtns[i]->inside(mouseX, mouseY);
            anomalyBtns[i]->setHover(hover);

            if (anomalyActive[i]) {
                anomalyBtns[i]->setBg(RGB(255, 191, 0));
                anomalyBtns[i]->setText(L"ON");
            }
            else if (hover) {
                anomalyBtns[i]->setBg(RGB(120, 120, 120));
            }
            else {
                anomalyBtns[i]->setBg(RGB(80, 80, 80));
                // 恢复按钮文本
                const wchar_t* anomalyNames[15] = {
                    L"N1S1Fail", L"N1S2Fail", L"DualN1Fail",
                    L"EGTS1Fail", L"EGTS2Fail", L"DualEGTFail",
                    L"LowFuel", L"FuelSFail", L"FuelFlowHigh",
                    L"OverSpd1", L"OverSpd2",
                    L"OverTemp1", L"OverTemp2", L"OverTemp3", L"OverTemp4"
                };
                if (i < 15) {
                    anomalyBtns[i]->setText(anomalyNames[i]);
                }
            }

            anomalyBtns[i]->draw();
        }

        // 绘制警报框
        drawAlertBox(alarms);

        // 绘制状态指示灯
        drawStatusBoxes(data, phaseManager);

        // 绘制运行时间
        settextcolor(RGB(200, 200, 200));
        settextstyle(16, 0, L"宋体");
        outtextxy(winW - 170, winH - 45, L"RunTime:");

        wchar_t timeStr[32];
        swprintf_s(timeStr, 32, L"%.3f s", data.runTime);

        settextcolor(RGB(0, 255, 0));
        settextstyle(24, 0, L"Arial");
        outtextxy(winW - 90, winH - 49, timeStr);

        // 绘制当前阶段
        settextcolor(RGB(200, 200, 200));
        settextstyle(16, 0, L"宋体");
        const wchar_t* phaseName = L"UNKNOWN";
        switch (currentPhase) {
        case EnginePhase::STOPPED: phaseName = L"STOPPED"; break;
        case EnginePhase::STARTING: phaseName = L"STARTING"; break;
        case EnginePhase::STABLE: phaseName = L"STABLE"; break;
        case EnginePhase::STOPPING: phaseName = L"STOPPING"; break;
        }
        outtextxy(winW - 170, winH - 80, phaseName);
    }

    void handleMouse(int x, int y) {
        mouseX = x;
        mouseY = y;
    }

    int checkClick(int x, int y) {
        if (startBtn->inside(x, y)) return 1;
        if (stopBtn->inside(x, y)) return 2;
        if (incBtn->inside(x, y)) return 3;
        if (decBtn->inside(x, y)) return 4;

        for (size_t i = 0; i < anomalyBtns.size(); i++) {
            if (anomalyBtns[i]->inside(x, y)) {
                return static_cast<int>(5 + i);
            }
        }

        return 0;
    }

    void setAnomalyActive(int idx, bool active) {
        if (idx >= 0 && idx < static_cast<int>(anomalyActive.size())) {
            anomalyActive[idx] = active;
        }
    }

    void resetAnomalyBtns() {
        fill(anomalyActive.begin(), anomalyActive.end(), false);
    }
};

// ============ 鼠标处理器 ============
class MouseHandler {
    Dashboard* dash;
    function<void(int)> clickCallback;

public:
    MouseHandler(Dashboard* d) : dash(d), clickCallback(nullptr) {}

    void setCallback(function<void(int)> cb) { clickCallback = cb; }

    void handleEvents() {
        if (MouseHit()) {
            MOUSEMSG msg = GetMouseMsg();

            switch (msg.uMsg) {
            case WM_MOUSEMOVE:
                dash->handleMouse(msg.x, msg.y);
                break;

            case WM_LBUTTONDOWN:
                handleMouseClick(msg.x, msg.y);
                break;

            default:
                break;
            }
        }
    }

    void handleMouseClick(int x, int y) {
        int btnId = dash->checkClick(x, y);
        if (btnId != 0 && clickCallback) {
            clickCallback(btnId);
        }
    }
};

// 数据记录器
class DataLogger {
    ofstream dataFile, logFile;
    string dataFileName, logFileName;
    bool isOpen;
    stringstream dataBuffer;
    size_t bufferSize;
    const size_t MAX_BUFFER_SIZE = 100;
    map<int, double> lastAnomalyLogTime;

    string getCurrentTimestamp() {
        auto now = chrono::system_clock::now();
        time_t t = chrono::system_clock::to_time_t(now);

        char timeStr[64];
        tm timeInfo;
        localtime_s(&timeInfo, &t);
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeInfo);

        return string(timeStr);
    }

    string wideToNarrow(const wstring& wstr) {
        string result;
        for (wchar_t c : wstr) {
            if (c <= 127) {
                result += static_cast<char>(c);
            }
            else {
                result += '?';
            }
        }
        return result;
    }

public:
    DataLogger() : isOpen(false), bufferSize(0) {}

    ~DataLogger() {
        close();
    }

    bool init() {
        // 创建日志目录
        system("if not exist logs mkdir logs");
        auto now = chrono::system_clock::now();
        time_t t = chrono::system_clock::to_time_t(now);
        char timeStr[64];
        tm timeInfo;
        localtime_s(&timeInfo, &t);
        strftime(timeStr, sizeof(timeStr), "%Y%m%d_%H%M%S", &timeInfo);
        dataFileName = string("./logs/excal_") + timeStr + ".csv";
        logFileName = string("./logs/log_") + timeStr + ".log";
        dataFile.open(dataFileName);
        if (!dataFile.is_open()) {
            cerr << "Failed to open data file: " << dataFileName << endl;
            return false;
        }
        dataFile << "RunTime,N_N1,N_RPM,T_EGT,C_Fuel,V_Flow" << endl;
        logFile.open(logFileName);
        if (!logFile.is_open()) {
            cerr << "Failed to open log file: " << logFileName << endl;
            dataFile.close();
            return false;
        }
        logFile << "Engine Monitoring System Log - Started at: " << getCurrentTimestamp() << endl;
        logFile << "==========================================" << endl;
        isOpen = true;
        return true;
    }

    void logData(const EngineData& data) {
        if (!isOpen) return;
        std::stringstream ss;
        ss.imbue(std::locale("C"));
        ss << fixed << setprecision(3)
            << data.runTime << ","
            << data.N1 << ","
            << data.N << ","
            << data.T << ","
            << data.C << ","
            << data.V << "\n";
        dataBuffer << ss.str();
        if (++bufferSize >= MAX_BUFFER_SIZE) {
            flush();
        }
    }
    void logAnomaly(const AnomalyInfo& anomaly, double currentTime) {
        if (!isOpen || anomaly.anomalyId == 0) return;

        auto it = lastAnomalyLogTime.find(anomaly.anomalyId);
        if (it != lastAnomalyLogTime.end()) {
            if (currentTime - it->second < 5.0) {
                return;
            }
        }

        lastAnomalyLogTime[anomaly.anomalyId] = currentTime;

        logFile << "[" << fixed << setprecision(3) << currentTime << "s] "
            << wideToNarrow(anomaly.message);

        if (anomaly.requiresShutdown) {
            logFile << " [EMERGENCY SHUTDOWN REQUIRED]";
        }

        logFile << endl;
        logFile.flush();
    }

    void flush() {
        if (!isOpen) return;

        if (dataBuffer.tellp() > 0) {
            dataFile << dataBuffer.str();
            dataBuffer.str("");
            dataBuffer.clear();
            bufferSize = 0;
            dataFile.flush();
        }
    }

    void close() {
        if (isOpen) {
            flush();

            if (dataFile.is_open()) {
                dataFile << "\n# End of data file\n";
                dataFile.close();
            }

            if (logFile.is_open()) {
                logFile << "\n# Log file closed at: " << getCurrentTimestamp() << endl;
                logFile.close();
            }

            isOpen = false;
        }
    }

    bool is_open() const {
        return isOpen;
    }
};

//定时器类
class Timer {
    LARGE_INTEGER frequency;
    LARGE_INTEGER startCount;
    double targetInterval;

public:
    Timer(double intervalMs) {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&startCount);
        targetInterval = intervalMs / 1000.0;
        timeBeginPeriod(1);
    }

    ~Timer() {
        timeEndPeriod(1);
    }

    void wait() {
        LARGE_INTEGER currentCount;
        do {
            QueryPerformanceCounter(&currentCount);
        } while ((currentCount.QuadPart - startCount.QuadPart) /
            static_cast<double>(frequency.QuadPart) < targetInterval);

        QueryPerformanceCounter(&startCount);
    }
};

// ============ 全局变量 ============
unique_ptr<DataSimulator> g_sim;
unique_ptr<PhaseManager> g_phase;
unique_ptr<DataLogger> g_logger;
Dashboard* g_dash = nullptr;

// 按钮点击
void handleButton(int id) {
    switch (id) {
    case 1: // START按钮
        cout << "START button clicked" << endl;
        if (g_phase->isStopped()) {
            g_sim->start();
            if (g_dash) g_dash->resetAnomalyBtns();
        }
        break;

    case 2: // STOP按钮
        cout << "STOP button clicked" << endl;
        if (!g_phase->isStopped()) {
            g_sim->stop();
        }
        break;

    case 3: // 增加推力按钮
        cout << "Increase Thrust clicked" << endl;
        if (g_phase->isStable()) {
            g_sim->incThrust();
        }
        break;

    case 4: // 减少推力按钮
        cout << "Decrease Thrust clicked" << endl;
        if (g_phase->isStable()) {
            g_sim->decThrust();
        }
        break;

    case 5: // N1S1Fail
        cout << "Anomaly test: N1 SENSOR 1 FAULT" << endl;
        g_sim->triggerAnomaly(1);
        if (g_dash) g_dash->setAnomalyActive(0, true);
        break;

    case 6: // N1S2Fail
        cout << "Anomaly test: N1 SENSOR 2 FAULT" << endl;
        g_sim->triggerAnomaly(2);
        if (g_dash) g_dash->setAnomalyActive(1, true);
        break;

    case 7: // DualN1Fail
        cout << "Anomaly test: DUAL N1 SENSOR FAIL" << endl;
        g_sim->triggerAnomaly(3);
        if (g_dash) g_dash->setAnomalyActive(2, true);
        break;

    case 8: // EGTS1Fail
        cout << "Anomaly test: EGT SENSOR 1 FAULT" << endl;
        g_sim->triggerAnomaly(4);
        if (g_dash) g_dash->setAnomalyActive(3, true);
        break;

    case 9: // EGTS2Fail
        cout << "Anomaly test: EGT SENSOR 2 FAULT" << endl;
        g_sim->triggerAnomaly(5);
        if (g_dash) g_dash->setAnomalyActive(4, true);
        break;

    case 10: // DualEGTFail
        cout << "Anomaly test: DUAL EGT SENSOR FAIL" << endl;
        g_sim->triggerAnomaly(6);
        if (g_dash) g_dash->setAnomalyActive(5, true);
        break;

    case 11: // LowFuel
        cout << "Anomaly test: LOW FUEL" << endl;
        g_sim->triggerAnomaly(7);
        if (g_dash) g_dash->setAnomalyActive(6, true);
        break;

    case 12: // FuelSFail
        cout << "Anomaly test: FUEL SENSOR FAIL" << endl;
        g_sim->triggerAnomaly(8);
        if (g_dash) g_dash->setAnomalyActive(7, true);
        break;

    case 13: // FuelFlowHigh
        cout << "Anomaly test: HIGH FUEL FLOW" << endl;
        g_sim->triggerAnomaly(9);
        if (g_dash) g_dash->setAnomalyActive(8, true);
        break;

    case 14: // OverSpd1
        cout << "Anomaly test: OVERSPEED 1" << endl;
        g_sim->triggerAnomaly(10);
        if (g_dash) g_dash->setAnomalyActive(9, true);
        break;

    case 15: // OverSpd2
        cout << "Anomaly test: OVERSPEED 2" << endl;
        g_sim->triggerAnomaly(11);
        if (g_dash) g_dash->setAnomalyActive(10, true);
        break;

    case 16: // OverTemp1
        if (!g_phase->isStarting()) {
            cout << "[ERROR] OVERTEMP 1 requires STARTING phase. Please start engine first." << endl;
            break;
        }
        cout << "Anomaly test: OVERTEMP 1 (START)" << endl;
        g_sim->triggerAnomaly(12);
        if (g_dash) g_dash->setAnomalyActive(11, true);
        break;

    case 17: // OverTemp2
        if (!g_phase->isStarting()) {
            cout << "[ERROR] OVERTEMP 2 requires STARTING phase. Please start engine first." << endl;
            break;
        }
        cout << "Anomaly test: OVERTEMP 2 (START)" << endl;
        g_sim->triggerAnomaly(13);
        if (g_dash) g_dash->setAnomalyActive(12, true);
        break;

    case 18: // OverTemp3
        if (!g_phase->isStable()) {
            cout << "[ERROR] OVERTEMP 3 requires STABLE phase. Engine must be running stably." << endl;
            break;
        }
        cout << "Anomaly test: OVERTEMP 3 (STABLE)" << endl;
        g_sim->triggerAnomaly(14);
        if (g_dash) g_dash->setAnomalyActive(13, true);
        break;

    case 19: // OverTemp4
        if (!g_phase->isStable()) {
            cout << "[ERROR] OVERTEMP 4 requires STABLE phase. Engine must be running stably." << endl;
            break;
        }
        cout << "Anomaly test: OVERTEMP 4 (STABLE)" << endl;
        g_sim->triggerAnomaly(15);
        if (g_dash) g_dash->setAnomalyActive(14, true);
        break;

    default:
        break;
    }
}

// ============ 主函数 ============
int main() {
    // 设置控制台代码页为UTF-8
    system("chcp 65001 > nul");
    SetConsoleOutputCP(CP_UTF8);

    // 设置本地化
    locale::global(locale(""));
    wcout.imbue(locale(""));

    cout << "========================================" << endl;
    cout << "  Virtual Engine Performance Monitor" << endl;
    cout << "========================================" << endl;
    cout << "Initializing system..." << endl;

    try {
        // 初始化系统组件
        g_phase = make_unique<PhaseManager>();
        g_sim = make_unique<DataSimulator>(g_phase.get());
        g_logger = make_unique<DataLogger>();

        // 初始化日志系统
        if (!g_logger->init()) {
            cerr << "Error: Cannot create log file!" << endl;
            return -1;
        }
        cout << "[OK] Log system initialized" << endl;

        // 初始化监测系统
        auto detector = make_unique<AnomalyDetector>();
        auto alarmMgr = make_unique<AlarmManager>();

        detector->reset();
        alarmMgr->clear();

        cout << "[OK] Monitor system initialized" << endl;

        // 初始化图形窗口
        const int WINDOW_WIDTH = 800;
        const int WINDOW_HEIGHT = 600;
        initgraph(WINDOW_WIDTH, WINDOW_HEIGHT);

        BeginBatchDraw();

        cout << "[OK] Graphics window initialized" << endl;

        // 初始化仪表板
        auto dashboard = make_unique<Dashboard>(WINDOW_WIDTH, WINDOW_HEIGHT);
        g_dash = dashboard.get();
        cout << "[OK] Dashboard initialized" << endl;

        // 初始化鼠标处理器
        auto mouseHandler = make_unique<MouseHandler>(dashboard.get());
        mouseHandler->setCallback(handleButton);

        // 初始化定时器 (5ms更新间隔)
        Timer timer(5.0);
        cout << "[OK] Timer initialized" << endl;

        // 显示使用说明
        cout << "\nSystem started!" << endl;
        cout << "------------------------" << endl;
        cout << "Instructions:" << endl;
        cout << "  - Click START button to start engine" << endl;
        cout << "  - Click STOP button to stop engine (highest priority)" << endl;
        cout << "  - Click Thrust+/- to adjust thrust (stable phase only)" << endl;
        cout << "  - Press ESC to exit" << endl;
        cout << "------------------------\n" << endl;

        // 主循环
        double currentTime = 0.0;
        bool running = true;

        while (running) {
            // 处理鼠标事件
            mouseHandler->handleEvents();

            // 检查键盘输入 (ESC退出)
            if (_kbhit()) {
                int ch = _getch();
                if (ch == 27) { // ESC键
                    running = false;
                    break;
                }
            }

            // 更新时间
            currentTime += 0.005;

            // 更新发动机数据
            EngineData data = g_sim->update(0.005);

            // 检测异常
            vector<AnomalyInfo> anomalies =
                detector->detect(data, g_phase->getPhase(), currentTime);

            // 更新警报管理器
            alarmMgr->update(anomalies, currentTime);

            // 检查是否需要紧急关机
            if (alarmMgr->hasEmergency()) {
                cout << "Emergency detected, auto shutdown!" << endl;
                if (!g_phase->isStopped() && !g_phase->isStopping()) {
                    g_sim->stop();
                }
            }

            // 记录数据
            g_logger->logData(data);

            // 记录异常
            for (const auto& anomaly : anomalies) {
                g_logger->logAnomaly(anomaly, currentTime);
            }

            // 渲染界面
            dashboard->render(data, alarmMgr->getAlarms(), g_phase.get());

            // 刷新显示
            FlushBatchDraw();

            // 等待下一帧
            timer.wait();
        }

        // 清理资源
        cout << "\nShutting down system..." << endl;

        EndBatchDraw();

        g_logger->flush();
        g_logger->close();
        closegraph();

        cout << "System safely shut down. Thank you!" << endl;

    }
    catch (const exception& e) {
        cerr << "Exception occurred: " << e.what() << endl;
        return -1;
    }

    return 0;
}