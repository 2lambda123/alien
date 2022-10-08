#pragma once

namespace Enums
{
    using Branching = int;
    enum Branching_
    {
        Branching_TokenBranchNumber = 0
    };

    using CellFunction = int;
    enum CellFunction_
    {
        CellFunction_Computation,
        CellFunction_NeuralNet,
        CellFunction_Scanner,
        CellFunction_Digestion,
        CellFunction_Constructor,
        CellFunction_Sensor,
        CellFunction_Muscle,
        CellFunction_Count
    };

    using EnergyGuidance = int;
    enum EnergyGuidance_
    {
        EnergyGuidance_Input = 1,
        EnergyGuidance_InValueCell = 2,
        EnergyGuidance_InValueToken = 3
    };

    using EnergyGuidanceIn = int;
    enum EnergyGuidanceIn_
    {
        EnergyGuidanceIn_Deactivated,
        EnergyGuidanceIn_BalanceCell,
        EnergyGuidanceIn_BalanceToken,
        EnergyGuidanceIn_BalanceBoth,
        EnergyGuidanceIn_HarvestCell,
        EnergyGuidanceIn_HarvestToken,
        EnergyGuidanceIn_Count
    };

    using ComputationOperation = int;
    enum ComputationOperation_
    {
        ComputationOperation_Mov,
        ComputationOperation_Add,
        ComputationOperation_Sub,
        ComputationOperation_Mul,
        ComputationOperation_Div,
        ComputationOperation_Xor,
        ComputationOperation_Or,
        ComputationOperation_And,
        ComputationOperation_Ifg,
        ComputationOperation_Ifge,
        ComputationOperation_Ife,
        ComputationOperation_Ifne,
        ComputationOperation_Ifle,
        ComputationOperation_Ifl,
        ComputationOperation_Else,
        ComputationOperation_Endif
    };
    using ComputationOpType = int;
    enum ComputationOpType_
    {
        ComputationOpType_Mem,
        ComputationOpType_MemMem,
        ComputationOpType_Cmem,
        ComputationOpType_Constant
    };

    using Constr = int;
    enum Constr_ {
        Constr_Output = 5,
        Constr_Input = 6,
        Constr_InOption = 7,
        Constr_InAngleAlignment = 38,  //0: no alignment, 2: alignment to 180 deg, 3: alignment to 120 deg, ... up to 6
        Constr_InUniformDist = 13,
        Constr_InOutAngle = 15,
        Constr_InDist = 16,
        Constr_InCellMaxConnections = 17,  //0: automatically; >0: max connections (not greater than MAX_CELL_CONNECTIONS)
        Constr_InCellBranchNumber = 18,
        Constr_InCellColor = 19,
        Constr_InCellFunction = 39,
        Constr_InCellFunctionData = 40
    };
    using ConstrOut = int;
    enum ConstrOut_
    {
        ConstrOut_Success,
        ConstrOut_ErrorNoEnergy,
        ConstrOut_ErrorConnection,
        ConstrOut_ErrorLock,
        ConstrOut_ErrorDist
    };
    using ConstrIn = int;
    enum ConstrIn_
    {
        ConstrIn_DoNothing,
        ConstrIn_Construct,
        ConstrIn_Count
    };
    using ConstrInOption = int;
    enum ConstrInOption_
    {
        ConstrInOption_Standard,
        ConstrInOption_CreateEmptyToken,
        ConstrInOption_CreateDupToken,
        ConstrInOption_FinishNoSep,
        ConstrInOption_FinishWithSep,
        ConstrInOption_FinishWithEmptyTokenSep,
        ConstrInOption_FinishWithDupTokenSep,
        ConstrInOption_Count
    };

    using ConstrInUniformDist = int;
    enum ConstrInUniformDist_
    {
        ConstrInUniformDist_No,
        ConstrInUniformDist_Yes,
        ConstrInUniformDist_Count
    };

    using NeuralNet = int;
    enum NeuralNet_
    {
        NeuralNet_InOut = 110,
    };

    using Scanner = int;
    enum Scanner_
    {
        Scanner_Output = 5,
        Scanner_InOutCellNumber = 12,
        Scanner_OutEnergy = 14,
        Scanner_OutAngle = 15,
        Scanner_OutDistance = 16,
        Scanner_OutCellMaxConnections = 17,
        Scanner_OutCellBranchNumber = 18,
        Scanner_OutCellColor = 19,
        Scanner_OutCellFunction = 39,
        Scanner_OutCellFunctionData = 40
    };
    using ScannerOut = int;
    enum ScannerOut_
    {
        ScannerOut_Success,
        ScannerOut_Finished
    };

    using Sensor = int;
    enum Sensor_
    {
        Sensor_Output = 5,
        Sensor_Input = 20,
        Sensor_InOutAngle = 21,
        Sensor_InMinDensity = 22,
        Sensor_InMaxDensity = 23,
        Sensor_InColor = 8,
        Sensor_OutDensity = 24,
        Sensor_OutDistance = 25
    };
    using SensorIn = int;
    enum SensorIn_
    {
        SensorIn_DoNothing,
        SensorIn_SearchVicinity,
        SensorIn_SearchByAngle,
        SensorIn_Count
    };
    using SensorOut = int;
    enum SensorOut_
    {
        SensorOut_NothingFound,
        SensorOut_ClusterFound
    };

    using Digestion = int;
    enum Digestion_
    {
        Digestion_Output = 5,
        Digestion_InColor = 8
    };
    using DigestionOut = int;
    enum DigestionOut_
    {
        DigestionOut_NoTarget,
        DigestionOut_Success,
        DigestionOut_Poisoned
    };

    using Muscle = int;
    enum Muscle_
    {
        Muscle_Output = 5,
        Muscle_Input = 36,
    };
    using MuscleOut = int;
    enum MuscleOut_
    {
        MuscleOut_Success,
        MuscleOut_LimitReached
    };
    using MuscleIn = int;
    enum MuscleIn_
    {
        MuscleIn_DoNothing,
        MuscleIn_Contract,
        MuscleIn_ContractRelax,
        MuscleIn_Expand,
        MuscleIn_ExpandRelax,
        MuscleIn_Count
    };
}
