#include <errno.h>
#include <iconv.h>
#include <strings.h>

#include <asterisk.h>
#include <asterisk/logger.h>
#include <asterisk/utils.h>

#include "sccp_msg.h"
#include "sccp_utils.h"

static const uint8_t softkey_default_onhook[] = {
	SOFTKEY_REDIAL,
	SOFTKEY_NEWCALL,
	SOFTKEY_CFWDALL,
	SOFTKEY_DND,
};

static const uint8_t softkey_default_connected[] = {
	SOFTKEY_HOLD,
	SOFTKEY_ENDCALL,
	SOFTKEY_TRNSFER,
};

static const uint8_t softkey_default_onhold[] = {
	SOFTKEY_RESUME,
	SOFTKEY_NEWCALL,
};

static const uint8_t softkey_default_ringin[] = {
	SOFTKEY_ANSWER,
	SOFTKEY_ENDCALL,
};

static const uint8_t softkey_default_ringout[] = {
	SOFTKEY_NONE,
	SOFTKEY_ENDCALL,
};

static const uint8_t softkey_default_offhook[] = {
	SOFTKEY_REDIAL,
	SOFTKEY_ENDCALL,
};

static const uint8_t softkey_default_dialintransfer[] = {
	SOFTKEY_REDIAL,
	SOFTKEY_ENDCALL,
};

static const uint8_t softkey_default_connintransfer[] = {
	SOFTKEY_NONE,
	SOFTKEY_ENDCALL,
	SOFTKEY_TRNSFER,
};

static const uint8_t softkey_default_callfwd[] = {
	SOFTKEY_BKSPC,
	SOFTKEY_CFWDALL,
};

struct softkey_definitions {
	const uint8_t mode;
	const uint8_t *defaults;
	const int count;
};

static const struct softkey_definitions softkey_default_definitions[] = {
	{KEYDEF_ONHOOK, softkey_default_onhook, ARRAY_LEN(softkey_default_onhook)},
	{KEYDEF_CONNECTED, softkey_default_connected, ARRAY_LEN(softkey_default_connected)},
	{KEYDEF_ONHOLD, softkey_default_onhold, ARRAY_LEN(softkey_default_onhold)},
	{KEYDEF_RINGIN, softkey_default_ringin, ARRAY_LEN(softkey_default_ringin)},
	{KEYDEF_RINGOUT, softkey_default_ringout, ARRAY_LEN(softkey_default_ringout)},
	{KEYDEF_OFFHOOK, softkey_default_offhook, ARRAY_LEN(softkey_default_offhook)},
	{KEYDEF_CONNINTRANSFER, softkey_default_connintransfer, ARRAY_LEN(softkey_default_connintransfer)},
	{KEYDEF_DIALINTRANSFER, softkey_default_dialintransfer, ARRAY_LEN(softkey_default_dialintransfer)},
	{KEYDEF_CALLFWD, softkey_default_callfwd, ARRAY_LEN(softkey_default_callfwd)},
};

static struct softkey_template_definition softkey_template_default[] = {
	{"\x80\x01", SOFTKEY_REDIAL},
	{"\x80\x02", SOFTKEY_NEWCALL},
	{"\x80\x03", SOFTKEY_HOLD},
	{"\x80\x04", SOFTKEY_TRNSFER},
	{"\x80\x05", SOFTKEY_CFWDALL},
	{"\x80\x06", SOFTKEY_CFWDBUSY},
	{"\x80\x07", SOFTKEY_CFWDNOANSWER},
	{"\x80\x08", SOFTKEY_BKSPC},
	{"\x80\x09", SOFTKEY_ENDCALL},
	{"\x80\x0A", SOFTKEY_RESUME},
	{"\x80\x0B", SOFTKEY_ANSWER},
	{"\x80\x0C", SOFTKEY_INFO},
	{"\x80\x0D", SOFTKEY_CONFRN},
	{"\x80\x0E", SOFTKEY_PARK},
	{"\x80\x0F", SOFTKEY_JOIN},
	{"\x80\x10", SOFTKEY_MEETME},
	{"\x80\x11", SOFTKEY_PICKUP},
	{"\x80\x12", SOFTKEY_GPICKUP},
	{"Dial", 0x13}, // Dial
	{"\200\77", SOFTKEY_DND},
};

