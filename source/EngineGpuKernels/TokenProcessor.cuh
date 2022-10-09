#pragma once

#include "cuda_runtime_api.h"

#include "AccessTOs.cuh"
#include "Base.cuh"
#include "EntityFactory.cuh"
#include "Map.cuh"
#include "Physics.cuh"
#include "EnergyGuidance.cuh"
#include "CellComputationProcessor.cuh"
#include "ConstructionProcessor.cuh"
#include "ScannerProcessor.cuh"
#include "DigestionProcessor.cuh"
#include "NeuralNetProcessor.cuh"
#include "MuscleProcessor.cuh"
#include "SensorProcessor.cuh"

class TokenProcessor
{
public:
    __inline__ __device__ void movement(SimulationData& data);  //prerequisite: cell tags = 0

    __inline__ __device__ void applyMutation(SimulationData& data);
    __inline__ __device__ void executeReadonlyCellFunctions(SimulationData& data, SimulationResult& result);  //energy values are allowed to change
    __inline__ __device__ void executeModifyingCellFunctions(SimulationData& data, SimulationResult& result);
    __inline__ __device__ void deleteTokenIfCellDeleted(SimulationData& data);
};

/************************************************************************/
/* Implementation                                                       */
/************************************************************************/

__inline__ __device__ void TokenProcessor::movement(SimulationData& data)
{
    auto& tokens = data.entities.tokenPointers;
    auto const partition = calcAllThreadsPartition(tokens.getNumOrigEntries());

    for (int index = partition.startIndex; index <= partition.endIndex; ++index) {
        auto& token = tokens.at(index);
        auto cell = token->cell;
        atomicAdd(&cell->cellFunctionInvocations, 1);

        int numNextTokenCells = 0;
        Cell* nextTokenCells[MAX_CELL_BONDS];
        EntityFactory factory;
        factory.init(&data);

        if (token->energy >= cudaSimulationParameters.tokenMinEnergy) {
            auto tokenBranchNumber = token->getTokenBranchNumber();

            auto cellMinEnergy =
                SpotCalculator::calcParameter(&SimulationParametersSpotValues::cellMinEnergy, data, cell->absPos);

            for (int i = 0; i < cell->numConnections; ++i) {
                auto const& connectedCell = cell->connections[i].cell;
                if (((tokenBranchNumber + 1 - connectedCell->branchNumber) % cudaSimulationParameters.cellMaxTokenBranchNumber) != 0) {
                    continue;
                }
                if (connectedCell->tokenBlocked) {
                    continue;
                }

                auto tokenIndex = atomicAdd(&connectedCell->tag, 1);
                if (tokenIndex >= cudaSimulationParameters.cellMaxToken) {
                    continue;
                }
                nextTokenCells[numNextTokenCells++] = connectedCell;
            }

            auto tokenEnergy = numNextTokenCells > 0 ? token->energy / numNextTokenCells : token->energy;

            for (int i = 0; i < numNextTokenCells; ++i) {
                auto const& connectedCell = nextTokenCells[i];

                SystemDoubleLock lock;
                lock.init(&cell->locked, &connectedCell->locked);
                if (lock.tryLock()) {
                    auto averageEnergy = (cell->energy + connectedCell->energy) / 2;
                    cell->energy = averageEnergy;
                    connectedCell->energy = averageEnergy;
                    lock.releaseLock();
                }
                token->memory[Enums::Branching_TokenBranchNumber] = connectedCell->branchNumber;
                
                auto newToken = token;
                if (0 == i) {
                    token->sourceCell = token->cell;
                    token->cell = connectedCell;
                } else {
                    newToken = factory.duplicateToken(connectedCell, token);
                }
                newToken->energy = tokenEnergy;

                //token has too low energy? => try to steal energy from underlying cell
                if (tokenEnergy < cudaSimulationParameters.tokenMinEnergy) {
                    if (cell->tryLock()) {
                        if (cell->energy > cudaSimulationParameters.tokenMinEnergy + cellMinEnergy + 0.1f) {
                            auto energyToTransfer = cudaSimulationParameters.tokenMinEnergy + 0.1f - tokenEnergy;
                            cell->energy -= energyToTransfer;
                            token->energy += energyToTransfer;
                        }
                        cell->releaseLock();
                    }
                }
            }
        }
        if (0 == numNextTokenCells) {
            //transferring energy needs a lock => make a certain number of attempts
            for (int i = 0; i < 100; ++i) {
                if (cell->tryLock()) {
                    cell->energy += token->energy;
                    token = nullptr;
                    cell->releaseLock();
                    break;
                }
            }
        }
    }
}

