﻿#include <iostream>
#include <fstream>

#include <QTime>
#include <QTimer>
#include <QProgressDialog>
#include <QCoreApplication>
#include <QMessageBox>

#include "Base/GlobalFactory.h"
#include "Base/ServiceLocator.h"

#include "EngineInterface/EngineInterfaceBuilderFacade.h"
#include "EngineInterface/SimulationController.h"
#include "EngineInterface/SimulationContext.h"
#include "EngineInterface/SpaceProperties.h"
#include "EngineInterface/SimulationAccess.h"
#include "EngineInterface/SimulationParameters.h"
#include "EngineInterface/Serializer.h"
#include "EngineInterface/DescriptionHelper.h"
#include "EngineInterface/SimulationMonitor.h"
#include "EngineInterface/SerializationHelper.h"
#include "EngineInterface/SimulationChanger.h"

#include "EngineGpu/SimulationAccessGpu.h"
#include "EngineGpu/SimulationControllerGpu.h"
#include "EngineGpu/EngineGpuBuilderFacade.h"
#include "EngineGpu/EngineGpuData.h"
#include "EngineGpu/SimulationMonitorGpu.h"

#include "Web/WebAccess.h"
#include "Web/WebBuilderFacade.h"

#include "MessageHelper.h"
#include "VersionController.h"
#include "InfoController.h"
#include "MainController.h"
#include "MainView.h"
#include "MainModel.h"
#include "DataRepository.h"
#include "Notifier.h"
#include "SimulationConfig.h"
#include "DataAnalyzer.h"
#include "QApplicationHelper.h"
#include "Queue.h"
#include "WebSimulationController.h"
#include "Settings.h"
#include "MonitorController.h"

namespace Const
{
    std::string const AutoSaveFilename = "autosave.sim";
    std::string const AutoSaveForLoadingFilename = "autosave_load.sim";
}

MainController::MainController(QObject * parent)
	: QObject(parent)
{
}

MainController::~MainController()
{
    delete _view;
}

void MainController::init()
{
    _model = new MainModel(this);
    _view = new MainView();

    _controllerBuildFunc = [](int typeId, IntVector2D const& universeSize, SymbolTable* symbols,
        SimulationParameters const& parameters, map<string, int> const& typeSpecificData, uint timestepAtBeginning) -> SimulationController*
    {
        if (ModelComputationType(typeId) == ModelComputationType::Gpu) {
            auto facade = ServiceLocator::getInstance().getService<EngineGpuBuilderFacade>();
            EngineGpuData data(typeSpecificData);
            return facade->buildSimulationController({ universeSize, symbols, parameters }, data, timestepAtBeginning);
        }
        else {
            THROW_NOT_IMPLEMENTED();
        }
    };
    _accessBuildFunc = [](SimulationController* controller) -> SimulationAccess*
    {
        if (auto controllerGpu = dynamic_cast<SimulationControllerGpu*>(controller)) {
            auto EngineGpuFacade = ServiceLocator::getInstance().getService<EngineGpuBuilderFacade>();
            SimulationAccessGpu* access = EngineGpuFacade->buildSimulationAccess();
            access->init(controllerGpu);
            return access;
        }
        else {
            THROW_NOT_IMPLEMENTED();
        }
    };
    _monitorBuildFunc = [](SimulationController* controller) -> SimulationMonitor*
    {
        if (auto controllerGpu = dynamic_cast<SimulationControllerGpu*>(controller)) {
            auto facade = ServiceLocator::getInstance().getService<EngineGpuBuilderFacade>();
            SimulationMonitorGpu* moni = facade->buildSimulationMonitor();
            moni->init(controllerGpu);
            return moni;
        }
        else {
            THROW_NOT_IMPLEMENTED();
        }
    };

    auto EngineInterfaceFacade = ServiceLocator::getInstance().getService<EngineInterfaceBuilderFacade>();
    auto serializer = EngineInterfaceFacade->buildSerializer();
    auto descHelper = EngineInterfaceFacade->buildDescriptionHelper();
    auto versionController = new VersionController();
    SET_CHILD(_serializer, serializer);
    SET_CHILD(_descHelper, descHelper);
    SET_CHILD(_versionController, versionController);
    _repository = new DataRepository(this);
    _notifier = new Notifier(this);
    _dataAnalyzer = new DataAnalyzer(this);
    auto worker = new Queue(this);
    SET_CHILD(_worker, worker);

    auto webFacade = ServiceLocator::getInstance().getService<WebBuilderFacade>();
    auto webAccess = webFacade->buildWebController();
    SET_CHILD(_webAccess, webAccess);

    auto webSimController = new WebSimulationController(webAccess, _view);
    SET_CHILD(_webSimController, webSimController);

    _serializer->init(_controllerBuildFunc, _accessBuildFunc);
    _view->init(_model, this, _serializer, _repository, _notifier, _webSimController);
    _worker->init(_serializer);

    QApplicationHelper::processEventsForMilliSec(1000);

    if (!onLoadSimulation(getPathToApp() + Const::AutoSaveFilename, LoadOption::Non)) {

        //default simulation
        auto const EngineGpuFacade = ServiceLocator::getInstance().getService<EngineGpuBuilderFacade>();

        auto config = boost::make_shared<_SimulationConfig>();
        config->cudaConstants = EngineGpuFacade->getDefaultCudaConstants();
        config->universeSize = IntVector2D({ 2000 , 1000 });
        config->symbolTable = EngineInterfaceFacade->getDefaultSymbolTable();
        config->parameters = EngineInterfaceFacade->getDefaultSimulationParameters();
        onNewSimulation(config, 0);
    }

    _view->initGettingStartedWindow();

    //auto save every 20 min
    _autosaveTimer = new QTimer(this);
    connect(_autosaveTimer, &QTimer::timeout, this, (void(MainController::*)())(&MainController::autoSave));
    _autosaveTimer->start(1000 * 60 * 20);
}

