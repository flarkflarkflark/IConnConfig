#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <cmath>
#include <vector>

/* Macros */

#define channelIndex(port, portclass, channel)                                 \
	((port * 10 + portclass) * 100 + channel)

/**
 * @brief The Command enum
 */
enum Command {
	CMD_ERROR = -1, /*!< Return if command is not valid */
	/* device commands */
	GET_DEVICE = 0x01,      /*!< Query for devices  */
	RET_DEVICE,             /*!< Answer from device to GET_DEVICE query*/
	GET_COMMAND_LIST,       /*!< Request devices command list. */
	RET_COMMAND_LIST,       /*!< List of commands supported by device. */
	GET_INFO_LIST,          /*!< Request list of infos, whch can be retrieved */
	RET_INFO_LIST,          /*!< List of infos supported by this device */
	GET_INFO,               /*!< Get a single information from device */
	RET_SET_INFO,           /*!< Return a single information from device */
	GET_RESET_LIST,         /*!< Request list of reset IDs */
	RET_RESET_LIST,         /*!< Return list of reset IDs */
	GET_SAVE_RESTORE_LIST,  /*!< Request the save / restore ids of */
	RET_SAVE_RESTORE_LIST,  /*!< Returns a list of the save/restore ids */
	GET_ETHERNET_PORT_INFO, /*!< Requests the information about a specific
							ethernet port */
	RET_SET_ETHERNET_PORT_INFO, /*!< Returns the information about a
								specific ethernet port or sets some
								values for a specific ethernet port */
	ACK,             /*!< Returns a sucess code (0x00) or an error code (0x01 -
				   0x03) */
	RESET,           /*!< Resets the device */
	SAVE_RESTORE,    /*!< Used to save or restore the current device
							configuration */
	GET_GIZMO_COUNT, /*!< Number of other devices connected to this device
					  */
	RET_GIZMO_COUNT, /*!< Returns the number of devices that are
					connected to this device */
	GET_GIZMO_INFO,  /*!< Queueries information about a connected
					device */
	RET_GIZMO_INFO,  /*!< Returns information about a connected
					device */
	/* MIDI Commands */
	GET_MIDI_INFO =
		0x20,           /*!< Queueries information about the midi parameters */
	RET_SET_MIDI_INFO,  /*!< Returns information about the midi parameters */
	GET_MIDI_PORT_INFO, /*!< Queuery the information about a specific MIDI
						Port */
	RET_SET_MIDI_PORT_INFO,   /*!< Returns or set the MIDI-Information
								  for a specific port */
	GET_MIDI_PORT_FILTER,     /*!< Queueries infomation about current midi port
									 filters for a specific port */
	RET_SET_MIDI_PORT_FILTER, /*!< Returns or writes infomation about
								current midi port filters for a specific
								port*/
	GET_MIDI_PORT_REMAP,      /*!< Queries remaps of MIDI ports */
	RET_SET_MIDI_PORT_REMAP,  /*!< Set the remapping of midi ports */
	GET_MIDI_PORT_ROUTE,      /*!< Queries the routing of a specific port */
	RET_SET_MIDI_PORT_ROUTE,  /*!< sets the routing destinations from a
								speific port */
	GET_MIDI_PORT_DETAIL,     /*!< get the details of a specific MIDI port */
	RET_SET_MIDI_PORT_DETAIL, /*!< set details of a specific MIDI port */
	GET_RTP_MIDI_CONNECTION_DETAIL, /*!< query details of a RTP MIDI port */
	RET_RTP_MIDI_CONNECTION_DETAIL, /*!< retrieve the details of an RTP MIDI
									port */
	GET_USB_HOST_MIDI_DETAIL,       /*!< query details of an USB host */
	RET_USB_HOST_MIDI_DETAIL,       /*!< retrieve the details of an USB host */

