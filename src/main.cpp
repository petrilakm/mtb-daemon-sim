#include <QSerialPort>
#include <QSerialPortInfo>
#include <QJsonArray>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QObject>
#include "main.h"
#include "errors.h"
#include "logging.h"
#include "mtbusb.h"
#include "unis.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

DaemonServer server;
std::array<std::unique_ptr<MtbModule>, Mtb::_MAX_MODULES> modules;
std::array<std::unordered_set<QTcpSocket*>, Mtb::_MAX_MODULES> subscribes;
std::unordered_set<QTcpSocket*> topoSubscribes;

#ifdef Q_OS_WIN
static BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType);
#endif

int main(int argc, char *argv[]) {
    DaemonCoreApplication a(argc, argv);
#ifdef Q_OS_WIN
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#endif
    if (a.startupError() != StartupError::Ok)
        return static_cast<int>(a.startupError());
    return a.exec();
}

const QJsonObject DEFAULT_CONFIG = {
    {"loglevel", static_cast<int>(Mtb::LogLevel::Info)},
    {"server", QJsonObject{
        {"host", "127.0.0.1"},
        {"port", static_cast<int>(SERVER_DEFAULT_PORT)},
        {"keepAlive", true},
        {"allowedClients", QJsonArray{"127.0.0.1"}}
    }},
    {"mtb-usb", QJsonObject{
        {"port", "auto"},
        {"keepAlive", true},
        // In case {"speed", 115200} is present, speed is forced to MTB-USB
        // If not present, MTB-USB chooses speed based on its EEPROM-saved value
    }},
    {"production_logging", QJsonObject{
        {"enabled", false},
        {"loglevel", static_cast<int>(Mtb::LogLevel::RawData)},
        {"history", 100},
        {"future", 20},
        {"directory", "prodLog"},
        {"detectLevel", static_cast<int>(Mtb::LogLevel::Warning)},
    }},
};


DaemonCoreApplication::DaemonCoreApplication(int &argc, char **argv)
     : QApplication(argc, argv) {
    QObject::connect(&server, SIGNAL(jsonReceived(QTcpSocket*, const QJsonObject&)),
                     this, SLOT(serverReceived(QTcpSocket*, const QJsonObject&)), Qt::DirectConnection);
    QObject::connect(&server, SIGNAL(clientDisconnected(QTcpSocket*)),
                     this, SLOT(serverClientDisconnected(QTcpSocket*)), Qt::DirectConnection);

    QObject::connect(&t_reconnect, SIGNAL(timeout()), this, SLOT(tReconnectTick()));
    QObject::connect(&t_reactivate, SIGNAL(timeout()), this, SLOT(tReactivateTick()));

#ifdef Q_OS_WIN
    SetConsoleOutputCP(CP_UTF8);
#endif

    log("Starting MTB Daemon v"+QString(VERSION)+"...", Mtb::LogLevel::Info);

    // show window
    simwin = new Tsimwin();

    { // Load config file
        this->configFileName = (argc > 1) ? argv[1] : DEFAULT_CONFIG_FILENAME;
        try {
            this->loadConfig(this->configFileName);
            log("Config file "+configFileName+" successfully loaded.", Mtb::LogLevel::Info);
        } catch (const ConfigNotFound&) {
            log("Unable to load config file "+configFileName+
                ", resetting config, writing default config file...",
                Mtb::LogLevel::Info);
            this->config = DEFAULT_CONFIG;
            this->saveConfig(configFileName);
        } catch (const JsonParseError& e) {
            log("Unable to load config file "+configFileName+": "+e.what(), Mtb::LogLevel::Error);
            startError = StartupError::ConfigLoad;
            return;
        }
    }

    logger.loadConfig(this->config);

    //mtbusb.loglevel = Mtb::LogLevel::Debug; // get everything, filter ourself
    //mtbusb.ping = this->config["mtb-usb"].toObject()["keepAlive"].toBool(true);

    { // Start server
        const QJsonObject serverConfig = this->config["server"].toObject();
        size_t port = serverConfig["port"].toInt();
        bool keepAlive = serverConfig["keepAlive"].toBool(true);
        QHostAddress host(serverConfig["host"].toString());
        log("Starting server: "+host.toString()+":"+QString::number(port)+"...", Mtb::LogLevel::Info);
        try {
            server.listen(host, port, keepAlive);
        } catch (const std::exception& e) {
            log(e.what(), Mtb::LogLevel::Error);
            startError = StartupError::ServerStart;
            return;
        }
    }

    this->t_reactivate.start(T_REACTIVATE_PERIOD);
}

