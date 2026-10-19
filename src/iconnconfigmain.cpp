#include "iconnconfigmain.h"
#include "aboutdialog.h"
#include "config/configuration.h"
#include "config/qsettings-xml.h"
#include "devicedetection.h"
#include "sysex/communicationexception.h"
#include "sysex/getaudiocontroldetail.h"
#include "sysex/getaudiocontroldetailvalue.h"
#include "sysex/getaudioclockparm.h"
#include "sysex/getaudiocontrolparm.h"
#include "sysex/getaudiodeviceparm.h"
#include "sysex/getaudioglobalparm.h"
#include "sysex/getaudiopatchbayparm.h"
#include "sysex/getaudioportparm.h"
#include "sysex/getcommandlist.h"
#include "sysex/midi.h"
#include "sysex/protocolexception.h"
#include "sysex/retcommandlist.h"
#include "sysex/retsetaudiocontroldetail.h"
#include "sysex/retsetaudiocontroldetailvalue.h"
#include "sysex/retsetaudioclockparm.h"
#include "sysex/retsetaudiocontrolparm.h"
#include "sysex/retsetaudiodeviceparm.h"
#include "sysex/retsetaudiopatchbayparm.h"
#include "sysex/sysexmessage.h"
#include "ui_iconnconfigmain.h"
#include "widgets/audioportswidget.h"
#include "widgets/audiowidget.h"
#include "widgets/centralwidget.h"
#include "widgets/deviceinfowidget.h"
#include "widgets/multiinfowidget.h"
#include "widgets/portswidget.h"

#include <cerrno>   // for errno
#include <csignal>  // for sigaction()
#include <cstring>  // for strerror()
#ifndef _WIN32
#include <unistd.h> // for pipe()
#endif

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopWidget>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QPixmap>
#include <QProcess>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QSettings>
#include <QSocketNotifier>
#include <QStyle>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QtDebug>

#include <map>
#include <algorithm>
#include <set>

namespace {
QString toHex(unsigned int value, int width = 2) {
	return QString("0x%1").arg(value, width, 16, QLatin1Char('0')).toUpper();
}

QString toCommandHex(int commandId) {
	if (commandId < 0)
		return "n/a";
	const int width = commandId > 0xFF ? 3 : 2;
	return toHex(static_cast<unsigned int>(commandId), width);
}

QString messageHex(const BYTE_VECTOR *message) {
	if (!message)
		return "n/a";
	return QString::fromStdString(MIDI::printMessageToHexString(
		const_cast<BYTE_VECTOR *>(message)));
}

int parseMessageCommandId(const BYTE_VECTOR *message) {
	if (!message || message->size() < 16)
		return -1;
	return static_cast<int>(MIDI::byteJoin7bit(
		const_cast<BYTE_VECTOR *>(message), 14, 2) &
						 1023);
}

int parseMessageTransactionId(const BYTE_VECTOR *message) {
	if (!message || message->size() < 14)
		return -1;
	return static_cast<int>(MIDI::byteJoin7bit(
		const_cast<BYTE_VECTOR *>(message), 12, 2));
}

QString commandNameForId(int commandId) {
	if (commandId < 0)
		return "UNKNOWN_COMMAND";
	return QString::fromLatin1(
		commandToString(static_cast<Command>(commandId)));
}

QString ackCodeToString(int code) {
	switch (code) {
	case SysExMessage::COMMAND_ACCEPTED:
		return "COMMAND_ACCEPTED";
	case SysExMessage::UNKNOWN_COMMAND:
		return "UNKNOWN_COMMAND";
	case SysExMessage::MALFORMED_MESSAGE:
		return "MALFORMED_MESSAGE";
	case SysExMessage::COMMAND_FAILED:
		return "COMMAND_FAILED";
	default:
		return QString("UNKNOWN_ACK_CODE(%1)").arg(code);
	}
}

struct DecodedAck {
	bool isAck = false;
	bool isError = false;
	int code = -1;
	int targetCommandId = -1;
	QString summary = "n/a";
};

DecodedAck decodeAck(const BYTE_VECTOR *message) {
	DecodedAck decoded;
	if (!message || message->size() < Device::DATA_OFFSET)
		return decoded;
	if (parseMessageCommandId(message) != ACK)
		return decoded;
	decoded.isAck = true;

	const long dataLength = MIDI::byteJoin7bit(
		const_cast<BYTE_VECTOR *>(message), Device::DATA_LENGTH_OFFSET,
		Device::DATA_LENGTH_LENGTH);
	if (dataLength >= 2 &&
		message->size() > static_cast<size_t>(Device::DATA_OFFSET + 1)) {
		decoded.targetCommandId = static_cast<int>(MIDI::byteJoin7bit(
			const_cast<BYTE_VECTOR *>(message), Device::DATA_OFFSET, 2) &
									 1023);
	}
	if (dataLength >= 3 &&
		message->size() > static_cast<size_t>(Device::DATA_OFFSET + 2)) {
		decoded.code = static_cast<int>(message->at(Device::DATA_OFFSET + 2));
		decoded.isError = decoded.code != SysExMessage::COMMAND_ACCEPTED;
		decoded.summary = QString("ACK %1").arg(ackCodeToString(decoded.code));
		if (decoded.targetCommandId >= 0) {
			decoded.summary +=
				QString(" target=%1 %2")
					.arg(toCommandHex(decoded.targetCommandId),
						 commandNameForId(decoded.targetCommandId));
		}
		return decoded;
	}
	decoded.summary = "ACK incomplete";
	return decoded;
}

QString tupleFromArgs(const QStringList &args) {
	return args.isEmpty() ? "()" : QString("(%1)").arg(args.join(", "));
}

struct ProbeFailure {
	int commandId = -1;
	QString commandName;
	QString argsTuple;
	QString reason;
	QString ackSummary;
	int responseCommandId = -1;
	QString responseCommandName;
	int transactionId = -1;
	QString queryRawHex;
	QString responseRawHex;
};

struct ProbeSuccess {
	int commandId = -1;
	QString commandName;
	QString argsTuple;
	int responseCommandId = -1;
	QString responseCommandName;
	int transactionId = -1;
	QString responseRawHex;
	QString details;
};

void appendProbeFailure(std::vector<ProbeFailure> &failures, Device *device,
						Command command, const QStringList &args,
						const QString &reason) {
	ProbeFailure failure;
	failure.commandId = static_cast<int>(command);
	failure.commandName = commandNameForId(failure.commandId);
	failure.argsTuple = tupleFromArgs(args);
	failure.reason = reason;

	const BYTE_VECTOR *lastQuery = device ? device->getLastSendMessage() : nullptr;
	const BYTE_VECTOR *lastResponse =
		device ? device->getLastRetrieveMessage() : nullptr;
	failure.queryRawHex = messageHex(lastQuery);
	failure.responseRawHex = messageHex(lastResponse);
	failure.responseCommandId = parseMessageCommandId(lastResponse);
	failure.responseCommandName =
		commandNameForId(failure.responseCommandId);
	failure.transactionId = parseMessageTransactionId(lastResponse);
	const DecodedAck ack = decodeAck(lastResponse);
	if (ack.isAck)
		failure.ackSummary = ack.summary;

	failures.push_back(failure);
}

void appendProbeSuccess(std::vector<ProbeSuccess> &successes, Device *device,
						Command command, const QStringList &args,
						const QString &details = QString()) {
	ProbeSuccess success;
	success.commandId = static_cast<int>(command);
	success.commandName = commandNameForId(success.commandId);
	success.argsTuple = tupleFromArgs(args);
	const BYTE_VECTOR *lastResponse =
		device ? device->getLastRetrieveMessage() : nullptr;
	success.responseCommandId = parseMessageCommandId(lastResponse);
	success.responseCommandName =
		commandNameForId(success.responseCommandId);
	success.transactionId = parseMessageTransactionId(lastResponse);
	success.responseRawHex = messageHex(lastResponse);
	success.details = details;
	successes.push_back(success);
}

DecodedAck responseAck(Device *device) {
	return decodeAck(device ? device->getLastRetrieveMessage() : nullptr);
}

QString extractSection(const QString &content, const QString &sectionHeader) {
	const QStringList allLines = content.split('\n');
	QStringList sectionLines;
	bool inSection = false;
	for (const QString &line : allLines) {
		if (line == sectionHeader) {
			inSection = true;
			continue;
		}
		if (inSection && line.startsWith("[") && line.endsWith("]"))
			break;
		if (inSection)
			sectionLines << line;
	}
	return sectionLines.join("\n").trimmed();
}

QString byteHex(unsigned int value) {
	return QString("0x%1")
		.arg(value & 0xFF, 2, 16, QLatin1Char('0'))
		.toUpper();
}

QString vectorHex(const BYTE_VECTOR &bytes) {
	QStringList parts;
	for (unsigned char byte : bytes)
		parts << QString("%1")
					 .arg(static_cast<unsigned int>(byte), 2, 16,
						  QLatin1Char('0'))
					 .toUpper();
	return parts.join(' ');
}

int sampleRateFromCodeCandidate(unsigned int code) {
	switch (code) {
	case 1:
		return 44100;
	case 2:
		return 48000;
	case 3:
		return 96000;
	case 4:
		return 176400;
	case 5:
		return 192000;
	default:
		return -1;
	}
}

QString sampleRateCodeLabel(unsigned int code) {
	const int hzCandidate = sampleRateFromCodeCandidate(code);
	if (hzCandidate > 0)
		return QString("%1 Hz (legacy-map hypothesis, unverified)")
			.arg(hzCandidate);
	return "unknown (no legacy-map hypothesis)";
}

int globalSampleRateFromLegacyParserCode(unsigned int sampleRateCode) {
	switch (sampleRateCode) {
	case 1:
		return 11025;
	case 2:
		return 12000;
	case 3:
		return 22050;
	case 4:
		return 24000;
	case 5:
		return 44100;
	case 6:
		return 48000;
	case 7:
		return 88200;
	case 8:
		return 96000;
	default:
		return -1;
	}
}

int globalBitDepthFromLegacyParserCode(unsigned int bitDepthCode) {
	switch (bitDepthCode) {
	case 1:
		return 4;
	case 2:
		return 8;
	case 3:
		return 12;
	case 4:
		return 16;
	case 5:
		return 20;
	case 6:
		return 24;
	case 7:
		return 28;
	case 8:
		return 32;
	default:
		return -1;
	}
}

QString globalSampleRateCodeLabel(unsigned int sampleRateCode) {
	const int hz = globalSampleRateFromLegacyParserCode(sampleRateCode);
	if (hz <= 0)
		return "unknown (legacy parser mapping unavailable)";
	return QString("%1 Hz (legacy parser mapping, hypothesis for AUDIO4c)")
		.arg(hz);
}

QString globalBitDepthCodeLabel(unsigned int bitDepthCode) {
	const int bits = globalBitDepthFromLegacyParserCode(bitDepthCode);
	if (bits <= 0)
		return "unknown (legacy parser mapping unavailable)";
	return QString("%1-bit (legacy parser mapping)").arg(bits);
}

QString clockSourceCodeLabel(unsigned int code) {
	switch (code) {
	case 0:
		return "none_or_unknown (candidate)";
	case 1:
		return "source_1 (candidate)";
	case 2:
		return "source_2 (candidate)";
	case 3:
		return "source_3 (candidate)";
	default:
		return QString("source_%1 (candidate)").arg(code);
	}
}

QString lockStatusCodeLabel(unsigned int code) {
	switch (code) {
	case 0:
		return "unlocked_or_na (candidate)";
	case 1:
		return "locked (candidate)";
	case 2:
		return "syncing_or_external (candidate)";
	default:
		return "unknown (candidate)";
	}
}

QString usbDeviceHostCodeLabel(unsigned int code) {
	switch (code) {
	case pk::UsbDeviceHost::NO_HOST:
		return "no_host";
	case pk::UsbDeviceHost::MAC_PC:
		return "mac_pc";
	case pk::UsbDeviceHost::IOS:
		return "ios";
	default:
		return QString("unknown_%1").arg(code);
	}
}

struct PipeWireRateProbe {
	bool commandAvailable = false;
	bool rateFound = false;
	int rateHz = 0;
	QString queriedField;
	QString rawOutput;
	QString note;
};

PipeWireRateProbe probePipeWireRate() {
	PipeWireRateProbe probe;
	const QStringList fields = {"clock.force-rate", "clock.rate"};
	QRegularExpression re("\\b([1-9]\\d{3,5})\\b");
	for (const QString &field : fields) {
		QProcess process;
		process.start("pw-metadata",
					  QStringList() << "-n"
									<< "settings"
									<< "0" << field);
		if (!process.waitForStarted(1500)) {
			if (process.error() == QProcess::FailedToStart)
				probe.note = "pw-metadata not available";
			continue;
		}
		probe.commandAvailable = true;
		process.waitForFinished(1500);
		const QString output = QString::fromUtf8(process.readAllStandardOutput()) +
							   QString::fromUtf8(process.readAllStandardError());
		QRegularExpressionMatch match = re.match(output);
		if (match.hasMatch()) {
			probe.rateFound = true;
			probe.rateHz = match.captured(1).toInt();
			probe.queriedField = field;
			probe.rawOutput = output.trimmed();
			return probe;
		}
		if (!output.trimmed().isEmpty()) {
			probe.queriedField = field;
			probe.rawOutput = output.trimmed();
		}
	}
	if (probe.commandAvailable && !probe.rateFound && probe.note.isEmpty())
		probe.note = "pw-metadata returned no numeric rate";
	return probe;
}

bool parseHexByteVector(const QString &raw, BYTE_VECTOR &bytesOut) {
	bytesOut.clear();
	const QStringList tokens =
		raw.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
	if (tokens.isEmpty())
		return false;
	for (const QString &token : tokens) {
		bool ok = false;
		const int value = token.toInt(&ok, 16);
		if (!ok || value < 0 || value > 0xFF)
			return false;
		bytesOut.push_back(static_cast<unsigned char>(value));
	}
	return true;
}

bool extractPayloadFromMessage(const BYTE_VECTOR &message,
							   Command responseCommand,
							   BYTE_VECTOR &payloadOut) {
	payloadOut.clear();
	if (message.size() < static_cast<size_t>(Device::DATA_OFFSET + 1))
		return false;
	if (parseMessageCommandId(&message) != responseCommand)
		return false;
	const long dataLength =
		MIDI::byteJoin7bit(const_cast<BYTE_VECTOR *>(&message),
						   Device::DATA_LENGTH_OFFSET,
						   Device::DATA_LENGTH_LENGTH);
	if (dataLength <= 0)
		return false;
	const size_t payloadStart = Device::DATA_OFFSET;
	const size_t payloadEnd =
		payloadStart + static_cast<size_t>(dataLength);
	if (payloadEnd > message.size())
		return false;
	payloadOut.insert(payloadOut.end(), message.begin() + payloadStart,
					  message.begin() + payloadEnd);
	return true;
}

bool extractClockPayloadFromMessage(const BYTE_VECTOR &message,
									BYTE_VECTOR &payloadOut) {
	return extractPayloadFromMessage(message, RET_SET_AUDIO_CLOCK_PARM,
									 payloadOut);
}

bool extractGlobalPayloadFromMessage(const BYTE_VECTOR &message,
									 BYTE_VECTOR &payloadOut) {
	return extractPayloadFromMessage(message, RET_SET_AUDIO_GLOBAL_PARM,
									 payloadOut);
}

bool containsPayload(const std::vector<BYTE_VECTOR> &payloads,
					 const BYTE_VECTOR &candidate) {
	for (const BYTE_VECTOR &payload : payloads) {
		if (payload == candidate)
			return true;
	}
	return false;
}

QString extractExternalKnownState(const QString &filePath,
								  const QString &content) {
	const QRegularExpression ratePattern(
		"\\b(44100|48000|88200|96000)\\b");
	QStringList labels;
	const QString filename = QFileInfo(filePath).fileName();
	QRegularExpressionMatchIterator fileMatches =
		ratePattern.globalMatch(filename);
	while (fileMatches.hasNext()) {
		const QString rate = fileMatches.next().captured(1);
		const QString label = QString("%1 (from filename)").arg(rate);
		if (!labels.contains(label))
			labels << label;
	}

	const QStringList headerLines = content.split('\n').mid(0, 120);
	const QString headerBlob = headerLines.join(' ');
	QRegularExpressionMatchIterator headerMatches =
		ratePattern.globalMatch(headerBlob);
	while (headerMatches.hasNext()) {
		const QString rate = headerMatches.next().captured(1);
		const QString label = QString("%1 (from header)").arg(rate);
		if (!labels.contains(label))
			labels << label;
	}

	if (labels.isEmpty())
		return "not_provided";
	return labels.join(", ");
}

bool extractHexMessageFromLine(const QString &line, BYTE_VECTOR &messageOut) {
	const QString lower = line.toLower();
	int start = lower.indexOf("f0 ");
	if (start < 0)
		start = lower.indexOf("f0");
	int end = lower.lastIndexOf(" f7");
	if (end >= 0)
		end += 1;
	else
		end = lower.lastIndexOf("f7");
	if (start < 0 || end < 0 || end < start)
		return false;
	const QString hex = line.mid(start, end - start + 2);
	return parseHexByteVector(hex, messageOut);
}

struct DecodedAudioGlobalConfigBlock {
	unsigned int configurationNumber = 0;
	unsigned int bitDepthCode = 0;
	unsigned int sampleRateCode = 0;
	int offsetConfigurationNumber = -1;
	int offsetBitDepthCode = -1;
	int offsetSampleRateCode = -1;
};

struct DecodedAudioGlobalPayload {
	bool valid = false;
	QString validationNote;
	unsigned int commandVersionNumber = 0;
	unsigned int numberOfAudioPorts = 0;
	unsigned int minAudioFramesBuffered = 0;
	unsigned int maxAudioFramesBuffered = 0;
	unsigned int currentAudioFramesBuffered = 0;
	unsigned int minAllowedSyncFactor = 0;
	unsigned int maxAllowedSyncFactor = 0;
	unsigned int currentSyncFactor = 0;
	unsigned int numberOfActiveAudioConfiguration = 0;
	unsigned int numberOfConfigBlocks = 0;
	std::vector<DecodedAudioGlobalConfigBlock> configBlocks;
	int activeConfigSampleRateByteOffset = -1;
	int activeConfigSampleRateCode = -1;
};

DecodedAudioGlobalPayload decodeAudioGlobalPayload(const BYTE_VECTOR &payload) {
	DecodedAudioGlobalPayload decoded;
	if (payload.size() < 11) {
		decoded.validationNote = "payload_too_short_for_global_header";
		return decoded;
	}

	decoded.commandVersionNumber = payload.at(0);
	decoded.numberOfAudioPorts =
		static_cast<unsigned int>((payload.at(1) << 7) | payload.at(2));
	decoded.minAudioFramesBuffered = payload.at(3);
	decoded.maxAudioFramesBuffered = payload.at(4);
	decoded.currentAudioFramesBuffered = payload.at(5);
	decoded.minAllowedSyncFactor = payload.at(6);
	decoded.maxAllowedSyncFactor = payload.at(7);
	decoded.currentSyncFactor = payload.at(8);
	decoded.numberOfActiveAudioConfiguration = payload.at(9);
	decoded.numberOfConfigBlocks = payload.at(10);

	for (unsigned int i = 0; i < decoded.numberOfConfigBlocks; ++i) {
		const int base = 11 + static_cast<int>(i) * 3;
		if (base + 2 >= static_cast<int>(payload.size()))
			break;
		DecodedAudioGlobalConfigBlock block;
		block.offsetConfigurationNumber = base;
		block.offsetBitDepthCode = base + 1;
		block.offsetSampleRateCode = base + 2;
		block.configurationNumber = payload.at(static_cast<size_t>(base));
		block.bitDepthCode = payload.at(static_cast<size_t>(base + 1));
		block.sampleRateCode = payload.at(static_cast<size_t>(base + 2));
		decoded.configBlocks.push_back(block);
	}

	if (decoded.numberOfActiveAudioConfiguration > 0) {
		const int activeIndex =
			static_cast<int>(decoded.numberOfActiveAudioConfiguration - 1);
		if (activeIndex >= 0 &&
			activeIndex < static_cast<int>(decoded.configBlocks.size())) {
			decoded.activeConfigSampleRateByteOffset =
				decoded.configBlocks.at(static_cast<size_t>(activeIndex))
					.offsetSampleRateCode;
			decoded.activeConfigSampleRateCode =
				static_cast<int>(decoded.configBlocks.at(
					static_cast<size_t>(activeIndex))
					 .sampleRateCode);
		}
	}

	decoded.valid = true;
	return decoded;
}

struct ClockCompareInput {
	QString filePath;
	QString externalKnownState;
	std::vector<BYTE_VECTOR> payloads;
	BYTE_VECTOR selectedPayload;
	bool hasPayload = false;
	bool stableWithinFile = true;
	QString note;
};

ClockCompareInput parseClockCompareInput(const QString &filePath) {
	ClockCompareInput input;
	input.filePath = filePath;

	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		input.note = QString("could_not_open: %1").arg(file.errorString());
		return input;
	}

