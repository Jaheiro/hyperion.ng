#ifndef MDNSENGINEWRAPPER_H
#define MDNSENGINEWRAPPER_H

#include <qmdnsengine/dns.h>
#include <qmdnsengine/server.h>
#include <qmdnsengine/service.h>
#include <qmdnsengine/hostname.h>
#include <qmdnsengine/provider.h>
#include <qmdnsengine/browser.h>
#include <qmdnsengine/cache.h>

// Qt includes
#include <QObject>
#include <QByteArray>

// Utility includes
#include <utils/Logger.h>

class MdnsEngineWrapper : public QObject
{
	Q_OBJECT

private:
	friend class HyperionDaemon;

	MdnsEngineWrapper(QObject* parent = nullptr);
	~MdnsEngineWrapper();

public:

	static MdnsEngineWrapper* instance;
	static MdnsEngineWrapper* getInstance() { return instance; }

public slots:

	///
	/// @brief Browse for a service of type
	///
	bool browseForServiceType(const QByteArray& serviceType);
	void resolveService(const QMdnsEngine::Service& service);

	QVariantList getServicesDiscoveredJson(const QByteArray& serviceType, const QString& filter = ".*") const;

private slots:

	void onHostnameChanged(const QByteArray& hostname);

	void onServiceAdded(const QMdnsEngine::Service& service);
	void onServiceUpdated(const QMdnsEngine::Service& service);

	void onServiceResolved(const QHostAddress& address);
	void onServiceRemoved(const QMdnsEngine::Service& service);

private:

	void printCache(const QByteArray& name = 0, quint16 type = QMdnsEngine::ANY) const;

	/// The logger instance for mDNS-Service
	Logger* _log;

	QMdnsEngine::Server* _server;

	QMdnsEngine::Hostname* _hostname;
	QMdnsEngine::Provider* _provider;

	QMdnsEngine::Cache* _cache;

	/// map of service names and browsers
	QMap<QByteArray, QMdnsEngine::Browser*> _browsedServiceTypes;
};

#endif // MDNSENGINEWRAPPER_H