/* MTB-USB handling ----------------------------------------------------------*/

void DaemonCoreApplication::mtbUsbConnect() {
    const QJsonObject mtbUsbConfig = this->config["mtb-usb"].toObject();
    QString port = mtbUsbConfig["port"].toString();

}

void DaemonCoreApplication::mtbUsbOnLog(QString message, Mtb::LogLevel loglevel) {
    log(message, loglevel);
}

void DaemonCoreApplication::mtbUsbOnConnect() {

}

void DaemonCoreApplication::mtbUsbGotInfo() {

    const QJsonObject& mtbusbObj = this->config["mtb-usb"].toObject();

    return this->mtbUsbProperSpeedSet();


}

void DaemonCoreApplication::mtbUsbProperSpeedSet() {

}

void DaemonCoreApplication::mtbUsbGotModules() {
    server.broadcast(this->mtbUsbEvent());

    QVector<uint8_t> activelist;
    activelist.append(1);
    activelist.append(2);
    activelist.append(3);
    const auto activeModules = activelist;

    { // Logging
        size_t count = 0;
        for (size_t i = 0; i < Mtb::_MAX_MODULES; i++)
            if (activeModules[i])
                count++;
        QString message = "Got "+QString::number(count)+" active modules";
        if (count > 0)
            message += ", activating...";
        log(message, Mtb::LogLevel::Info);
    }

    for (size_t i = 0; i < Mtb::_MAX_MODULES; i++)
        if (activeModules[i])
            this->activateModule(i);
}

void DaemonCoreApplication::activateModule(uint8_t addr, size_t attemptsRemaining) {
    (void) attemptsRemaining;
    log("New module "+QString::number(addr)+" discovered, activating...", Mtb::LogLevel::Info);
}

void DaemonCoreApplication::moduleGotInfo(uint8_t addr, Mtb::ModuleInfo info) {
    if (modules[addr] == nullptr) { // module not created yet
        modules[addr] = this->newModule(info.type, addr);
        log("Created new module "+QString::number(addr), Mtb::LogLevel::Info);
    }

    modules[addr]->mtbBusActivate(info);
}

void DaemonCoreApplication::mtbUsbOnDisconnect() {
    server.broadcast(this->mtbUsbEvent());

    for (size_t i = 0; i < Mtb::_MAX_MODULES; i++)
        if (modules[i] != nullptr)
            modules[i]->mtbUsbDisconnected();

    this->t_reconnect.start(T_RECONNECT_PERIOD);
    log("Waiting for MTB-USB to appear...", Mtb::LogLevel::Info);
}

void DaemonCoreApplication::mtbUsbOnNewModule(uint8_t addr) {
    if ((modules[addr] == nullptr) || ((!modules[addr]->isActive()) && (!modules[addr]->isRebooting()) &&
        (!modules[addr]->isFirmwareUpgrading())))
        this->activateModule(addr);

    // Send new-module event to clients with topology change subscription
    // Usually, more modules occur in a short time -> avoid sending multiple events
    // after each other. Rather wait for T_MTBUSB_EVENT_PERIOD to send the event.
    if (!this->newTimerPending) {
        this->newTimerPending = true;
        QTimer::singleShot(T_MTBUSB_EVENT_PERIOD, [this]() {
            this->newTimerPending = false;
            for (auto& socket : topoSubscribes)
                server.send(socket, this->mtbUsbEvent());
        });
    }
}