	GET_AUDIO_CHANNEL_NAME = 0x3c, /*!< query the audio channel name */
	RET_SET_AUDIO_CHANNEL_NAME, /*!< retrieve or set the pthe audio channel name
								 */
	GET_AUDIO_PORT_METER_VALUE, /*!< query the meter value of an audio channel
								 */
	RET_AUDIO_PORT_METER_VALUE, /*!< retrieve or set the meter value of an audio
								  channel */

	GET_AUDIO_GLOBAL_PARM =
		0x40, /*!< query global params for the audio section of an device */
	RET_SET_AUDIO_GLOBAL_PARM, /*!< retieve or set global params for the audio
								 section of an device */
	GET_AUDIO_PORT_PARM,       /*!< query the parameters for an audio port */
	RET_SET_AUDIO_PORT_PARM,   /*!< retrieve or set parameters for an audio port
								*/
	GET_AUDIO_DEVICE_PARM,     /*!< query the parameters for an audio port */
	RET_SET_AUDIO_DEVICE_PARM, /*!< retrieve or set parameters for an audio port
								*/
	GET_AUDIO_CONTROL_PARM,    /*!< query the controllers for an audio port */
	RET_SET_AUDIO_CONTROL_PARM,   /*!< retrieve or set controllers for an audio
									 port */
	GET_AUDIO_CONTROL_DETAIL,     /*!< query the parameters for an audio control
									 detail */
	RET_SET_AUDIO_CONTROL_DETAIL, /*!< retrieve or set parameters for an audio
									 control detail */
	GET_AUDIO_CONTROL_DETAIL_VALUE, /*!< query the values for an control detail
									 */
	RET_SET_AUDIO_CONTROL_DETAIL_VALUE, /*!< retrieve or set values for an
										  control detail */
	GET_AUDIO_CLOCK_PARM,     /*!< query the parameters for an audio clock
							   */
	RET_SET_AUDIO_CLOCK_PARM, /*!< retrieve or set values for an
										  control detail */
	GET_AUDIO_PATCHBAY_PARM, /*!< query the parameters for the audio patchbay */
	RET_SET_AUDIO_PATCHBAY_PARM, /*!< retrieve or set the parameters for the
									audio patchbay */
	GET_MIXER_PARM = 0x50, /*!< query the parameters for the audio mixer */
	RET_SET_MIXER_PARM,    /*!< retrieve or set the parameters for the
									   audio mixer */
	GET_MIXER_PORT_PARM, /*!< query the parameters for a port in the audio mixer
						  */
	RET_SET_MIXER_PORT_PARM, /*!< retrieve or set the parameters for an port in
								the audio mixer */
	GET_MIXER_INPUT_PARM, /*!< query the parameters for an audio mixer input */
	RET_SET_MIXER_INPUT_PARM, /*!< retrieve or set the parameters for an
									audio mixer input */
	GET_MIXER_OUTPUT_PARM, /*!< query the parameters for an audio mixer output
							*/
	RET_SET_MIXER_OUTPUT_PARM, /*!< retrieve or set the parameters for an
									audio mixer output */
	GET_MIXER_INPUT_CONTROL,   /*!< query the controls for an audio mixer input
								*/
	RET_MIXER_INPUT_CONTROL, /*!< retrieve the controls for an audio mixer input
							  */
	GET_MIXER_OUTPUT_CONTROL, /*!< query the controls for an audio mixer output
							   */
	RET_MIXER_OUTPUT_CONTROL, /*!< retrieve the controls for an audio mixer
							   * output
							   */
	GET_MIXER_INPUT_CONTROL_VALUE, /*!< query the control values for an audio
									* mixer input
									*/
	RET_SET_MIXER_INPUT_CONTROL_VALUE, /*!< retrieve the control values for an
										* audio mixer input
										*/
	GET_MIXER_OUTPUT_CONTROL_VALUE, /*!< query the control values for an audio
									 * mixer output
									 */
	RET_SET_MIXER_OUTPUT_CONTROL_VALUE, /*!< retrieve the control values for an
										 * audio mixer output
										 */
	GET_MIXER_METER_VALUE,  /*!< query the meter values for the audio
							 * mixer
							 */
	RET_MIXER_METER_VALUES, /*!< retrieve the meter values for the audio
							 * mixer
							 */
};

