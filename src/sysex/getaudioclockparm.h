#ifndef GETAUDIOCLOCKPARM_H
#define GETAUDIOCLOCKPARM_H

#include "sysexmessage.h"

class GetAudioClockParm : public SysExMessage {
  public:
	GetAudioClockParm(Device *m_pDevice);
	~GetAudioClockParm() override;

	void createAnswer(Command m_Command, std::vector<unsigned char> *message,
				  Device *m_pDevice) override;
};

#endif // GETAUDIOCLOCKPARM_H
