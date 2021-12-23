#include <imgui.h>

#include "EngineImpl/SimulationController.h"
#include "EngineInterface/Serializer.h"
#include "ImFileDialog.h"
#include "SaveSelectionDialog.h"

_SaveSelectionDialog::_SaveSelectionDialog(SimulationController const& simController)
    : _simController(simController)
{}

void _SaveSelectionDialog::process()
{
    if (!ifd::FileDialog::Instance().IsDone("SaveSelectionDialog")) {
        return;
    }
    if (ifd::FileDialog::Instance().HasResult()) {
        const std::vector<std::filesystem::path>& res = ifd::FileDialog::Instance().GetResults();
        auto firstFilename = res.front();

        auto content = _simController->getSelectedSimulationData(_includeClusters);

        Serializer serializer = boost::make_shared<_Serializer>();
        serializer->serializeContentToFile(firstFilename.string(), content);
    }
    ifd::FileDialog::Instance().Close();
}

void _SaveSelectionDialog::show(bool includeClusters)
{
    _includeClusters = includeClusters;
    ifd::FileDialog::Instance().Save("SaveSelectionDialog", "Save selection", "Selection file (*.sim){.sim},.*");
}