namespace pk {
enum PortDirection { INPUT = 1, OUTPUT, BOTH };

enum AudioPortType {
	APT_NONE = 0,
	APT_USB_DEVICE = 2,
	APT_USB_HOST,
	APT_ETHERNET,
	APT_ANALOGUE
};

enum AudioControllerType {
	CT_NONE = 0,
	CT_SELECTOR = 5,
	CT_FEATUERE = 6,
	CT_CLOCK_SOURCE = 10
};

enum PanCurve {
	NO_CURVE = 0,
	CENTER_AT_0,
	CENTER_AT_M_1_5,
	CENTER_AT_M_3,
	CENTER_AT_M_4_5,
	CENTER_AT_M_6
};

enum UsbDeviceHost { NO_HOST = 0, MAC_PC = 1, IOS = 2 };
enum ChannelDirection { CD_NONE = 0, CD_INPUT, CD_OUTPUT };
enum AudioPortClass { PHYSICAL_PORT = 0, MIXER_PORT };

} // namespace pk

typedef unsigned int AudioPortId;
typedef unsigned int AudioChannelId;
typedef unsigned int AudioPortChannelId;

typedef struct {
	AudioChannelId channel;
	int volume;
} ChannelVolume;

typedef struct {
	std::vector<ChannelVolume> in;
	std::vector<ChannelVolume> out;
} ChannelVolumes;