void MainController::autoSave()
{
    auto progress = MessageHelper::createProgressDialog("Autosaving...", _view);
    autoSaveIntern(getPathToApp() + Const::AutoSaveFilename);
    delete progress;
}

void MainController::serializeSimulationAndWaitUntilFinished()
{
    QEventLoop pause;
    bool finished = false;
    auto connection = _serializer->connect(_serializer, &Serializer::serializationFinished, [&]() {
        finished = true;
        pause.quit();
    });
    if (dynamic_cast<SimulationControllerGpu*>(_simController)) {
        _serializer->serialize(_simController, int(ModelComputationType::Gpu));
    }
    else {
        THROW_NOT_IMPLEMENTED();
    }
    while (!finished) {
        pause.exec();
    }
    QObject::disconnect(connection);
}

void MainController::autoSaveIntern(std::string const& filename)
{
    saveSimulationIntern(filename);
	QApplicationHelper::processEventsForMilliSec(1000);
}

void MainController::saveSimulationIntern(string const & filename)
{
    serializeSimulationAndWaitUntilFinished();
    SerializationHelper::saveToFile(filename, [&]() { return _serializer->retrieveSerializedSimulation(); });
}

string MainController::getPathToApp() const
{
    auto result = qApp->applicationDirPath();
    if (!result.endsWith("/") && !result.endsWith("\\")) {
        result += "/";
    }
    return result.toStdString();
}

void MainController::showErrorMessageAndTerminate(QString what) const
{
    QMessageBox messageBox;
    messageBox.critical(0, "CUDA error", QString(Const::ErrorCuda).arg(what));
    exit(EXIT_FAILURE);
}

void MainController::onRunSimulation(bool run)
{
	_simController->setRun(run);
	_versionController->clearStack();
}

void MainController::onStepForward()
{
	_versionController->saveSimulationContentToStack();
	_simController->calculateSingleTimestep();
}

void MainController::onStepBackward(bool& emptyStack)
{
	_versionController->loadSimulationContentFromStack();
	emptyStack = _versionController->isStackEmpty();
}

void MainController::onMakeSnapshot()
{
	_versionController->makeSnapshot();
}

void MainController::onRestoreSnapshot()
{
	_versionController->restoreSnapshot();
}

void MainController::onDisplayLink(bool toggled)
{
    _simController->setEnableCalculateFrames(toggled);
}

