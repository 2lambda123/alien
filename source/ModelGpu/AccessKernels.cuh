#pragma once

#include "device_functions.h"
#include "sm_60_atomic_functions.h"

#include "CudaAccessTOs.cuh"
#include "CudaConstants.cuh"
#include "Base.cuh"
#include "Map.cuh"
#include "EntityFactory.cuh"

#include "SimulationData.cuh"

__device__ void getClusterAccessData(int2 const& rectUpperLeft, int2 const& rectLowerRight,
    SimulationData const& data, DataAccessTO& simulationTO, int clusterIndex)
{
    Cluster const& cluster = data.clusters.getEntireArray()[clusterIndex];

    int startCellIndex, endCellIndex;
    calcPartition(cluster.numCellPointers, threadIdx.x, blockDim.x, startCellIndex, endCellIndex);

    __shared__ bool containedInRect;
    __shared__ BasicMap map;
    if (0 == threadIdx.x) {
        containedInRect = false;
        map.init(data.size);
    }
    __syncthreads();

    for (auto cellIndex = startCellIndex; cellIndex <= endCellIndex; ++cellIndex) {
        auto pos = cluster.cellPointers[cellIndex]->absPos;
        map.mapPosCorrection(pos);
        if (isContained(rectUpperLeft, rectLowerRight, pos)) {
            containedInRect = true;
        }
    }
    __syncthreads();

    if (containedInRect) {
        __shared__ int cellTOIndex;
        __shared__ int tokenTOIndex;
        __shared__ CellAccessTO* cellTOs;
        __shared__ TokenAccessTO* tokenTOs;
        if (0 == threadIdx.x) {
            int clusterAccessIndex = atomicAdd(simulationTO.numClusters, 1);
            cellTOIndex = atomicAdd(simulationTO.numCells, cluster.numCellPointers);
            cellTOs = &simulationTO.cells[cellTOIndex];

            tokenTOIndex = atomicAdd(simulationTO.numTokens, cluster.numTokens);
            tokenTOs = &simulationTO.tokens[tokenTOIndex];

            ClusterAccessTO& clusterTO = simulationTO.clusters[clusterAccessIndex];
            clusterTO.id = cluster.id;
            clusterTO.pos = cluster.pos;
            clusterTO.vel = cluster.vel;
            clusterTO.angle = cluster.angle;
            clusterTO.angularVel = cluster.angularVel;
            clusterTO.numCells = cluster.numCellPointers;
            clusterTO.numTokens = cluster.numTokens;
            clusterTO.cellStartIndex = cellTOIndex;
            clusterTO.tokenStartIndex = tokenTOIndex;
        }
        __syncthreads();

        for (auto cellIndex = startCellIndex; cellIndex <= endCellIndex; ++cellIndex) {
            Cell& cell = *cluster.cellPointers[cellIndex];
            cell.tag = cellIndex;
        }
        __syncthreads();

        for (auto cellIndex = startCellIndex; cellIndex <= endCellIndex; ++cellIndex) {
            Cell const& cell = *cluster.cellPointers[cellIndex];
            CellAccessTO& cellTO = cellTOs[cellIndex];
            cellTO.id = cell.id;
            cellTO.pos = cell.absPos;
            cellTO.energy = cell.energy;
            cellTO.maxConnections = cell.maxConnections;
            cellTO.numConnections = cell.numConnections;
            cellTO.branchNumber = cell.branchNumber;
            cellTO.tokenBlocked = cell.tokenBlocked;
            cellTO.cellFunctionType = cell.cellFunctionType;
            cellTO.numStaticBytes = cell.numStaticBytes;
            for (int i = 0; i < MAX_CELL_STATIC_BYTES; ++i) {
                cellTO.staticData[i] = cell.staticData[i];
            }
            cellTO.numMutableBytes = cell.numMutableBytes;
            for (int i = 0; i < MAX_CELL_MUTABLE_BYTES; ++i) {
                cellTO.mutableData[i] = cell.mutableData[i];
            }
            for (int i = 0; i < cell.numConnections; ++i) {
                int connectingCellIndex = cell.connections[i]->tag + cellTOIndex;
                cellTO.connectionIndices[i] = connectingCellIndex;
            }
        }

        int startTokenIndex, endTokenIndex;
        calcPartition(cluster.numTokens, threadIdx.x, blockDim.x, startTokenIndex, endTokenIndex);
        for (auto tokenIndex = startTokenIndex; tokenIndex <= endTokenIndex; ++tokenIndex) {
            Token const& token = cluster.tokens[tokenIndex];
            TokenAccessTO& tokenTO = tokenTOs[tokenIndex];
            tokenTO.energy = token.energy;
            for (int i = 0; i < cudaSimulationParameters.tokenMemorySize; ++i) {
                tokenTO.memory[i] = token.memory[i];
            }
            int tokenCellIndex = token.cell->tag + cellTOIndex;
            tokenTO.cellIndex = tokenCellIndex;
        }
    }
}