void DaemonCoreApplication::mtbUsbOnModuleFail(uint8_t addr) {
    // Warning: any operation could be pending on module
    // Beware module instance deletion!
    if ((modules[addr] != nullptr) && (!modules[addr]->isFirmwareUpgrading()) && (!modules[addr]->isRebooting()))
        modules[addr]->mtbBusLost();

    // Send module-lost event to clients with topology change subscription
    // Usually, more modules fail in a short time -> avoid sending multiple events
    // after each other. Rather wait for T_MTBUSB_EVENT_PERIOD to send the event.
    if (!this->failTimerPending) {
        this->failTimerPending = true;
        QTimer::singleShot(T_MTBUSB_EVENT_PERIOD, [this]() {
            this->failTimerPending = false;
            for (auto& socket : topoSubscribes)
                server.send(socket, this->mtbUsbEvent());
        });
    }
}

void DaemonCoreApplication::mtbUsbOnInputsChange(uint8_t addr, const std::vector<uint8_t> &data) {
    if (modules[addr] != nullptr)
        modules[addr]->mtbBusInputsChanged(data);
}

void DaemonCoreApplication::simOnInputChanged(int addr, int pin, bool state)
{
    std::vector<uint8_t> simdata;
    simdata.push_back(pin);
    simdata.push_back(state);

    if (modules[addr] != nullptr) {
        modules[addr]->mtbBusInputsChanged(simdata);
    }
}

void DaemonCoreApplication::mtbUsbOnDiagStateChange(uint8_t addr, const std::vector<uint8_t> &data) {
    if (modules[addr] != nullptr)
        modules[addr]->mtbBusDiagStateChanged(data);
}

void DaemonCoreApplication::tReconnectTick() {
}

void DaemonCoreApplication::tReactivateTick() {

    for (size_t i = 0; i < Mtb::_MAX_MODULES; i++)
        if (modules[i] != nullptr)
            modules[i]->reactivateCheck();
}

/* JSON server handling ------------------------------------------------------*/

void DaemonCoreApplication::serverReceived(QTcpSocket *socket, const QJsonObject &request) {
    try {
        if (!request.contains("command"))
            return; // probably some kind of empty ping or something like this -> no response
        QString command = QJsonSafe::safeString(request, "command");

        if (command == "mtbusb") {
            this->serverCmdMtbusb(socket, request);

        } else if (command == "version") {
            this->serverCmdVersion(socket, request);

        } else if (command == "save_config") {
            this->serverCmdSaveConfig(socket, request);

        } else if (command == "load_config") {
            this->serverCmdLoadConfig(socket, request);

        } else if (command == "module") {
            this->serverCmdModule(socket, request);

        } else if (command == "module_delete") {
            this->serverCmdModuleDelete(socket, request);

        } else if (command == "modules") {
            this->serverCmdModules(socket, request);

        } else if (command == "module_subscribe") {
            this->serverCmdModuleSubscribe(socket, request);

        } else if (command == "module_unsubscribe") {
            this->serverCmdModuleUnsubscribe(socket, request);

        } else if (command == "my_module_subscribes") {
            this->serverCmdMyModuleSubscribes(socket, request);

        } else if (command == "module_set_config") {
            this->serverCmdModuleSetConfig(socket, request);

        } else if (command == "module_specific_command") {
            this->serverCmdModuleSpecificCommand(socket, request);

        } else if (command == "set_address") {
            this->serverCmdSetAddress(socket, request);

        } else if (command == "reset_my_outputs") {
            this->serverCmdResetMyOutputs(socket, request);

        } else if (command == "topology_subscribe") {
            this->serverCmdTopoSubscribe(socket, request);

        } else if (command == "topology_unsubscribe") {
            this->serverCmdTopoUnsubscribe(socket, request);

        } else if (command.startsWith("module_")) {
            size_t addr = request["address"].toInt();
            if ((Mtb::isValidModuleAddress(addr)) && (modules[addr] != nullptr)) {
                modules[addr]->jsonCommand(socket, request, this->hasWriteAccess(socket));
            } else {
                sendError(socket, request, MTB_MODULE_INVALID_ADDR, "Invalid module address");
            }

        } else {
            // Explicitly answer "unknown command"
            sendError(socket, request, MTB_UNKNOWN_COMMAND, "Unknown command!");
        }
    } catch (const JsonParseError &e) {
        sendError(socket, request, MTB_INVALID_JSON, "JSON parse error: "+QString(e.what()));
    } catch (const std::exception &e) {
        sendError(socket, request, MTB_INVALID_JSON, "General error!");
        log("serverReceived exception: "+QString(e.what()), Mtb::LogLevel::Error);
    } catch (...) {
        log("serverReceived general exception!", Mtb::LogLevel::Error);
    }
}