	const QString content = QString::fromUtf8(file.readAll());
	file.close();
	input.externalKnownState = extractExternalKnownState(filePath, content);

	const QRegularExpression responsePattern(
		"^(raw_response_\\d+|clock_raw_response_\\d+)=(.+)$",
		QRegularExpression::MultilineOption);
	QRegularExpressionMatchIterator responseMatches =
		responsePattern.globalMatch(content);
	while (responseMatches.hasNext()) {
		const QRegularExpressionMatch match = responseMatches.next();
		BYTE_VECTOR message;
		if (!parseHexByteVector(match.captured(2), message))
			continue;
		BYTE_VECTOR payload;
		if (!extractClockPayloadFromMessage(message, payload))
			continue;
		if (!containsPayload(input.payloads, payload))
			input.payloads.push_back(payload);
	}

	if (input.payloads.empty()) {
		const QRegularExpression payloadPattern(
			"^raw_payload=(.+)$", QRegularExpression::MultilineOption);
		QRegularExpressionMatchIterator payloadMatches =
			payloadPattern.globalMatch(content);
		while (payloadMatches.hasNext()) {
			const QRegularExpressionMatch match = payloadMatches.next();
			BYTE_VECTOR payload;
			if (!parseHexByteVector(match.captured(1), payload))
				continue;
			if (!containsPayload(input.payloads, payload))
				input.payloads.push_back(payload);
		}
	}

	if (!input.payloads.empty()) {
		input.hasPayload = true;
		input.selectedPayload = input.payloads.front();
		for (const BYTE_VECTOR &payload : input.payloads) {
			if (payload != input.selectedPayload) {
				input.stableWithinFile = false;
				break;
			}
		}
	} else {
		input.note = "no RET_SET_AUDIO_CLOCK_PARM payload found";
	}
	return input;
}

struct GlobalCompareInput {
	QString filePath;
	QString externalKnownState;
	std::vector<BYTE_VECTOR> payloads;
	BYTE_VECTOR selectedPayload;
	DecodedAudioGlobalPayload decodedSelected;
	bool hasPayload = false;
	bool stableWithinFile = true;
	QString note;
};

GlobalCompareInput parseGlobalCompareInput(const QString &filePath) {
	GlobalCompareInput input;
	input.filePath = filePath;

	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		input.note = QString("could_not_open: %1").arg(file.errorString());
		return input;
	}

	const QString content = QString::fromUtf8(file.readAll());
	file.close();
	input.externalKnownState = extractExternalKnownState(filePath, content);

	const QStringList lines = content.split('\n');
	for (const QString &line : lines) {
		BYTE_VECTOR message;
		if (!extractHexMessageFromLine(line, message))
			continue;
		BYTE_VECTOR payload;
		if (!extractGlobalPayloadFromMessage(message, payload))
			continue;
		if (!containsPayload(input.payloads, payload))
			input.payloads.push_back(payload);
	}

	if (input.payloads.empty()) {
		const QRegularExpression payloadPattern(
			"^global_raw_payload=(.+)$", QRegularExpression::MultilineOption);
		QRegularExpressionMatchIterator payloadMatches =
			payloadPattern.globalMatch(content);
		while (payloadMatches.hasNext()) {
			const QRegularExpressionMatch match = payloadMatches.next();
			BYTE_VECTOR payload;
			if (!parseHexByteVector(match.captured(1), payload))
				continue;
			if (!containsPayload(input.payloads, payload))
				input.payloads.push_back(payload);
		}
	}

	if (!input.payloads.empty()) {
		input.hasPayload = true;
		input.selectedPayload = input.payloads.front();
		input.decodedSelected = decodeAudioGlobalPayload(input.selectedPayload);
		for (const BYTE_VECTOR &payload : input.payloads) {
			if (payload != input.selectedPayload) {
				input.stableWithinFile = false;
				break;
			}
		}
	} else {
		input.note = "no RET_SET_AUDIO_GLOBAL_PARM payload found";
	}

	return input;
}

QString payloadByteAt(const BYTE_VECTOR &payload, int offset) {
	if (offset < 0 || offset >= static_cast<int>(payload.size()))
		return "--";
	return byteHex(payload.at(static_cast<size_t>(offset)));
}

QString externalStateRateOnly(const QString &externalKnownState) {
	const QRegularExpression ratePattern("\\b(44100|48000|88200|96000)\\b");
	QRegularExpressionMatch match = ratePattern.match(externalKnownState);
	if (!match.hasMatch())
		return QString();
	return match.captured(1);
}
} // namespace

int MioMain::sigpipe[2];

MioMain *MioMain::pMainWindow = nullptr;

MioMain::MioMain(QCommandLineParser *parser, QWidget *parent)
	: QMainWindow(parent), m_pUi(new Ui::MioMain) {
	m_pUi->setupUi(this);
	pMainWindow = this;
	m_bDumpAudio4cClockOnStartup = parser->isSet("dump-audio4c-clock");
	m_bShowReadonlyPanels = parser->isSet("show-readonly-panels");
	m_bCompareAudio4cClockOnStartup =
		parser->isSet("compare-audio4c-clock");
	m_bCompareAudio4cGlobalOnStartup =
		parser->isSet("compare-audio4c-global");
	m_compareAudio4cClockFiles = parser->positionalArguments();
	m_bDumpDiagnosticsOnStartup =
		parser->isSet("dump-device-diagnostics") || m_bDumpAudio4cClockOnStartup;
	m_pActionDumpDiagnostics =
		new QAction(tr("Dump Device Diagnostics"), this);
	m_pUi->menuDevice->addSeparator();
	m_pUi->menuDevice->addAction(m_pActionDumpDiagnostics);
	m_pActionDumpDiagnostics->setEnabled(false);
	setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::ForceTabbedDocks |
				   QMainWindow::VerticalTabs);
	if (parser->isSet("filename")) {
		QString *fileName = new QString(parser->value("filename"));
		setConfigurationFile(fileName);
	}
	if (m_sConfigurationFile) {
		m_pConfiguration = new QSettings(*m_sConfigurationFile, xmlFormat);
		m_pConfiguration->beginGroup("Test");
		m_pConfiguration->setValue("ConfigName", "lala");
		m_pConfiguration->endGroup();
	}
	connectSignals();
#ifdef __LINUX_SESSION_MANAGEMENT__
	if (!installSignalHandlers())
        qWarning("%s", "Signal handlers not installed!");
#endif
	if (m_bCompareAudio4cClockOnStartup && m_bCompareAudio4cGlobalOnStartup) {
		std::cerr << "Use only one compare mode at a time: "
					 "--compare-audio4c-clock or --compare-audio4c-global."
				  << std::endl;
		QCoreApplication::exit(2);
		delete parser;
		return;
	}
	if (m_bCompareAudio4cClockOnStartup) {
		QTimer::singleShot(0, this, SLOT(runCompareAudio4cClockFromStartup()));
		delete parser;
		return;
	}
	if (m_bCompareAudio4cGlobalOnStartup) {
		QTimer::singleShot(0, this, SLOT(runCompareAudio4cGlobalFromStartup()));
		delete parser;
		return;
	}
	readSettings();
	createLanguageMenu();
	if (readDevicesFromSettings())
		openDefaultDevice();
	else
		QTimer::singleShot(100, this, SLOT(openDetectionWindow()));
	delete parser;
}

MioMain::~MioMain() {
	if (m_pDeviceDetectionWindow)
		delete m_pDeviceDetectionWindow;
	if (m_pCentralDeviceWidget)
		delete m_pCentralDeviceWidget;
	m_DockWidgetAreasMap.clear();
	delete m_pUi;
}

void MioMain::openDefaultDevice() {
	writeDevicesToSettings();
	unsigned long defaultDeviceSN =
		Configuration::getInstance().getDefaultDevice();
	if (Configuration::getInstance().getDevices()->size() > 0) {
		try {
			std::shared_ptr<Device> d =
				Configuration::getInstance().getDevices()->at(defaultDeviceSN);
			addDevicesToSelectionMenu(defaultDeviceSN);
			openDeviceGUI(d.get());
		} catch (const std::out_of_range &oor) {
			std::cerr << oor.what() << std::endl;
		}
		m_pUi->menuSelect->setEnabled(true);
		m_pActionDumpDiagnostics->setEnabled(true);
	} else
		m_pUi->menuSelect->setEnabled(false);
	if (Configuration::getInstance().getDevices()->size() == 0)
		m_pActionDumpDiagnostics->setEnabled(false);
}

void MioMain::addDevicesToSelectionMenu(unsigned long defaultDeviceSN) {
	Devices *devices = Configuration::getInstance().getDevices();
	QActionGroup *devicesGroup = new QActionGroup(this);
	devicesGroup->setExclusive(true);
	for (Devices::iterator it = devices->begin(); it != devices->end(); ++it) {
		std::shared_ptr<Device> d = it->second;
		QAction *a = m_pUi->menuSelect->addAction(
			QString::fromStdString(d->getDeviceName()));
		a->setCheckable(true);
		devicesGroup->addAction(a);
		connect(a, &QAction::triggered, this,
				[this, d](bool) { openDeviceGUI(d.get()); });
		if (it->first == defaultDeviceSN)
			a->setChecked(true);
	}
}

void MioMain::addDock(QDockWidget *dockWidget, Qt::DockWidgetArea area) {
	if (MultiInfoWidget *miw = dynamic_cast<MultiInfoWidget *>(dockWidget))
		miw->createInfoSections();
	switch (area) {
	case Qt::NoDockWidgetArea:
		setCentralWidget(dockWidget);
		break;
	case Qt::LeftDockWidgetArea:
		this->addDockWidget(Qt::LeftDockWidgetArea, dockWidget);
		break;
	default:
		break;
	}
	std::vector<QDockWidget *> v = m_DockWidgetAreasMap[area];
	if (v.size() > 0)
		tabifyDockWidget(v[v.size() - 1], dockWidget);
	m_DockWidgetAreasMap[area].push_back(dockWidget);
}

void MioMain::clearDocWidgets() {
	removeToolBar(m_pToolBar);
	delete m_pToolBar;
	m_pToolBar = nullptr;
	for (std::map<Qt::DockWidgetArea, std::vector<QDockWidget *>>::iterator it =
			 m_DockWidgetAreasMap.begin();
		 it != m_DockWidgetAreasMap.end(); ++it) {
		std::vector<QDockWidget *> v = it->second;
		for (unsigned int j = 0; j < v.size(); j++) {
			QWidget *w = v.at(j);
			delete w;
		}
		v.clear();
	}
	m_DockWidgetAreasMap.clear();
}

void MioMain::replacePanel(QWidget *w) {
	CentralWidget *cw = dynamic_cast<CentralWidget *>(centralWidget());
	cw->replacePanel(w);
}

void MioMain::addDeviceToolButtons() {
	if (m_pCurrentDevice && m_pCurrentDevice->isWriteGuardEnabled())
		return;
	BYTE_VECTOR *saveRestoreList = this->m_pCurrentDevice->m_pSaveRestoreList;
	if (nullptr == saveRestoreList)
		return;
	for (unsigned int i = 0; i < saveRestoreList->size(); ++i) {
		switch (
			static_cast<SaveRestore::SaveResstoreId>((*saveRestoreList)[i])) {
		case SaveRestore::SAVE_TO_DEVICE: {
			QToolButton *btn = new QToolButton();
			btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
			btn->setText("Save");
			btn->setToolTip(tr("Save current settings to device"));
			m_pToolBar->addWidget(btn);
			btn->setIcon(QIcon(":/pixmaps/saveto"));
			connect(btn, SIGNAL(pressed()), this, SLOT(storeToDevice()));
		} break;
		case SaveRestore::RESTORE_FROM_DEVICE: {
			QToolButton *btn = new QToolButton();
			btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
			btn->setText("Restore");
			btn->setToolTip(tr("Restore settings from device"));
			m_pToolBar->addWidget(btn);
			btn->setIcon(QIcon(":/pixmaps/readfrom"));
			connect(btn, SIGNAL(pressed()), this, SLOT(restoreFromDevice()));
		} break;
		case SaveRestore::SET_TO_FACTORY_DEFAULT: {
			QToolButton *btn = new QToolButton();
			btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
			btn->setText("Fact");
			btn->setToolTip(tr("Reset settings to factory default"));
			m_pToolBar->addWidget(btn);
			btn->setIcon(QIcon(":/pixmaps/restore"));
			connect(btn, SIGNAL(pressed()), this,
					SLOT(resetToFactoryDefaults()));
		} break;
		default:
			break;
		}
	}
}