inline const char *commandToString(Command cmd) {
	switch (cmd) {
	case CMD_ERROR:
		return "CMD_ERROR";
	case GET_DEVICE:
		return "GET_DEVICE";
	case RET_DEVICE:
		return "RET_DEVICE";
	case GET_COMMAND_LIST:
		return "GET_COMMAND_LIST";
	case RET_COMMAND_LIST:
		return "RET_COMMAND_LIST";
	case GET_INFO_LIST:
		return "GET_INFO_LIST";
	case RET_INFO_LIST:
		return "RET_INFO_LIST";
	case GET_INFO:
		return "GET_INFO";
	case RET_SET_INFO:
		return "RET_SET_INFO";
	case GET_RESET_LIST:
		return "GET_RESET_LIST";
	case RET_RESET_LIST:
		return "RET_RESET_LIST";
	case GET_SAVE_RESTORE_LIST:
		return "GET_SAVE_RESTORE_LIST";
	case RET_SAVE_RESTORE_LIST:
		return "RET_SAVE_RESTORE_LIST";
	case GET_ETHERNET_PORT_INFO:
		return "GET_ETHERNET_PORT_INFO";
	case RET_SET_ETHERNET_PORT_INFO:
		return "RET_SET_ETHERNET_PORT_INFO";
	case ACK:
		return "ACK";
	case RESET:
		return "RESET";
	case SAVE_RESTORE:
		return "SAVE_RESTORE";
	case GET_GIZMO_COUNT:
		return "GET_GIZMO_COUNT";
	case RET_GIZMO_COUNT:
		return "RET_GIZMO_COUNT";
	case GET_GIZMO_INFO:
		return "GET_GIZMO_INFO";
	case RET_GIZMO_INFO:
		return "RET_GIZMO_INFO";
	case GET_MIDI_INFO:
		return "GET_MIDI_INFO";
	case RET_SET_MIDI_INFO:
		return "RET_SET_MIDI_INFO";
	case GET_MIDI_PORT_INFO:
		return "GET_MIDI_PORT_INFO";
	case RET_SET_MIDI_PORT_INFO:
		return "RET_SET_MIDI_PORT_INFO";
	case GET_MIDI_PORT_FILTER:
		return "GET_MIDI_PORT_FILTER";
	case RET_SET_MIDI_PORT_FILTER:
		return "RET_SET_MIDI_PORT_FILTER";
	case GET_MIDI_PORT_REMAP:
		return "GET_MIDI_PORT_REMAP";
	case RET_SET_MIDI_PORT_REMAP:
		return "RET_SET_MIDI_PORT_REMAP";
	case GET_MIDI_PORT_ROUTE:
		return "GET_MIDI_PORT_ROUTE";
	case RET_SET_MIDI_PORT_ROUTE:
		return "RET_SET_MIDI_PORT_ROUTE";
	case GET_MIDI_PORT_DETAIL:
		return "GET_MIDI_PORT_DETAIL";
	case RET_SET_MIDI_PORT_DETAIL:
		return "RET_SET_MIDI_PORT_DETAIL";
	case GET_RTP_MIDI_CONNECTION_DETAIL:
		return "GET_RTP_MIDI_CONNECTION_DETAIL";
	case RET_RTP_MIDI_CONNECTION_DETAIL:
		return "RET_RTP_MIDI_CONNECTION_DETAIL";
	case GET_USB_HOST_MIDI_DETAIL:
		return "GET_USB_HOST_MIDI_DETAIL";
	case RET_USB_HOST_MIDI_DETAIL:
		return "RET_USB_HOST_MIDI_DETAIL";
	case GET_AUDIO_CHANNEL_NAME:
		return "GET_AUDIO_CHANNEL_NAME";
	case RET_SET_AUDIO_CHANNEL_NAME:
		return "RET_SET_AUDIO_CHANNEL_NAME";
	case GET_AUDIO_PORT_METER_VALUE:
		return "GET_AUDIO_PORT_METER_VALUE";
	case RET_AUDIO_PORT_METER_VALUE:
		return "RET_AUDIO_PORT_METER_VALUE";
	case GET_AUDIO_GLOBAL_PARM:
		return "GET_AUDIO_GLOBAL_PARM";
	case RET_SET_AUDIO_GLOBAL_PARM:
		return "RET_SET_AUDIO_GLOBAL_PARM";
	case GET_AUDIO_PORT_PARM:
		return "GET_AUDIO_PORT_PARM";
	case RET_SET_AUDIO_PORT_PARM:
		return "RET_SET_AUDIO_PORT_PARM";
	case GET_AUDIO_DEVICE_PARM:
		return "GET_AUDIO_DEVICE_PARM";
	case RET_SET_AUDIO_DEVICE_PARM:
		return "RET_SET_AUDIO_DEVICE_PARM";
	case GET_AUDIO_CONTROL_PARM:
		return "GET_AUDIO_CONTROL_PARM";
	case RET_SET_AUDIO_CONTROL_PARM:
		return "RET_SET_AUDIO_CONTROL_PARM";
	case GET_AUDIO_CONTROL_DETAIL:
		return "GET_AUDIO_CONTROL_DETAIL";
	case RET_SET_AUDIO_CONTROL_DETAIL:
		return "RET_SET_AUDIO_CONTROL_DETAIL";
	case GET_AUDIO_CONTROL_DETAIL_VALUE:
		return "GET_AUDIO_CONTROL_DETAIL_VALUE";
	case RET_SET_AUDIO_CONTROL_DETAIL_VALUE:
		return "RET_SET_AUDIO_CONTROL_DETAIL_VALUE";
	case GET_AUDIO_CLOCK_PARM:
		return "GET_AUDIO_CLOCK_PARM";
	case RET_SET_AUDIO_CLOCK_PARM:
		return "RET_SET_AUDIO_CLOCK_PARM";
	case GET_AUDIO_PATCHBAY_PARM:
		return "GET_AUDIO_PATCHBAY_PARM";
	case RET_SET_AUDIO_PATCHBAY_PARM:
		return "RET_SET_AUDIO_PATCHBAY_PARM";
	case GET_MIXER_PARM:
		return "GET_MIXER_PARM";
	case RET_SET_MIXER_PARM:
		return "RET_SET_MIXER_PARM";
	case GET_MIXER_PORT_PARM:
		return "GET_MIXER_PORT_PARM";
	case RET_SET_MIXER_PORT_PARM:
		return "RET_SET_MIXER_PORT_PARM";
	case GET_MIXER_INPUT_PARM:
		return "GET_MIXER_INPUT_PARM";
	case RET_SET_MIXER_INPUT_PARM:
		return "RET_SET_MIXER_INPUT_PARM";
	case GET_MIXER_OUTPUT_PARM:
		return "GET_MIXER_OUTPUT_PARM";
	case RET_SET_MIXER_OUTPUT_PARM:
		return "RET_SET_MIXER_OUTPUT_PARM";
	case GET_MIXER_INPUT_CONTROL:
		return "GET_MIXER_INPUT_CONTROL";
	case RET_MIXER_INPUT_CONTROL:
		return "RET_MIXER_INPUT_CONTROL";
	case GET_MIXER_OUTPUT_CONTROL:
		return "GET_MIXER_OUTPUT_CONTROL";
	case RET_MIXER_OUTPUT_CONTROL:
		return "RET_MIXER_OUTPUT_CONTROL";
	case GET_MIXER_INPUT_CONTROL_VALUE:
		return "GET_MIXER_INPUT_CONTROL_VALUE";
	case RET_SET_MIXER_INPUT_CONTROL_VALUE:
		return "RET_SET_MIXER_INPUT_CONTROL_VALUE";
	case GET_MIXER_OUTPUT_CONTROL_VALUE:
		return "GET_MIXER_OUTPUT_CONTROL_VALUE";
	case RET_SET_MIXER_OUTPUT_CONTROL_VALUE:
		return "RET_SET_MIXER_OUTPUT_CONTROL_VALUE";
	case GET_MIXER_METER_VALUE:
		return "GET_MIXER_METER_VALUE";
	case RET_MIXER_METER_VALUES:
		return "RET_MIXER_METER_VALUES";
	default:
		return "UNKNOWN_COMMAND";
	}
}