void DaemonCoreApplication::serverCmdMtbusb(QTcpSocket *socket, const QJsonObject &request) {
    if (request.contains("mtbusb")) { // Changing MTB-USB
        QJsonObject jsonMtbUsb = request["mtbusb"].toObject();
          //size_t speed = jsonMtbUsb["speed"].toInt();
          QJsonObject response = jsonOkResponse(request);
          response["mtbusb"] = this->mtbUsbJson();
          server.send(socket, response);
    }
}

void DaemonCoreApplication::serverCmdVersion(QTcpSocket *socket, const QJsonObject &request) {
    QJsonObject response = jsonOkResponse(request);
    QJsonObject version{
        {"sw_version", VERSION},
        {"sw_version_major", VERSION_MAJOR},
        {"sw_version_minor", VERSION_MINOR},
    };
    response["version"] = version;
    server.send(socket, response);
}

void DaemonCoreApplication::serverCmdSaveConfig(QTcpSocket *socket, const QJsonObject &request) {
    if (!this->hasWriteAccess(socket))
        return sendAccessDenied(socket, request);

    QString filename = this->configFileName;
    if (request.contains("filename"))
        filename = QJsonSafe::safeString(request, "filename");
    bool ok = true;
    try {
        this->saveConfig(filename);
    } catch (...) {
        ok = false;
    }
    QJsonObject response {
        {"command", "save_config"},
        {"type", "response"},
        {"status", ok ? "ok" : "error"},
    };
    if (!ok)
        response["error"] = jsonError(MTB_FILE_CANNOT_ACCESS, "Cannot access file "+filename);
    if (request.contains("id"))
        response["id"] = static_cast<int>(request["id"].toInt());

    server.send(socket, response);
}

void DaemonCoreApplication::serverCmdLoadConfig(QTcpSocket *socket, const QJsonObject &request) {
    if (!this->hasWriteAccess(socket))
        return sendAccessDenied(socket, request);

    QString filename = this->configFileName;
    if (request.contains("filename"))
        filename = QJsonSafe::safeString(request, "filename");
    bool ok = true;
    try {
        log("Config file "+filename+" reload request.", Mtb::LogLevel::Info);
        this->loadConfig(filename);
        log("Config file "+filename+" successfully loaded.", Mtb::LogLevel::Info);
    } catch (const std::exception &e) {
        log("Config file "+filename+" load error: "+e.what(), Mtb::LogLevel::Error);
        ok = false;
    } catch (...) {
        ok = false;
    }
    QJsonObject response {
        {"command", "load_config"},
        {"type", "response"},
        {"status", ok ? "ok" : "error"},
    };
    if (!ok)
        response["error"] = jsonError(MTB_FILE_CANNOT_ACCESS, "Cannot load file "+filename);
    if (request.contains("id"))
        response["id"] = static_cast<int>(request["id"].toInt());

    server.send(socket, response);
}

