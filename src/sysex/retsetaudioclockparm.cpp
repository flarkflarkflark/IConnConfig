#include "retsetaudioclockparm.h"

RetSetAudioClockParm::RetSetAudioClockParm(Device *m_pDevice)
	: SysExMessage(Command::RET_SET_AUDIO_CLOCK_PARM, SysExMessage::QUERY,
				   m_pDevice) {}

RetSetAudioClockParm::~RetSetAudioClockParm() {}

void RetSetAudioClockParm::parseAnswerData() {
	m_payloadBytes.clear();
	m_candidateTriples.clear();
	m_candidatePairs.clear();
	m_hasCurrentClockSourceCandidate = false;
	m_hasCurrentSampleRateCodeCandidate = false;
	m_hasTrailingCandidateByte = false;
	m_iTrailingCandidateByte = 0;
	m_iCurrentClockSourceCandidate = 0;
	m_iCurrentSampleRateCodeCandidate = 0;
	m_iProtocolVersion = 0;

	if (!m_pData || m_pData->empty())
		return;

	m_payloadBytes = *m_pData;
	m_iProtocolVersion = m_payloadBytes.at(0);

	if (m_payloadBytes.size() > 1) {
		m_hasCurrentClockSourceCandidate = true;
		m_iCurrentClockSourceCandidate = m_payloadBytes.at(1);
	}

	if (m_payloadBytes.size() > 2) {
		m_hasCurrentSampleRateCodeCandidate = true;
		m_iCurrentSampleRateCodeCandidate = m_payloadBytes.at(2);
	}

	for (size_t i = 1; i + 1 < m_payloadBytes.size(); i += 2) {
		BYTE_VECTOR pair;
		pair.push_back(m_payloadBytes.at(i));
		pair.push_back(m_payloadBytes.at(i + 1));
		m_candidatePairs.push_back(pair);
	}
	if (((m_payloadBytes.size() - 1) % 2) == 1) {
		m_hasTrailingCandidateByte = true;
		m_iTrailingCandidateByte = m_payloadBytes.back();
	}

	if (m_payloadBytes.size() > 3) {
		for (size_t i = 3; i + 2 < m_payloadBytes.size(); i += 3) {
			BYTE_VECTOR triple;
			triple.push_back(m_payloadBytes.at(i));
			triple.push_back(m_payloadBytes.at(i + 1));
			triple.push_back(m_payloadBytes.at(i + 2));
			m_candidateTriples.push_back(triple);
		}
	}
}

unsigned int RetSetAudioClockParm::getProtocolVersion() const {
	return m_iProtocolVersion;
}

bool RetSetAudioClockParm::hasCurrentClockSourceCandidate() const {
	return m_hasCurrentClockSourceCandidate;
}

unsigned int RetSetAudioClockParm::getCurrentClockSourceCandidate() const {
	return m_iCurrentClockSourceCandidate;
}

bool RetSetAudioClockParm::hasCurrentSampleRateCodeCandidate() const {
	return m_hasCurrentSampleRateCodeCandidate;
}

unsigned int RetSetAudioClockParm::getCurrentSampleRateCodeCandidate() const {
	return m_iCurrentSampleRateCodeCandidate;
}

const BYTE_VECTOR &RetSetAudioClockParm::getPayloadBytes() const {
	return m_payloadBytes;
}

const std::vector<BYTE_VECTOR> &RetSetAudioClockParm::getCandidateTriples()
	const {
	return m_candidateTriples;
}

const std::vector<BYTE_VECTOR> &RetSetAudioClockParm::getCandidatePairs() const {
	return m_candidatePairs;
}

bool RetSetAudioClockParm::hasTrailingCandidateByte() const {
	return m_hasTrailingCandidateByte;
}

unsigned int RetSetAudioClockParm::getTrailingCandidateByte() const {
	return m_iTrailingCandidateByte;
}
