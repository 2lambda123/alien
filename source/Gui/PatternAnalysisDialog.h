#pragma once

#include "EngineInterface/Descriptions.h"
#include "Definitions.h"

class _PatternAnalysisDialog
{
public:
    _PatternAnalysisDialog(SimulationController const& simController);
    ~_PatternAnalysisDialog();

    void process();
    void show();

private:
    void saveRepetitiveActiveClustersToFiles(std::string const& filename);

    struct CellFeatureAnalysisDescription
    {
        int cellFunction;

        bool operator<(CellFeatureAnalysisDescription const& other) const
        {
            if (cellFunction != other.cellFunction) {
                return cellFunction < other.cellFunction;
            }
            return false;
        }
        bool operator==(CellFeatureAnalysisDescription const& other) const { return cellFunction == other.cellFunction; }
        bool operator!=(CellFeatureAnalysisDescription const& other) const { return !operator==(other); }
    };
    struct CellAnalysisDescription
    {
        int maxConnections;
        int numConnections;
        bool tokenBlocked;
        int tokenBranchNumber;
        int color;
        CellFeatureAnalysisDescription feature;

        bool operator==(CellAnalysisDescription const& other) const
        {
            return maxConnections == other.maxConnections && numConnections == other.numConnections && tokenBlocked == other.tokenBlocked
                && tokenBranchNumber == other.tokenBranchNumber && feature == other.feature
                /*&& color == other.color*/;
        }

        bool operator!=(CellAnalysisDescription const& other) const { return !operator==(other); }

        bool operator<(CellAnalysisDescription const& other) const
        {
            if (maxConnections != other.maxConnections) {
                return maxConnections < other.maxConnections;
            }
            if (numConnections != other.numConnections) {
                return numConnections < other.numConnections;
            }
            if (tokenBlocked != other.tokenBlocked) {
                return tokenBlocked < other.tokenBlocked;
            }
            if (tokenBranchNumber != other.tokenBranchNumber) {
                return tokenBranchNumber < other.tokenBranchNumber;
            }
            if (feature != other.feature) {
                return feature < other.feature;
            }
            /*
            if (color != other.color) {
                return color < other.color;
            }
*/
            return false;
        }
    };
    struct ClusterAnalysisDescription
    {
        bool hasToken;
        std::set<std::set<CellAnalysisDescription>> connectedCells;

        bool operator<(ClusterAnalysisDescription const& other) const
        {
            if (hasToken != other.hasToken) {
                return hasToken < other.hasToken;
            }
            if (connectedCells != other.connectedCells) {
                return connectedCells < other.connectedCells;
            }
            return false;
        }
    };
    struct PartitionClassData
    {
        int numberOfElements = 0;
        ClusterDescription representant;

        bool operator<(PartitionClassData const& other) const { return numberOfElements < other.numberOfElements; };
    };

    std::map<ClusterAnalysisDescription, PartitionClassData> calcPartitionData() const;

    ClusterAnalysisDescription getAnalysisDescription(ClusterDescription const& cluster) const;

private:
    SimulationController _simController;

    std::string _startingPath;
};
