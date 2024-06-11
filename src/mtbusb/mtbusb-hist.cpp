#include "mtbusb.h"

namespace Mtb {

void MtbUsb::histTimerTick() {

}

void MtbUsb::pendingResend() {
	PendingCmd pending = std::move(m_pending.front());
	m_pending.pop_front();

	// to_send guarantees us that conflict can never occur in pending buffer
	// we just check conflict in out buffer

	if (this->conflictWithOut(*(pending.cmd))) {
		log("Not sending again, conflict: " + pending.cmd->msg(), LogLevel::Warning);
		pending.cmd->callError(CmdError::PendingConflict);
		if (!m_out.empty())
			this->sendNextOut();
		return;
	}

	log("Sending again: " + pending.cmd->msg(), LogLevel::Warning);

	try {
		this->write(std::move(pending.cmd), pending.no_sent+1);
	} catch (...) {}
}

bool MtbUsb::conflictWithPending(const Cmd &cmd) const {
	for (const PendingCmd &pending : m_pending)
		if (pending.cmd->conflict(cmd) || cmd.conflict(*(pending.cmd)))
			return true;
	return false;
}

bool MtbUsb::conflictWithOut(const Cmd &cmd) const {
	for (const auto &out : m_out)
		if (out->conflict(cmd) || cmd.conflict(*out))
			return true;
	return false;
}

} // namespace Mtb