void MioMain::openDeviceGUI(Device *d) {
	clearDocWidgets();
	m_pToolBar = new QToolBar(tr("Device Actions"), this);
	m_pToolBar->setObjectName("DeviceActions");
	this->addToolBar(m_pToolBar);

	this->m_pCurrentDevice = d;
	m_pActionDumpDiagnostics->setEnabled(true);
	d->connect();
	// only the panel visibility can be changed here, never the write guard
	if (m_bShowReadonlyPanels)
		d->setHideConfigPanels(false);
	std::shared_ptr<RetCommandList> c = d->getCommands();
	if (c == nullptr) {
		// TODO throw error
		exit(2);
	}
	setWindowTitle(this->m_sTitle + QString(": ") +
				   QString::fromStdString(d->getDeviceName()));

	m_pCentralDeviceWidget = new CentralWidget(this, d);
	this->addDock(m_pCentralDeviceWidget);

	DeviceInfoWidget *deviceInfoWidget =
		new DeviceInfoWidget(this, d, d->getDeviceInfo());
	this->addDock(deviceInfoWidget, Qt::LeftDockWidgetArea);

	if (!d->shouldHideConfigPanels() && d->hasMidiSupport()) {
		PortsWidget *portsWidget = new PortsWidget(this, d);
		this->addDock(portsWidget, Qt::LeftDockWidgetArea);
	}
	if (!d->shouldHideConfigPanels() && d->hasAudioSupport()) {
		AudioPortsWidget *audioPortsWidget = new AudioPortsWidget(this, d);
		this->addDock(audioPortsWidget, Qt::LeftDockWidgetArea);
		if (d->getCommands()->isCommandSupported(GET_AUDIO_PATCHBAY_PARM) ||
			d->getCommands()->isCommandSupported(GET_MIXER_PARM)) {
			AudioWidget *audioRoutingsWidget = new AudioWidget(this, d);
			this->addDock(audioRoutingsWidget, Qt::LeftDockWidgetArea);
		}
	}
	addDeviceToolButtons();

	QSettings *settings = Configuration::getInstance().getSettings();
	settings->beginGroup("MainWindow");
	restoreGeometry(settings->value("geometry").toByteArray());
	settings->endGroup();
	settings->beginGroup("Docks");
	// restoreState(settings->value("DockWindows").toByteArray());
	settings->endGroup();
	delete settings;
	deviceInfoWidget->show();
	deviceInfoWidget->raise();
	if (m_bDumpDiagnosticsOnStartup) {
		const bool previousLogState = SysExMessage::isStructuredLoggingEnabled();
		SysExMessage::setStructuredLoggingEnabled(true);
		const QString dumpContent = dumpDeviceDiagnostics();
		const QString filePrefix =
			m_bDumpAudio4cClockOnStartup
				? (m_pCurrentDevice->isExperimentalAudio4c() ? "audio4c-clock"
															 : "device-clock")
				: (m_pCurrentDevice->isExperimentalAudio4c() ? "audio4c"
															 : "device");
		const QString filename = dumpDiagnosticsToFile(dumpContent, filePrefix);
		SysExMessage::setStructuredLoggingEnabled(previousLogState);
		if (!filename.isEmpty()) {
			statusBar()->showMessage(
				tr("Diagnostics written to %1").arg(filename), 10000);
			std::cout << "Diagnostics written to " << filename.toStdString()
					  << std::endl;
			if (m_bDumpAudio4cClockOnStartup) {
				const QString clockSection =
					extractSection(dumpContent, "[Clock Investigation]");
				std::cout << "Clock Investigation Summary\n"
						  << clockSection.toStdString() << std::endl;
			}
		} else {
			std::cerr << "Failed to write diagnostics file" << std::endl;
		}
		m_bDumpDiagnosticsOnStartup = false;
		m_bDumpAudio4cClockOnStartup = false;
		QTimer::singleShot(0, qApp, SLOT(quit()));
	}
	showExperimentalWarningIfNeeded();
}

void MioMain::storeToDevice() {
	if (m_pCurrentDevice && m_pCurrentDevice->isWriteGuardEnabled()) {
		QMessageBox::warning(this, tr("Read-only Mode"),
							 tr("Device writes are disabled in experimental "
								"read-only mode."));
		return;
	}
	QMessageBox msgBox;
	msgBox.setText(tr("Store current settings to device?"));
	msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
	msgBox.setDefaultButton(QMessageBox::Save);
	int ret = msgBox.exec();
	if (ret == QMessageBox::Ok)
		saveRestore(SaveRestore::SAVE_TO_DEVICE);
}

void MioMain::reinitDevice() {
	QProgressDialog progress(
		tr("Waiting 10 seconds for device to be responsive again"),
		tr("Exit application"), 0, 10, this);
	progress.setWindowModality(Qt::WindowModal);

	for (int i = 0; i < 10; i++) {
		SLEEP(1000);
		progress.setValue(i);
		if (progress.wasCanceled())
			exit(-1);
		if (i == 5)
			m_pCurrentDevice->disconnect();
	}
	m_pCurrentDevice->connect();
	progress.setValue(10);
	openDeviceGUI(m_pCurrentDevice);
	bool valid = m_pCurrentDevice->isDeviceValid();
	if (!valid)
		exit(-1);
}

void MioMain::readConfigurationFromFile() {}

void MioMain::writeConfigurtionToFile() {}

void MioMain::restoreFromDevice() {
	if (m_pCurrentDevice && m_pCurrentDevice->isWriteGuardEnabled()) {
		QMessageBox::warning(this, tr("Read-only Mode"),
							 tr("Device writes are disabled in experimental "
								"read-only mode."));
		return;
	}
	QMessageBox msgBox;
	msgBox.setText(tr("Read all settings from device?"));
	msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
	msgBox.setDefaultButton(QMessageBox::Save);
	int ret = msgBox.exec();
	if (ret == QMessageBox::Ok) {
		saveRestore(SaveRestore::RESTORE_FROM_DEVICE);
		reinitDevice();
	}
}

void MioMain::resetToFactoryDefaults() {
	if (m_pCurrentDevice && m_pCurrentDevice->isWriteGuardEnabled()) {
		QMessageBox::warning(this, tr("Read-only Mode"),
							 tr("Device writes are disabled in experimental "
								"read-only mode."));
		return;
	}
	QMessageBox msgBox;
	msgBox.setText(tr("Reset all settings to factory default?"));
	msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
	msgBox.setDefaultButton(QMessageBox::Save);
	int ret = msgBox.exec();
	if (ret == QMessageBox::Ok) {
		saveRestore(SaveRestore::SET_TO_FACTORY_DEFAULT);
		reinitDevice();
	}
}

void MioMain::saveRestore(SaveRestore::SaveResstoreId saveRestoreId) {
	if (m_pCurrentDevice && m_pCurrentDevice->isWriteGuardEnabled()) {
		std::cerr << "Save/restore blocked in read-only experimental mode"
				  << std::endl;
		return;
	}
	SaveRestore *saveRestore = new SaveRestore(m_pCurrentDevice);
	saveRestore->setSaveRestoreId(saveRestoreId);
	saveRestore->execute();
}

void MioMain::showExperimentalWarningIfNeeded() {
	if (!m_pCurrentDevice || !m_pCurrentDevice->isWriteGuardEnabled() ||
		m_bWarnedExperimentalMode) {
		return;
	}
	m_bWarnedExperimentalMode = true;
	const QString warningText =
		tr("AUDIO4c support is experimental; writes to the device are blocked.");
	const QStringList args = QCoreApplication::arguments();
	if (args.contains("--dump-device-diagnostics") ||
		args.contains("--dump-audio4c-clock")) {
		std::cout << warningText.toStdString() << std::endl;
		statusBar()->showMessage(warningText, 10000);
		return;
	}
	QMessageBox::warning(this, tr("Experimental Device Support"), warningText);
	statusBar()->showMessage(warningText, 10000);
}

