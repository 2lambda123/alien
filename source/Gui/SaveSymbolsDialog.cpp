#include "SaveSymbolsDialog.h"

#include <imgui.h>

#include "EngineInterface/Serializer.h"
#include "EngineInterface/SimulationController.h"
#include "ImFileDialog.h"
#include "GlobalSettings.h"
#include "MessageDialog.h"

_SaveSymbolsDialog::_SaveSymbolsDialog(SimulationController const& simController)
    : _simController(simController)
{
    auto path = std::filesystem::current_path();
    if (path.has_parent_path()) {
        path = path.parent_path();
    }
    _startingPath = GlobalSettings::getInstance().getStringState("dialogs.save symbols.starting path", path.string());
}

_SaveSymbolsDialog::~_SaveSymbolsDialog()
{
    GlobalSettings::getInstance().setStringState("dialogs.save symbols.starting path", _startingPath);
}

void _SaveSymbolsDialog::process()
{
    if (!ifd::FileDialog::Instance().IsDone("SaveSymbolsDialog")) {
        return;
    }
    if (ifd::FileDialog::Instance().HasResult()) {
        auto firstFilename = ifd::FileDialog::Instance().GetResult();
        auto firstFilenameCopy = firstFilename;
        _startingPath = firstFilenameCopy.remove_filename().string();

        if (!Serializer::serializeSymbolsToFile(firstFilename.string(), _simController->getSymbolMap())) {
            MessageDialog::getInstance().show("Save symbols", "The symbols could not be saved to the specified file.");
        }
    }
    ifd::FileDialog::Instance().Close();
}

void _SaveSymbolsDialog::show()
{
    ifd::FileDialog::Instance().Save("SaveSymbolsDialog", "Save symbols", "Symbols file (*.json){.json},.*", _startingPath);
}
