#ifndef RETSETAUDIOCLOCKPARM_H
#define RETSETAUDIOCLOCKPARM_H

#include "sysexmessage.h"

class RetSetAudioClockParm : public SysExMessage {
  public:
	RetSetAudioClockParm(Device *m_pDevice);

	RetSetAudioClockParm(Command cmd, BYTE_VECTOR *message, Device *device)
		: SysExMessage(cmd, message, device) {}

	~RetSetAudioClockParm() override;
	void parseAnswerData() override;

	unsigned int getProtocolVersion() const;
	bool hasCurrentClockSourceCandidate() const;
	unsigned int getCurrentClockSourceCandidate() const;
	bool hasCurrentSampleRateCodeCandidate() const;
	unsigned int getCurrentSampleRateCodeCandidate() const;
	const BYTE_VECTOR &getPayloadBytes() const;
	const std::vector<BYTE_VECTOR> &getCandidateTriples() const;
	const std::vector<BYTE_VECTOR> &getCandidatePairs() const;
	bool hasTrailingCandidateByte() const;
	unsigned int getTrailingCandidateByte() const;

  private:
	unsigned int m_iProtocolVersion = 0;
	BYTE_VECTOR m_payloadBytes;
	bool m_hasCurrentClockSourceCandidate = false;
	unsigned int m_iCurrentClockSourceCandidate = 0;
	bool m_hasCurrentSampleRateCodeCandidate = false;
	unsigned int m_iCurrentSampleRateCodeCandidate = 0;
	std::vector<BYTE_VECTOR> m_candidateTriples;
	std::vector<BYTE_VECTOR> m_candidatePairs;
	bool m_hasTrailingCandidateByte = false;
	unsigned int m_iTrailingCandidateByte = 0;
};

#endif // RETSETAUDIOCLOCKPARM_H
