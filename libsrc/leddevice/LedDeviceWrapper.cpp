#include <leddevice/LedDeviceWrapper.h>

// bonjour wrapper
#include <HyperionConfig.h>
#include <mdns/mdnsenginewrapper.h>
#include <leddevice/LedDeviceMdnsRegister.h>

#ifdef ENABLE_AVAHI
#include <bonjour/bonjourbrowserwrapper.h>
#include <leddevice/LedDeviceBonjourRegister.h>
#endif

#include <leddevice/LedDevice.h>
#include <leddevice/LedDeviceFactory.h>

// following file is auto generated by cmake! it contains all available leddevice headers
#include "LedDevice_headers.h"

// util
#include <hyperion/Hyperion.h>
#include <utils/JsonUtils.h>

// qt
#include <QMutexLocker>
#include <QThread>
#include <QDir>

LedDeviceRegistry LedDeviceWrapper::_ledDeviceMap {};
QMutex LedDeviceWrapper::_ledDeviceMapLock {QMutex::Recursive};

LedDeviceWrapper::LedDeviceWrapper(Hyperion* hyperion)
	: QObject(hyperion)
	, _hyperion(hyperion)
	, _ledDevice(nullptr)
	, _enabled(false)
	, _mdnsEngine(MdnsEngineWrapper::getInstance())
#ifdef ENABLE_AVAHI
	, _bonjour(BonjourBrowserWrapper::getInstance())
#endif
{
	// prepare the device constructor map
	#define REGISTER(className) LedDeviceWrapper::addToDeviceMap(QString(#className).toLower(), LedDevice##className::construct);

	// the REGISTER() calls are auto-generated by cmake.
	#include "LedDevice_register.cpp"

	#undef REGISTER

	_hyperion->setNewComponentState(hyperion::COMP_LEDDEVICE, false);

	//Register all LED - Devices configured for mDNS discovery
	MdnsConfigMap deviceConfig = LedDeviceMdnsRegister::getAllConfigs();
	MdnsConfigMap::const_iterator mdnsConfigIterator;
	for (mdnsConfigIterator = deviceConfig.begin(); mdnsConfigIterator != deviceConfig.end(); ++mdnsConfigIterator)
	{
		QMetaObject::invokeMethod(_mdnsEngine, "browseForServiceType", Qt::BlockingQueuedConnection, Q_ARG(QByteArray, mdnsConfigIterator.value().serviceType));
	}

#ifdef ENABLE_AVAHI
	//Register all LED-Devices configured for Bonjour discovery
	BonjourConfigMap deviceConfig = LedDeviceBonjourRegister::getAllConfigs();
	BonjourConfigMap::const_iterator bonjourConfigIterator;
	for (bonjourConfigIterator = deviceConfig.begin(); bonjourConfigIterator != deviceConfig.end(); ++bonjourConfigIterator)
	{
		QMetaObject::invokeMethod(_bonjour, "browseForServiceType", Qt::BlockingQueuedConnection,  Q_ARG(QString,bonjourConfigIterator.value().serviceType));
	}
#endif
}

LedDeviceWrapper::~LedDeviceWrapper()
{
	stopDeviceThread();
}

void LedDeviceWrapper::createLedDevice(const QJsonObject& config)
{
	if(_ledDevice != nullptr)
	{
		stopDeviceThread();
	}

	// create thread and device
	QThread* thread = new QThread(this);
	thread->setObjectName("LedDeviceThread");
	_ledDevice = LedDeviceFactory::construct(config);
	_ledDevice->moveToThread(thread);
	// setup thread management
	connect(thread, &QThread::started, _ledDevice, &LedDevice::start);

	// further signals
	connect(this, &LedDeviceWrapper::updateLeds, _ledDevice, &LedDevice::updateLeds, Qt::QueuedConnection);

	connect(this, &LedDeviceWrapper::enable, _ledDevice, &LedDevice::enable);
	connect(this, &LedDeviceWrapper::disable, _ledDevice, &LedDevice::disable);

	connect(this, &LedDeviceWrapper::switchOn, _ledDevice, &LedDevice::switchOn);
	connect(this, &LedDeviceWrapper::switchOff, _ledDevice, &LedDevice::switchOff);

	connect(this, &LedDeviceWrapper::stopLedDevice, _ledDevice, &LedDevice::stop, Qt::BlockingQueuedConnection);

	connect(_ledDevice, &LedDevice::enableStateChanged, this, &LedDeviceWrapper::handleInternalEnableState, Qt::QueuedConnection);

	// start the thread
	thread->start();
}

