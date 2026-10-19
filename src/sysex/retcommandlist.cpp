#include "retcommandlist.h"
#include "midi.h"

RetCommandList::RetCommandList(Device *device)
    : SysExMessage(Command::RET_COMMAND_LIST, SysExMessage::QUERY, device) {}

RetCommandList::~RetCommandList() { delete m_pSupportedCommands; }

std::vector<int> RetCommandList::getSupportedCommands() {
	std::vector<int> result;
	if (!m_pSupportedCommands)
		return result;
	for (Command cmd : *m_pSupportedCommands)
		result.push_back(static_cast<int>(cmd));
	return result;
}

bool RetCommandList::isCommandSupported(const Command cmd) {
	if (!m_pSupportedCommands)
		return false;
	std::vector<Command>::iterator it;
	for (it = m_pSupportedCommands->begin(); it != m_pSupportedCommands->end();
	     it++) {
		if (*it == cmd)
			return true;
	}
	return false;
}

void RetCommandList::parseAnswerData() {
	m_pSupportedCommands = new std::vector<Command>();
	unsigned int nCommandsSize = static_cast<unsigned int>(m_pData->size());
	nCommandsSize = m_iDataLength;
	Command c;
	for (unsigned int i = 0; i < nCommandsSize; i += 2) {
		unsigned int val =
		    static_cast<unsigned int>(MIDI::byteJoin8bit(m_pData, i, 2));
		c = static_cast<Command>(val);
		if (val != 0)
			m_pSupportedCommands->push_back(c);
	}
}