void MainController::onSimulationChanger(bool toggled)
{
    if (toggled) {
        auto parameters = _simController->getContext()->getSimulationParameters();
        _simChanger->activate(parameters);
    }
    else {
        _simChanger->deactivate();
    }
}

void MainController::initSimulation(SymbolTable* symbolTable, SimulationParameters const& parameters)
{
    auto const EngineInterfaceFacade = ServiceLocator::getInstance().getService<EngineInterfaceBuilderFacade>();

	_model->setSimulationParameters(parameters);
    _model->setExecutionParameters(EngineInterfaceFacade->getDefaultExecutionParameters());
	_model->setSymbolTable(symbolTable);

	connectSimController();
    connect(_simController->getContext(), &SimulationContext::errorThrown, this, &MainController::showErrorMessageAndTerminate);

	auto context = _simController->getContext();
	_descHelper->init(context);
	_versionController->init(_simController->getContext(), _accessBuildFunc(_simController));
	_repository->init(_notifier, _accessBuildFunc(_simController), _descHelper, context);
    _dataAnalyzer->init(_accessBuildFunc(_simController), _repository, _notifier);

	auto simMonitor = _monitorBuildFunc(_simController);
	SET_CHILD(_simMonitor, simMonitor);

    auto webSimMonitor = _monitorBuildFunc(_simController);
    auto space = context->getSpaceProperties();
    _webSimController->init(_accessBuildFunc(_simController), webSimMonitor, getSimulationConfig());

    auto simChanger = EngineInterfaceFacade->buildSimulationChanger(simMonitor, context->getNumberGenerator());
    for (auto const& connection : _simChangerConnections) {
        QObject::disconnect(connection);
    }
    SET_CHILD(_simChanger, simChanger);
    _simChangerConnections.emplace_back(connect(
        _simController,
        &SimulationController::nextTimestepCalculated,
        _simChanger,
        &SimulationChanger::notifyNextTimestep));
    _simChangerConnections.emplace_back(connect(_simChanger, &SimulationChanger::simulationParametersChanged, [&] {
        auto newParameters = _simChanger->retrieveSimulationParameters();
        onUpdateSimulationParameters(newParameters);
    }));

    SimulationAccess* accessForWidgets;
	_view->setupEditors(_simController, _accessBuildFunc(_simController));
}

void MainController::recreateSimulation(string const & serializedSimulation)
{
	delete _simController;
    _simController = nullptr;

    try {
	    _simController = _serializer->deserializeSimulation(serializedSimulation);
    }
    catch (std::exception const& exception)
    {
        showErrorMessageAndTerminate(exception.what());
    }

	auto symbolTable = _simController->getContext()->getSymbolTable();
	auto simulationParameters = _simController->getContext()->getSimulationParameters();

    initSimulation(symbolTable, simulationParameters);

    _view->getMonitorController()->continueTimer();
	_view->refresh();
}

void MainController::onNewSimulation(SimulationConfig const& config, double energyAtBeginning)
{
    _view->getMonitorController()->pauseTimer();

    delete _simController;
    _simController = nullptr;
    auto facade = ServiceLocator::getInstance().getService<EngineGpuBuilderFacade>();
    auto simulationControllerConfig =
        EngineGpuBuilderFacade::Config{ config->universeSize, config->symbolTable, config->parameters};
    auto data = EngineGpuData(config->cudaConstants);
    try {
	    _simController = facade->buildSimulationController(simulationControllerConfig, data);
    } catch (std::exception const& e) {
        showErrorMessageAndTerminate(e.what());
    }

	initSimulation(config->symbolTable, config->parameters);
    _view->getMonitorController()->continueTimer();

	addRandomEnergy(energyAtBeginning);

	_view->refresh();
}

void MainController::onSaveSimulation(string const& filename)
{
    auto progress = MessageHelper::createProgressDialog("Saving...", _view);

    saveSimulationIntern(filename);

    QApplicationHelper::processEventsForMilliSec(1000);
    delete progress;
}

