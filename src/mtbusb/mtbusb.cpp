#include <QSerialPortInfo>
#include "mtbusb.h"
#include "mtbusb-win-com-discover.h"

namespace Mtb {

MtbUsb::MtbUsb(QObject *parent) : QObject(parent) {
    //QObject::connect(&m_histTimer, SIGNAL(timeout()), this, SLOT(histTimerTick()));
    //QObject::connect(&m_pingTimer, SIGNAL(timeout()), this, SLOT(pingTimerTick()));

    //m_pingTimer.setInterval(_PING_SEND_PERIOD_MS);
}

void MtbUsb::log(const QString &message, const LogLevel loglevel) {
	if (loglevel <= this->loglevel)
		emit onLog(message, loglevel);
}

QString flowControlToStr(QSerialPort::FlowControl fc) {
	if (fc == QSerialPort::FlowControl::HardwareControl)
		return "hardware";
	if (fc == QSerialPort::FlowControl::SoftwareControl)
		return "software";
	if (fc == QSerialPort::FlowControl::NoFlowControl)
		return "no";
	return "unknown";
}

void MtbUsb::pingTimerTick() {
	if (this->connected() && this->ping) {
		this->send(
			Mtb::CmdMtbUsbPing(
				{[](void*) {}},
				{[this](Mtb::CmdError, void*) { this->disconnect(); }}
			)
		);
	}
}

/* Public functions API ------------------------------------------------------*/

void MtbUsb::connect(const QString &portname, int32_t br, QSerialPort::FlowControl fc) {
	log("Connecting to " + portname + ", br=" + QString::number(br) +
	    ", fc=" + flowControlToStr(fc) + "...", LogLevel::Info);

	log("Connected", LogLevel::Info);
	emit onConnect();
}

void MtbUsb::disconnect() {
	if (!this->connected())
		return;

	log("Disconnecting...", LogLevel::Info);
	emit onDisconnect();
}

bool MtbUsb::connected() const { return true; }

std::vector<QSerialPortInfo> MtbUsb::ports() {
#ifdef Q_OS_WIN
	return winMtbUsbPorts();
#else
	std::vector<QSerialPortInfo> result;
	QList<QSerialPortInfo> ports(QSerialPortInfo::availablePorts());
	for (const QSerialPortInfo &info : ports)
		if (info.description() == "MTB-USB v4")
			result.push_back(info);
	return result;
#endif
}

void MtbUsb::changeSpeed(MtbBusSpeed newSpeed, std::function<void()> onOk, std::function<void(Mtb::CmdError)> onError) {
	if ((!this->connected()) || (!this->m_mtbUsbInfo.has_value()))
		return;

	// Send 3× broadcast to change module speed
	this->send(Mtb::CmdMtbModuleChangeSpeed(newSpeed));
	this->send(Mtb::CmdMtbModuleChangeSpeed(newSpeed));
	this->send(
		Mtb::CmdMtbModuleChangeSpeed(
			newSpeed,
			{[this, onOk, onError, newSpeed](void*) {
				// Send MTB-USB speed change request
				this->send(
					Mtb::CmdMtbUsbChangeSpeed(
						newSpeed,
						{[this, newSpeed, onOk](void*) {
							this->m_mtbUsbInfo.value().speed = newSpeed;
							onOk();
						}},
						{[onError](Mtb::CmdError cmdError, void*) { onError(cmdError); }}
					)
				);
			}},
			{[onError](Mtb::CmdError cmdError, void*) { onError(cmdError); }}
		)
	);
}

} // namespace Mtb