/*
 * rule on usage of htolel and similar functions:
 *
 * - no need to do it for 0 values
 * - no need to do it when lvalue is uint8_t
 * - no need to do it when lvalue is a struct composed of only uint8_t member
 */

static void prepare_msg(struct sccp_msg *msg, uint32_t data_length, uint32_t msg_id)
{
	msg->length = htolel(SCCP_MSG_LEN_FROM_DATA_LEN(data_length));
	msg->reserved = 0;
	msg->id = htolel(msg_id);
	memset(&msg->data, 0, data_length);
}

void sccp_msg_button_template_res(struct sccp_msg *msg, struct button_definition *definition, size_t n)
{
	size_t i;

	prepare_msg(msg, sizeof(struct button_template_res_message), BUTTON_TEMPLATE_RES_MESSAGE);

	for (i = 0; i < n; i++) {
		msg->data.buttontemplate.definition[i] = definition[i];
	}

	for (; i < MAX_BUTTON_DEFINITION; i++) {
		msg->data.buttontemplate.definition[i].buttonDefinition = BT_NONE;
		msg->data.buttontemplate.definition[i].lineInstance = 0;
	}

	msg->data.buttontemplate.buttonOffset = 0;
	msg->data.buttontemplate.buttonCount = htolel(n);
	msg->data.buttontemplate.totalButtonCount = htolel(n);
}

void sccp_msg_callinfo(struct sccp_msg *msg, const char *from_name, const char *from_num, const char *to_name, const char *to_num, uint32_t line_instance, uint32_t callid, enum sccp_direction direction)
{
	prepare_msg(msg, sizeof(struct call_info_message), CALL_INFO_MESSAGE);

	/* XXX the check should be done in caller function, not here, so that every sccp_msg_* function does
	 *     the same thing, i.e. no check
	 */
	if (from_name) {
		ast_copy_string(msg->data.callinfo.callingPartyName, from_name, sizeof(msg->data.callinfo.callingPartyName));
	}

	if (from_num) {
		ast_copy_string(msg->data.callinfo.callingParty, from_num, sizeof(msg->data.callinfo.callingParty));
	}

	if (to_name) {
		ast_copy_string(msg->data.callinfo.calledPartyName, to_name, sizeof(msg->data.callinfo.calledPartyName));
		ast_copy_string(msg->data.callinfo.originalCalledPartyName, to_name, sizeof(msg->data.callinfo.originalCalledPartyName));
	}

	if (to_num) {
		ast_copy_string(msg->data.callinfo.calledParty, to_num, sizeof(msg->data.callinfo.calledParty));
		ast_copy_string(msg->data.callinfo.originalCalledParty, to_num, sizeof(msg->data.callinfo.originalCalledParty));
	}

	msg->data.callinfo.lineInstance = htolel(line_instance);
	msg->data.callinfo.callInstance = htolel(callid);
	msg->data.callinfo.type = htolel(direction);
}

void sccp_msg_callstate(struct sccp_msg *msg, enum sccp_state state, uint32_t line_instance, uint32_t callid)
{
	prepare_msg(msg, sizeof(struct call_state_message), CALL_STATE_MESSAGE);

	msg->data.callstate.callState = htolel(state);
	msg->data.callstate.lineInstance = htolel(line_instance);
	msg->data.callstate.callReference = htolel(callid);
	msg->data.callstate.visibility = 0;
	msg->data.callstate.priority = htolel(4);
}

void sccp_msg_capabilities_req(struct sccp_msg *msg)
{
	prepare_msg(msg, 0, CAPABILITIES_REQ_MESSAGE);
}

void sccp_msg_config_status_res(struct sccp_msg *msg, const char *name, uint32_t line_count, uint32_t speeddial_count)
{
	prepare_msg(msg, sizeof(struct config_status_res_message), CONFIG_STATUS_RES_MESSAGE);

	ast_copy_string(msg->data.configstatus.deviceName, name, sizeof(msg->data.configstatus.deviceName));
	msg->data.configstatus.stationUserId = 0;
	msg->data.configstatus.stationInstance = htolel(1);
	msg->data.configstatus.numberLines = htolel(line_count);
	msg->data.configstatus.numberSpeedDials = htolel(speeddial_count);
}