QString MioMain::dumpDeviceDiagnostics() {
	if (!m_pCurrentDevice)
		return QString();

	Device *device = m_pCurrentDevice;
	QStringList lines;
	std::vector<ProbeFailure> failures;
	std::vector<ProbeSuccess> successes;
	QStringList unsupportedCommands;
	std::map<unsigned int, unsigned int> maxControllersByPort;
	std::map<int, int> successCountByCommand;
	QStringList clockRawResponses;
	std::vector<std::shared_ptr<RetSetAudioClockParm>> decodedClockReads;
	BYTE_VECTOR globalPayload;
	bool hasGlobalPayload = false;
	DecodedAudioGlobalPayload decodedGlobalPayload;
	bool hasDecodedGlobalPayload = false;

	struct ControlProbeTarget {
		unsigned int portId = 0;
		unsigned int controllerNumber = 0;
		pk::AudioControllerType controllerType = pk::AudioControllerType::CT_NONE;
		unsigned int detailCount = 0;
	};
	struct DetailProbeTarget {
		unsigned int portId = 0;
		unsigned int controllerNumber = 0;
		unsigned int detailNumber = 0;
		pk::AudioControllerType controllerType = pk::AudioControllerType::CT_NONE;
	};
	std::vector<ControlProbeTarget> controlTargets;
	std::vector<DetailProbeTarget> detailTargets;
	std::vector<ProbeSuccess> clockInvestigationSuccesses;
	std::shared_ptr<RetCommandList> commandList = device->getCommands();
	struct PortHardwareSnapshot {
		bool hasAudioPortParm = false;
		QString portName;
		QString portTypeName;
		unsigned int inputChannels = 0;
		unsigned int outputChannels = 0;
		int deviceSpecificPortNumber = -1;
		int jackSpecificDeviceNumber = -1;
		bool hasPortIOSFlags = false;
		bool portSupportsIOS = false;
		bool portSupportsPC = false;
		bool portIOSEnabled = false;
		bool portPCEnabled = false;

		bool hasAudioDeviceParm = false;
		unsigned int maxControllers = 0;
		bool hasHostName = false;
		QString hostName;
		bool hasUsbDeviceHost = false;
		unsigned int usbDeviceHostCode = 0;
		bool hasHostConnected = false;
		bool hostConnected = false;
		bool hasPortReserved = false;
		bool portReservedForVendorProduct = false;
	};
	std::map<unsigned int, PortHardwareSnapshot> portSnapshots;

	auto markUnsupported = [&](Command command, const QString &why = QString()) {
		QString line = QString("%1 %2")
						   .arg(toCommandHex(static_cast<int>(command)),
								commandNameForId(static_cast<int>(command)));
		if (!why.isEmpty())
			line += QString(" | %1").arg(why);
		unsupportedCommands << line;
	};
	auto markSuccess = [&](Command command) {
		successCountByCommand[static_cast<int>(command)]++;
	};
	auto isSupported = [&](Command command) {
		return commandList &&
			   commandList->isCommandSupported(command);
	};

	QDateTime now = QDateTime::currentDateTime();
	lines << "iConnConfig Device Diagnostics";
	lines << QString("generated_at=%1").arg(now.toString(Qt::ISODate));
	lines << QString("device_name=%1")
				 .arg(QString::fromStdString(device->getDeviceName()));
	lines << QString("model_name=%1")
				 .arg(QString::fromStdString(device->getModelName()));
	lines << QString("model_number=%1")
				 .arg(QString::fromStdString(device->getModelNumber()));
	lines << QString("serial_number=%1")
				 .arg(QString::fromStdString(device->getSerialNumberString()));
	lines << QString("product_id=%1")
				 .arg(toHex(static_cast<unsigned int>(
					 device->getProductId()->getLongValue())));
	lines << QString("experimental_audio4c=%1")
				 .arg(device->isExperimentalAudio4c() ? "true" : "false");
	lines << QString("read_only_mode=%1")
				 .arg(device->isReadOnlyMode() ? "true" : "false");
	lines << "";
	lines << "[MIDI Ports]";
	lines << QString("input_port_index=%1").arg(device->getInPortNumer());
	lines << QString("output_port_index=%1").arg(device->getOutPortNumer());
	lines << QString("input_port_name=%1")
				 .arg(QString::fromStdString(device->getDetectedInputPortName()));
	lines << QString("output_port_name=%1").arg(
		QString::fromStdString(device->getDetectedOutputPortName()));
	lines << "";

	if (!commandList) {
		std::shared_ptr<GetCommandList> getCommandList =
			std::make_shared<GetCommandList>(device);
			try {
				commandList = std::dynamic_pointer_cast<RetCommandList>(
					getCommandList->querySmart());
				appendProbeSuccess(successes, device, GET_COMMAND_LIST, {});
			} catch (CommunicationException *e) {
				appendProbeFailure(failures, device, GET_COMMAND_LIST, {},
					QString::fromStdString(e->getErrorMessage()).trimmed());
				delete e;
			} catch (ProtocolException *e) {
				appendProbeFailure(failures, device, GET_COMMAND_LIST, {},
					QString::fromStdString(e->getErrorMessage()).trimmed());
				delete e;
			} catch (ProtocolException &e) {
				appendProbeFailure(failures, device, GET_COMMAND_LIST, {},
					QString::fromStdString(e.getErrorMessage()).trimmed());
			} catch (const std::exception &e) {
				appendProbeFailure(failures, device, GET_COMMAND_LIST, {},
					QString("std::exception: %1").arg(e.what()));
			}
	}

	std::vector<int> supportedCommands;
	lines << "[Command List]";
	if (commandList) {
		supportedCommands = commandList->getSupportedCommands();
		for (int commandId : supportedCommands) {
			const Command cmd = static_cast<Command>(commandId);
			lines << QString("%1 %2")
						 .arg(toHex(static_cast<unsigned int>(commandId)))
						 .arg(commandToString(cmd));
		}
	} else {
		lines << "unavailable";
	}
	lines << "";

	lines << "[Device / Global Info]";
	std::shared_ptr<GetInfo> info = device->getDeviceInfo();
	if (info && info->getRetSetInfos()) {
		for (auto infoPair : *info->getRetSetInfos()) {
			std::shared_ptr<RetSetInfo> infoValue = infoPair.second;
			if (!infoValue)
				continue;
			lines << QString("%1=%2")
						 .arg(QString::fromStdString(infoValue->getItemName()))
						 .arg(QString::fromStdString(infoValue->getValue()));
		}
	} else {
		lines << "unavailable";
	}
	lines << "";

	lines << "[Supported Audio Commands]";
	for (int commandId : supportedCommands) {
		const Command cmd = static_cast<Command>(commandId);
		if (isAudioRelatedCommand(cmd)) {
			lines << QString("%1 %2")
						 .arg(toHex(static_cast<unsigned int>(commandId)))
						 .arg(commandToString(cmd));
		}
	}
	lines << "";

	unsigned int numberOfAudioPorts = 0;
	lines << "[Audio Global Parameters]";
	if (isSupported(Command::GET_AUDIO_GLOBAL_PARM)) {
		try {
			std::shared_ptr<GetAudioGlobalParm> getAudioGlobal =
				std::make_shared<GetAudioGlobalParm>(device);
			std::shared_ptr<RetSetAudioGlobalParm> global =
				std::dynamic_pointer_cast<RetSetAudioGlobalParm>(
					getAudioGlobal->querySmart());
			const DecodedAck ack = responseAck(device);
			if (ack.isError) {
				appendProbeFailure(failures, device, GET_AUDIO_GLOBAL_PARM, {},
								   ack.summary);
				lines << "response unavailable";
			} else if (global) {
				appendProbeSuccess(successes, device, GET_AUDIO_GLOBAL_PARM, {});
				markSuccess(GET_AUDIO_GLOBAL_PARM);
				numberOfAudioPorts = global->getNumberOfAudioPorts();
				BYTE_VECTOR extractedGlobalPayload;
				const BYTE_VECTOR *lastGlobalResponse = device->getLastRetrieveMessage();
				if (lastGlobalResponse &&
					extractGlobalPayloadFromMessage(*lastGlobalResponse,
												 extractedGlobalPayload)) {
					hasGlobalPayload = true;
					globalPayload = extractedGlobalPayload;
					decodedGlobalPayload =
						decodeAudioGlobalPayload(globalPayload);
					hasDecodedGlobalPayload = decodedGlobalPayload.valid;
				}
				lines << QString("number_of_audio_ports=%1")
							 .arg(global->getNumberOfAudioPorts());
				lines << QString("audio_frames_buffered_current=%1")
							 .arg(global->getCurrentAudioFramesBuffered());
				lines << QString("audio_frames_buffered_min=%1")
							 .arg(global->getMinAudioFramesBuffered());
				lines << QString("audio_frames_buffered_max=%1")
							 .arg(global->getMaxAudioFramesBuffered());
				lines << QString("sync_factor_current=%1")
							 .arg(global->getCurrentSyncFactor());
				lines << QString("sync_factor_min=%1")
							 .arg(global->getMinAllowedSyncFactor());
				lines << QString("sync_factor_max=%1")
							 .arg(global->getMaxAllowedSyncFactor());
				lines << QString("active_audio_configuration=%1")
							 .arg(global->getNumberOfActiveAudioConfiguration());
				if (hasGlobalPayload)
					lines << QString("global_raw_payload=%1")
								 .arg(vectorHex(globalPayload));
			} else {
				appendProbeFailure(failures, device, GET_AUDIO_GLOBAL_PARM, {},
								   "response unavailable");
				lines << "response unavailable";
			}
		} catch (CommunicationException *e) {
			appendProbeFailure(failures, device, GET_AUDIO_GLOBAL_PARM, {},
				QString::fromStdString(e->getErrorMessage()).trimmed());
			delete e;
		} catch (ProtocolException *e) {
			appendProbeFailure(failures, device, GET_AUDIO_GLOBAL_PARM, {},
				QString::fromStdString(e->getErrorMessage()).trimmed());
			delete e;
		} catch (ProtocolException &e) {
			appendProbeFailure(failures, device, GET_AUDIO_GLOBAL_PARM, {},
				QString::fromStdString(e.getErrorMessage()).trimmed());
		}
	} else {
		lines << "unsupported";
		markUnsupported(GET_AUDIO_GLOBAL_PARM, "unsupported_by_command_list");
	}
	lines << "";

	lines << "[Audio Port Parameters]";
	if (isSupported(Command::GET_AUDIO_PORT_PARM) && numberOfAudioPorts > 0) {
		for (unsigned int portId = 1; portId <= numberOfAudioPorts; ++portId) {
			const QStringList args = {QString("port=%1").arg(portId)};
			try {
				std::unique_ptr<GetAudioPortParm> getAudioPort =
					std::make_unique<GetAudioPortParm>(device);
				getAudioPort->setPortId(portId);
				std::shared_ptr<RetSetAudioPortParm> audioPortParm =
					std::dynamic_pointer_cast<RetSetAudioPortParm>(
						getAudioPort->querySmart());
				const DecodedAck ack = responseAck(device);
				if (ack.isError) {
					appendProbeFailure(failures, device, GET_AUDIO_PORT_PARM,
									   args, ack.summary);
					continue;
				}
				if (audioPortParm) {
					appendProbeSuccess(successes, device, GET_AUDIO_PORT_PARM,
									   args);
					markSuccess(GET_AUDIO_PORT_PARM);
					PortHardwareSnapshot &snapshot = portSnapshots[portId];
					snapshot.hasAudioPortParm = true;
					snapshot.portName =
						QString::fromStdString(audioPortParm->getPortName());
					snapshot.portTypeName =
						QString::fromStdString(audioPortParm->getAudioPortTypeName());
					snapshot.inputChannels = audioPortParm->getInputChannels();
					snapshot.outputChannels = audioPortParm->getOutputChannels();
					snapshot.deviceSpecificPortNumber =
						audioPortParm->getDeviceSpecficPortNumber();
					snapshot.jackSpecificDeviceNumber =
						audioPortParm->getJackSpecificDeviceNumber();
					snapshot.hasPortIOSFlags = true;
					snapshot.portSupportsIOS = audioPortParm->getPortSupportsIOS();
					snapshot.portSupportsPC = audioPortParm->getPortSupportsPC();
					snapshot.portIOSEnabled = audioPortParm->getPortIOSEnabled();
					snapshot.portPCEnabled = audioPortParm->getPortPCEnabled();
					lines << QString("port_%1.name=%2")
								 .arg(portId)
								 .arg(QString::fromStdString(
									 audioPortParm->getPortName()));
					lines << QString("port_%1.type=%2")
								 .arg(portId)
								 .arg(QString::fromStdString(
									 audioPortParm->getAudioPortTypeName()));
					lines << QString("port_%1.input_channels=%2")
								 .arg(portId)
								 .arg(audioPortParm->getInputChannels());
					lines << QString("port_%1.output_channels=%2")
								 .arg(portId)
								 .arg(audioPortParm->getOutputChannels());
					lines << QString("port_%1.usb_ios_supported_candidate=%2")
								 .arg(portId)
								 .arg(audioPortParm->getPortSupportsIOS() ? "true"
																	 : "false");
					lines << QString("port_%1.usb_pc_supported_candidate=%2")
								 .arg(portId)
								 .arg(audioPortParm->getPortSupportsPC() ? "true"
																   : "false");
					lines << QString("port_%1.usb_ios_enabled_candidate=%2")
								 .arg(portId)
								 .arg(audioPortParm->getPortIOSEnabled() ? "true"
																  : "false");
					lines << QString("port_%1.usb_pc_enabled_candidate=%2")
								 .arg(portId)
								 .arg(audioPortParm->getPortPCEnabled() ? "true"
																 : "false");
				} else {
					appendProbeFailure(failures, device, GET_AUDIO_PORT_PARM,
									   args, "response unavailable");
				}
			} catch (CommunicationException *e) {
				appendProbeFailure(failures, device, GET_AUDIO_PORT_PARM, args,
					QString::fromStdString(e->getErrorMessage()).trimmed());
				delete e;
			} catch (ProtocolException *e) {
				appendProbeFailure(failures, device, GET_AUDIO_PORT_PARM, args,
					QString::fromStdString(e->getErrorMessage()).trimmed());
				delete e;
			} catch (ProtocolException &e) {
				appendProbeFailure(failures, device, GET_AUDIO_PORT_PARM, args,
					QString::fromStdString(e.getErrorMessage()).trimmed());
			}
		}
	} else {
		lines << "unsupported";
		if (!isSupported(Command::GET_AUDIO_PORT_PARM))
			markUnsupported(GET_AUDIO_PORT_PARM, "unsupported_by_command_list");
	}
	lines << "";

	lines << "[Audio Device Parameters]";
	if (isSupported(Command::GET_AUDIO_DEVICE_PARM) && numberOfAudioPorts > 0) {
		for (unsigned int portId = 1; portId <= numberOfAudioPorts; ++portId) {
			const QStringList args = {QString("port=%1").arg(portId)};
			try {
				std::unique_ptr<GetAudioDeviceParm> getAudioDevice =
					std::make_unique<GetAudioDeviceParm>(device);
				getAudioDevice->setPortId(portId);
				std::shared_ptr<RetSetAudioDeviceParm> audioDeviceParm =
					std::dynamic_pointer_cast<RetSetAudioDeviceParm>(
						getAudioDevice->querySmart());
				const DecodedAck ack = responseAck(device);
				if (ack.isError) {
					appendProbeFailure(failures, device, GET_AUDIO_DEVICE_PARM,
									   args, ack.summary);
					continue;
				}
				if (audioDeviceParm) {
					appendProbeSuccess(successes, device, GET_AUDIO_DEVICE_PARM,
									   args);
					markSuccess(GET_AUDIO_DEVICE_PARM);
					maxControllersByPort[portId] =
						audioDeviceParm->getMaxControllers();
					PortHardwareSnapshot &snapshot = portSnapshots[portId];
					snapshot.hasAudioDeviceParm = true;
					snapshot.maxControllers = audioDeviceParm->getMaxControllers();
					snapshot.hasUsbDeviceHost = true;
					snapshot.usbDeviceHostCode = static_cast<unsigned int>(
						audioDeviceParm->getUsbDeviceHost());
					snapshot.hasHostConnected = true;
					snapshot.hostConnected = audioDeviceParm->getHostConnected();
					snapshot.hasPortReserved = true;
					snapshot.portReservedForVendorProduct =
						audioDeviceParm->getPortReservedForVendorProduct();
					lines << QString("port_%1.max_controllers=%2")
								 .arg(portId)
								 .arg(audioDeviceParm->getMaxControllers());
					lines << QString("port_%1.usb_device_host_code_candidate=%2")
								 .arg(portId)
								 .arg(static_cast<unsigned int>(
									 audioDeviceParm->getUsbDeviceHost()));
					lines << QString("port_%1.usb_device_host_label_candidate=%2")
								 .arg(portId)
								 .arg(usbDeviceHostCodeLabel(
									 static_cast<unsigned int>(
										 audioDeviceParm->getUsbDeviceHost())));
					lines << QString("port_%1.usb_host_connected_candidate=%2")
								 .arg(portId)
								 .arg(audioDeviceParm->getHostConnected() ? "true"
																	 : "false");
					lines
						<< QString(
							   "port_%1.usb_port_reserved_for_vendor_candidate=%2")
							   .arg(portId)
							   .arg(audioDeviceParm
											->getPortReservedForVendorProduct()
										? "true"
										: "false");
					if (audioDeviceParm->hastHostName()) {
						snapshot.hasHostName = true;
						snapshot.hostName =
							QString::fromStdString(audioDeviceParm->getHostName());
						lines << QString("port_%1.host_name=%2")
									 .arg(portId)
									 .arg(QString::fromStdString(
										 audioDeviceParm->getHostName()));
					}
				} else {
					appendProbeFailure(failures, device, GET_AUDIO_DEVICE_PARM,
									   args, "response unavailable");
				}
			} catch (CommunicationException *e) {
				appendProbeFailure(failures, device, GET_AUDIO_DEVICE_PARM, args,
					QString::fromStdString(e->getErrorMessage()).trimmed());
				delete e;
			} catch (ProtocolException *e) {
				appendProbeFailure(failures, device, GET_AUDIO_DEVICE_PARM, args,
					QString::fromStdString(e->getErrorMessage()).trimmed());
				delete e;
			} catch (ProtocolException &e) {
				appendProbeFailure(failures, device, GET_AUDIO_DEVICE_PARM, args,
					QString::fromStdString(e.getErrorMessage()).trimmed());
			}
		}
	} else {
		lines << "unsupported";
		if (!isSupported(Command::GET_AUDIO_DEVICE_PARM))
			markUnsupported(GET_AUDIO_DEVICE_PARM, "unsupported_by_command_list");
	}
	lines << "";

	lines << "[Audio Control Parameters]";
	if (isSupported(Command::GET_AUDIO_CONTROL_PARM) && numberOfAudioPorts > 0) {
		for (unsigned int portId = 1; portId <= numberOfAudioPorts; ++portId) {
			const unsigned int maxControllers = maxControllersByPort[portId];
			lines << QString("port_%1.max_controllers=%2")
						 .arg(portId)
						 .arg(maxControllers);
			if (maxControllers == 0)
				continue;
			for (unsigned int controllerNumber = 1; controllerNumber <= maxControllers;
				 ++controllerNumber) {
				const QStringList args = {QString("port=%1").arg(portId),
										  QString("controller=%1").arg(controllerNumber)};
				try {
					std::unique_ptr<GetAudioControlParm> getAudioControlParm =
						std::make_unique<GetAudioControlParm>(device);
					getAudioControlParm->setPortId(portId);
					getAudioControlParm->setControllerNumber(controllerNumber);
					std::shared_ptr<RetSetAudioControlParm> controlParm =
						std::dynamic_pointer_cast<RetSetAudioControlParm>(
							getAudioControlParm->querySmart());
					const DecodedAck ack = responseAck(device);
					if (ack.isError) {
						appendProbeFailure(failures, device, GET_AUDIO_CONTROL_PARM,
										   args, ack.summary);
						continue;
					}
					if (!controlParm) {
						appendProbeFailure(failures, device, GET_AUDIO_CONTROL_PARM,
										   args, "response unavailable");
						continue;
					}
					appendProbeSuccess(successes, device, GET_AUDIO_CONTROL_PARM,
									   args);
					markSuccess(GET_AUDIO_CONTROL_PARM);
					const pk::AudioControllerType controllerType =
						controlParm->getControllerType();
					unsigned int detailCount = 0;
					if (controllerType == pk::AudioControllerType::CT_FEATUERE)
						detailCount = controlParm->getNumberOfFeatuireChannels();
					else if (controllerType ==
							 pk::AudioControllerType::CT_CLOCK_SOURCE)
						detailCount = controlParm->getNumberOfClockSourceInputs();
					else if (controllerType == pk::AudioControllerType::CT_SELECTOR)
						detailCount = controlParm->getNumberOfSelectorInputs();
					controlTargets.push_back({portId, controllerNumber, controllerType,
											 detailCount});
					lines << QString("port_%1.controller_%2.type=%3")
								 .arg(portId)
								 .arg(controllerNumber)
								 .arg(static_cast<int>(controllerType));
					lines << QString("port_%1.controller_%2.name=%3")
								 .arg(portId)
								 .arg(controllerNumber)
								 .arg(QString::fromStdString(
									 controlParm->getControllerName()));
					lines << QString("port_%1.controller_%2.detail_count=%3")
								 .arg(portId)
								 .arg(controllerNumber)
								 .arg(detailCount);
					if (controllerType == pk::AudioControllerType::CT_CLOCK_SOURCE) {
						lines << QString("port_%1.controller_%2.current_clock_input=%3")
									 .arg(portId)
									 .arg(controllerNumber)
									 .arg(controlParm->getCurrenClockSourceInput());
						clockInvestigationSuccesses.push_back(successes.back());
					}
				} catch (CommunicationException *e) {
					appendProbeFailure(
						failures, device, GET_AUDIO_CONTROL_PARM, args,
						QString::fromStdString(e->getErrorMessage()).trimmed());
					delete e;
				} catch (ProtocolException *e) {
					appendProbeFailure(
						failures, device, GET_AUDIO_CONTROL_PARM, args,
						QString::fromStdString(e->getErrorMessage()).trimmed());
					delete e;
				} catch (ProtocolException &e) {
					appendProbeFailure(
						failures, device, GET_AUDIO_CONTROL_PARM, args,
						QString::fromStdString(e.getErrorMessage()).trimmed());
				}
			}
		}
	} else {
		lines << "unsupported";
		if (!isSupported(Command::GET_AUDIO_CONTROL_PARM))
			markUnsupported(GET_AUDIO_CONTROL_PARM, "unsupported_by_command_list");
	}
	lines << "";

	lines << "[Audio Control Details]";
	if (isSupported(Command::GET_AUDIO_CONTROL_DETAIL) &&
		!controlTargets.empty()) {
		for (const ControlProbeTarget &target : controlTargets) {
			if (target.detailCount == 0)
				continue;
			for (unsigned int detailNumber = 1; detailNumber <= target.detailCount;
				 ++detailNumber) {
				const QStringList args = {QString("port=%1").arg(target.portId),
										  QString("controller=%1")
											  .arg(target.controllerNumber),
										  QString("detail=%1").arg(detailNumber)};
				try {
					std::unique_ptr<GetAudioControlDetail> getControlDetail =
						std::make_unique<GetAudioControlDetail>(device);
					getControlDetail->setPortId(target.portId);
					getControlDetail->setControllerNumber(
						target.controllerNumber);
					getControlDetail->setDetailNumber(detailNumber);
					std::shared_ptr<RetSetAudioControlDetail> detail =
						std::dynamic_pointer_cast<RetSetAudioControlDetail>(
							getControlDetail->querySmart());
					const DecodedAck ack = responseAck(device);
					if (ack.isError) {
						appendProbeFailure(failures, device, GET_AUDIO_CONTROL_DETAIL,
										   args, ack.summary);
						continue;
					}
					if (!detail) {
						appendProbeFailure(failures, device, GET_AUDIO_CONTROL_DETAIL,
										   args, "response unavailable");
						continue;
					}
					appendProbeSuccess(successes, device, GET_AUDIO_CONTROL_DETAIL,
									   args);
					markSuccess(GET_AUDIO_CONTROL_DETAIL);
					detailTargets.push_back({target.portId, target.controllerNumber,
											detailNumber,
											target.controllerType});
					lines << QString("port_%1.controller_%2.detail_%3.response_raw=%4")
								 .arg(target.portId)
								 .arg(target.controllerNumber)
								 .arg(detailNumber)
								 .arg(messageHex(device->getLastRetrieveMessage()));
					if (target.controllerType == pk::AudioControllerType::CT_FEATUERE)
						lines << QString("port_%1.controller_%2.detail_%3.channel_name=%4")
									 .arg(target.portId)
									 .arg(target.controllerNumber)
									 .arg(detailNumber)
									 .arg(QString::fromStdString(
										 detail->getChannelName()));
					if (target.controllerType ==
						pk::AudioControllerType::CT_CLOCK_SOURCE)
						clockInvestigationSuccesses.push_back(successes.back());
				} catch (CommunicationException *e) {
					appendProbeFailure(
						failures, device, GET_AUDIO_CONTROL_DETAIL, args,
						QString::fromStdString(e->getErrorMessage()).trimmed());
					delete e;
				} catch (ProtocolException *e) {
					appendProbeFailure(
						failures, device, GET_AUDIO_CONTROL_DETAIL, args,
						QString::fromStdString(e->getErrorMessage()).trimmed());
					delete e;
				} catch (ProtocolException &e) {
					appendProbeFailure(
						failures, device, GET_AUDIO_CONTROL_DETAIL, args,
						QString::fromStdString(e.getErrorMessage()).trimmed());
				}
			}
		}
	} else if (!isSupported(Command::GET_AUDIO_CONTROL_DETAIL)) {
		lines << "unsupported";
		markUnsupported(GET_AUDIO_CONTROL_DETAIL, "unsupported_by_command_list");
	} else {
		lines << "no controller targets discovered";
	}
	lines << "";

	lines << "[Audio Control Detail Values]";
	if (isSupported(Command::GET_AUDIO_CONTROL_DETAIL_VALUE) &&
		!detailTargets.empty()) {
		for (const DetailProbeTarget &target : detailTargets) {
			const QStringList args = {QString("port=%1").arg(target.portId),
									  QString("controller=%1")
										  .arg(target.controllerNumber),
									  QString("detail=%1").arg(target.detailNumber)};
			try {
				std::unique_ptr<GetAudioControlDetailValue> getControlDetailValue =
					std::make_unique<GetAudioControlDetailValue>(device);
				getControlDetailValue->setPortId(target.portId);
				getControlDetailValue->setControllerNumber(
					static_cast<unsigned char>(target.controllerNumber));
				getControlDetailValue->setDetailNumber(
					static_cast<unsigned char>(target.detailNumber));
				std::shared_ptr<RetSetAudioControlDetailValue> detailValue =
					std::dynamic_pointer_cast<RetSetAudioControlDetailValue>(
						getControlDetailValue->querySmart());
				const DecodedAck ack = responseAck(device);
				if (ack.isError) {
					appendProbeFailure(failures, device,
									   GET_AUDIO_CONTROL_DETAIL_VALUE, args,
									   ack.summary);
					continue;
				}
				if (!detailValue) {
					appendProbeFailure(failures, device,
									   GET_AUDIO_CONTROL_DETAIL_VALUE, args,
									   "response unavailable");
					continue;
				}
				appendProbeSuccess(successes, device,
								   GET_AUDIO_CONTROL_DETAIL_VALUE, args);
				markSuccess(GET_AUDIO_CONTROL_DETAIL_VALUE);
				lines << QString("port_%1.controller_%2.detail_%3.response_raw=%4")
							 .arg(target.portId)
							 .arg(target.controllerNumber)
							 .arg(target.detailNumber)
							 .arg(messageHex(device->getLastRetrieveMessage()));
				if (target.controllerType == pk::AudioControllerType::CT_FEATUERE) {
					lines << QString("port_%1.controller_%2.detail_%3.has_volume=%4")
								 .arg(target.portId)
								 .arg(target.controllerNumber)
								 .arg(target.detailNumber)
								 .arg(detailValue->hasVolumeControl() ? "true"
																  : "false");
					if (detailValue->hasVolumeControl())
						lines << QString("port_%1.controller_%2.detail_%3.volume=%4")
									 .arg(target.portId)
									 .arg(target.controllerNumber)
									 .arg(target.detailNumber)
									 .arg(detailValue->getVolume());
				}
				if (target.controllerType == pk::AudioControllerType::CT_CLOCK_SOURCE)
					clockInvestigationSuccesses.push_back(successes.back());
			} catch (CommunicationException *e) {
				appendProbeFailure(
					failures, device, GET_AUDIO_CONTROL_DETAIL_VALUE, args,
					QString::fromStdString(e->getErrorMessage()).trimmed());
				delete e;
			} catch (ProtocolException *e) {
				appendProbeFailure(
					failures, device, GET_AUDIO_CONTROL_DETAIL_VALUE, args,
					QString::fromStdString(e->getErrorMessage()).trimmed());
				delete e;
			} catch (ProtocolException &e) {
				appendProbeFailure(
					failures, device, GET_AUDIO_CONTROL_DETAIL_VALUE, args,
					QString::fromStdString(e.getErrorMessage()).trimmed());
			}
		}
	} else if (!isSupported(Command::GET_AUDIO_CONTROL_DETAIL_VALUE)) {
		lines << "unsupported";
		markUnsupported(GET_AUDIO_CONTROL_DETAIL_VALUE,
						"unsupported_by_command_list");
	} else {
		lines << "no detail targets discovered";
	}
	lines << "";

	lines << "[Audio Clock Parameters]";
	if (isSupported(Command::GET_AUDIO_CLOCK_PARM)) {
		const int clockReadAttempts = 3;
		lines << QString("read_attempts=%1").arg(clockReadAttempts);
		for (int readIndex = 1; readIndex <= clockReadAttempts; ++readIndex) {
			const QStringList args = {QString("read=%1").arg(readIndex)};
			try {
				std::unique_ptr<GetAudioClockParm> getAudioClock =
					std::make_unique<GetAudioClockParm>(device);
				std::shared_ptr<RetSetAudioClockParm> clockParm =
					std::dynamic_pointer_cast<RetSetAudioClockParm>(
						getAudioClock->querySmart());
				const DecodedAck ack = responseAck(device);
				if (ack.isError) {
					appendProbeFailure(failures, device, GET_AUDIO_CLOCK_PARM, args,
									   ack.summary);
					lines << QString("raw_response_%1=n/a").arg(readIndex);
					continue;
				}
				if (!clockParm) {
					appendProbeFailure(failures, device, GET_AUDIO_CLOCK_PARM, args,
									   "response unavailable");
					lines << QString("raw_response_%1=n/a").arg(readIndex);
					continue;
				}
				appendProbeSuccess(successes, device, GET_AUDIO_CLOCK_PARM, args);
				markSuccess(GET_AUDIO_CLOCK_PARM);
				clockInvestigationSuccesses.push_back(successes.back());
				const QString rawClock = messageHex(device->getLastRetrieveMessage());
				lines << QString("raw_response_%1=%2").arg(readIndex).arg(rawClock);
				clockRawResponses << rawClock;
				decodedClockReads.push_back(clockParm);
			} catch (CommunicationException *e) {
				appendProbeFailure(failures, device, GET_AUDIO_CLOCK_PARM, args,
								   QString::fromStdString(e->getErrorMessage())
									   .trimmed());
				delete e;
			} catch (ProtocolException *e) {
				appendProbeFailure(failures, device, GET_AUDIO_CLOCK_PARM, args,
								   QString::fromStdString(e->getErrorMessage())
									   .trimmed());
				delete e;
			} catch (ProtocolException &e) {
				appendProbeFailure(failures, device, GET_AUDIO_CLOCK_PARM, args,
								   QString::fromStdString(e.getErrorMessage())
									   .trimmed());
			}
		}
		if (clockRawResponses.isEmpty()) {
			lines << "raw_response=none";
			lines << "raw_stable_across_reads=false";
		} else {
			const QString firstRaw = clockRawResponses.first();
			bool rawStable = true;
			for (const QString &raw : clockRawResponses) {
				if (raw != firstRaw) {
					rawStable = false;
					break;
				}
			}
			lines << QString("raw_stable_across_reads=%1")
						 .arg(rawStable ? "true" : "false");
		}
	} else {
		lines << "unsupported";
		markUnsupported(GET_AUDIO_CLOCK_PARM, "unsupported_by_command_list");
	}
	lines << "";

	lines << "[Decoded Clock Parameters]";
	if (decodedClockReads.empty()) {
		lines << "unavailable";
		lines << "confidence_level=none";
		lines << "pipewire_rate_check_todo=No decoded clock payload to compare.";
	} else {
		const std::shared_ptr<RetSetAudioClockParm> firstClockRead =
			decodedClockReads.front();
		const BYTE_VECTOR payload = firstClockRead->getPayloadBytes();
		bool payloadStable = true;
		for (const std::shared_ptr<RetSetAudioClockParm> &clockRead :
			 decodedClockReads) {
			if (clockRead->getPayloadBytes() != payload) {
				payloadStable = false;
				break;
			}
		}
		lines << QString("raw_payload=%1").arg(vectorHex(payload));
		lines << QString("payload_size_bytes=%1")
					 .arg(static_cast<int>(payload.size()));
		lines << QString("payload_stable_across_reads=%1")
					 .arg(payloadStable ? "true" : "false");
		for (size_t i = 0; i < decodedClockReads.size(); ++i) {
			lines << QString("decoded_read_%1.payload=%2")
						 .arg(static_cast<int>(i + 1))
						 .arg(vectorHex(decodedClockReads.at(i)->getPayloadBytes()));
		}

		lines << "field_offset_table_begin";
		for (size_t i = 0; i < payload.size(); ++i) {
			lines << QString("offset_%1.byte=%2")
						 .arg(static_cast<int>(i))
						 .arg(byteHex(payload.at(i)));
		}
		lines << "field_offset_table_end";

		lines << QString("protocol_version=%1")
					 .arg(firstClockRead->getProtocolVersion());
		lines << "protocol_version_confidence=high";

		bool sourceStable = true;
		bool hasSourceCandidate = firstClockRead->hasCurrentClockSourceCandidate();
		unsigned int sourceCandidate =
			firstClockRead->getCurrentClockSourceCandidate();
		for (const std::shared_ptr<RetSetAudioClockParm> &clockRead :
			 decodedClockReads) {
			if (!clockRead->hasCurrentClockSourceCandidate() ||
				clockRead->getCurrentClockSourceCandidate() != sourceCandidate) {
				sourceStable = false;
				break;
			}
		}
		if (hasSourceCandidate) {
			lines << QString("clock_source_candidate_code=%1")
						 .arg(sourceCandidate);
			lines << QString("clock_source_candidate_label=%1")
						 .arg(clockSourceCodeLabel(sourceCandidate));
			lines << QString("clock_source_confidence=%1")
						 .arg(sourceStable
								  ? "medium (stable bytes, semantic candidate)"
								  : "low (unstable bytes, semantic candidate)");
		} else {
			lines << "clock_source_candidate_code=unknown";
			lines << "clock_source_confidence=low";
		}

		bool rateStable = true;
		bool hasRateCandidate =
			firstClockRead->hasCurrentSampleRateCodeCandidate();
		unsigned int rateCodeCandidate =
			firstClockRead->getCurrentSampleRateCodeCandidate();
		for (const std::shared_ptr<RetSetAudioClockParm> &clockRead :
			 decodedClockReads) {
			if (!clockRead->hasCurrentSampleRateCodeCandidate() ||
				clockRead->getCurrentSampleRateCodeCandidate() !=
					rateCodeCandidate) {
				rateStable = false;
				break;
			}
		}
		int rateHzCandidate = -1;
		if (hasRateCandidate) {
			rateHzCandidate = sampleRateFromCodeCandidate(rateCodeCandidate);
			lines << QString("sample_rate_code_candidate=%1 (experimental)")
						 .arg(rateCodeCandidate);
			lines << QString("sample_rate_hz_hypothesis_legacy_map=%1")
						 .arg(sampleRateCodeLabel(rateCodeCandidate));
			lines << "sample_rate_decode_status=experimental_unverified";
			lines << QString("sample_rate_confidence=%1")
						 .arg(rateStable
								  ? "low (stable byte, semantic meaning unverified)"
								  : "very-low (unstable byte, semantic meaning "
									"unverified)");
		} else {
			lines << "sample_rate_code_candidate=unknown";
			lines << "sample_rate_decode_status=experimental_unverified";
			lines << "sample_rate_confidence=very-low";
		}

		const std::vector<BYTE_VECTOR> candidateTriples =
			firstClockRead->getCandidateTriples();
		lines << QString("candidate_triplet_count=%1")
					 .arg(static_cast<int>(candidateTriples.size()));
		for (size_t i = 0; i < candidateTriples.size(); ++i) {
			const BYTE_VECTOR triple = candidateTriples.at(i);
			const unsigned int sourceCode = triple.at(0);
			const unsigned int sampleRateCode = triple.at(1);
			const unsigned int lockCode = triple.at(2);
			const int startOffset = 3 + static_cast<int>(i) * 3;
			lines << QString("triplet_%1.offset_start=%2")
						 .arg(static_cast<int>(i + 1))
						 .arg(startOffset);
			lines << QString("triplet_%1.bytes=%2 %3 %4")
						 .arg(static_cast<int>(i + 1))
						 .arg(byteHex(sourceCode), byteHex(sampleRateCode),
							  byteHex(lockCode));
			lines << QString("triplet_%1.source_code_candidate=%2 (%3)")
						 .arg(static_cast<int>(i + 1))
						 .arg(sourceCode)
						 .arg(clockSourceCodeLabel(sourceCode));
			lines << QString("triplet_%1.sample_rate_code_candidate=%2 (%3)")
						 .arg(static_cast<int>(i + 1))
						 .arg(sampleRateCode)
						 .arg(sampleRateCodeLabel(sampleRateCode));
			lines << QString("triplet_%1.lock_status_code_candidate=%2 (%3)")
						 .arg(static_cast<int>(i + 1))
						 .arg(lockCode)
						 .arg(lockStatusCodeLabel(lockCode));
		}
		lines << "sync_lock_status_confidence=low (layout hypothesis only)";

		const std::vector<BYTE_VECTOR> candidatePairs =
			firstClockRead->getCandidatePairs();
		lines << QString("candidate_pair_count=%1")
					 .arg(static_cast<int>(candidatePairs.size()));
		for (size_t i = 0; i < candidatePairs.size(); ++i) {
			const BYTE_VECTOR pair = candidatePairs.at(i);
			const int startOffset = 1 + static_cast<int>(i) * 2;
			lines << QString("pair_%1.offset_start=%2")
						 .arg(static_cast<int>(i + 1))
						 .arg(startOffset);
			lines << QString("pair_%1.bytes=%2 %3")
						 .arg(static_cast<int>(i + 1))
						 .arg(byteHex(pair.at(0)), byteHex(pair.at(1)));
		}
		if (firstClockRead->hasTrailingCandidateByte()) {
			lines << QString("trailing_candidate_byte=%1")
						 .arg(byteHex(firstClockRead->getTrailingCandidateByte()));
		}

		const PipeWireRateProbe pipeWireRateProbe = probePipeWireRate();
		lines << QString("pipewire_rate_probe.command_available=%1")
					 .arg(pipeWireRateProbe.commandAvailable ? "true" : "false");
		if (pipeWireRateProbe.rateFound) {
			lines << QString("pipewire_rate_probe.field=%1")
						 .arg(pipeWireRateProbe.queriedField);
			lines << QString("pipewire_rate_hz=%1")
						 .arg(pipeWireRateProbe.rateHz);
			if (rateHzCandidate > 0) {
				lines << QString("sample_rate_vs_pipewire_hypothesis=%1")
							 .arg(pipeWireRateProbe.rateHz == rateHzCandidate
									  ? "match"
									  : "mismatch");
			} else {
				lines << "sample_rate_vs_pipewire_hypothesis=unknown_rate_candidate";
			}
		} else {
			lines << "pipewire_rate_hz=unavailable";
			lines
				<< "pipewire_rate_check_todo=No PipeWire rate available; compare "
				   "once pw-metadata exposes rate.";
		}
		if (!pipeWireRateProbe.rawOutput.isEmpty())
			lines << QString("pipewire_rate_probe.raw_output=%1")
						 .arg(pipeWireRateProbe.rawOutput);
		if (!pipeWireRateProbe.note.isEmpty())
			lines << QString("pipewire_rate_probe.note=%1")
						 .arg(pipeWireRateProbe.note);

		QString overallConfidence = "low";
		if (payloadStable && sourceStable && rateStable)
			overallConfidence =
				"low-medium (stable bytes; semantic mapping still candidate)";
		lines << QString("confidence_level=%1").arg(overallConfidence);
		lines
			<< "sample_rate_identification_instruction=To identify sample-rate "
			   "byte(s), collect dumps at known hardware rates: 44100, 48000, "
			   "96000, then run --compare-audio4c-clock.";
	}
	lines << "";

	lines << "[Audio Patchbay Parameters]";
	if (isSupported(Command::GET_AUDIO_PATCHBAY_PARM) &&
		numberOfAudioPorts > 0) {
		for (unsigned int portId = 1; portId <= numberOfAudioPorts; ++portId) {
			const QStringList args = {QString("port=%1").arg(portId)};
			try {
				std::unique_ptr<GetAudioPatchbayParm> getAudioPatchbay =
					std::make_unique<GetAudioPatchbayParm>(device);
				getAudioPatchbay->setPortId(portId);
				std::shared_ptr<RetSetAudioPatchbayParm> patchbay =
					std::dynamic_pointer_cast<RetSetAudioPatchbayParm>(
						getAudioPatchbay->querySmart());
				const DecodedAck ack = responseAck(device);
				if (ack.isError) {
					appendProbeFailure(failures, device, GET_AUDIO_PATCHBAY_PARM,
									   args, ack.summary);
					continue;
				}
				if (patchbay) {
					appendProbeSuccess(successes, device, GET_AUDIO_PATCHBAY_PARM,
									   args);
					markSuccess(GET_AUDIO_PATCHBAY_PARM);
					lines << QString("port_%1.patchbay_blocks=%2")
								 .arg(portId)
								 .arg(patchbay->getNumberOfPatchbayConfigBlocks());
					lines << QString("port_%1.patchbay_routes=%2")
								 .arg(portId)
								 .arg(static_cast<int>(
									 patchbay->getAudioPatchbayConfiguration()
										 ->size()));
				} else {
					appendProbeFailure(failures, device, GET_AUDIO_PATCHBAY_PARM,
									   args, "response unavailable");
				}
			} catch (CommunicationException *e) {
				appendProbeFailure(failures, device, GET_AUDIO_PATCHBAY_PARM, args,
					QString::fromStdString(e->getErrorMessage()).trimmed());
				delete e;
			} catch (ProtocolException *e) {
				appendProbeFailure(failures, device, GET_AUDIO_PATCHBAY_PARM, args,
					QString::fromStdString(e->getErrorMessage()).trimmed());
				delete e;
			} catch (ProtocolException &e) {
				appendProbeFailure(failures, device, GET_AUDIO_PATCHBAY_PARM, args,
					QString::fromStdString(e.getErrorMessage()).trimmed());
			}
		}
	} else {
		lines << "unsupported";
		if (!isSupported(Command::GET_AUDIO_PATCHBAY_PARM))
			markUnsupported(GET_AUDIO_PATCHBAY_PARM, "unsupported_by_command_list");
	}
	lines << "";

	lines << "[AUDIO4c Hardware Settings / Global Audio Configuration]";
	lines << QString("read_only_mode=%1")
				 .arg(device->isReadOnlyMode() ? "true" : "false");
	lines << QString("device_global_info=%1")
				 .arg((info && info->getRetSetInfos()) ? "known" : "unknown");
	lines << QString("audio_global_configuration=%1")
				 .arg(hasGlobalPayload ? "known" : "unsupported_or_unknown");
	lines << QString("audio_device_parameters=%1")
				 .arg(successCountByCommand[static_cast<int>(GET_AUDIO_DEVICE_PARM)] > 0
						  ? "known"
						  : (isSupported(GET_AUDIO_DEVICE_PARM) ? "unknown"
															 : "unsupported"));
	lines << QString("audio_port_parameters=%1")
				 .arg(successCountByCommand[static_cast<int>(GET_AUDIO_PORT_PARM)] > 0
						  ? "known"
						  : (isSupported(GET_AUDIO_PORT_PARM) ? "unknown"
														   : "unsupported"));
	lines << QString("audio_patchbay_parameters=%1")
				 .arg(successCountByCommand[static_cast<int>(GET_AUDIO_PATCHBAY_PARM)] > 0
						  ? "known"
						  : (isSupported(GET_AUDIO_PATCHBAY_PARM) ? "unknown"
															 : "unsupported"));
	lines << QString("audio_clock_parameters=%1")
				 .arg(successCountByCommand[static_cast<int>(GET_AUDIO_CLOCK_PARM)] > 0
						  ? "known"
						  : (isSupported(GET_AUDIO_CLOCK_PARM) ? "unknown"
														   : "unsupported"));
	lines
		<< "audio_clock_sample_rate_correlation=not_observed_to_vary_with_"
		   "sample_rate_in_known_44100_48000_dumps (hypothesis)";

	if (hasGlobalPayload) {
		lines << QString("global_raw_payload=%1").arg(vectorHex(globalPayload));
		lines << QString("global_payload_size_bytes=%1")
					 .arg(static_cast<int>(globalPayload.size()));
		lines << "global_field_offset_table_begin";
		for (size_t i = 0; i < globalPayload.size(); ++i) {
			lines << QString("global_offset_%1.byte=%2")
						 .arg(static_cast<int>(i))
						 .arg(byteHex(globalPayload.at(i)));
		}
		lines << "global_field_offset_table_end";
		if (hasDecodedGlobalPayload) {
			lines << QString("global.command_version=%1")
						 .arg(decodedGlobalPayload.commandVersionNumber);
			lines << QString("global.number_of_audio_ports=%1")
						 .arg(decodedGlobalPayload.numberOfAudioPorts);
			lines << QString("global.audio_frames_buffered_min=%1")
						 .arg(decodedGlobalPayload.minAudioFramesBuffered);
			lines << QString("global.audio_frames_buffered_max=%1")
						 .arg(decodedGlobalPayload.maxAudioFramesBuffered);
			lines << QString("global.audio_frames_buffered_current=%1")
						 .arg(decodedGlobalPayload.currentAudioFramesBuffered);
			lines << QString("global.sync_factor_min=%1")
						 .arg(decodedGlobalPayload.minAllowedSyncFactor);
			lines << QString("global.sync_factor_max=%1")
						 .arg(decodedGlobalPayload.maxAllowedSyncFactor);
			lines << QString("global.sync_factor_current=%1")
						 .arg(decodedGlobalPayload.currentSyncFactor);
			lines << QString("global.active_audio_configuration=%1")
						 .arg(decodedGlobalPayload.numberOfActiveAudioConfiguration);
			lines << QString("global.config_block_count=%1")
						 .arg(decodedGlobalPayload.numberOfConfigBlocks);
			for (size_t i = 0; i < decodedGlobalPayload.configBlocks.size(); ++i) {
				const DecodedAudioGlobalConfigBlock &block =
					decodedGlobalPayload.configBlocks.at(i);
				lines << QString("global.config_%1.configuration_number=%2")
							 .arg(static_cast<int>(i + 1))
							 .arg(block.configurationNumber);
				lines << QString("global.config_%1.bit_depth_code=%2")
							 .arg(static_cast<int>(i + 1))
							 .arg(block.bitDepthCode);
				lines << QString("global.config_%1.bit_depth_label=%2")
							 .arg(static_cast<int>(i + 1))
							 .arg(globalBitDepthCodeLabel(block.bitDepthCode));
				lines << QString("global.config_%1.sample_rate_code=%2")
							 .arg(static_cast<int>(i + 1))
							 .arg(block.sampleRateCode);
				lines << QString("global.config_%1.sample_rate_label=%2")
							 .arg(static_cast<int>(i + 1))
							 .arg(globalSampleRateCodeLabel(block.sampleRateCode));
				lines << QString("global.config_%1.offsets=config=%2 bit=%3 rate=%4")
							 .arg(static_cast<int>(i + 1))
							 .arg(block.offsetConfigurationNumber)
							 .arg(block.offsetBitDepthCode)
							 .arg(block.offsetSampleRateCode);
			}
			if (decodedGlobalPayload.activeConfigSampleRateByteOffset >= 0) {
				lines << QString("candidate_hardware_sample_rate_byte_offset=%1")
							 .arg(decodedGlobalPayload.activeConfigSampleRateByteOffset);
				lines << QString("candidate_hardware_sample_rate_code=%1")
							 .arg(decodedGlobalPayload.activeConfigSampleRateCode);
				lines << QString("candidate_hardware_sample_rate_label=%1")
							 .arg(globalSampleRateCodeLabel(static_cast<unsigned int>(
								 decodedGlobalPayload.activeConfigSampleRateCode)));
			} else {
				lines << "candidate_hardware_sample_rate_byte_offset=unknown";
			}
			const PipeWireRateProbe globalRateProbe = probePipeWireRate();
			lines << QString("known_state_probe.pipewire_command_available=%1")
						 .arg(globalRateProbe.commandAvailable ? "true" : "false");
			if (globalRateProbe.rateFound) {
				lines << QString("known_state_probe.pipewire_rate_hz=%1")
							 .arg(globalRateProbe.rateHz);
				const int activeRateHypothesis =
					decodedGlobalPayload.activeConfigSampleRateCode > 0
						? globalSampleRateFromLegacyParserCode(static_cast<unsigned int>(
							  decodedGlobalPayload.activeConfigSampleRateCode))
						: -1;
				if (activeRateHypothesis > 0) {
					lines << QString("known_state_correlation=%1")
								 .arg(activeRateHypothesis == globalRateProbe.rateHz
										  ? "match (hypothesis)"
										  : "mismatch (hypothesis)");
				} else {
					lines << "known_state_correlation=unknown (no active-rate "
								"hypothesis)";
				}
			} else {
				lines << "known_state_probe.pipewire_rate_hz=unknown";
				lines << "known_state_correlation=unknown";
			}
			lines << "global_confidence_level=low-medium (parser field layout "
						"known; AUDIO4c semantics still hypothesis)";
		} else {
			lines << QString("global_decode_status=unknown (%1)")
						 .arg(decodedGlobalPayload.validationNote);
			lines << "global_confidence_level=low";
		}
	} else {
		lines << "global_raw_payload=unknown";
		lines << "global_decode_status=unsupported_or_unavailable";
		lines << "global_confidence_level=unknown";
	}

	if (portSnapshots.empty()) {
		lines << "ports=unknown";
	} else {
		for (const auto &entry : portSnapshots) {
			const unsigned int portId = entry.first;
			const PortHardwareSnapshot &snapshot = entry.second;
			lines << QString("port_%1.state=%2")
						 .arg(portId)
						 .arg(snapshot.hasAudioPortParm ? "known" : "unknown");
			if (snapshot.hasAudioPortParm) {
				lines << QString("port_%1.label=%2")
							 .arg(portId)
							 .arg(snapshot.portName);
				lines << QString("port_%1.type=%2")
							 .arg(portId)
							 .arg(snapshot.portTypeName);
				lines << QString("port_%1.channels_in_out=%2/%3")
							 .arg(portId)
							 .arg(snapshot.inputChannels)
							 .arg(snapshot.outputChannels);
				lines << QString("port_%1.usb_ios_supported_candidate=%2")
							 .arg(portId)
							 .arg(snapshot.portSupportsIOS ? "true" : "false");
				lines << QString("port_%1.usb_pc_supported_candidate=%2")
							 .arg(portId)
							 .arg(snapshot.portSupportsPC ? "true" : "false");
				lines << QString("port_%1.usb_ios_enabled_candidate=%2")
							 .arg(portId)
							 .arg(snapshot.portIOSEnabled ? "true" : "false");
				lines << QString("port_%1.usb_pc_enabled_candidate=%2")
							 .arg(portId)
							 .arg(snapshot.portPCEnabled ? "true" : "false");
			}
			if (snapshot.hasAudioDeviceParm) {
				lines << QString("port_%1.max_controllers=%2")
							 .arg(portId)
							 .arg(snapshot.maxControllers);
				if (snapshot.hasHostName) {
					lines << QString("port_%1.host_name=%2")
								 .arg(portId)
								 .arg(snapshot.hostName);
				}
				if (snapshot.hasUsbDeviceHost) {
					lines << QString("port_%1.usb_device_host_code_candidate=%2")
								 .arg(portId)
								 .arg(snapshot.usbDeviceHostCode);
					lines << QString("port_%1.usb_device_host_label_candidate=%2")
								 .arg(portId)
								 .arg(usbDeviceHostCodeLabel(
									 snapshot.usbDeviceHostCode));
				}
				if (snapshot.hasHostConnected) {
					lines << QString("port_%1.usb_host_connected_candidate=%2")
								 .arg(portId)
								 .arg(snapshot.hostConnected ? "true" : "false");
				}
				if (snapshot.hasPortReserved) {
					lines << QString(
								   "port_%1.usb_port_reserved_for_vendor_candidate=%2")
								   .arg(portId)
								   .arg(snapshot.portReservedForVendorProduct
											? "true"
											: "false");
				}
			}
		}
	}
	lines << "write_actions=disabled_for_audio4c_read_only_mode";
	lines << "";

	lines << "[Successful Audio Query Counts]";
	const std::vector<Command> trackedAudioCommands = {
		GET_AUDIO_GLOBAL_PARM,     GET_AUDIO_CLOCK_PARM,
		GET_AUDIO_PATCHBAY_PARM,   GET_AUDIO_PORT_PARM,
		GET_AUDIO_DEVICE_PARM,     GET_AUDIO_CONTROL_PARM,
		GET_AUDIO_CONTROL_DETAIL,  GET_AUDIO_CONTROL_DETAIL_VALUE};
	for (Command cmd : trackedAudioCommands) {
		lines << QString("%1 %2 success_count=%3")
					 .arg(toCommandHex(static_cast<int>(cmd)),
						  commandNameForId(static_cast<int>(cmd)))
					 .arg(successCountByCommand[static_cast<int>(cmd)]);
	}
	lines << "";

	lines << "[Successful Audio Responses]";
	bool anyTrackedSuccess = false;
	for (const ProbeSuccess &success : successes) {
		const Command cmd = static_cast<Command>(success.commandId);
		if (std::find(trackedAudioCommands.begin(), trackedAudioCommands.end(),
					  cmd) == trackedAudioCommands.end()) {
			continue;
		}
		anyTrackedSuccess = true;
		lines << QString("cmd=%1 %2 args=%3 response_cmd=%4 %5 tid=%6")
					 .arg(toCommandHex(success.commandId), success.commandName,
						  success.argsTuple, toCommandHex(success.responseCommandId),
						  success.responseCommandName)
					 .arg(success.transactionId >= 0
							  ? QString::number(success.transactionId)
							  : "n/a");
		lines << QString("  raw_response=%1").arg(success.responseRawHex);
		if (!success.details.isEmpty())
			lines << QString("  details=%1").arg(success.details);
	}
	if (!anyTrackedSuccess)
		lines << "none";
	lines << "";

	lines << "[Clock Investigation]";
	lines << QString("get_audio_clock_parm_supported=%1")
				 .arg(isSupported(GET_AUDIO_CLOCK_PARM) ? "true" : "false");
	unsigned int clockControllerCount = 0;
	for (const ControlProbeTarget &target : controlTargets) {
		if (target.controllerType == pk::AudioControllerType::CT_CLOCK_SOURCE)
			clockControllerCount++;
	}
	lines << QString("clock_source_controllers_discovered=%1")
				 .arg(clockControllerCount);
	lines << QString("clock_variant_success_count=%1")
				 .arg(static_cast<int>(clockInvestigationSuccesses.size()));
	for (size_t i = 0; i < clockInvestigationSuccesses.size(); ++i) {
		const ProbeSuccess &success = clockInvestigationSuccesses.at(i);
		lines << QString("clock_variant_%1=cmd=%2 %3 args=%4 response_cmd=%5 %6 "
						 "tid=%7")
					 .arg(static_cast<int>(i + 1))
					 .arg(toCommandHex(success.commandId), success.commandName,
						  success.argsTuple, toCommandHex(success.responseCommandId),
						  success.responseCommandName)
					 .arg(success.transactionId >= 0
							  ? QString::number(success.transactionId)
							  : "n/a");
		lines << QString("  raw_response=%1").arg(success.responseRawHex);
	}
	lines << QString("clock_raw_response_count=%1")
				 .arg(clockRawResponses.size());
	for (int i = 0; i < clockRawResponses.size(); ++i)
		lines << QString("clock_raw_response_%1=%2")
					 .arg(i + 1)
					 .arg(clockRawResponses.at(i));
	if (clockRawResponses.isEmpty())
		lines << "clock_raw_response=none";
	lines << "";

	lines << "[Unsupported Commands]";
	if (unsupportedCommands.isEmpty())
		lines << "none";
	else
		lines << unsupportedCommands;
	lines << "";

	lines << "[Failed Queries Grouped]";
	if (failures.empty()) {
		lines << "none";
	} else {
		std::map<QString, std::vector<ProbeFailure>> grouped;
		for (const ProbeFailure &failure : failures) {
			const QString key = QString("%1|%2")
									.arg(failure.commandId)
									.arg(failure.argsTuple);
			grouped[key].push_back(failure);
		}
		for (const auto &entry : grouped) {
			const std::vector<ProbeFailure> &group = entry.second;
			if (group.empty())
				continue;
			const ProbeFailure &first = group.front();
			lines << QString("cmd=%1 %2 args=%3 failures=%4")
						 .arg(toCommandHex(first.commandId), first.commandName,
							  first.argsTuple)
						 .arg(static_cast<int>(group.size()));
			for (size_t i = 0; i < group.size(); ++i) {
				const ProbeFailure &failure = group.at(i);
				QString detail = QString("  #%1 reason=%2")
									 .arg(static_cast<int>(i + 1))
									 .arg(failure.reason);
				if (!failure.ackSummary.isEmpty())
					detail += QString(", ack=%1").arg(failure.ackSummary);
				detail += QString(", response_cmd=%1 %2")
							  .arg(toCommandHex(failure.responseCommandId),
								   failure.responseCommandName);
				if (failure.transactionId >= 0)
					detail +=
						QString(", tid=%1").arg(failure.transactionId);
				lines << detail;
				lines << QString("    tx_raw=%1").arg(failure.queryRawHex);
				lines << QString("    rx_raw=%1").arg(failure.responseRawHex);
			}
		}
	}
	lines << "";
	return lines.join("\n");
}