QJsonObject LedDeviceWrapper::getLedDeviceSchemas()
{
	// make sure the resources are loaded (they may be left out after static linking)
	Q_INIT_RESOURCE(LedDeviceSchemas);

	// read the JSON schema from the resource
	QDir dir(":/leddevices/");
	QJsonObject result, schemaJson;

	for(QString &item : dir.entryList())
	{
		QString schemaPath(QString(":/leddevices/")+item);
		QString devName = item.remove("schema-");

		QString data;
		if(!FileUtils::readFile(schemaPath, data, Logger::getInstance("LedDevice")))
		{
			throw std::runtime_error("ERROR: Schema not found: " + item.toStdString());
		}

		QJsonObject schema;
		if(!JsonUtils::parse(schemaPath, data, schema, Logger::getInstance("LedDevice")))
		{
			throw std::runtime_error("ERROR: JSON schema wrong of file: " + item.toStdString());
		}

		schemaJson = schema;
		schemaJson["title"] = QString("edt_dev_spec_header_title");

		result[devName] = schemaJson;
	}

	return result;
}

int LedDeviceWrapper::addToDeviceMap(QString name, LedDeviceCreateFuncType funcPtr)
{
	QMutexLocker lock(&_ledDeviceMapLock);

	_ledDeviceMap.emplace(name,funcPtr);

	return 0;
}

const LedDeviceRegistry& LedDeviceWrapper::getDeviceMap()
{
	QMutexLocker lock(&_ledDeviceMapLock);

	return _ledDeviceMap;
}

int LedDeviceWrapper::getLatchTime() const
{
	int value = 0;
	QMetaObject::invokeMethod(_ledDevice, "getLatchTime", Qt::BlockingQueuedConnection, Q_RETURN_ARG(int, value));
	return value;
}

QString LedDeviceWrapper::getActiveDeviceType() const
{
	QString value = 0;
	QMetaObject::invokeMethod(_ledDevice, "getActiveDeviceType", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QString, value));
	return value;
}

QString LedDeviceWrapper::getColorOrder() const
{
	QString value;
	QMetaObject::invokeMethod(_ledDevice, "getColorOrder", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QString, value));
	return value;
}

unsigned int LedDeviceWrapper::getLedCount() const
{
	int value = 0;
	QMetaObject::invokeMethod(_ledDevice, "getLedCount", Qt::BlockingQueuedConnection, Q_RETURN_ARG(int, value));
	return value;
}

bool LedDeviceWrapper::enabled() const
{
	return _enabled;
}

void LedDeviceWrapper::handleComponentState(hyperion::Components component, bool state)
{
	if(component == hyperion::COMP_LEDDEVICE)
	{
		if ( state )
		{
			emit enable();
		}
		else
		{
			emit disable();
		}

		//Get device's state, considering situations where it is not ready
		bool deviceState = false;
		QMetaObject::invokeMethod(_ledDevice, "componentState", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, deviceState));
		_hyperion->setNewComponentState(hyperion::COMP_LEDDEVICE, deviceState);
		_enabled = deviceState;
	}
}

void LedDeviceWrapper::handleInternalEnableState(bool newState)
{
	_hyperion->setNewComponentState(hyperion::COMP_LEDDEVICE, newState);
	_enabled = newState;

	if (_enabled)
	{
		_hyperion->update();
	}
}

void LedDeviceWrapper::stopDeviceThread()
{
	// turns the LEDs off & stop refresh timers
	emit stopLedDevice();

	// get current thread
	QThread* oldThread = _ledDevice->thread();
	disconnect(oldThread, nullptr, nullptr, nullptr);
	oldThread->quit();
	oldThread->wait();
	delete oldThread;

	disconnect(_ledDevice, nullptr, nullptr, nullptr);
	delete _ledDevice;
	_ledDevice = nullptr;
}