void sccp_msg_clear_message(struct sccp_msg *msg)
{
	prepare_msg(msg, 0, CLEAR_NOTIFY_MESSAGE);
}

void sccp_msg_close_receive_channel(struct sccp_msg *msg, uint32_t callid)
{
	prepare_msg(msg, sizeof(struct close_receive_channel_message), CLOSE_RECEIVE_CHANNEL_MESSAGE);

	msg->data.closereceivechannel.conferenceId = htolel(callid);
	msg->data.closereceivechannel.partyId = htolel(callid ^ 0xFFFFFFFF);
	msg->data.closereceivechannel.conferenceId1 = htolel(callid);
}

void sccp_msg_dialed_number(struct sccp_msg *msg, const char *extension, uint32_t line_instance, uint32_t callid)
{
	prepare_msg(msg, sizeof(struct dialed_number_message), DIALED_NUMBER_MESSAGE);

	ast_copy_string(msg->data.dialednumber.calledParty, extension, sizeof(msg->data.dialednumber.calledParty));
	msg->data.dialednumber.lineInstance = htolel(line_instance);
	msg->data.dialednumber.callInstance = htolel(callid);
}

void sccp_msg_display_message(struct sccp_msg *msg, const char *text)
{
	prepare_msg(msg, sizeof(struct display_notify_message), DISPLAY_NOTIFY_MESSAGE);

	msg->data.notify.displayTimeout = 0;
	ast_copy_string(msg->data.notify.displayMessage, text, sizeof(msg->data.notify.displayMessage));
}

void sccp_msg_feature_status(struct sccp_msg *msg, uint32_t instance, enum sccp_button_type type, enum sccp_blf_status status, const char *label)
{
	prepare_msg(msg, sizeof(struct feature_stat_message), FEATURE_STAT_MESSAGE);

	msg->data.featurestatus.bt_instance = htolel(instance);
	msg->data.featurestatus.type = htolel(type);
	msg->data.featurestatus.status = htolel(status);
	ast_copy_string(msg->data.featurestatus.label, label, sizeof(msg->data.featurestatus.label));
}

void sccp_msg_forward_status_res(struct sccp_msg *msg, uint32_t line_instance, const char *extension, uint32_t status)
{
	prepare_msg(msg, sizeof(struct forward_status_res_message), FORWARD_STATUS_RES_MESSAGE);

	msg->data.forwardstatus.status = htolel(status);
	msg->data.forwardstatus.lineInstance = htolel(line_instance);
	msg->data.forwardstatus.cfwdAllStatus = htolel(status);
	ast_copy_string(msg->data.forwardstatus.cfwdAllNumber, extension, sizeof(msg->data.forwardstatus.cfwdAllNumber));
	msg->data.forwardstatus.cfwdBusyStatus = htolel(0);
	msg->data.forwardstatus.cfwdBusyNumber[0] = '\0';
	msg->data.forwardstatus.cfwdNoAnswerStatus = htolel(0);
	msg->data.forwardstatus.cfwdNoAnswerNumber[0] = '\0';
}

void sccp_msg_keep_alive_ack(struct sccp_msg *msg)
{
	prepare_msg(msg, 0, KEEP_ALIVE_ACK_MESSAGE);
}

void sccp_msg_lamp_state(struct sccp_msg *msg, enum sccp_stimulus_type stimulus, uint32_t instance, enum sccp_lamp_state indication)
{
	prepare_msg(msg, sizeof(struct set_lamp_message), SET_LAMP_MESSAGE);

	msg->data.setlamp.stimulus = htolel(stimulus);
	msg->data.setlamp.lineInstance = htolel(instance);
	msg->data.setlamp.state = htolel(indication);
}

