'''
Created on Jun 14, 2011

@author: lebleu1
'''
from sccp.sccpmessage import SCCPMessage
from sccp.sccpmessagetype import SCCPMessageType
from struct import unpack


class SCCPCallState(SCCPMessage):

    SCCP_CHANNELSTATE_DOWN = 0
    SCCP_CHANNELSTATE_OFFHOOK = 1
    SCCP_CHANNELSTATE_ONHOOK = 2
    SCCP_CHANNELSTATE_RINGOUT = 3
    SCCP_CHANNELSTATE_RINGING = 4
    SCCP_CHANNELSTATE_CONNECTED = 5
    SCCP_CHANNELSTATE_BUSY = 6
    SCCP_CHANNELSTATE_CONGESTION = 7
    SCCP_CHANNELSTATE_HOLD = 8
    SCCP_CHANNELSTATE_CALLWAITING = 9
    SCCP_CHANNELSTATE_CALLTRANSFER = 10
    SCCP_CHANNELSTATE_CALLPARK = 11
    SCCP_CHANNELSTATE_PROCEED = 12
    SCCP_CHANNELSTATE_CALLREMOTEMULTILINE = 13
    SCCP_CHANNELSTATE_INVALIDNUMBER = 14
    SCCP_CHANNELSTATE_DIALING = 20
    SCCP_CHANNELSTATE_PROGRESS = 21
    SCCP_CHANNELSTATE_GETDIGITS = 0xA0
    SCCP_CHANNELSTATE_CALLCONFERENCE = 0xA1
    SCCP_CHANNELSTATE_SPEEDDIAL = 0xA2
    SCCP_CHANNELSTATE_DIGITSFOLL = 0xA3
    SCCP_CHANNELSTATE_INVALIDCONFERENCE = 0xA4
    SCCP_CHANNELSTATE_CONNECTEDCONFERENCE = 0xA5
    SCCP_CHANNELSTATE_BLINDTRANSFER = 0xA6
    SCCP_CHANNELSTATE_ZOMBIE = 0xFE
    SCCP_CHANNELSTATE_DND = 0xFF


    sccp_channelstates = {SCCP_CHANNELSTATE_DOWN : "DOWN",
                          SCCP_CHANNELSTATE_OFFHOOK: "OFFHOOK",
                          SCCP_CHANNELSTATE_ONHOOK: "ONHOOK",
                          SCCP_CHANNELSTATE_RINGOUT: "RINGOUT",
                          SCCP_CHANNELSTATE_RINGING: "RINGING",
                          SCCP_CHANNELSTATE_CONNECTED: "CONNECTED",
                          SCCP_CHANNELSTATE_BUSY: "BUSY    ",
                          SCCP_CHANNELSTATE_CONGESTION: "CONGESTION",
                          SCCP_CHANNELSTATE_HOLD: "HOLD    ",
                          SCCP_CHANNELSTATE_CALLWAITING: "CALLWAITING",
                          SCCP_CHANNELSTATE_CALLTRANSFER: "CALLTRANSFER",
                          SCCP_CHANNELSTATE_CALLPARK: "CALLPARK",
                          SCCP_CHANNELSTATE_PROCEED: "PROCEED",
                          SCCP_CHANNELSTATE_CALLREMOTEMULTILINE: "CALLREMOTEMULTILINE",
                          SCCP_CHANNELSTATE_INVALIDNUMBER: "INVALIDNUMBER",
                          SCCP_CHANNELSTATE_DIALING: "DIALING",
                          SCCP_CHANNELSTATE_PROGRESS: "PROGRESS",
                          SCCP_CHANNELSTATE_GETDIGITS: "GETDIGITS",
                          SCCP_CHANNELSTATE_CALLCONFERENCE: "CALLCONFERENCE",
                          SCCP_CHANNELSTATE_SPEEDDIAL: "SPEEDDIAL",
                          SCCP_CHANNELSTATE_DIGITSFOLL: "DIGITSFOLL",
                          SCCP_CHANNELSTATE_INVALIDCONFERENCE: "INVALIDCONFERENCE",
                          SCCP_CHANNELSTATE_CONNECTEDCONFERENCE: "CONNECTEDCONFERENCE",
                          SCCP_CHANNELSTATE_BLINDTRANSFER: "BLINDTRANSFER",
                          SCCP_CHANNELSTATE_ZOMBIE: "ZOMBIE",
                          SCCP_CHANNELSTATE_DND: "DND"}

    def __init__(self):
        SCCPMessage.__init__(self, SCCPMessageType.CallStateMessage)
        self.line=0
        self.callState=self.SCCP_CHANNELSTATE_DOWN
        self.callId=0


    def unPack(self,buffer):
        datas = unpack("III",buffer[:12])
        self.callState = datas[0]
        self.line =datas[1]
        self.callId =datas[2]