QString MioMain::dumpDiagnosticsToFile(const QString &content,
									   const QString &filePrefix) {
	const QString diagnosticsDir = QDir::current().filePath("diagnostics");
	QDir dir;
	if (!dir.mkpath(diagnosticsDir))
		return QString();
	const QString timestamp =
		QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
	const QString filename =
		QString("%1/%2-%3.txt").arg(diagnosticsDir, filePrefix, timestamp);
	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return QString();
	QTextStream stream(&file);
	stream << content << "\n";
	file.close();
	return filename;
}

QString MioMain::compareAudio4cClockDiagnostics(
	const QStringList &filePaths) const {
	QStringList lines;
	lines << "AUDIO4c Clock Comparative Analysis (read-only)";
	lines << "sample_rate_decode_status=experimental_unverified";
	lines << QString("file_count=%1").arg(filePaths.size());

	std::vector<ClockCompareInput> parsedFiles;
	parsedFiles.reserve(static_cast<size_t>(filePaths.size()));
	for (const QString &filePath : filePaths)
		parsedFiles.push_back(parseClockCompareInput(filePath));

	size_t maxPayloadSize = 0;
	int filesWithPayload = 0;
	for (size_t i = 0; i < parsedFiles.size(); ++i) {
		const ClockCompareInput &parsed = parsedFiles.at(i);
		lines << QString("file_%1.path=%2")
					 .arg(static_cast<int>(i + 1))
					 .arg(parsed.filePath);
		lines << QString("file_%1.external_known_state=%2")
					 .arg(static_cast<int>(i + 1))
					 .arg(parsed.externalKnownState.isEmpty()
							  ? "not_provided"
							  : parsed.externalKnownState);
		lines << QString("file_%1.unique_payloads_found=%2")
					 .arg(static_cast<int>(i + 1))
					 .arg(static_cast<int>(parsed.payloads.size()));
		lines << QString("file_%1.payload_stable_within_file=%2")
					 .arg(static_cast<int>(i + 1))
					 .arg(parsed.stableWithinFile ? "true" : "false");
		if (parsed.hasPayload) {
			++filesWithPayload;
			maxPayloadSize =
				std::max(maxPayloadSize, parsed.selectedPayload.size());
			lines << QString("file_%1.selected_payload=%2")
						 .arg(static_cast<int>(i + 1))
						 .arg(vectorHex(parsed.selectedPayload));
		} else {
			lines << QString("file_%1.selected_payload=missing")
						 .arg(static_cast<int>(i + 1));
		}
		if (!parsed.note.isEmpty()) {
			lines << QString("file_%1.note=%2")
						 .arg(static_cast<int>(i + 1))
						 .arg(parsed.note);
		}
	}
	lines << "";

	if (filesWithPayload < 2) {
		lines << "[Comparison]";
		lines << "insufficient_payload_data=true";
		lines << "need_at_least_two_files_with_clock_payload=true";
		lines
			<< "sample_rate_interpretation=unknown_until_comparative_variation";
		lines
			<< "instruction=To identify sample-rate byte(s), collect dumps at "
			   "known hardware rates: 44100, 48000, 96000, then run "
			   "--compare-audio4c-clock.";
		return lines.join("\n");
	}

	std::vector<int> stableOffsets;
	std::vector<int> differingOffsets;
	lines << "[Byte Offset Table]";
	for (size_t offset = 0; offset < maxPayloadSize; ++offset) {
		QStringList perFile;
		bool haveReference = false;
		unsigned int referenceValue = 0;
		bool mismatch = false;
		bool hasMissing = false;
		for (size_t i = 0; i < parsedFiles.size(); ++i) {
			const ClockCompareInput &parsed = parsedFiles.at(i);
			QString byteValue = "--";
			if (parsed.hasPayload &&
				offset < parsed.selectedPayload.size()) {
				const unsigned int value = parsed.selectedPayload.at(offset);
				byteValue = byteHex(value);
				if (!haveReference) {
					haveReference = true;
					referenceValue = value;
				} else if (referenceValue != value) {
					mismatch = true;
				}
			} else {
				hasMissing = true;
			}
			perFile << QString("f%1=%2")
						   .arg(static_cast<int>(i + 1))
						   .arg(byteValue);
		}
		const bool isStable = haveReference && !mismatch && !hasMissing;
		if (isStable)
			stableOffsets.push_back(static_cast<int>(offset));
		else
			differingOffsets.push_back(static_cast<int>(offset));
		lines << QString("offset_%1: %2 status=%3")
					 .arg(static_cast<int>(offset))
					 .arg(perFile.join(' '))
					 .arg(isStable ? "stable" : "diff_or_missing");
	}
	lines << QString("stable_offset_count=%1")
				 .arg(static_cast<int>(stableOffsets.size()));
	lines << QString("differing_offset_count=%1")
				 .arg(static_cast<int>(differingOffsets.size()));
	if (stableOffsets.empty()) {
		lines << "stable_offsets=none";
	} else {
		QStringList offsetList;
		for (int offset : stableOffsets)
			offsetList << QString::number(offset);
		lines << QString("stable_offsets=%1").arg(offsetList.join(","));
	}
	if (differingOffsets.empty()) {
		lines << "differing_offsets=none";
	} else {
		QStringList offsetList;
		for (int offset : differingOffsets)
			offsetList << QString::number(offset);
		lines << QString("differing_offsets=%1").arg(offsetList.join(","));
	}
	lines << "";

	lines << "[Hypothesis Groupings]";
	lines << "warning=hypothesis_only_not_a_definitive_protocol_map";
	lines << "hypothesis_a.offsets_1_2=clock_source_code_candidate + "
				"sample_rate_code_candidate";
	lines << "hypothesis_b.offsets_3_plus_triplets=source/rate/lock tuples";
	for (size_t i = 0; i < parsedFiles.size(); ++i) {
		const ClockCompareInput &parsed = parsedFiles.at(i);
		if (!parsed.hasPayload)
			continue;
		const BYTE_VECTOR &payload = parsed.selectedPayload;
		const QString pair12 =
			payload.size() >= 3
				? QString("%1 %2")
					  .arg(payloadByteAt(payload, 1), payloadByteAt(payload, 2))
				: "insufficient_bytes";
		lines << QString("file_%1.hypothesis_a_pair_1_2=%2")
					 .arg(static_cast<int>(i + 1))
					 .arg(pair12);
		const int tripletCount =
			payload.size() > 3 ? static_cast<int>((payload.size() - 3) / 3) : 0;
		lines << QString("file_%1.hypothesis_b_triplet_count=%2")
					 .arg(static_cast<int>(i + 1))
					 .arg(tripletCount);
		for (int triplet = 0; triplet < tripletCount; ++triplet) {
			const int base = 3 + (triplet * 3);
			lines << QString("file_%1.hypothesis_triplet_%2.offset_start=%3")
						 .arg(static_cast<int>(i + 1))
						 .arg(triplet + 1)
						 .arg(base);
			lines << QString("file_%1.hypothesis_triplet_%2.bytes=%3 %4 %5")
						 .arg(static_cast<int>(i + 1))
						 .arg(triplet + 1)
						 .arg(payloadByteAt(payload, base),
							  payloadByteAt(payload, base + 1),
							  payloadByteAt(payload, base + 2));
		}
	}
	lines << "";

	lines << "[External Known State Correlation]";
	std::set<QString> knownRates;
	for (size_t i = 0; i < parsedFiles.size(); ++i) {
		const QString rate =
			externalStateRateOnly(parsedFiles.at(i).externalKnownState);
		if (!rate.isEmpty())
			knownRates.insert(rate);
		lines << QString("file_%1.external_state_rate=%2")
					 .arg(static_cast<int>(i + 1))
					 .arg(rate.isEmpty() ? "unknown" : rate);
	}
	lines << QString("distinct_external_state_rates=%1")
				 .arg(static_cast<int>(knownRates.size()));
	if (knownRates.size() < 2) {
		lines << "correlation_status=insufficient_known_rate_labels";
	} else if (differingOffsets.empty()) {
		lines << "correlation_status=known_rates_differ_but_payload_bytes_do_not";
	} else {
		lines << "correlation_status=payload_variation_present_compare_offsets";
	}
	lines << "sample_rate_interpretation=unknown_until_comparative_variation";
	lines
		<< "instruction=To identify sample-rate byte(s), collect dumps at known "
		   "hardware rates: 44100, 48000, 96000, then run "
		   "--compare-audio4c-clock.";

	return lines.join("\n");
}

