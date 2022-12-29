#include "SimulationParametersWindow.h"

#include <imgui.h>

#include "EngineInterface/SimulationController.h"

#include "AlienImGui.h"
#include "StyleRepository.h"
#include "GlobalSettings.h"
#include "SimulationParametersChanger.h"

namespace
{
    auto const MaxContentTextWidth = 260.0f;

    template <int numRows, int numCols>
    std::vector<std::vector<float>> toVector(float const v[numRows][numCols])
    {
        std::vector<std::vector<float>> result;
        for (int row = 0; row < numRows; ++row) {
            std::vector<float> rowVector;
            for (int col = 0; col < numCols; ++col) {
                rowVector.emplace_back(v[row][col]);
            }
            result.emplace_back(rowVector);
        }
        return result;
    }

    template<int numElements>
    std::vector<float> toVector(float const v[numElements])
    {
        std::vector<float> result;
        for (int i = 0; i < numElements; ++i) {
            result.emplace_back(v[i]);
        }
        return result;
    }
}

_SimulationParametersWindow::_SimulationParametersWindow(SimulationController const& simController)
    : _AlienWindow("Simulation parameters", "windows.simulation parameters", false)
    , _simController(simController)
{
    for (int n = 0; n < IM_ARRAYSIZE(_savedPalette); n++) {
        ImVec4 color;
        ImGui::ColorConvertHSVtoRGB(n / 31.0f, 0.8f, 0.2f, color.x, color.y, color.z);
        color.w = 1.0f; //alpha
        _savedPalette[n] = static_cast<ImU32>(ImColor(color));
    }

    auto timestepsPerEpoch = GlobalSettings::getInstance().getIntState("windows.simulation parameters.time steps per epoch", 10000);

    _simulationParametersChanger = std::make_shared<_SimulationParametersChanger>(simController, timestepsPerEpoch);
}

_SimulationParametersWindow::~_SimulationParametersWindow()
{
    GlobalSettings::getInstance().setIntState("windows.simulation parameters.time steps per epoch", _simulationParametersChanger->getTimestepsPerEpoch());
}

