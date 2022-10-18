#include "AutosaveController.h"

#include <imgui.h>

#include "Base/Resources.h"
#include "EngineInterface/Serializer.h"
#include "GlobalSettings.h"

_AutosaveController::_AutosaveController(SimulationController const& simController)
    : _simController(simController)
{
    _startTimePoint = std::chrono::steady_clock::now();
    _on = GlobalSettings::getInstance().getBoolState("controllers.auto save.active", true);
}

_AutosaveController::~_AutosaveController()
{
    GlobalSettings::getInstance().setBoolState("controllers.auto save.active", _on);
}

void _AutosaveController::shutdown()
{
    if (!_on) {
        return;
    }
    onSave();
}

bool _AutosaveController::isOn() const
{
    return _on;
}

void _AutosaveController::setOn(bool value)
{
    _on = value;
}

void _AutosaveController::process()
{
    if (!_on) {
        return;
    }

    auto durationSinceStart = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::steady_clock::now() - *_startTimePoint).count();
    if (durationSinceStart > 0 && durationSinceStart % 20 == 0 && !_alreadySaved) {
        onSave();
        _alreadySaved = true;
    }
    if (durationSinceStart > 0 && durationSinceStart % 20 == 1 && _alreadySaved) {
        _alreadySaved = false;
    }
}

void _AutosaveController::onSave()
{
    DeserializedSimulation sim;
    sim.timestep = _simController->getCurrentTimestep();
    sim.settings = _simController->getSettings();
    sim.content = _simController->getClusteredSimulationData();
    Serializer::serializeSimulationToFiles(Const::AutosaveFile, sim);
}