QString MioMain::compareAudio4cGlobalDiagnostics(
	const QStringList &filePaths) const {
	QStringList lines;
	lines << "AUDIO4c Global Audio Comparative Analysis (read-only)";
	lines << "sample_rate_decode_status=experimental_unverified";
	lines << QString("file_count=%1").arg(filePaths.size());

	std::vector<GlobalCompareInput> parsedFiles;
	parsedFiles.reserve(static_cast<size_t>(filePaths.size()));
	for (const QString &filePath : filePaths)
		parsedFiles.push_back(parseGlobalCompareInput(filePath));

	size_t maxPayloadSize = 0;
	int filesWithPayload = 0;
	for (size_t i = 0; i < parsedFiles.size(); ++i) {
		const GlobalCompareInput &parsed = parsedFiles.at(i);
		lines << QString("file_%1.path=%2")
					 .arg(static_cast<int>(i + 1))
					 .arg(parsed.filePath);
		lines << QString("file_%1.external_known_state=%2")
					 .arg(static_cast<int>(i + 1))
					 .arg(parsed.externalKnownState.isEmpty()
							  ? "not_provided"
							  : parsed.externalKnownState);
		lines << QString("file_%1.unique_global_payloads_found=%2")
					 .arg(static_cast<int>(i + 1))
					 .arg(static_cast<int>(parsed.payloads.size()));
		lines << QString("file_%1.payload_stable_within_file=%2")
					 .arg(static_cast<int>(i + 1))
					 .arg(parsed.stableWithinFile ? "true" : "false");
		if (parsed.hasPayload) {
			++filesWithPayload;
			maxPayloadSize =
				std::max(maxPayloadSize, parsed.selectedPayload.size());
			lines << QString("file_%1.selected_global_payload=%2")
						 .arg(static_cast<int>(i + 1))
						 .arg(vectorHex(parsed.selectedPayload));
		} else {
			lines << QString("file_%1.selected_global_payload=missing")
						 .arg(static_cast<int>(i + 1));
		}
		if (!parsed.note.isEmpty())
			lines << QString("file_%1.note=%2")
						 .arg(static_cast<int>(i + 1))
						 .arg(parsed.note);
	}
	lines << "";

	lines << "[Existing Parser Field Map (RetSetAudioGlobalParm)]";
	lines << "offset_0=command_version";
	lines << "offset_1_2=number_of_audio_ports (7-bit joined)";
	lines << "offset_3=min_audio_frames_buffered";
	lines << "offset_4=max_audio_frames_buffered";
	lines << "offset_5=current_audio_frames_buffered";
	lines << "offset_6=min_allowed_sync_factor";
	lines << "offset_7=max_allowed_sync_factor";
	lines << "offset_8=current_sync_factor";
	lines << "offset_9=number_of_active_audio_configuration";
	lines << "offset_10=number_of_config_blocks";
	lines << "offset_11_plus_triplets=config block triplets "
				"(configuration_number, bit_depth_code, sample_rate_code)";
	lines << "note=Field layout is existing parser behavior; AUDIO4c semantics "
				"remain hypotheses unless cross-state variation confirms them.";
	lines << "";

	if (filesWithPayload < 2) {
		lines << "[Comparison]";
		lines << "insufficient_payload_data=true";
		lines << "need_at_least_two_files_with_global_payload=true";
		lines
			<< "sample_rate_interpretation=unknown_until_comparative_variation";
		lines
			<< "instruction=To identify sample-rate byte(s), collect dumps at "
			   "known hardware rates: 44100, 48000, 96000, then run "
			   "--compare-audio4c-global.";
		return lines.join("\n");
	}

	std::vector<int> stableOffsets;
	std::vector<int> differingOffsets;
	lines << "[Global Payload Byte Offset Table]";
	for (size_t offset = 0; offset < maxPayloadSize; ++offset) {
		QStringList perFile;
		bool haveReference = false;
		unsigned int referenceValue = 0;
		bool mismatch = false;
		bool hasMissing = false;
		for (size_t i = 0; i < parsedFiles.size(); ++i) {
			const GlobalCompareInput &parsed = parsedFiles.at(i);
			QString byteValue = "--";
			if (parsed.hasPayload &&
				offset < parsed.selectedPayload.size()) {
				const unsigned int value = parsed.selectedPayload.at(offset);
				byteValue = byteHex(value);
				if (!haveReference) {
					haveReference = true;
					referenceValue = value;
				} else if (referenceValue != value) {
					mismatch = true;
				}
			} else {
				hasMissing = true;
			}
			perFile << QString("f%1=%2")
						   .arg(static_cast<int>(i + 1))
						   .arg(byteValue);
		}
		const bool isStable = haveReference && !mismatch && !hasMissing;
		if (isStable)
			stableOffsets.push_back(static_cast<int>(offset));
		else
			differingOffsets.push_back(static_cast<int>(offset));
		lines << QString("offset_%1: %2 status=%3")
					 .arg(static_cast<int>(offset))
					 .arg(perFile.join(' '))
					 .arg(isStable ? "stable" : "diff_or_missing");
	}
	lines << QString("stable_offset_count=%1")
				 .arg(static_cast<int>(stableOffsets.size()));
	lines << QString("differing_offset_count=%1")
				 .arg(static_cast<int>(differingOffsets.size()));
	if (stableOffsets.empty()) {
		lines << "stable_offsets=none";
	} else {
		QStringList offsetList;
		for (int offset : stableOffsets)
			offsetList << QString::number(offset);
		lines << QString("stable_offsets=%1").arg(offsetList.join(","));
	}
	if (differingOffsets.empty()) {
		lines << "differing_offsets=none";
	} else {
		QStringList offsetList;
		for (int offset : differingOffsets)
			offsetList << QString::number(offset);
		lines << QString("differing_offsets=%1").arg(offsetList.join(","));
	}
	lines << "";

	lines << "[Per-File Global Decode]";
	for (size_t i = 0; i < parsedFiles.size(); ++i) {
		const GlobalCompareInput &parsed = parsedFiles.at(i);
		if (!parsed.hasPayload) {
			lines << QString("file_%1.decode_status=missing_payload")
						 .arg(static_cast<int>(i + 1));
			continue;
		}
		if (!parsed.decodedSelected.valid) {
			lines << QString("file_%1.decode_status=invalid (%2)")
						 .arg(static_cast<int>(i + 1))
						 .arg(parsed.decodedSelected.validationNote);
			continue;
		}
		const DecodedAudioGlobalPayload &decoded = parsed.decodedSelected;
		lines << QString("file_%1.decode_status=known")
					 .arg(static_cast<int>(i + 1));
		lines << QString("file_%1.active_audio_configuration=%2")
					 .arg(static_cast<int>(i + 1))
					 .arg(decoded.numberOfActiveAudioConfiguration);
		lines << QString("file_%1.active_config_sample_rate_offset_candidate=%2")
					 .arg(static_cast<int>(i + 1))
					 .arg(decoded.activeConfigSampleRateByteOffset);
		lines << QString("file_%1.active_config_sample_rate_code_candidate=%2")
					 .arg(static_cast<int>(i + 1))
					 .arg(decoded.activeConfigSampleRateCode);
		lines << QString("file_%1.active_config_sample_rate_label_hypothesis=%2")
					 .arg(static_cast<int>(i + 1))
					 .arg(globalSampleRateCodeLabel(static_cast<unsigned int>(
						 decoded.activeConfigSampleRateCode)));
	}
	lines << "";

	lines << "[External Known State Correlation]";
	std::set<QString> knownRates;
	for (size_t i = 0; i < parsedFiles.size(); ++i) {
		const QString rate =
			externalStateRateOnly(parsedFiles.at(i).externalKnownState);
		if (!rate.isEmpty())
			knownRates.insert(rate);
		lines << QString("file_%1.external_state_rate=%2")
					 .arg(static_cast<int>(i + 1))
					 .arg(rate.isEmpty() ? "unknown" : rate);
	}
	lines << QString("distinct_external_state_rates=%1")
				 .arg(static_cast<int>(knownRates.size()));

	lines << "[Sample-Rate Byte Hypotheses]";
	bool anyHypothesis = false;
	for (int offset : differingOffsets) {
		std::map<unsigned int, std::set<QString>> valueToRates;
		std::set<QString> representedRates;
		for (const GlobalCompareInput &parsed : parsedFiles) {
			const QString rate = externalStateRateOnly(parsed.externalKnownState);
			if (rate.isEmpty() || !parsed.hasPayload ||
				offset >= static_cast<int>(parsed.selectedPayload.size()))
				continue;
			const unsigned int value =
				parsed.selectedPayload.at(static_cast<size_t>(offset));
			valueToRates[value].insert(rate);
			representedRates.insert(rate);
		}
		if (representedRates.size() < 2)
			continue;
		anyHypothesis = true;
		lines << QString("offset_%1.hypothesis_status=candidate").arg(offset);
		QStringList mappingParts;
		bool oneToOne = true;
		for (const auto &entry : valueToRates) {
			const unsigned int value = entry.first;
			const std::set<QString> &rates = entry.second;
			QStringList rateList;
			for (const QString &rate : rates)
				rateList << rate;
			mappingParts << QString("%1->%2")
								.arg(byteHex(value), rateList.join("/"));
			if (rates.size() != 1)
				oneToOne = false;
		}
		lines << QString("offset_%1.value_rate_mapping_hypothesis=%2")
					 .arg(offset)
					 .arg(mappingParts.join(", "));
		lines << QString("offset_%1.mapping_confidence=%2")
					 .arg(offset)
					 .arg(oneToOne
							  ? "medium-hypothesis (variation aligns to labels)"
							  : "low-hypothesis (non-unique mapping)");
	}
	if (!anyHypothesis)
		lines << "no_sample_rate_byte_hypothesis_from_current_inputs";
	if (knownRates.size() < 2) {
		lines << "correlation_status=insufficient_known_rate_labels";
	} else if (differingOffsets.empty()) {
		lines << "correlation_status=known_rates_differ_but_global_payload_"
				   "bytes_do_not";
	} else {
		lines << "correlation_status=global_payload_variation_present";
	}
	lines << "sample_rate_interpretation=unknown_until_comparative_variation";
	lines
		<< "instruction=To identify sample-rate byte(s), collect dumps at known "
		   "hardware rates: 44100, 48000, 96000, then run "
		   "--compare-audio4c-global.";

	return lines.join("\n");
}