void _SimulationParametersWindow::processIntern()
{
    auto simParameters = _simController->getSimulationParameters();
    auto origSimParameters = _simController->getOriginalSimulationParameters();
    auto lastSimParameters = simParameters;

    auto simParametersSpots = _simController->getSimulationParametersSpots();
    auto origSimParametersSpots = _simController->getOriginalSimulationParametersSpots();
    auto lastSimParametersSpots = simParametersSpots;

    if (ImGui::BeginChild("##", ImVec2(0, ImGui::GetContentRegionAvail().y - StyleRepository::getInstance().scaleContent(78)), false)) {

        if (ImGui::BeginTabBar("##Flow", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_FittingPolicyResizeDown)) {

            if (simParametersSpots.numSpots < 2) {
                if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
                    int index = simParametersSpots.numSpots;
                    simParametersSpots.spots[index] = createSpot(simParameters, index);
                    _simController->setOriginalSimulationParametersSpot(simParametersSpots.spots[index], index);
                    ++simParametersSpots.numSpots;
                }
                AlienImGui::Tooltip("Add spot");
            }

            if (ImGui::BeginTabItem("Base", nullptr, ImGuiTabItemFlags_None)) {
                processBase(simParameters, origSimParameters);
                ImGui::EndTabItem();
            }

            for (int tab = 0; tab < simParametersSpots.numSpots; ++tab) {
                SimulationParametersSpot& spot = simParametersSpots.spots[tab];
                SimulationParametersSpot const& origSpot = origSimParametersSpots.spots[tab];
                bool open = true;
                std::string name = "Spot " + std::to_string(tab+1);
                if (ImGui::BeginTabItem(name.c_str(), &open, ImGuiTabItemFlags_None)) {
                    processSpot(spot, origSpot);
                    ImGui::EndTabItem();
                }

                if (!open) {
                    for (int i = tab; i < simParametersSpots.numSpots - 1; ++i) {
                        simParametersSpots.spots[i] = simParametersSpots.spots[i + 1];
                        _simController->setOriginalSimulationParametersSpot(simParametersSpots.spots[i], i);
                    }
                    --simParametersSpots.numSpots;
                }
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::EndChild();

    AlienImGui::Separator();
    if (AlienImGui::ToggleButton(AlienImGui::ToggleButtonParameters().name("Change automatically"), _changeAutomatically)) {
        if (_changeAutomatically) {
            _simulationParametersChanger->activate();
        } else {
            _simulationParametersChanger->deactivate();
        }
    }
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::BeginDisabled(!_changeAutomatically);
    auto timestepsPerEpoch = _simulationParametersChanger->getTimestepsPerEpoch();
    if(AlienImGui::InputInt(
            AlienImGui::InputIntParameters()
                .name("Epoch time steps")
                .defaultValue(_simulationParametersChanger->getOriginalTimestepsPerEpoch())
                .textWidth(MaxContentTextWidth)
                .tooltip("Duration in time steps after which a change is applied."),
        timestepsPerEpoch)) {
        _simulationParametersChanger->setTimestepsPerEpoch(timestepsPerEpoch);
    }
    ImGui::EndDisabled();

    if (simParameters != lastSimParameters) {
        _simController->setSimulationParameters_async(simParameters);
    }

    if (simParametersSpots != lastSimParametersSpots) {
        _simController->setSimulationParametersSpots_async(simParametersSpots);
    }
}

void _SimulationParametersWindow::processBackground()
{
    _simulationParametersChanger->process();
}

SimulationParametersSpot _SimulationParametersWindow::createSpot(SimulationParameters const& simParameters, int index)
{
    auto worldSize = _simController->getWorldSize();
    SimulationParametersSpot spot;
    spot.posX = toFloat(worldSize.x / 2);
    spot.posY = toFloat(worldSize.y / 2);

    auto maxRadius = toFloat(std::min(worldSize.x, worldSize.y)) / 2;
    spot.coreRadius = maxRadius / 3;
    spot.fadeoutRadius = maxRadius / 3;
    spot.color = _savedPalette[(2 + index) * 8];

    spot.values = simParameters.spotValues;
    return spot;
}

void _SimulationParametersWindow::processBase(
    SimulationParameters& simParameters,
    SimulationParameters const& origSimParameters)
{
    if (ImGui::BeginChild("##", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {

        /**
         * Colors
         */
        AlienImGui::Group("Color");
        AlienImGui::ColorButtonWithPicker(
            AlienImGui::ColorButtonWithPickerParameters().name("Background color").textWidth(MaxContentTextWidth).defaultValue(origSimParameters.spaceColor),
            simParameters.spaceColor,
            _backupColor,
            _savedPalette);

        /**
         * Numerics
         */
        AlienImGui::Group("Numerics");
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Time step size")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(1.0f)
                .defaultValue(origSimParameters.timestepSize)
                .tooltip(std::string("Time duration calculated in a single step. Smaller values increase the accuracy "
                                     "of the simulation.")),
            simParameters.timestepSize);

        /**
         * Physics: General
         */
        AlienImGui::Group("Physics: General");
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Friction")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(1.0f)
                .logarithmic(true)
                .format("%.4f")
                .defaultValue(origSimParameters.spotValues.friction)
                .tooltip(std::string("Specifies how much the movements are slowed down per time step.")),
            simParameters.spotValues.friction);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Rigidity")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(1.0f)
                .format("%.2f")
                .defaultValue(origSimParameters.spotValues.rigidity)
                .tooltip(std::string("Controls the rigidity of connected cells.\nA higher value will cause connected cells to move more uniformly.")),
            simParameters.spotValues.rigidity);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Maximum velocity")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(6.0f)
                .defaultValue(origSimParameters.cellMaxVelocity)
                .tooltip(std::string("Maximum velocity that a cell can reach.")),
            simParameters.cellMaxVelocity);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Maximum force")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(3.0f)
                .defaultValue(origSimParameters.spotValues.cellMaxForce)
                .tooltip(std::string("Maximum force that can be applied to a cell without causing it to disintegrate.")),
            simParameters.spotValues.cellMaxForce);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Minimum distance")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(1.0f)
                .defaultValue(origSimParameters.cellMinDistance)
                .tooltip(std::string("Minimum distance between two cells without them annihilating each other.")),
            simParameters.cellMinDistance);

        /**
         * Physics: Radiation
         */
        AlienImGui::Group("Physics: Radiation");
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Radiation factor")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(0.01f)
                .logarithmic(true)
                .format("%.6f")
                .defaultValue(origSimParameters.spotValues.radiationFactor)
                .tooltip(std::string("Indicates how energetic the emitted particles of cells are.")),
            simParameters.spotValues.radiationFactor);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Minimum energy")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(100000)
                .logarithmic(true)
                .defaultValue(origSimParameters.radiationMinCellEnergy),
            simParameters.radiationMinCellEnergy);
        AlienImGui::SliderInt(
            AlienImGui::SliderIntParameters()
                .name("Minimum age")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(1000000)
                .logarithmic(true)
                .defaultValue(origSimParameters.radiationMinCellAge),
            simParameters.radiationMinCellAge);
        AlienImGui::InputColorVector(
            AlienImGui::InputColorVectorParameters()
                .name("Absorption factors")
                .textWidth(MaxContentTextWidth)
                .defaultValue(toVector<MAX_COLORS>(origSimParameters.radiationAbsorptionByCellColor)),
            simParameters.radiationAbsorptionByCellColor);

        /**
         * Physics: Particle transformation
         */
        ImGui::PushID("Transformation");
        AlienImGui::Group("Physics: Particle transformation");
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Minimum energy")
                .textWidth(MaxContentTextWidth)
                .min(10.0f)
                .max(200.0f)
                .defaultValue(origSimParameters.spotValues.cellMinEnergy)
                .tooltip(std::string("Minimum energy a cell needs to exist.")),
            simParameters.spotValues.cellMinEnergy);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Normal energy")
                .textWidth(MaxContentTextWidth)
                .min(10.0f)
                .max(200.0f)
                .defaultValue(origSimParameters.cellNormalEnergy),
            simParameters.cellNormalEnergy);
        AlienImGui::Checkbox(
            AlienImGui::CheckboxParameters()
                .name("Cell cluster decay")
                .textWidth(MaxContentTextWidth).defaultValue(origSimParameters.clusterDecay),
            simParameters.clusterDecay);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Cell cluster decay probability")
                .textWidth(MaxContentTextWidth)
                .min(0.0)
                .max(1.0f)
                .defaultValue(origSimParameters.clusterDecayProb),
            simParameters.clusterDecayProb);
        ImGui::PopID();

        /**
         * Physics: Collision and binding
         */
        AlienImGui::Group("Physics: Collision and binding");
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Repulsion strength")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(0.3f)
                .defaultValue(origSimParameters.cellRepulsionStrength)
                .tooltip(std::string("The strength of the repulsive forces, between two cells that do not connect.")),
            simParameters.cellRepulsionStrength);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Maximum collision distance")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(3.0f)
                .defaultValue(origSimParameters.cellMaxCollisionDistance)
                .tooltip(std::string("Maximum distance up to which a collision of two cells is possible.")),
            simParameters.cellMaxCollisionDistance);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Maximum binding distance")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(5.0f)
                .defaultValue(origSimParameters.cellMaxBindingDistance)
                .tooltip(std::string("Maximum distance up to which a connection of two cells is possible.")),
            simParameters.cellMaxBindingDistance);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Binding creation velocity")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(1.0f)
                .defaultValue(origSimParameters.spotValues.cellFusionVelocity)
                .tooltip(std::string("Minimum velocity of two colliding cells so that a connection can be established.")),
            simParameters.spotValues.cellFusionVelocity);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Binding maximum energy")
                .textWidth(MaxContentTextWidth)
                .min(50.0f)
                .max(1000000.0f)
                .logarithmic(true)
                .format("%.0f")
                .defaultValue(origSimParameters.spotValues.cellMaxBindingEnergy)
                .tooltip(std::string("Maximum energy of a cell at which they can maintain a connection.")),
            simParameters.spotValues.cellMaxBindingEnergy);
        if (simParameters.spotValues.cellMaxBindingEnergy < simParameters.spotValues.cellMinEnergy + 10.0f) {
            simParameters.spotValues.cellMaxBindingEnergy = simParameters.spotValues.cellMinEnergy + 10.0f;
        }
        AlienImGui::SliderInt(
            AlienImGui::SliderIntParameters()
                .name("Maximum cell bonds")
                .textWidth(MaxContentTextWidth)
                .defaultValue(origSimParameters.cellMaxBonds)
                .min(0)
                .max(6)
                .tooltip(std::string("Maximum number of connections a cell can establish with others.")),
            simParameters.cellMaxBonds);

        /**
         * Cell color transition rules
         */
        AlienImGui::Group("Cell color transition rules");
        for (int color = 0; color < MAX_COLORS; ++color) {
            ImGui::PushID(color);
            auto parameters = AlienImGui::InputColorTransitionParameters()
                                  .textWidth(MaxContentTextWidth)
                                  .color(color)
                                  .defaultTargetColor(origSimParameters.spotValues.cellColorTransitionTargetColor[color])
                                  .defaultTransitionAge(origSimParameters.spotValues.cellColorTransitionDuration[color])
                                  .logarithmic(true);
            if (0 == color) {
                parameters.name("Target color and duration")
                    .tooltip("Rules can be defined that describe how the colors of cells will change over time. For this purpose, a subsequent color can "
                             "be defined for each cell color. In addition, durations must be specified that define how many time steps the corresponding color are kept.");
            }
            AlienImGui::InputColorTransition(
                parameters, color, simParameters.spotValues.cellColorTransitionTargetColor[color], simParameters.spotValues.cellColorTransitionDuration[color]);
            ImGui::PopID();
        }

        /**
         * Mutation 
         */
        AlienImGui::Group("Cell function: Genome mutation probabilities");
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Neuron weights and bias")
                .textWidth(MaxContentTextWidth)
                .min(0.0f)
                .max(0.01f)
                .format("%.6f")
                .logarithmic(true)
                .defaultValue(origSimParameters.spotValues.cellFunctionConstructorMutationNeuronDataProbability),
            simParameters.spotValues.cellFunctionConstructorMutationNeuronDataProbability);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Cell function properties")
                .textWidth(MaxContentTextWidth)
                .min(0.0f)
                .max(0.01f)
                .format("%.6f")
                .logarithmic(true)
                .defaultValue(origSimParameters.spotValues.cellFunctionConstructorMutationDataProbability),
            simParameters.spotValues.cellFunctionConstructorMutationDataProbability);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Cell function type")
                .textWidth(MaxContentTextWidth)
                .min(0.0f)
                .max(0.01f)
                .format("%.6f")
                .logarithmic(true)
                .defaultValue(origSimParameters.spotValues.cellFunctionConstructorMutationCellFunctionProbability),
            simParameters.spotValues.cellFunctionConstructorMutationCellFunctionProbability);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Cell insertion")
                .textWidth(MaxContentTextWidth)
                .min(0.0f)
                .max(0.01f)
                .format("%.6f")
                .logarithmic(true)
                .defaultValue(origSimParameters.spotValues.cellFunctionConstructorMutationInsertionProbability),
            simParameters.spotValues.cellFunctionConstructorMutationInsertionProbability);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Cell deletion")
                .textWidth(MaxContentTextWidth)
                .min(0.0f)
                .max(0.01f)
                .format("%.6f")
                .logarithmic(true)
                .defaultValue(origSimParameters.spotValues.cellFunctionConstructorMutationDeletionProbability),
            simParameters.spotValues.cellFunctionConstructorMutationDeletionProbability);

        /**
         * Constructor
         */
        AlienImGui::Group("Cell function: Constructor");
        AlienImGui::Checkbox(
            AlienImGui::CheckboxParameters()
                .name("Offspring inherit color")
                .textWidth(MaxContentTextWidth)
                .defaultValue(origSimParameters.cellFunctionConstructionInheritColor),
            simParameters.cellFunctionConstructionInheritColor);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Offspring distance")
                .textWidth(MaxContentTextWidth)
                .min(0.1f)
                .max(3.0f)
                .defaultValue(origSimParameters.cellFunctionConstructorOffspringDistance),
            simParameters.cellFunctionConstructorOffspringDistance);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Offspring connection distance")
                .textWidth(MaxContentTextWidth)
                .min(0.1f)
                .max(3.0f)
                .defaultValue(origSimParameters.cellFunctionConstructorConnectingCellMaxDistance),
            simParameters.cellFunctionConstructorConnectingCellMaxDistance);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Activity threshold")
                .textWidth(MaxContentTextWidth)
                .min(0.0f)
                .max(1.0f)
                .defaultValue(origSimParameters.cellFunctionConstructorActivityThreshold),
            simParameters.cellFunctionConstructorActivityThreshold);

        /**
         * Attacker
         */
        ImGui::PushID("Attacker");
        AlienImGui::Group("Cell function: Attacker");
        AlienImGui::InputColorMatrix(
            AlienImGui::InputColorMatrixParameters()
                .name("Food chain color matrix")
                .textWidth(MaxContentTextWidth)
                .tooltip(
                    "This matrix can be used to determine how well one cell can attack another cell. The color of the attacking cell is shown in the header "
                    "column while the color of the attacked cell is shown in the header row. A value of 0 means that the attacked cell cannot be digested, "
                    "i.e. no energy can be obtained. A value of 1 means that the maximum energy can be obtained in the digestion process.\n\nExample: If a 0 "
                    "is "
                    "entered in row 2 (red) and column 3 (green), it means that red cells cannot eat green cells.")
                .defaultValue(toVector<MAX_COLORS, MAX_COLORS>(origSimParameters.spotValues.cellFunctionAttackerFoodChainColorMatrix)),
            simParameters.spotValues.cellFunctionAttackerFoodChainColorMatrix);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Energy cost")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(1.0f)
                .defaultValue(origSimParameters.spotValues.cellFunctionAttackerEnergyCost)
                .tooltip(std::string("Amount of energy lost by an attempted attack of a cell in the form of emitted energy particles.")),
            simParameters.spotValues.cellFunctionAttackerEnergyCost);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Geometry penalty")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(5.0f)
                .defaultValue(origSimParameters.spotValues.cellFunctionAttackerGeometryDeviationExponent)
                .tooltip(std::string("The larger this value is, the less energy a cell can gain from an attack if the local "
                                     "geometry of the attacked cell does not match the attacking cell.")),
            simParameters.spotValues.cellFunctionAttackerGeometryDeviationExponent);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Connections mismatch penalty")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(1.0f)
                .defaultValue(origSimParameters.spotValues.cellFunctionAttackerConnectionsMismatchPenalty)
                .tooltip(std::string("The larger this parameter is, the more difficult it is to digest cells that contain more connections.")),
            simParameters.spotValues.cellFunctionAttackerConnectionsMismatchPenalty);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Attack radius")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(2.5f)
                .defaultValue(origSimParameters.cellFunctionAttackerRadius),
            simParameters.cellFunctionAttackerRadius);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Attack strength")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(0.1f)
                .defaultValue(origSimParameters.cellFunctionAttackerStrength),
            simParameters.cellFunctionAttackerStrength);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Energy distribution radius")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(5.0f)
                .defaultValue(origSimParameters.cellFunctionAttackerEnergyDistributionRadius),
            simParameters.cellFunctionAttackerEnergyDistributionRadius);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Energy distribution Value")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(20.0f)
                .defaultValue(origSimParameters.cellFunctionAttackerEnergyDistributionValue),
            simParameters.cellFunctionAttackerEnergyDistributionValue);
        AlienImGui::Checkbox(
            AlienImGui::CheckboxParameters()
                .name("Same color energy distribution")
                .textWidth(MaxContentTextWidth)
                .defaultValue(origSimParameters.cellFunctionAttackerEnergyDistributionSameColor),
            simParameters.cellFunctionAttackerEnergyDistributionSameColor);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Color inhomogeneity factor")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(1.0f)
                .defaultValue(origSimParameters.cellFunctionAttackerColorInhomogeneityFactor),
            simParameters.cellFunctionAttackerColorInhomogeneityFactor);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Activity threshold")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(1.0f)
                .defaultValue(origSimParameters.cellFunctionAttackerActivityThreshold),
            simParameters.cellFunctionAttackerActivityThreshold);
        ImGui::PopID();

        /**
         * Transmitter
         */
        AlienImGui::Group("Cell function: Transmitter");
        AlienImGui::Checkbox(
            AlienImGui::CheckboxParameters()
                .name("Same color energy distribution")
                .textWidth(MaxContentTextWidth)
                .defaultValue(origSimParameters.cellFunctionTransmitterEnergyDistributionSameColor),
            simParameters.cellFunctionTransmitterEnergyDistributionSameColor);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Energy distribution radius")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(5.0f)
                .defaultValue(origSimParameters.cellFunctionTransmitterEnergyDistributionRadius),
            simParameters.cellFunctionTransmitterEnergyDistributionRadius);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Energy distribution Value")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(20.0f)
                .defaultValue(origSimParameters.cellFunctionTransmitterEnergyDistributionValue),
            simParameters.cellFunctionTransmitterEnergyDistributionValue);

        /**
         * Muscle
         */
        AlienImGui::Group("Cell function: Muscle");
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Contraction and expansion delta")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(0.1f)
                .defaultValue(origSimParameters.cellFunctionMuscleContractionExpansionDelta),
            simParameters.cellFunctionMuscleContractionExpansionDelta);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Acceleration")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(0.15f)
                .logarithmic(true)
                .defaultValue(origSimParameters.cellFunctionMuscleMovementAcceleration),
            simParameters.cellFunctionMuscleMovementAcceleration);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Bending angle")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(10.0f)
                .defaultValue(origSimParameters.cellFunctionMuscleBendingAngle),
            simParameters.cellFunctionMuscleBendingAngle);

        /**
         * Sensor
         */
        AlienImGui::Group("Cell function: Sensor");
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Range")
                .textWidth(MaxContentTextWidth)
                .min(10.0f)
                .max(512.0f)
                .defaultValue(origSimParameters.cellFunctionSensorRange)
                .tooltip(std::string("The maximum radius in which a sensor can detect mass concentrations.")),
            simParameters.cellFunctionSensorRange);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Activity threshold")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(1.0f)
                .defaultValue(origSimParameters.cellFunctionSensorActivityThreshold),
            simParameters.cellFunctionSensorActivityThreshold);

        /**
         * Danger zone
         */
        AlienImGui::Group("Danger zone");
        AlienImGui::Checkbox(
            AlienImGui::CheckboxParameters()
                .name("Unlimited energy for constructor")
                .textWidth(MaxContentTextWidth)
                .defaultValue(origSimParameters.cellFunctionConstructionUnlimitedEnergy),
            simParameters.cellFunctionConstructionUnlimitedEnergy);
    }
    ImGui::EndChild();
    validationAndCorrection(simParameters);
}

