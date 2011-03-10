/****************************************************************************

Dell (vendor) PowerConnect 6224/6248/8024 (model) Control Module header file SwitchCtrl_Session_PowerConnect8000.h
Created by  Xi Yang, 2011
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#ifndef SWITCHCTRL_SESSION_POWERCONNECT6200_8000_H_
#define SWITCHCTRL_SESSION_POWERCONNECT6200_8000_H_

#include "CLI_Session.h"

#ifndef DELL_ERROR_PROMPT 
#define DELL_ERROR_PROMPT "% "
#endif

#define SwitchCtrl_Session_PowerConnect6200 SwitchCtrl_Session_PowerConnect8000

class SwitchCtrl_Session_PowerConnect8000: public CLI_Session
{
public:
	SwitchCtrl_Session_PowerConnect8000(): CLI_Session() { }	
	SwitchCtrl_Session_PowerConnect8000(const String& sName, const NetAddress& swAddr): CLI_Session(sName, swAddr) { }
	virtual ~SwitchCtrl_Session_PowerConnect8000() { this->disconnectSwitch(); }

	virtual bool connectSwitch();
	virtual void disconnectSwitch();
    virtual bool pipeAlive();
    virtual bool preAction();
	virtual bool postAction();

	// Dell PowerConnect specific
	// Port name convention: all 1/xgN named 1/1/N and 1/gN named 1/0/N in ospfd.conf
	void portToName(uint32 port, char* buf)
		{
            if (((port>>8)&0x000f) == 0)
                sprintf(buf, "1/g%d", port&0xff); 
            else
                sprintf(buf, "1/xg%d", port&0xff);             
		}
	uint32 portToBit(uint32 port)
		{
            uint32 offset;
            if (SWITCH_VENDOR_MODEL == PowerConnect6024 || SWITCH_VENDOR_MODEL == PowerConnect6224 )
                offset = 24;
            else if (SWITCH_VENDOR_MODEL == PowerConnect6248)
                offset = 48;
            else // PowerConnect8024
                offset = 0;
            if (((port>>8)&0x000f) == 0)
                return (port&0x00ff); 
            else
                return (port&0x00ff)+offset; 
		}
	uint32 bitToPort(uint32 bit)
		{
            if (SWITCH_VENDOR_MODEL == PowerConnect6024 || SWITCH_VENDOR_MODEL == PowerConnect6224)
            {
                if (bit <= 24)
                    return (0x10<<8)|bit;
                else 
                    return (0x11<<8)|(bit-24);
            }
            else if (SWITCH_VENDOR_MODEL == PowerConnect6248)
            {
                if (bit <= 48)
                    return (0x10<<8)|bit;
                else 
                    return (0x11<<8)|(bit-48);
            }
            else // PowerConnect8024
            {
                return (0x0011<<8)|(bit&0x00ff);
            }
		}

	////////// Below are vendor specific functions/////////
	virtual bool movePortToVLANAsTagged(uint32 port, uint32 vlanID);
	virtual bool movePortToVLANAsUntagged(uint32 port, uint32 vlanID);
	virtual bool removePortFromVLAN(uint32 port, uint32 vlanID);
	virtual bool setVLANPortsTagged(uint32 taggedPorts, uint32 vlanID) { return true; }

	///////////------QoS Functions ------/////////
	virtual bool policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);
	virtual bool limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);

	//Vendor/Model specific hook functions
	bool deleteVLANPort_ShellScript(uint32 portID, uint32 vlanID, bool isTagged = false);
	bool addVLANPort_ShellScript(uint32 portID, uint32 vlanID, bool isTagged = false);
	virtual bool hook_createVLAN(const uint32 vlanID);
	virtual bool hook_removeVLAN(const uint32 vlanID);
	virtual bool hook_isVLANEmpty(const vlanPortMap &vpm);
	virtual void hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars);
	virtual bool hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port);
	virtual bool hook_getPortListbyVLAN(PortList& portList, uint32  vlanID);
};

#endif /*SWITCHCTRL_SESSION_POWERCONNECT6200_8000_H_*/