void sccp_msg_line_status_res(struct sccp_msg *msg, const char *cid_name, const char *cid_num, uint32_t line_instance)
{
	prepare_msg(msg, sizeof(struct line_status_res_message), LINE_STATUS_RES_MESSAGE);

	msg->data.linestatus.lineNumber = htolel(line_instance);
	ast_copy_string(msg->data.linestatus.lineDirNumber, cid_num, sizeof(msg->data.linestatus.lineDirNumber));
	ast_copy_string(msg->data.linestatus.lineDisplayName, cid_name, sizeof(msg->data.linestatus.lineDisplayName));
	ast_copy_string(msg->data.linestatus.lineDisplayAlias, cid_num, sizeof(msg->data.linestatus.lineDisplayAlias));
}

void sccp_msg_open_receive_channel(struct sccp_msg *msg, uint32_t callid, uint32_t packets, uint32_t capability)
{
	prepare_msg(msg, sizeof(struct open_receive_channel_message), OPEN_RECEIVE_CHANNEL_MESSAGE);

	msg->data.openreceivechannel.conferenceId = htolel(callid);
	msg->data.openreceivechannel.partyId = htolel(callid ^ 0xFFFFFFFF);
	msg->data.openreceivechannel.packets = packets;
	msg->data.openreceivechannel.capability = capability;
	msg->data.openreceivechannel.echo = htolel(0);
	msg->data.openreceivechannel.bitrate = htolel(0);
	msg->data.openreceivechannel.conferenceId1 = htolel(callid);
	msg->data.openreceivechannel.rtpTimeout = htolel(10);
}

void sccp_msg_register_ack(struct sccp_msg *msg, const char *datefmt, uint32_t keepalive, uint8_t proto_version, uint8_t unknown1, uint8_t unknown2, uint8_t unknown3)
{
	prepare_msg(msg, sizeof(struct register_ack_message), REGISTER_ACK_MESSAGE);

	msg->data.regack.keepAlive = htolel(keepalive);
	msg->data.regack.secondaryKeepAlive = htolel(keepalive);
	ast_copy_string(msg->data.regack.dateTemplate, datefmt, sizeof(msg->data.regack.dateTemplate));
	msg->data.regack.protoVersion = proto_version;
	msg->data.regack.unknown1 = unknown1;
	msg->data.regack.unknown2 = unknown2;
	msg->data.regack.unknown3 = unknown3;
}

void sccp_msg_register_rej(struct sccp_msg *msg)
{
	prepare_msg(msg, sizeof(struct register_rej_message), REGISTER_REJ_MESSAGE);

	msg->data.regrej.errMsg[0] = '\0';
}

void sccp_msg_ringer_mode(struct sccp_msg *msg, enum sccp_ringer_mode mode)
{
	prepare_msg(msg, sizeof(struct set_ringer_message), SET_RINGER_MESSAGE);

	msg->data.setringer.ringerMode = htolel(mode);
	msg->data.setringer.unknown1 = htolel(1);
	msg->data.setringer.unknown2 = htolel(1);
}

void sccp_msg_select_softkeys(struct sccp_msg *msg, uint32_t line_instance, uint32_t callid, enum sccp_softkey_status softkey)
{
	prepare_msg(msg, sizeof(struct select_soft_keys_message), SELECT_SOFT_KEYS_MESSAGE);

	msg->data.selectsoftkey.lineInstance = htolel(line_instance);
	msg->data.selectsoftkey.callInstance = htolel(callid);
	msg->data.selectsoftkey.softKeySetIndex = htolel(softkey);
	msg->data.selectsoftkey.validKeyMask = htolel(0xFFFFFFFF);
}