void _SimulationParametersWindow::processSpot(SimulationParametersSpot& spot, SimulationParametersSpot const& origSpot)
{
    if (ImGui::BeginChild("##", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
        auto worldSize = _simController->getWorldSize();

        /**
         * Color and location
         */
        AlienImGui::Group("Color and location");

        AlienImGui::ColorButtonWithPicker(
            AlienImGui::ColorButtonWithPickerParameters().name("Background color").textWidth(MaxContentTextWidth).defaultValue(origSpot.color),
            spot.color,
            _backupColor,
            _savedPalette);

        int shape = static_cast<int>(spot.shape);
        AlienImGui::Combo(
            AlienImGui::ComboParameters()
                .name("Shape")
                .values({"Circular", "Rectangular"})
                .textWidth(MaxContentTextWidth)
                .defaultValue(static_cast<int>(origSpot.shape)),
            shape);
        spot.shape = static_cast<SpotShape>(shape);
        auto maxRadius = toFloat(std::min(worldSize.x, worldSize.y)) / 2;
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Position X")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(toFloat(worldSize.x))
                .defaultValue(origSpot.posX)
                .format("%.1f"),
            spot.posX);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Position Y")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(toFloat(worldSize.y))
                .defaultValue(origSpot.posY)
                .format("%.1f"),
            spot.posY);
        if (spot.shape == SpotShape::Circular) {
            AlienImGui::SliderFloat(
                AlienImGui::SliderFloatParameters()
                    .name("Core radius")
                    .textWidth(MaxContentTextWidth)
                    .min(0)
                    .max(maxRadius)
                    .defaultValue(origSpot.coreRadius)
                    .format("%.1f"),
                spot.coreRadius);
        }
        if (spot.shape == SpotShape::Rectangular) {
            AlienImGui::SliderFloat(
                AlienImGui::SliderFloatParameters()
                    .name("Core width")
                    .textWidth(MaxContentTextWidth)
                    .min(0)
                    .max(worldSize.x)
                    .defaultValue(origSpot.width)
                    .format("%.1f"),
                spot.width);
            AlienImGui::SliderFloat(
                AlienImGui::SliderFloatParameters()
                    .name("Core height")
                    .textWidth(MaxContentTextWidth)
                    .min(0)
                    .max(worldSize.y)
                    .defaultValue(origSpot.height)
                    .format("%.1f"),
                spot.height);
        }

        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Fade-out radius")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(maxRadius)
                .defaultValue(origSpot.fadeoutRadius)
                .format("%.1f"),
            spot.fadeoutRadius);

        /**
         * General physics
         */
        AlienImGui::Group("Physics: General");
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Friction")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(1)
                .logarithmic(true)
                .defaultValue(origSpot.values.friction)
                .format("%.4f"),
            spot.values.friction);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Rigidity")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(1)
                .defaultValue(origSpot.values.rigidity)
                .format("%.2f"),
            spot.values.rigidity);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Maximum force")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(3.0f)
                .defaultValue(origSpot.values.cellMaxForce),
            spot.values.cellMaxForce);

        /**
         * Physics: Radiation
         */
        AlienImGui::Group("Physics: Radiation");
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Radiation factor")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(0.01f)
                .logarithmic(true)
                .defaultValue(origSpot.values.radiationFactor)
                .format("%.6f"),
            spot.values.radiationFactor);

        /**
         * Physics: Particle transformation
         */
        AlienImGui::Group("Physics: Particle transformation");
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Minimum energy")
                .textWidth(MaxContentTextWidth)
                .min(10.0f)
                .max(200.0f)
                .defaultValue(origSpot.values.cellMinEnergy),
            spot.values.cellMinEnergy);

        /**
         * Collision and binding
         */
        AlienImGui::Group("Physics: Collision and binding");
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Binding creation velocity")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(1.0f)
                .defaultValue(origSpot.values.cellFusionVelocity),
            spot.values.cellFusionVelocity);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Binding max energy")
                .textWidth(MaxContentTextWidth)
                .min(50.0f)
                .max(1000000.0f)
                .logarithmic(true)
                .format("%.0f")
                .defaultValue(origSpot.values.cellMaxBindingEnergy),
            spot.values.cellMaxBindingEnergy);
        if (spot.values.cellMaxBindingEnergy < spot.values.cellMinEnergy + 10.0f) {
            spot.values.cellMaxBindingEnergy = spot.values.cellMinEnergy + 10.0f;
        }

        /**
         * Cell color transition rules
         */
        AlienImGui::Group("Cell color transition rules");
        for (int color = 0; color < MAX_COLORS; ++color) {
            ImGui::PushID(color);
            auto parameters = AlienImGui::InputColorTransitionParameters()
                                  .textWidth(MaxContentTextWidth)
                                  .color(color)
                                  .defaultTargetColor(origSpot.values.cellColorTransitionTargetColor[color])
                                  .defaultTransitionAge(origSpot.values.cellColorTransitionDuration[color])
                                  .logarithmic(true);
            if (0 == color) {
                parameters.name("Target color and duration");
            }
            AlienImGui::InputColorTransition(
                parameters, color, spot.values.cellColorTransitionTargetColor[color], spot.values.cellColorTransitionDuration[color]);
            ImGui::PopID();
        }

        /**
         * Mutation 
         */
        AlienImGui::Group("Cell function: Genome mutation probabilities");
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Neuron weights and bias")
                .textWidth(MaxContentTextWidth)
                .min(0.0f)
                .max(0.01f)
                .format("%.6f")
                .logarithmic(true)
                .defaultValue(origSpot.values.cellFunctionConstructorMutationNeuronDataProbability),
            spot.values.cellFunctionConstructorMutationNeuronDataProbability);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Cell function properties")
                .textWidth(MaxContentTextWidth)
                .min(0.0f)
                .max(0.01f)
                .format("%.6f")
                .logarithmic(true)
                .defaultValue(origSpot.values.cellFunctionConstructorMutationDataProbability),
            spot.values.cellFunctionConstructorMutationDataProbability);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Cell function type")
                .textWidth(MaxContentTextWidth)
                .min(0.0f)
                .max(0.01f)
                .format("%.6f")
                .logarithmic(true)
                .defaultValue(origSpot.values.cellFunctionConstructorMutationCellFunctionProbability),
            spot.values.cellFunctionConstructorMutationCellFunctionProbability);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Cell insertion")
                .textWidth(MaxContentTextWidth)
                .min(0.0f)
                .max(0.01f)
                .format("%.6f")
                .logarithmic(true)
                .defaultValue(origSpot.values.cellFunctionConstructorMutationInsertionProbability),
            spot.values.cellFunctionConstructorMutationInsertionProbability);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Cell deletion")
                .textWidth(MaxContentTextWidth)
                .min(0.0f)
                .max(0.01f)
                .format("%.6f")
                .logarithmic(true)
                .defaultValue(origSpot.values.cellFunctionConstructorMutationDeletionProbability),
            spot.values.cellFunctionConstructorMutationDeletionProbability);

        /**
         * Attacker
         */
        AlienImGui::Group("Cell function: Attacker");
        AlienImGui::InputColorMatrix(
            AlienImGui::InputColorMatrixParameters()
                .name("Food chain color matrix")
                .textWidth(MaxContentTextWidth)
                .defaultValue(toVector<MAX_COLORS, MAX_COLORS>(origSpot.values.cellFunctionAttackerFoodChainColorMatrix)),
            spot.values.cellFunctionAttackerFoodChainColorMatrix);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Energy cost")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(1.0f)
                .defaultValue(origSpot.values.cellFunctionAttackerEnergyCost),
            spot.values.cellFunctionAttackerEnergyCost);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Geometry penalty")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(5.0f)
                .defaultValue(origSpot.values.cellFunctionAttackerGeometryDeviationExponent),
            spot.values.cellFunctionAttackerGeometryDeviationExponent);
        AlienImGui::SliderFloat(
            AlienImGui::SliderFloatParameters()
                .name("Connections mismatch penalty")
                .textWidth(MaxContentTextWidth)
                .min(0)
                .max(1.0f)
                .defaultValue(origSpot.values.cellFunctionAttackerConnectionsMismatchPenalty),
            spot.values.cellFunctionAttackerConnectionsMismatchPenalty);
    }
    ImGui::EndChild();
    validationAndCorrection(spot);
}

void _SimulationParametersWindow::validationAndCorrection(SimulationParameters& parameters) const
{
    for (int i = 0; i < MAX_COLORS; ++i) {
        for (int j = 0; j < MAX_COLORS; ++j) {
            parameters.spotValues.cellFunctionAttackerFoodChainColorMatrix[i][j] =
                std::max(0.0f, std::min(1.0f, parameters.spotValues.cellFunctionAttackerFoodChainColorMatrix[i][j]));
        }
        parameters.radiationAbsorptionByCellColor[i] = std::max(0.0f, std::min(1.0f, parameters.radiationAbsorptionByCellColor[i]));
    }
}

void _SimulationParametersWindow::validationAndCorrection(SimulationParametersSpot& spot) const
{
    for (int i = 0; i < MAX_COLORS; ++i) {
        for (int j = 0; j < MAX_COLORS; ++j) {
            spot.values.cellFunctionAttackerFoodChainColorMatrix[i][j] =
                std::max(0.0f, std::min(1.0f, spot.values.cellFunctionAttackerFoodChainColorMatrix[i][j]));
        }
    }
}
