﻿#include "MonitorKernelsLauncher.cuh"

#include "MonitorKernels.cuh"

void _MonitorKernelsLauncher::getMonitorData(GpuSettings const& gpuSettings, SimulationData const& data, CudaMonitor const& monitorData)
{
    KERNEL_CALL_1_1(cudaGetCudaMonitorData_substep1, data, monitorData);
    KERNEL_CALL(cudaGetCudaMonitorData_substep2, data, monitorData);
    KERNEL_CALL_1_1(cudaGetCudaMonitorData_substep3, data, monitorData);
}