__inline__ __device__ void TokenProcessor::applyMutation(SimulationData& data)
{
    auto& tokens = data.entities.tokenPointers;
    auto const partition = calcAllThreadsPartition(tokens.getNumOrigEntries());

    for (int index = partition.startIndex; index <= partition.endIndex; ++index) {
        auto& token = tokens.at(index);
        auto const& cell = token->cell;
        auto mutationRate = SpotCalculator::calcParameter(&SimulationParametersSpotValues::tokenMutationRate, data, cell->absPos);
        if (data.numberGen1.random() < mutationRate) {
            token->memory[data.numberGen1.random(MAX_TOKEN_MEM_SIZE - 1)] = data.numberGen1.random(255);
        }
    }
}

__inline__ __device__ void TokenProcessor::executeReadonlyCellFunctions(SimulationData& data, SimulationResult& result)
{
    auto& tokens = data.entities.tokenPointers;
    auto partition =
        calcPartition(tokens.getNumEntries(), threadIdx.x + blockIdx.x * blockDim.x, blockDim.x * gridDim.x);

    for (int index = partition.startIndex; index <= partition.endIndex; ++index) {
        auto& token = tokens.at(index);
        auto& cell = token->cell;
        if (token) {
            auto cellFunctionType = cell->getCellFunctionType();
            if (Enums::CellFunction_Scanner == cellFunctionType) {
                ScannerProcessor::process(token, data);
            }
            if (Enums::CellFunction_Sensor == cellFunctionType) {
                SensorProcessor::scheduleOperation(token, data);
            }
/*
            if (Enums::CellFunction_NeuralNet == cellFunctionType) {
                NeuralNetProcessor::scheduleOperation(token, data);
            }
*/
            if (Enums::CellFunction_Digestion == cellFunctionType) {  //modifies energy
                DigestionProcessor::process(token, data, result);
            }
        }
    }
}

__inline__ __device__ void
TokenProcessor::executeModifyingCellFunctions(SimulationData& data, SimulationResult& result)
{
    auto& tokens = data.entities.tokenPointers;
    auto const partition = calcAllThreadsPartition(tokens.getNumOrigEntries());

    for (int index = partition.startIndex; index <= partition.endIndex; ++index) {
        auto& token = tokens.at(index);
        auto& cell = token->cell;
        if (token) {
            auto cellFunctionType = cell->getCellFunctionType();

            //cell functions need a lock since they should be executed consecutively on a cell
            //make a certain number of attempts
            for (int i = 0; i < 100; ++i) {
                if (cell->tryLock()) {

                    EnergyGuidance::processing(data, token);
                    if (Enums::CellFunction_Computation == cellFunctionType) {
                        CellComputationProcessor::process(token);
                    }
                    if (Enums::CellFunction_Constructor == cellFunctionType) {
                        ConstructionProcessor::process(token, data, result);
                    }
                    if (Enums::CellFunction_Muscle == cellFunctionType) {
                        MuscleProcessor::process(token, data, result);
                    }

                    cell->releaseLock();
                    break;
                }
            }
        }
    }
}

__inline__ __device__ void TokenProcessor::deleteTokenIfCellDeleted(SimulationData& data)
{
    auto& tokens = data.entities.tokenPointers;
    auto partition =
        calcPartition(tokens.getNumEntries(), threadIdx.x + blockIdx.x * blockDim.x, blockDim.x * gridDim.x);

    for (int index = partition.startIndex; index <= partition.endIndex; ++index) {
        if (auto& token = tokens.at(index)) {
            auto& cell = token->cell;
            if (cell->isDeleted()) {
                EntityFactory factory;
                factory.init(&data);
                factory.createParticle(token->energy, cell->absPos, cell->vel, {cell->metadata.color});

                token = nullptr;
            }
        }
    }
}