__device__ void getParticleAccessData(int2 const& rectUpperLeft, int2 const& rectLowerRight,
    SimulationData const& data, DataAccessTO& access, int particleIndex)
{
    Particle const& particle = data.particles.getEntireArray()[particleIndex];
    if (particle.pos.x >= rectUpperLeft.x
        && particle.pos.x <= rectLowerRight.x
        && particle.pos.y >= rectUpperLeft.y
        && particle.pos.y <= rectLowerRight.y)
    {
        int particleAccessIndex = atomicAdd(access.numParticles, 1);
        ParticleAccessTO& particleAccess = access.particles[particleAccessIndex];

        particleAccess.id = particle.id;
        particleAccess.pos = particle.pos;
        particleAccess.vel = particle.vel;
        particleAccess.energy = particle.energy;
    }
}

__global__ void getSimulationAccessData(int2 rectUpperLeft, int2 rectLowerRight,
    SimulationData data, DataAccessTO access)
{
    int indexResource = blockIdx.x;
    int numEntities = data.clusters.getNumEntries();
    int startIndex;
    int endIndex;
    calcPartition(numEntities, indexResource, gridDim.x, startIndex, endIndex);

    for (int clusterIndex = startIndex; clusterIndex <= endIndex; ++clusterIndex) {
        getClusterAccessData(rectUpperLeft, rectLowerRight, data, access, clusterIndex);
    }

    indexResource = threadIdx.x + blockIdx.x * blockDim.x;
    numEntities = data.particles.getNumEntries();

    calcPartition(numEntities, indexResource, blockDim.x * gridDim.x, startIndex, endIndex);
    for (int particleIndex = startIndex; particleIndex <= endIndex; ++particleIndex) {
        getParticleAccessData(rectUpperLeft, rectLowerRight, data, access, particleIndex);
    }

}

