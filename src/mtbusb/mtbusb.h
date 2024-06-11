#ifndef MTBUSB_H
#define MTBUSB_H

/* Low-level access to MTB-USB module via CDC serial port. */

#include <QDateTime>
#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <functional>
#include <memory>
#include <optional>
#include <queue>

#include "mtbusb-commands.h"

namespace Mtb {

constexpr size_t _MAX_MODULES = 256;
constexpr size_t _MAX_HISTORY_LEN = 32;
constexpr size_t _HIST_CHECK_INTERVAL = 100; // ms
constexpr size_t _HIST_TIMEOUT = 300; // ms
constexpr size_t _HIST_SEND_MAX = 3;
constexpr size_t _BUF_IN_TIMEOUT = 50; // ms
constexpr size_t _MAX_HIST_BUF_COUNT = 3;
constexpr size_t _PING_SEND_PERIOD_MS = 5000;

struct EOpenError : public MtbUsbError {
	EOpenError(const std::string &str) : MtbUsbError(str) {}
	EOpenError(const QString &str) : MtbUsbError(str) {}
};

struct EWriteError : public MtbUsbError {
	EWriteError(const std::string &str) : MtbUsbError(str) {}
};

enum class LogLevel {
	None = 0,
	Error = 1,
	Warning = 2,
	Info = 3,
	Commands = 4,
	RawData = 5,
	Debug = 6,
};

QString flowControlToStr(QSerialPort::FlowControl);

static bool isValidModuleAddress(uint8_t addr) {
    (void) addr;
    return true;
}

template <typename DataT, typename ItemType>
QString dataToStr(DataT data, size_t len = 0) {
	QString out;
	size_t i = 0;
	for (auto d = data.begin(); (d != data.end() && (len == 0 || i < len)); d++, i++)
		out += "0x" +
		       QString("%1 ").arg(static_cast<ItemType>(*d), 2, 16, QLatin1Char('0')).toUpper();
	return out.trimmed();
}

struct HistoryItem {
	HistoryItem(std::unique_ptr<const Cmd> &cmd, QDateTime timeout, size_t no_sent)
	    : cmd(std::move(cmd))
	    , timeout(timeout)
		, no_sent(no_sent) {}
	HistoryItem(HistoryItem &&hist) noexcept
	    : cmd(std::move(hist.cmd))
	    , timeout(hist.timeout)
		, no_sent(hist.no_sent) {}
	HistoryItem& operator=(HistoryItem &&hist) {
		cmd = std::move(hist.cmd);
		timeout = hist.timeout;
		no_sent = hist.no_sent;
		return *this;
	}

	std::unique_ptr<const Cmd> cmd;
	QDateTime timeout;
	size_t no_sent = 0;
};

struct MtbUsbInfo {
	uint8_t type;
	MtbBusSpeed speed;
	uint8_t fw_major;
	uint8_t fw_minor;
	uint8_t proto_major;
	uint8_t proto_minor;

	QString fw_version() const { return QString::number(fw_major) + "." + QString::number(fw_minor); }
	QString proto_version() const { return QString::number(proto_major) + "." + QString::number(proto_minor); }
	uint16_t fw_raw() const { return (fw_major << 8) | fw_minor; }
	bool fw_deprecated() const { return (fw_raw() < 0x0103); }
};


// Templated functions must be in header file to compile

} // namespace Mtb

#endif