void sccp_msg_softkey_set_res(struct sccp_msg *msg)
{
	const struct softkey_definitions *softkeymode;
	int keyset_count = ARRAY_LEN(softkey_default_definitions);
	int i;
	int j;

	prepare_msg(msg, sizeof(struct softkey_set_res_message), SOFTKEY_SET_RES_MESSAGE);

	msg->data.softkeysets.softKeySetOffset = htolel(0);
	msg->data.softkeysets.softKeySetCount = htolel(keyset_count);
	msg->data.softkeysets.totalSoftKeySetCount = htolel(keyset_count);

	for (i = 0; i < keyset_count; i++) {
		softkeymode = &softkey_default_definitions[i];
		for (j = 0; j < softkeymode->count; j++) {
			msg->data.softkeysets.softKeySetDefinition[softkeymode->mode].softKeyTemplateIndex[j]
				= htolel(softkeymode->defaults[j]);

			msg->data.softkeysets.softKeySetDefinition[softkeymode->mode].softKeyInfoIndex[j]
				= htolel(softkeymode->defaults[j]);
		}
	}
}
void sccp_msg_softkey_template_res(struct sccp_msg *msg)
{
	prepare_msg(msg, sizeof(struct softkey_template_res_message), SOFTKEY_TEMPLATE_RES_MESSAGE);

	msg->data.softkeytemplate.softKeyOffset = htolel(0);
	msg->data.softkeytemplate.softKeyCount = htolel(ARRAY_LEN(softkey_template_default));
	msg->data.softkeytemplate.totalSoftKeyCount = htolel(ARRAY_LEN(softkey_template_default));
	memcpy(msg->data.softkeytemplate.softKeyTemplateDefinition, softkey_template_default, sizeof(softkey_template_default));
}

void sccp_msg_speaker_mode(struct sccp_msg *msg, enum sccp_speaker_mode mode)
{
	prepare_msg(msg, sizeof(struct set_speaker_message), SET_SPEAKER_MESSAGE);

	msg->data.setspeaker.mode = htolel(mode);
}

void sccp_msg_speeddial_stat_res(struct sccp_msg *msg, uint32_t index, const char *extension, const char *label)
{
	prepare_msg(msg, sizeof(struct speeddial_stat_res_message), SPEEDDIAL_STAT_RES_MESSAGE);

	msg->data.speeddialstatus.instance = htolel(index);
	memcpy(msg->data.speeddialstatus.extension, extension, sizeof(msg->data.speeddialstatus.extension));
	memcpy(msg->data.speeddialstatus.label, label, sizeof(msg->data.speeddialstatus.label));
}

void sccp_msg_start_media_transmission(struct sccp_msg *msg, uint32_t callid, uint32_t packet_size, uint32_t payload_type, uint32_t precedence, struct sockaddr_in *endpoint)
{
	prepare_msg(msg, sizeof(struct start_media_transmission_message), START_MEDIA_TRANSMISSION_MESSAGE);

	msg->data.startmedia.conferenceId = htolel(callid);
	msg->data.startmedia.passThruPartyId = htolel(callid ^ 0xFFFFFFFF);
	msg->data.startmedia.remoteIp = htolel(endpoint->sin_addr.s_addr);
	msg->data.startmedia.remotePort = htolel(ntohs(endpoint->sin_port));
	msg->data.startmedia.packetSize = htolel(packet_size);
	msg->data.startmedia.payloadType = htolel(payload_type);
	msg->data.startmedia.qualifier.precedence = htolel(precedence);
	msg->data.startmedia.qualifier.vad = htolel(0);
	msg->data.startmedia.qualifier.packets = htolel(0);
	msg->data.startmedia.qualifier.bitRate = htolel(0);
	msg->data.startmedia.conferenceId1 = htolel(callid);
	msg->data.startmedia.rtpTimeout = htolel(10);
}

void sccp_msg_stop_media_transmission(struct sccp_msg *msg, uint32_t callid)
{
	prepare_msg(msg, sizeof(struct stop_media_transmission_message), STOP_MEDIA_TRANSMISSION_MESSAGE);

	msg->data.stopmedia.conferenceId = htolel(callid);
	msg->data.stopmedia.partyId = htolel(callid ^ 0xFFFFFFFF);
	msg->data.stopmedia.conferenceId1 = htolel(callid);
}

void sccp_msg_stop_tone(struct sccp_msg *msg, uint32_t line_instance, uint32_t callid)
{
	prepare_msg(msg, sizeof(struct stop_tone_message), STOP_TONE_MESSAGE);

	msg->data.stop_tone.lineInstance = htolel(line_instance);
	msg->data.stop_tone.callInstance = htolel(callid);
}