void DaemonCoreApplication::serverCmdModule(QTcpSocket *socket, const QJsonObject &request) {
    QJsonObject response = jsonOkResponse(request);

    size_t addr = request["address"].toInt();
    if ((Mtb::isValidModuleAddress(addr)) && (modules[addr] != nullptr)) {
        response["module"] = modules[addr]->moduleInfo(request["state"].toBool(), true);
        response["status"] = "ok";
    } else {
        response["status"] = "error";
        response["error"] = DaemonServer::error(MTB_MODULE_INVALID_ADDR, "Invalid module address");
    }

    server.send(socket, response);
}

void DaemonCoreApplication::serverCmdModuleDelete(QTcpSocket *socket, const QJsonObject &request) {
    QJsonObject response = jsonOkResponse(request);

    size_t addr = request["address"].toInt();
    response["address"] = static_cast<int>(addr);

    if ((!Mtb::isValidModuleAddress(addr)) || (modules[addr] == nullptr)) {
        response["status"] = "error";
        response["error"] = DaemonServer::error(MTB_MODULE_INVALID_ADDR, "Invalid module address");
    } else if (modules[addr]->isActive() || modules[addr]->isActivating()) {
        response["status"] = "error";
        response["error"] = DaemonServer::error(MTB_MODULE_ACTIVE, "Cannot delete active module");
    } else {
        modules[addr] = nullptr;
        log("Module "+QString::number(addr)+": deleted on client request!", Mtb::LogLevel::Info);

        // Send module-delete event
        std::unordered_set<QTcpSocket*> clients(topoSubscribes);
        clients.insert(subscribes[addr].begin(), subscribes[addr].end());
        for (auto& sock : clients) {
            if (socket != sock) {
                server.send(sock, {
                    {"command", "module_deleted"},
                    {"type", "event"},
                    {"module", static_cast<int>(addr)},
                });
            }
        }
    }

    server.send(socket, response);
}

void DaemonCoreApplication::serverCmdModules(QTcpSocket *socket, const QJsonObject &request) {
    QJsonObject response = jsonOkResponse(request);
    QJsonObject jsonModules;

    for (size_t i = 0; i < Mtb::_MAX_MODULES; i++) {
        if (modules[i] != nullptr)
            jsonModules[QString::number(i)] = modules[i]->moduleInfo(
                request["state"].toBool(), true
            );
    }
    response["modules"] = jsonModules;

    server.send(socket, response);
}

bool DaemonCoreApplication::validateAddrs(const QJsonArray &addrs, QJsonObject& response) {
    for (const auto &value : addrs) {
        int addr = value.toInt(-1);
        if ((addr < 0) || (!Mtb::isValidModuleAddress(addr))) {
            response["status"] = "error";
            response["error"] = DaemonServer::error(MTB_MODULE_INVALID_ADDR,
                "Invalid module address: "+value.toVariant().toString());
            return false;
        }
    }
    return true;
}

void DaemonCoreApplication::serverCmdModuleSubscribe(QTcpSocket *socket, const QJsonObject &request) {
    // First validate addresses (do not change anything if validation fails)
    QJsonObject response = jsonOkResponse(request);
    if (request.contains("addresses")) {
        const QJsonArray reqAddrs = QJsonSafe::safeArray(request, "addresses");
        if (!DaemonCoreApplication::validateAddrs(reqAddrs, response))
            goto cmdModuleSubscribeEnd;

        // Addresses already validated
        for (const auto &value : reqAddrs)
            subscribes[QJsonSafe::safeUInt(value)].emplace(socket);
        response["addresses"] = reqAddrs;
    } else {
        // Subscribe to all addresses
        for (size_t addr = 1; addr < Mtb::_MAX_MODULES; addr++)
            subscribes[addr].emplace(socket);
    }

cmdModuleSubscribeEnd:
    server.send(socket, response);
}

