#include "OpenSimulationDialog.h"

#include <imgui.h>
#include <ImFileDialog.h>

#include "EngineInterface/Serializer.h"
#include "EngineInterface/SimulationController.h"
#include "StatisticsWindow.h"
#include "Viewport.h"
#include "TemporalControlWindow.h"
#include "GlobalSettings.h"
#include "MessageDialog.h"

_OpenSimulationDialog::_OpenSimulationDialog(
    SimulationController const& simController,
    TemporalControlWindow const& temporalControlWindow,
    StatisticsWindow const& statisticsWindow,
    Viewport const& viewport)
    : _simController(simController)
    , _temporalControlWindow(temporalControlWindow)
    , _statisticsWindow(statisticsWindow)
    , _viewport(viewport)
{
    auto path = std::filesystem::current_path();
    if (path.has_parent_path()) {
        path = path.parent_path();
    }
    _startingPath = GlobalSettings::getInstance().getStringState("dialogs.open simulation.starting path", path.string());
}

_OpenSimulationDialog::~_OpenSimulationDialog()
{
    GlobalSettings::getInstance().setStringState("dialogs.open simulation.starting path", _startingPath);
}

void _OpenSimulationDialog::process()
{
    if (!ifd::FileDialog::Instance().IsDone("SimulationOpenDialog")) {
        return;
    }
    if (ifd::FileDialog::Instance().HasResult()) {
        auto firstFilename = ifd::FileDialog::Instance().GetResult();
        auto firstFilenameCopy = firstFilename;
        _startingPath = firstFilenameCopy.remove_filename().string();

        DeserializedSimulation deserializedData;
        if (Serializer::deserializeSimulationFromFiles(deserializedData, firstFilename.string())) {
            _simController->closeSimulation();
            _statisticsWindow->reset();

            _simController->newSimulation(deserializedData.timestep, deserializedData.settings);
            _simController->setClusteredSimulationData(deserializedData.content);
            _viewport->setCenterInWorldPos(
                {toFloat(deserializedData.settings.generalSettings.worldSizeX) / 2,
                 toFloat(deserializedData.settings.generalSettings.worldSizeY) / 2});
            _viewport->setZoomFactor(12.0f);
            _temporalControlWindow->onSnapshot();
        } else {
            MessageDialog::getInstance().show("Open simulation", "The selected file could not be opened.");
        }
    }
    ifd::FileDialog::Instance().Close();
}

void _OpenSimulationDialog::show()
{
    ifd::FileDialog::Instance().Open("SimulationOpenDialog", "Open simulation", "Simulation file (*.sim){.sim},.*", false, _startingPath);
}
