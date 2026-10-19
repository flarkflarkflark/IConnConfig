#include "getaudioclockparm.h"
#include "retsetaudioclockparm.h"

GetAudioClockParm::GetAudioClockParm(Device *m_pDevice)
	: SysExMessage(GET_AUDIO_CLOCK_PARM, SysExMessage::QUERY, m_pDevice) {}

GetAudioClockParm::~GetAudioClockParm() {}

void GetAudioClockParm::createAnswer(Command m_Command,
					 std::vector<unsigned char> *message,
					 Device *m_pDevice) {
	m_pAnswer =
		std::make_shared<RetSetAudioClockParm>(m_Command, message, m_pDevice);
	if (debug)
		m_pAnswer->setDebug(true);
	m_pAnswer->parseAnswerData();
}