void DaemonCoreApplication::serverCmdModuleUnsubscribe(QTcpSocket *socket, const QJsonObject &request) {
    // First validate addresses (do not change anything if validation fails)
    QJsonObject response = jsonOkResponse(request);
    if (request.contains("addresses")) {
        const QJsonArray reqAddrs = QJsonSafe::safeArray(request, "addresses");
        if (!DaemonCoreApplication::validateAddrs(reqAddrs, response))
            goto cmdModuleUnsubscribeEnd;

        // Addresses already validated
        for (const auto &value : reqAddrs)
            subscribes[QJsonSafe::safeUInt(value)].erase(socket);

        response["addresses"] = reqAddrs;
    } else {
        // Unsubscribe to all addresses
        for (size_t addr = 0; addr < Mtb::_MAX_MODULES; addr++)
            subscribes[addr].erase(socket);
    }
cmdModuleUnsubscribeEnd:
    server.send(socket, response);
}

void DaemonCoreApplication::serverCmdMyModuleSubscribes(QTcpSocket *socket, const QJsonObject &request) {
    // First validate addresses (do not change anything if validation fails)
    QJsonObject response = jsonOkResponse(request);

    if (request.contains("addresses")) {
        const QJsonArray reqAddrs = QJsonSafe::safeArray(request, "addresses");
        if (!DaemonCoreApplication::validateAddrs(reqAddrs, response))
            goto cmdMyModuleSubscribesEnd;

        // Remove all subscriptions of the client
        for (size_t addr = 0; addr < Mtb::_MAX_MODULES; addr++)
            subscribes[addr].erase(socket);

        // Subscribe to specific addresses
        for (const auto &value : reqAddrs)
            subscribes[QJsonSafe::safeUInt(value)].emplace(socket);
    }

cmdMyModuleSubscribesEnd:
    QJsonArray clientsSubscribes;
    for (size_t addr = 0; addr < Mtb::_MAX_MODULES; addr++)
        if (subscribes[addr].find(socket) != subscribes[addr].end())
            clientsSubscribes.push_back(static_cast<int>(addr));
    response["addresses"] = clientsSubscribes;
    server.send(socket, response);
}

void DaemonCoreApplication::serverCmdModuleSetConfig(QTcpSocket *socket, const QJsonObject &request) {
    // Set config can create new module
    if (!this->hasWriteAccess(socket))
        return sendAccessDenied(socket, request);

    size_t addr = request["address"].toInt();
    if (!Mtb::isValidModuleAddress(addr))
        return sendError(socket, request, MTB_MODULE_INVALID_ADDR, "Invalid module address");

    if (modules[addr] == nullptr) {
        uint8_t type = QJsonSafe::safeUInt(request, "type_code");
        modules[addr] = this->newModule(type, addr);
    }

    if ((modules[addr]->isActive()) && (request.contains("type_code")) &&
        (static_cast<size_t>(QJsonSafe::safeUInt(request, "type_code")) != static_cast<size_t>(modules[addr]->moduleType())))
        return sendError(socket, request, MTB_ALREADY_STARTED, "Cannot change type of active module!");

    modules[addr]->jsonSetConfig(socket, request);
}

void DaemonCoreApplication::serverCmdModuleSpecificCommand(QTcpSocket *socket, const QJsonObject &request) {
    if (!this->hasWriteAccess(socket))
        return sendAccessDenied(socket, request);

    const QJsonArray &dataAr = QJsonSafe::safeArray(request, "data");
    std::vector<uint8_t> data;
    for (const auto var : dataAr) {
        unsigned int value = QJsonSafe::safeUInt(var);
        if (value > 0xFF)
            throw JsonParseError("each item if data must be <= 0xFF");
        data.push_back(value);
    }

    if ((request.contains("address")) && (QJsonSafe::safeUInt(request, "address") > 0)) {
        // For module
        QJsonObject json = jsonOkResponse(request);
        size_t addr = QJsonSafe::safeUInt(request, "address");


        QJsonArray responseDataAr;
        //std::copy(responseData.begin(), responseData.end(), std::back_inserter(responseDataAr));
        json["address"] = QJsonValue((int) addr);
        json["response"] = QJsonObject {
                                       {"command", static_cast<int>(data[0])},
                                       {"data", responseDataAr},
                                       };
        server.send(socket, json);
    } else {
        // Broadcast

    }
}