void MioMain::runCompareAudio4cClockFromStartup() {
	int exitCode = 0;
	if (m_compareAudio4cClockFiles.size() < 2) {
		std::cerr << "compare-audio4c-clock requires at least two files.\n"
				  << "Usage: ./iconnconfig --compare-audio4c-clock FILE1 FILE2 "
					 "[FILE3...]\n";
		exitCode = 2;
	} else {
		std::cout << compareAudio4cClockDiagnostics(m_compareAudio4cClockFiles)
						 .toStdString()
				  << std::endl;
	}
	QCoreApplication::exit(exitCode);
}

void MioMain::runCompareAudio4cGlobalFromStartup() {
	int exitCode = 0;
	if (m_compareAudio4cClockFiles.size() < 2) {
		std::cerr << "compare-audio4c-global requires at least two files.\n"
				  << "Usage: ./iconnconfig --compare-audio4c-global FILE1 FILE2 "
					 "[FILE3...]\n";
		exitCode = 2;
	} else {
		std::cout << compareAudio4cGlobalDiagnostics(m_compareAudio4cClockFiles)
						 .toStdString()
				  << std::endl;
	}
	QCoreApplication::exit(exitCode);
}

void MioMain::on_actionDumpDeviceDiagnostics_triggered() {
	if (!m_pCurrentDevice) {
		QMessageBox::warning(this, tr("No Device Selected"),
							 tr("No active device is available."));
		return;
	}
	const bool previousLogState = SysExMessage::isStructuredLoggingEnabled();
	SysExMessage::setStructuredLoggingEnabled(true);
	const QString dumpContent = dumpDeviceDiagnostics();
	SysExMessage::setStructuredLoggingEnabled(previousLogState);
	const QString filePrefix =
		m_pCurrentDevice->isExperimentalAudio4c() ? "audio4c" : "device";
	const QString filename = dumpDiagnosticsToFile(dumpContent, filePrefix);
	if (filename.isEmpty()) {
		QMessageBox::critical(
			this, tr("Diagnostics Failed"),
			tr("Could not write diagnostics output file."));
		return;
	}
	statusBar()->showMessage(
		tr("Diagnostics written to %1").arg(filename), 10000);
	QMessageBox::information(
		this, tr("Diagnostics Complete"),
		tr("Device diagnostics written to:\n%1").arg(filename));
}