__device__ void filterClusterData(int2 const& rectUpperLeft, int2 const& rectLowerRight,
    SimulationData& data, int clusterIndex)
{
    Cluster const& cluster = data.clusters.getEntireArray()[clusterIndex];

    int startCellIndex;
    int endCellIndex;
    calcPartition(cluster.numCellPointers, threadIdx.x, blockDim.x, startCellIndex, endCellIndex);

    __shared__ bool containedInRect;
    if (0 == threadIdx.x) {
        containedInRect = false;
    }
    __syncthreads();

    for (auto cellIndex = startCellIndex; cellIndex <= endCellIndex; ++cellIndex) {
        Cell const& cell = *cluster.cellPointers[cellIndex];
        if (isContained(rectUpperLeft, rectLowerRight, cell.absPos)) {
            containedInRect = true;
        }
    }
    __syncthreads();

    if (!containedInRect) {
        __shared__ Token* newTokens;
        __shared__ Cluster* newCluster;
        if (0 == threadIdx.x) {
            newCluster = data.clustersNew.getNewElement();
            newTokens = data.tokensNew.getNewSubarray(cluster.numTokens);
            *newCluster = cluster;
            newCluster->tokens = newTokens;
        }
        __syncthreads();

/*
        for (auto cellIndex = startCellIndex; cellIndex <= endCellIndex; ++cellIndex) {
            Cell& cell = cluster.cells[cellIndex];
            newCells[cellIndex] = cell;
            newCells[cellIndex].cluster = newCluster;
            cell.nextTimestep = &newCells[cellIndex];
        }
        __syncthreads();

        for (auto cellIndex = startCellIndex; cellIndex <= endCellIndex; ++cellIndex) {
            Cell& newCell = newCells[cellIndex];
            int numConnections = newCell.numConnections;
            for (int i = 0; i < numConnections; ++i) {
                newCell.connections[i] = newCell.connections[i]->nextTimestep;
            }
        }
        __syncthreads();
*/

        if (newCluster->numTokens > 0) {
            int startTokenIndex;
            int endTokenIndex;
            calcPartition(cluster.numTokens, threadIdx.x, blockDim.x, startTokenIndex, endTokenIndex);

            for (auto tokenIndex = startTokenIndex; tokenIndex <= endTokenIndex; ++tokenIndex) {
                Token& token = cluster.tokens[tokenIndex];
                newTokens[tokenIndex] = token;
/*
                auto& tokenCell = newTokens[tokenIndex].cell;
                tokenCell = tokenCell->nextTimestep;
*/
            }
            __syncthreads();
        }
    }
}

__device__ void filterParticleData(int2 const& rectUpperLeft, int2 const& rectLowerRight,
    SimulationData& data, int particleIndex)
{
    Particle const& particle = data.particles.getEntireArray()[particleIndex];
    if (!isContained(rectUpperLeft, rectLowerRight, particle.pos)) {
        Particle* newParticle = data.particlesNew.getNewElement();
        *newParticle = particle;
    }
}

__global__ void filterData(int2 rectUpperLeft, int2 rectLowerRight, SimulationData data)
{
    int indexResource = blockIdx.x;
    int numEntities = data.clusters.getNumEntries();
    int startIndex;
    int endIndex;
    calcPartition(numEntities, indexResource, gridDim.x, startIndex, endIndex);
    for (int clusterIndex = startIndex; clusterIndex <= endIndex; ++clusterIndex) {
        filterClusterData(rectUpperLeft, rectLowerRight, data, clusterIndex);
    }

    indexResource = threadIdx.x + blockIdx.x * blockDim.x;
    numEntities = data.particles.getNumEntries();
    calcPartition(numEntities, indexResource, blockDim.x * gridDim.x, startIndex, endIndex);
    for (int particleIndex = startIndex; particleIndex <= endIndex; ++particleIndex) {
        filterParticleData(rectUpperLeft, rectLowerRight, data, particleIndex);
    }
    __syncthreads();
}


__device__ void convertData(SimulationData data, DataAccessTO const& simulationTO)
{
    __shared__ EntityFactory factory;
    if (0 == threadIdx.x) {
        factory.init(&data);
    }
    __syncthreads();

    int indexResource = blockIdx.x;
    int numEntities = *simulationTO.numClusters;
    int startIndex;
    int endIndex;
    calcPartition(numEntities, indexResource, gridDim.x, startIndex, endIndex);

    for (int clusterIndex = startIndex; clusterIndex <= endIndex; ++clusterIndex) {
        factory.createClusterFromTO_blockCall(simulationTO.clusters[clusterIndex], &simulationTO);
    }

    indexResource = threadIdx.x + blockIdx.x * blockDim.x;
    numEntities = *simulationTO.numParticles;
    calcPartition(numEntities, indexResource, blockDim.x * gridDim.x, startIndex, endIndex);
    for (int particleIndex = startIndex; particleIndex <= endIndex; ++particleIndex) {
        factory.createParticleFromTO(simulationTO.particles[particleIndex], &simulationTO);
    }
}

__global__ void setSimulationAccessData(int2 rectUpperLeft, int2 rectLowerRight,
    SimulationData data, DataAccessTO access)
{
    convertData(data, access);
}