void DaemonCoreApplication::serverCmdSetAddress(QTcpSocket *socket, const QJsonObject &request) {
    //uint8_t newaddr = request["new_address"].toInt(1);
    QJsonObject json = jsonOkResponse(request);
    server.send(socket, json);
}

void DaemonCoreApplication::serverCmdResetMyOutputs(QTcpSocket *socket, const QJsonObject &request) {
    if (!this->hasWriteAccess(socket))
        return sendAccessDenied(socket, request);

    this->clientResetOutputs(
        socket,
        [socket, request]() { server.send(socket, jsonOkResponse(request)); },
        [socket, request]() { sendError(socket, request, Mtb::CmdError::BusNoResponse); }
    );
}

void DaemonCoreApplication::serverCmdTopoSubscribe(QTcpSocket *socket, const QJsonObject &request) {
    QJsonObject response = jsonOkResponse(request);
    topoSubscribes.emplace(socket);
    server.send(socket, response);
}

void DaemonCoreApplication::serverCmdTopoUnsubscribe(QTcpSocket *socket, const QJsonObject &request) {
    QJsonObject response = jsonOkResponse(request);
    topoSubscribes.erase(socket);
    server.send(socket, response);
}

QJsonObject DaemonCoreApplication::mtbUsbJson() const {
    QJsonObject status;
    bool connected = true;
    status["connected"] = connected;
    if (connected) {
        //const std::array<bool, Mtb::_MAX_MODULES> activeModules = mtbusb.activeModules().value();
        status["type"] = 1;
        try {
            status["speed"] = 19200;
        } catch (...) {}
        status["firmware_version"] = 1;
        status["protocol_version"] = 1;

        QJsonArray jsonActiveModules;
        for (size_t i = 0; i < Mtb::_MAX_MODULES; i++)
            if (i<5)
                jsonActiveModules.push_back(static_cast<int>(i));

        status["active_modules"] = jsonActiveModules;
    }
    return status;
}

QJsonObject DaemonCoreApplication::mtbUsbEvent() const {
    return {
        {"command", "mtbusb"},
        {"type", "event"},
        {"mtbusb", this->mtbUsbJson()},
    };
}

/* Configuration ------------------------------------------------------------ */

void DaemonCoreApplication::loadConfig(const QString& filename) {
    // Warning: this function never changes module type as there could be MTBbus
    // command with 'this' pointer pending. Destroying module in this situation would
    // cause segfault.
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        throw ConfigNotFound(QString("Configuration file not found!"));
    QString content = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8(), &parseError);
    if (doc.isNull())
        throw JsonParseError("Unable to parse config file "+filename+": "+parseError.errorString()+" offset: "+QString::number(parseError.offset));
    this->config = doc.object();

    {
        // Load modules
        // Warning: may end up in partially loaded state (when exception occurs)
        QJsonObject _modules = this->config["modules"].toObject();
        for (const QString &_addr : _modules.keys()) {
            try {
                size_t addr = _addr.toInt();
                QJsonObject module = QJsonSafe::safeObject(_modules[_addr]);
                size_t type = QJsonSafe::safeUInt(module, "type");
            if (modules[addr] == nullptr) {
                modules[addr] = this->newModule(type, addr);
                modules[addr]->loadConfig(module);
                //connect(modules[addr], SIGNAL(), );
            } else {
                if (static_cast<size_t>(modules[addr]->moduleType()) == type) {
                    modules[addr]->loadConfig(module);
                } else {
                    if (static_cast<size_t>(modules[addr]->moduleType()) == type) {
                        modules[addr]->loadConfig(module);
                    } else {
                        log("Module "+QString::number(addr)+": file & real module type mismatch, ignoring config!",
                            Mtb::LogLevel::Warning);
                    }
                }
            } catch (const JsonParseError &e) {
                throw JsonParseError("Module "+_addr+": "+e.what());
            }
        }
    }

    TtlacAZD *pT;
    for(int i = 0; i < simwin->prvTlac.count(); i++) {
        pT = simwin->prvTlac.at(i);
        connect(pT, SIGNAL(contactChanged(int,int,bool)), this, SLOT(simOnInputChanged(int,int,bool)));
    }

    this->config.remove("modules");

    {
        // Load allowed clients
        const QJsonObject serverConfig = QJsonSafe::safeObject(this->config, "server");
        this->writeAccess.clear();
        for (const auto& value : QJsonSafe::safeArray(serverConfig, "allowedClients"))
            this->writeAccess.insert(QHostAddress(QJsonSafe::safeString(value)));
    }
}