void sccp_msg_time_date_res(struct sccp_msg *msg)
{
	struct timeval now;
	struct ast_tm cmtime;

	prepare_msg(msg, sizeof(struct time_date_res_message), DATE_TIME_RES_MESSAGE);

	now = ast_tvnow();
	ast_localtime(&now, &cmtime, NULL);

	msg->data.timedate.year = htolel(cmtime.tm_year + 1900);
	msg->data.timedate.month = htolel(cmtime.tm_mon + 1);
	msg->data.timedate.dayOfWeek = htolel(cmtime.tm_wday);
	msg->data.timedate.day = htolel(cmtime.tm_mday);
	msg->data.timedate.hour = htolel(cmtime.tm_hour);
	msg->data.timedate.minute = htolel(cmtime.tm_min);
	msg->data.timedate.seconds = htolel(cmtime.tm_sec);
	msg->data.timedate.milliseconds = htolel(0);
	msg->data.timedate.systemTime = htolel(now.tv_sec);
}

void sccp_msg_tone(struct sccp_msg *msg, enum sccp_tone tone, uint32_t line_instance, uint32_t callid)
{
	prepare_msg(msg, sizeof(struct start_tone_message), START_TONE_MESSAGE);

	msg->data.starttone.tone = htolel(tone);
	msg->data.starttone.lineInstance = htolel(line_instance);
	msg->data.starttone.callInstance = htolel(callid);
}

void sccp_msg_reset(struct sccp_msg *msg, enum sccp_reset_type type)
{
	prepare_msg(msg, sizeof(struct reset_message), RESET_MESSAGE);

	msg->data.reset.type = htolel(type);
}

void sccp_msg_version_res(struct sccp_msg *msg, const char *version)
{
	prepare_msg(msg, sizeof(struct version_res_message), VERSION_RES_MESSAGE);

	ast_copy_string(msg->data.version.version, version, sizeof(msg->data.version.version));
}

static int utf8_to_iso88591(char *out, const char *in, size_t n)
{
	iconv_t cd;
	char *inbuf = (char *) in;
	char *outbuf = out;
	size_t outbytesleft;
	size_t inbytesleft;
	size_t iconv_value;
	int ret;

	/* A: n > 0 */

	cd = iconv_open("ISO-8859-1//TRANSLIT", "UTF-8");
	if (cd == (iconv_t) -1) {
		ast_log(LOG_ERROR, "utf8_to_iso88591 failed: iconv_open: %s\n", strerror(errno));
		return -1;
	}

	inbytesleft = strlen(in);
	outbytesleft = n - 1;

	iconv_value = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
	if (iconv_value == (size_t) -1) {
		ast_log(LOG_ERROR, "utf8_to_iso88591 failed: iconv: %s\n", strerror(errno));
		ret = -1;
	} else {
		*outbuf = '\0';
		ret = 0;
	}

	iconv_close(cd);

	return ret;
}

void sccp_msg_builder_init(struct sccp_msg_builder *msg_builder, uint32_t type, uint8_t proto_version)
{
	msg_builder->type = type;
	msg_builder->proto = proto_version;
}

void sccp_msg_builder_callinfo(struct sccp_msg_builder *builder, struct sccp_msg *msg, const char *from_name, const char *from_num, const char *to_name, const char *to_num, uint32_t line_instance, uint32_t callid, enum sccp_direction direction)
{
	char *tmp;

	if (builder->proto <= 11) {
		if (!ast_strlen_zero(from_name)) {
			tmp = ast_alloca(sizeof(msg->data.callinfo.callingPartyName));
			if (!utf8_to_iso88591(tmp, from_name, sizeof(msg->data.callinfo.callingPartyName))) {
				from_name = tmp;
			}
		}

		if (!ast_strlen_zero(from_num)) {
			tmp = ast_alloca(sizeof(msg->data.callinfo.callingParty));
			if (!utf8_to_iso88591(tmp, from_num, sizeof(msg->data.callinfo.callingParty))) {
				from_num = tmp;
			}
		}

		if (!ast_strlen_zero(to_name)) {
			tmp = ast_alloca(sizeof(msg->data.callinfo.calledPartyName));
			if (!utf8_to_iso88591(tmp, to_name, sizeof(msg->data.callinfo.calledPartyName))) {
				to_name = tmp;
			}
		}

		if (!ast_strlen_zero(to_num)) {
			tmp = ast_alloca(sizeof(msg->data.callinfo.calledParty));
			if (!utf8_to_iso88591(tmp, to_num, sizeof(msg->data.callinfo.calledParty))) {
				to_num = tmp;
			}
		}
	}

	sccp_msg_callinfo(msg, from_name, from_num, to_name, to_num, line_instance, callid, direction);
}