bool MainController::onLoadSimulation(string const & filename, LoadOption option)
{
    _view->getMonitorController()->pauseTimer();

    auto progress = MessageHelper::createProgressDialog("Loading...", _view);

    if (LoadOption::SaveOldSim == option) {
        autoSaveIntern(getPathToApp() + Const::AutoSaveForLoadingFilename);
    }
	delete _simController;
    _simController = nullptr;

    try {
        if (!SerializationHelper::loadFromFile<SimulationController*>(
                filename,
                [&](string const& data) { return _serializer->deserializeSimulation(data); },
                _simController)) {

            //load old simulation
            if (LoadOption::SaveOldSim == option) {
                CHECK(SerializationHelper::loadFromFile<SimulationController*>(
                    getPathToApp() + Const::AutoSaveForLoadingFilename,
                    [&](string const& data) { return _serializer->deserializeSimulation(data); },
                    _simController));
            }
            delete progress;
            return false;
        }
    } catch (std::exception const& exception) {
        showErrorMessageAndTerminate(exception.what());
    }

	initSimulation(_simController->getContext()->getSymbolTable(), _simController->getContext()->getSimulationParameters());
    _view->getMonitorController()->continueTimer();
	_view->refresh();

    delete progress;
    return true;
}

void MainController::onRecreateUniverse(SimulationConfig const& config, bool extrapolateContent)
{
    _view->getMonitorController()->pauseTimer();

    auto const recreateFunction = [&](Serializer* serializer) {
        recreateSimulation(serializer->retrieveSerializedSimulation());
    };
    _worker->add(boost::make_shared<_ExecuteLaterFunc>(recreateFunction));

    auto data = EngineGpuData(config->cudaConstants);

    Serializer::Settings settings{ config->universeSize, data.getData(), extrapolateContent };
    _serializer->serialize(_simController, static_cast<int>(ModelComputationType::Gpu), settings);
}

void MainController::onUpdateSimulationParameters(SimulationParameters const& parameters)
{
    auto progress = MessageHelper::createProgressDialog("Updating simulation parameters...", _view);

	_simController->getContext()->setSimulationParameters(parameters);

    QApplicationHelper::processEventsForMilliSec(500);
    delete progress;
}

void MainController::onUpdateExecutionParameters(ExecutionParameters const & parameters)
{
    auto progress = MessageHelper::createProgressDialog("Updating execution parameters...", _view);

    _simController->getContext()->setExecutionParameters(parameters);

    QApplicationHelper::processEventsForMilliSec(500);
    delete progress;
}

void MainController::onRestrictTPS(optional<int> const& tps)
{
	_simController->setRestrictTimestepsPerSecond(tps);
}

void MainController::onAddMostFrequentClusterToSimulation()
{
    _dataAnalyzer->addMostFrequenceClusterRepresentantToSimulation();
}

int MainController::getTimestep() const
{
    if (_simController) {
        return _simController->getContext()->getTimestep();
    }
    return 0;
}

SimulationConfig MainController::getSimulationConfig() const
{
	auto context = _simController->getContext();

	if (dynamic_cast<SimulationControllerGpu*>(_simController)) {
        auto data = EngineGpuData(context->getSpecificData());
        auto result = boost::make_shared<_SimulationConfig>();
        result->cudaConstants = data.getCudaConstants();
        result->universeSize = context->getSpaceProperties()->getSize();
		result->symbolTable = context->getSymbolTable();
		result->parameters = context->getSimulationParameters();
		return result;
	}
	else {
		THROW_NOT_IMPLEMENTED();
	}
}

SimulationMonitor * MainController::getSimulationMonitor() const
{
    return _simMonitor;
}

void MainController::connectSimController() const
{
	connect(_simController, &SimulationController::nextTimestepCalculated, [this]() {
		_view->getInfoController()->increaseTimestep();
	});
}

void MainController::addRandomEnergy(double amount)
{
	double maxEnergyPerCell = _simController->getContext()->getSimulationParameters().cellMinEnergy * 10;
	_repository->addRandomParticles(amount, maxEnergyPerCell);
	Q_EMIT _notifier->notifyDataRepositoryChanged({
		Receiver::DataEditor,
		Receiver::Simulation,
		Receiver::VisualEditor,
		Receiver::ActionController
	}, UpdateDescription::All);
}