void DaemonCoreApplication::saveConfig(const QString &filename) {
    log("Saving config to "+filename+"...", Mtb::LogLevel::Info);

    QJsonObject root = this->config;
    QJsonObject jsonModules;
    for (size_t i = 0; i < Mtb::_MAX_MODULES; i++) {
        if (modules[i] != nullptr) {
            QJsonObject module;
            modules[i]->saveConfig(module);
            jsonModules[QString("%3").arg(i, 3, 10, QChar('0'))] = module;
        }
    }
    root["modules"] = jsonModules;

    QJsonDocument doc(root);

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        throw FileWriteError("Unable to open "+filename+" for writing!");
    file.write(doc.toJson(QJsonDocument::JsonFormat::Indented));
    file.close();
}

std::vector<QTcpSocket*> outputSetters() {
    std::vector<QTcpSocket*> result;
    for (const auto& modulePtr : modules) {
        if (modulePtr != nullptr) {
            for (QTcpSocket* socket : modulePtr->outputSetters())
                if (std::find(result.begin(), result.end(), socket) == result.end())
                    result.push_back(socket);
        }
    }
    return result;
}

void DaemonCoreApplication::serverClientDisconnected(QTcpSocket* socket) {
    for (size_t i = 0; i < Mtb::_MAX_MODULES; i++) {
        subscribes[i].erase(socket);
        if (modules[i] != nullptr)
            modules[i]->clientDisconnected(socket);
    }
    topoSubscribes.erase(socket);

    this->clientResetOutputs(socket, [](){}, [](){});
}

void DaemonCoreApplication::clientResetOutputs(
        QTcpSocket* socket,
        std::function<void()> onOk,
        std::function<void()> onError) {
    const std::vector<QTcpSocket*>& setters = outputSetters();
    (void)onError;

    if (setters.size() >= 2) {
        for (size_t i = 0; i < Mtb::_MAX_MODULES; i++)
            if (modules[i] != nullptr)
                modules[i]->resetOutputsOfClient(socket);
        onOk();
    } else if ((setters.size() == 1) && (setters[0] == socket)) {
        // Reset outputs of all modules with broadcast
        for (size_t i = 0; i < Mtb::_MAX_MODULES; i++)
            if (modules[i] != nullptr)
                modules[i]->allOutputsReset();
        onOk();
    } else {
        onOk();
    }
}

bool DaemonCoreApplication::hasWriteAccess(const QTcpSocket *socket) {
    return this->writeAccess.contains(socket->peerAddress());
}

std::unique_ptr<MtbModule> DaemonCoreApplication::newModule(size_t type, uint8_t addr) {
    if (type == static_cast<size_t>(MtbModuleType::Unis10)) {
        return std::make_unique<MtbUnis>(addr);
    }

    log("Unknown module type: "+QString::number(addr)+": 0x"+
        QString::number(type, 16)+"!", Mtb::LogLevel::Warning);
    return std::make_unique<MtbModule>(addr);
}

#ifdef Q_OS_WIN
// Handler function will be called on separate thread!
static BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
    case CTRL_C_EVENT: // Ctrl+C
        QCoreApplication::quit();
        return TRUE;
    case CTRL_CLOSE_EVENT: // Closing the console window
        QCoreApplication::quit();
        return TRUE;
    }

    // Return TRUE if handled this message, further handler functions won't be called.
    // Return FALSE to pass this message to further handlers until default handler calls ExitProcess().
    return FALSE;
}
#endif