inline bool isRetSetCommand(Command cmd) {
	switch (cmd) {
	case RET_SET_INFO:
	case RET_SET_ETHERNET_PORT_INFO:
	case RET_SET_MIDI_INFO:
	case RET_SET_MIDI_PORT_INFO:
	case RET_SET_MIDI_PORT_FILTER:
	case RET_SET_MIDI_PORT_REMAP:
	case RET_SET_MIDI_PORT_ROUTE:
	case RET_SET_MIDI_PORT_DETAIL:
	case RET_SET_AUDIO_CHANNEL_NAME:
	case RET_SET_AUDIO_GLOBAL_PARM:
	case RET_SET_AUDIO_PORT_PARM:
	case RET_SET_AUDIO_DEVICE_PARM:
	case RET_SET_AUDIO_CONTROL_PARM:
	case RET_SET_AUDIO_CONTROL_DETAIL:
	case RET_SET_AUDIO_CONTROL_DETAIL_VALUE:
	case RET_SET_AUDIO_CLOCK_PARM:
	case RET_SET_AUDIO_PATCHBAY_PARM:
	case RET_SET_MIXER_PARM:
	case RET_SET_MIXER_PORT_PARM:
	case RET_SET_MIXER_INPUT_PARM:
	case RET_SET_MIXER_OUTPUT_PARM:
	case RET_SET_MIXER_INPUT_CONTROL_VALUE:
	case RET_SET_MIXER_OUTPUT_CONTROL_VALUE:
		return true;
	default:
		return false;
	}
}

inline bool isMutatingCommand(Command cmd) {
	return (cmd == RESET) || (cmd == SAVE_RESTORE) || isRetSetCommand(cmd);
}

inline bool isAudioRelatedCommand(Command cmd) {
	return (cmd >= GET_AUDIO_CHANNEL_NAME && cmd <= RET_MIXER_METER_VALUES);
}

#endif // DEFINITIONS_H
