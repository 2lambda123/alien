#include "StatisticsHistory.h"

#include "EngineInterface/MonitorData.h"

#include <imgui.h>

void LiveStatistics::truncate()
{
    if (!timepointsHistory.empty() && timepointsHistory.back() - timepointsHistory.front() > (MaxLiveHistory + 1.0f)) {
        timepointsHistory.erase(timepointsHistory.begin());
        for(auto& data : datas) {
            data.erase(data.begin());
        }
    }
}

void LiveStatistics::add(MonitorData const& newStatistics)
{
    truncate();

    timepoint += ImGui::GetIO().DeltaTime;
    timepointsHistory.emplace_back(timepoint);
    int numCells = 0;
    for (int i = 0; i < 7; ++i) {
        numCells += newStatistics.timestepData.numCellsByColor[i];
    }
    datas[0].emplace_back(toFloat(numCells));
    for (int i = 0; i < 7; ++i) {
        datas[1 + i].emplace_back(toFloat(newStatistics.timestepData.numCellsByColor[i]));
    }
    datas[8].emplace_back(toFloat(newStatistics.timestepData.numConnections));
    datas[9].emplace_back(toFloat(newStatistics.timestepData.numParticles));
    datas[10].emplace_back(newStatistics.timeIntervalData.numCreatedCells);
    datas[11].emplace_back(newStatistics.timeIntervalData.numSuccessfulAttacks);
    datas[12].emplace_back(newStatistics.timeIntervalData.numFailedAttacks);
    datas[13].emplace_back(newStatistics.timeIntervalData.numMuscleActivities);
}

void LongtermStatistics::add(MonitorData const& newStatistics)
{
    if (timestepHistory.empty() || newStatistics.timestep - timestepHistory.back() > LongtermTimestepDelta) {
        timestepHistory.emplace_back(toFloat(newStatistics.timestep));
        int numCells = 0;
        for (int i = 0; i < 7; ++i) {
            numCells += newStatistics.timestepData.numCellsByColor[i];
        }
        datas[0].emplace_back(toFloat(numCells));
        for (int i = 0; i < 7; ++i) {
            datas[1 + i].emplace_back(toFloat(newStatistics.timestepData.numCellsByColor[i]));
        }
        datas[8].emplace_back(toFloat(newStatistics.timestepData.numConnections));
        datas[9].emplace_back(toFloat(newStatistics.timestepData.numParticles));

        datas[10].emplace_back(accumulatedCreatedCells / numberOfAccumulation);
        datas[11].emplace_back(accumulatedSuccessfulAttacks / numberOfAccumulation);
        datas[12].emplace_back(accumulatedFailedAttack / numberOfAccumulation);
        datas[13].emplace_back(accumulatedMuscleActivities / numberOfAccumulation);
        accumulatedCreatedCells = 0;
        accumulatedSuccessfulAttacks = 0;
        accumulatedFailedAttack = 0;
        accumulatedMuscleActivities = 0;
        numberOfAccumulation = 1;
    } else {
        accumulatedCreatedCells += newStatistics.timeIntervalData.numCreatedCells;
        accumulatedSuccessfulAttacks += newStatistics.timeIntervalData.numSuccessfulAttacks;
        accumulatedFailedAttack += newStatistics.timeIntervalData.numFailedAttacks;
        accumulatedMuscleActivities += newStatistics.timeIntervalData.numMuscleActivities;
        ++numberOfAccumulation;
    }
}