void sccp_msg_builder_line_status_res(struct sccp_msg_builder *builder, struct sccp_msg *msg, const char *cid_name, const char *cid_num, uint32_t line_instance)
{
	char *tmp;

	if (builder->proto <= 11) {
		tmp = ast_alloca(sizeof(msg->data.linestatus.lineDisplayName));
		if (!utf8_to_iso88591(tmp, cid_name, sizeof(msg->data.linestatus.lineDisplayName))) {
			cid_name = tmp;
		}
	}

	sccp_msg_line_status_res(msg, cid_name, cid_num, line_instance);
}

void sccp_msg_builder_register_ack(struct sccp_msg_builder *builder, struct sccp_msg *msg, const char *datefmt, uint32_t keepalive)
{
	uint8_t proto_version;
	uint8_t unknown1;
	uint8_t unknown2;
	uint8_t unknown3;

	if (builder->proto <= 3) {
		proto_version = 3;
		unknown1 = 0x00;
		unknown2 = 0x00;
		unknown3 = 0x00;
	} else if (builder->proto <= 10) {
		proto_version = builder->proto;
		unknown1 = 0x20;
		unknown2 = 0x00;
		unknown3 = 0xFE;
	} else {
		proto_version = 11;
		unknown1 = 0x20;
		unknown2 = 0xF1;
		unknown3 = 0xFF;
	}

	sccp_msg_register_ack(msg, datefmt, keepalive, proto_version, unknown1, unknown2, unknown3);
}

const char *sccp_msg_id_str(uint32_t msg_id) {
	switch (msg_id) {
	case KEEP_ALIVE_MESSAGE:
		return "keep alive";
	case REGISTER_MESSAGE:
		return "register";
	case IP_PORT_MESSAGE:
		return "ip port";
	case ENBLOC_CALL_MESSAGE:
		return "enbloc call";
	case KEYPAD_BUTTON_MESSAGE:
		return "keypad button";
	case STIMULUS_MESSAGE:
		return "stimulus";
	case OFFHOOK_MESSAGE:
		return "offhook";
	case ONHOOK_MESSAGE:
		return "onhook";
	case FORWARD_STATUS_REQ_MESSAGE:
		return "forward status req";
	case SPEEDDIAL_STAT_REQ_MESSAGE:
		return "speeddial stat req";
	case LINE_STATUS_REQ_MESSAGE:
		return "line status req";
	case CONFIG_STATUS_REQ_MESSAGE:
		return "config status req";
	case TIME_DATE_REQ_MESSAGE:
		return "time date req";
	case BUTTON_TEMPLATE_REQ_MESSAGE:
		return "button template req";
	case VERSION_REQ_MESSAGE:
		return "version req";
	case CAPABILITIES_RES_MESSAGE:
		return "capabilities res";
	case ALARM_MESSAGE:
		return "alarm";
	case OPEN_RECEIVE_CHANNEL_ACK_MESSAGE:
		return "open receive channel ack";
	case SOFTKEY_SET_REQ_MESSAGE:
		return "softkey set req";
	case SOFTKEY_EVENT_MESSAGE:
		return "softkey event";
	case UNREGISTER_MESSAGE:
		return "unregister";
	case SOFTKEY_TEMPLATE_REQ_MESSAGE:
		return "softkey template req";
	case REGISTER_AVAILABLE_LINES_MESSAGE:
		return "register available lines";
	case FEATURE_STATUS_REQ_MESSAGE:
		return "feature status req";
	case ACCESSORY_STATUS_MESSAGE:
		return "accessory status";
	case REGISTER_ACK_MESSAGE:
		return "register ack";
	case START_TONE_MESSAGE:
		return "start tone";
	case STOP_TONE_MESSAGE:
		return "stop tone";
	case SET_RINGER_MESSAGE:
		return "set ringer";
	case SET_LAMP_MESSAGE:
		return "set lamp";
	case SET_SPEAKER_MESSAGE:
		return "set speaker";
	case STOP_MEDIA_TRANSMISSION_MESSAGE:
		return "stop media transmission";
	case START_MEDIA_TRANSMISSION_MESSAGE:
		return "start media transmission";
	case CALL_INFO_MESSAGE:
		return "call info";
	case FORWARD_STATUS_RES_MESSAGE:
		return "forward status res";
	case SPEEDDIAL_STAT_RES_MESSAGE:
		return "speeddial stat res";
	case LINE_STATUS_RES_MESSAGE:
		return "line status res";
	case CONFIG_STATUS_RES_MESSAGE:
		return "config status res";
	case DATE_TIME_RES_MESSAGE:
		return "date time res";
	case BUTTON_TEMPLATE_RES_MESSAGE:
		return "button template res";
	case VERSION_RES_MESSAGE:
		return "version res";
	case CAPABILITIES_REQ_MESSAGE:
		return "capabilities req";
	case REGISTER_REJ_MESSAGE:
		return "register rej";
	case RESET_MESSAGE:
		return "reset";
	case KEEP_ALIVE_ACK_MESSAGE:
		return "keep alive ack";
	case OPEN_RECEIVE_CHANNEL_MESSAGE:
		return "open receive channel";
	case CLOSE_RECEIVE_CHANNEL_MESSAGE:
		return "close receive channel";
	case SOFTKEY_TEMPLATE_RES_MESSAGE:
		return "softkey template res";
	case SOFTKEY_SET_RES_MESSAGE:
		return "softkey set res";
	case SELECT_SOFT_KEYS_MESSAGE:
		return "select soft keys";
	case CALL_STATE_MESSAGE:
		return "call state";
	case DISPLAY_NOTIFY_MESSAGE:
		return "display notify";
	case CLEAR_NOTIFY_MESSAGE:
		return "clear notify";
	case ACTIVATE_CALL_PLANE_MESSAGE:
		return "activate call plane";
	case DIALED_NUMBER_MESSAGE:
		return "dialed number";
	case FEATURE_STAT_MESSAGE:
		return "feature stat";
	case START_MEDIA_TRANSMISSION_ACK_MESSAGE:
		return "start media transmission ack";
	}

	return "unknown";
}