void MioMain::closeEvent(QCloseEvent *event) {
	writeSettings();
	event->accept();
}

void MioMain::changeEvent(QEvent *event) {
	if (nullptr != event) {
		switch (event->type()) {
		// this event is send if a translator is loaded
		case QEvent::LanguageChange:
			m_pUi->retranslateUi(this);
			break;
		// this event is send, if the system, language changes
		case QEvent::LocaleChange: {
			QString locale = QLocale::system().name();
			locale.truncate(locale.lastIndexOf('_'));
			loadLanguage(locale);
		} break;
		default:
			break;
		}
	}

	QMainWindow::changeEvent(event);
}

void MioMain::slotLanguageChanged(QAction *action) {
	if (nullptr != action) {
		// load the language dependant on the action content
		loadLanguage(action->data().toString());
		setWindowIcon(action->icon());
	}
}

void MioMain::createLanguageMenu() {
	QActionGroup *langGroup = new QActionGroup(m_pUi->menuLanguage);
	langGroup->setExclusive(true);

	connect(langGroup, SIGNAL(triggered(QAction *)), this,
			SLOT(slotLanguageChanged(QAction *)));

	// format systems language
	QString defaultLocale = QLocale::system().name(); // e.g. "de_DE"
	// defaultLocale.truncate(defaultLocale.lastIndexOf("_")); // e.g. "de"

	QDir dir(":/translations/tr");
	QStringList fileNames = dir.entryList(QStringList("iconnconfig_*.qm"));

	for (int i = 0; i < fileNames.size(); ++i) {
		// get locale extracted by filename
		QString locale;
		locale = fileNames[i];                     // "TranslationExample_de.qm"
		locale.truncate(locale.lastIndexOf('.'));  // "TranslationExample_de"
		locale.remove(0, locale.indexOf("_") + 1); // "de"

		QString lang = QLocale::languageToString(QLocale(locale).language());

		QAction *action = new QAction(lang, this);
		action->setCheckable(true);
		action->setData(locale);
		action->setEnabled(false);

		m_pUi->menuLanguage->addAction(action);
		langGroup->addAction(action);

		// set default translators and language checked
		if (defaultLocale == locale) {
			action->setChecked(true);
		}
	}
	m_pUi->menuLanguage->setVisible(false);
}

void MioMain::loadLanguage(const QString &rLanguage) {
	if (m_currLang != rLanguage) {
		m_currLang = rLanguage;
		QLocale locale = QLocale(m_currLang);
		QLocale::setDefault(locale);
		QString languageName = QLocale::languageToString(locale.language());
		QString filePath =
			QString(":/translations/tr/iconnconfig_%1.qm").arg(rLanguage);
		QString langPath = QString("qt_%1.qm").arg(rLanguage);
		switchTranslator(
			m_translator,
			QString(":/translations/tr/iconnconfig_%1.qm").arg(rLanguage));
		switchTranslator(m_translatorQt, QString("qt_%1.qm").arg(rLanguage));
		this->m_pUi->statusBar->showMessage(
			tr("Current Language changed to %1").arg(languageName));
	}
}

void MioMain::openDetectionWindow() {
	m_pDeviceDetectionWindow = new DeviceDetection(this);
	m_pDeviceDetectionWindow->exec();
}

void MioMain::writeSettings() {
	QSettings *settings = Configuration::getInstance().getSettings();
	settings->beginGroup("MainWindow");
	settings->setValue("geometry", saveGeometry());
	settings->setValue("size", size());
	settings->setValue("pos", pos());
	settings->endGroup();
	settings->beginGroup("Docks");
	settings->setValue("DockWindows", saveState());
	settings->endGroup();
	delete settings;
}

void MioMain::writeDevicesToSettings() {
	QSettings *settings = Configuration::getInstance().getSettings();
	Devices *devices = Configuration::getInstance().getDevices();
	settings->remove("Devices");
	settings->beginWriteArray("Devices");
	int i = 0;
	for (Devices::iterator it = devices->begin(); it != devices->end(); ++it) {
		settings->setArrayIndex(i);
		std::shared_ptr<Device> d = it->second;
		settings->setValue("Device Name",
						   QString::fromStdString(d->getDeviceName()));
		settings->setValue(
			"Serial Number",
			static_cast<qlonglong>(d->getSerialNumber()->getLongValue()));
		settings->setValue("Input Port", d->getInPortNumer());
		settings->setValue("Output Port", d->getOutPortNumer());
		settings->setValue(
			"Product Id",
			static_cast<qlonglong>(d->getProductId()->getLongValue()));
		++i;
	}
	settings->endArray();
	delete settings;
}

void MioMain::connectSlots() {
	// connect(this->)
}

void MioMain::readSettings() {
	this->m_sTitle = windowTitle();
	QSettings *settings = Configuration::getInstance().getSettings();
	settings->beginGroup("MainWindow");
	resize(settings->value("size", QSize(400, 400)).toSize());
	move(settings->value("pos", QPoint(200, 200)).toPoint());
	settings->endGroup();
	delete settings;
}

bool MioMain::readDevicesFromSettings() {
	Devices *devices = Configuration::getInstance().getDevices();
	devices->clear();
	QSettings *settings = Configuration::getInstance().getSettings();
	int size = settings->beginReadArray("Devices");
	if (size == 0)
		return false;
	for (int i = 0; i < size; ++i) {
		std::shared_ptr<Device> device = nullptr;
		settings->setArrayIndex(i);
		unsigned int productId =
			static_cast<unsigned int>(settings->value("Product Id").toInt());
		unsigned long serialNumber = static_cast<unsigned long>(
			settings->value("Serial Number").toLongLong());
		unsigned int inputPort =
			static_cast<unsigned int>(settings->value("Input Port").toInt());
		unsigned int outputPort =
			static_cast<unsigned int>(settings->value("Output Port").toInt());
		try {
			device = std::make_shared<Device>(inputPort, outputPort,
											  serialNumber, productId);
			std::unique_ptr<RtMidiIn> midiIn(MIDI::createMidiIn());
			std::unique_ptr<RtMidiOut> midiOut(MIDI::createMidiOut());
			std::string inputPortName;
			std::string outputPortName;
			if (midiIn && inputPort < midiIn->getPortCount())
				inputPortName = midiIn->getPortName(inputPort);
			if (midiOut && outputPort < midiOut->getPortCount())
				outputPortName = midiOut->getPortName(outputPort);
			device->setDetectedMidiPortNames(inputPortName, outputPortName);
		} catch (CommunicationException *e) {
			std::cerr << e->getErrorMessage();
		}
		if (device)
			try {
				device->queryDeviceInfo();
				devices->insert(std::pair<long, std::shared_ptr<Device>>(
					serialNumber, device));
			} catch (CommunicationException *e) {
				std::cerr << e->getErrorMessage();
				std::cerr << "Device: Product Id: " << productId
						  << ", Serial Number: " << serialNumber
						  << " did not answer" << std::endl;
				std::cerr << "Device not added" << std::endl;
			} catch (ProtocolException *e) {
				std::cerr << e->getErrorMessage();
				std::cerr << "Device: Product Id: " << productId
						  << ", Serial Number: " << serialNumber
						  << " had an error in the communication stream"
						  << std::endl;
				std::cerr << "Device not added" << std::endl;
			}
	}
	settings->endArray();
	delete settings;
	if (devices->size() == 0)
		return false;
	return true;
}
void MioMain::on_actionQuit_triggered() { close(); }

// kind of singleton reference.
MioMain *MioMain::getMainWin() { return pMainWindow; }

CentralWidget *MioMain::getCentralDeviceWidget() const {
	return m_pCentralDeviceWidget;
}

#ifdef __LINUX_SESSION_MANAGEMENT__
/******************************************************************************
 *************** Methods for handling ladish events ***************************
 *****************************************************************************/

/* Handler for system signals (SIGUSR1, SIGINT...)
 * Write a message to the pipe and leave as soon as possible
 */
void MioMain::handleSignal(int sig) {
	if (write(sigpipe[1], &sig, sizeof(sig)) == -1)
		qWarning("write() failed: %s", std::strerror(errno));
}

/* Install signal handlers (may be more than one; called from the
 * constructor of your MainWindow class*/
bool MioMain::installSignalHandlers() {
	/*install pipe to forward received system signals*/
	if (pipe(sigpipe) < 0) {
		qWarning("pipe() failed: %s", std::strerror(errno));
		return false;
	}

	/*install notifier to handle pipe messages*/
	QSocketNotifier *signalNotifier =
		new QSocketNotifier(sigpipe[0], QSocketNotifier::Read, this);
	connect(signalNotifier, SIGNAL(activated(int)), this,
			SLOT(signalAction(int)));

	/*install signal handlers*/
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_handler = handleSignal;

	if (sigaction(SIGUSR1, &action, nullptr) == -1) {
		qWarning("sigaction() failed: %s", std::strerror(errno));
		return false;
	}

	/* optional: register more signals to handle: */

	return true;
}

/* Slot to give response to the incoming pipe message;
e.g.: save current file */
void MioMain::signalAction(int fd) {
	int message;

	if (read(fd, &message, sizeof(message)) == -1) {
		qWarning("read() failed: %s", std::strerror(errno));
		return;
	}

	switch (message) {
	case SIGUSR1:
		writeConfigurtionToFile();
		break;
	/* optional: handle more signals: */
	default:
		qWarning("Unexpected signal received: %d", message);
		break;
	}
}
#endif

void MioMain::openAboutDialog() {
	AboutDialog *aboutDialog = new AboutDialog(this);
	aboutDialog->exec();
}

void MioMain::connectSignals() {
	connect(this->m_pUi->actionAbout, SIGNAL(triggered()), this,
			SLOT(openAboutDialog()));
	connect(this->m_pUi->actionRedetectDevices, SIGNAL(triggered()), this,
			SLOT(openDetectionWindow()));
	connect(m_pActionDumpDiagnostics, SIGNAL(triggered()), this,
			SLOT(on_actionDumpDeviceDiagnostics_triggered()));
}

void switchTranslator(QTranslator &translator, const QString &filename) {
	// remove the old translator
	qApp->removeTranslator(&translator);

	// load the new translator
	if (translator.load(filename))
		qApp->installTranslator(&translator);
}