const char *sccp_device_type_str(enum sccp_device_type device_type)
{
	switch (device_type) {
	case SCCP_DEVICE_7905:
		return "7905";
	case SCCP_DEVICE_7906:
		return "7906";
	case SCCP_DEVICE_7911:
		return "7911";
	case SCCP_DEVICE_7912:
		return "7912";
	case SCCP_DEVICE_7920:
		return "7920";
	case SCCP_DEVICE_7921:
		return "7921";
	case SCCP_DEVICE_7931:
		return "7931";
	case SCCP_DEVICE_7937:
		return "7937";
	case SCCP_DEVICE_7940:
		return "7940";
	case SCCP_DEVICE_7941:
		return "7941";
	case SCCP_DEVICE_7941GE:
		return "7941GE";
	case SCCP_DEVICE_7942:
		return "7942";
	case SCCP_DEVICE_7960:
		return "7960";
	case SCCP_DEVICE_7961:
		return "7961";
	case SCCP_DEVICE_7962:
		return "7962";
	case SCCP_DEVICE_7970:
		return "7970";
	case SCCP_DEVICE_CIPC:
		return "CIPC";
	}

	return "unknown";
}

const char *sccp_state_str(enum sccp_state state)
{
	switch (state) {
	case SCCP_OFFHOOK:
		return "Offhook";
	case SCCP_ONHOOK:
		return "Onhook";
	case SCCP_RINGOUT:
		return "Ringout";
	case SCCP_RINGIN:
		return "Ringin";
	case SCCP_CONNECTED:
		return "Connected";
	case SCCP_BUSY:
		return "Busy";
	case SCCP_CONGESTION:
		return "Congestion";
	case SCCP_HOLD:
		return "Hold";
	case SCCP_CALLWAIT:
		return "Callwait";
	case SCCP_TRANSFER:
		return "Transfer";
	case SCCP_PARK:
		return "Park";
	case SCCP_PROGRESS:
		return "Progress";
	case SCCP_INVALID:
		return "Invalid";
	}

	return "Unknown";
}
