/****************************************************************************

CLI Based Switch Control Module source file SwitchCtrl_Session_JuniperEX3200.cc
Created by Xi Yang on 03/12/2009
 To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Session_JuniperEX3200.h"
#include "SwitchCtrl_JUNOScript.h"
#include "RSVP.h"
#include "RSVP_Log.h"

bool SwitchCtrl_Session_JuniperEX3200::connectSwitch()
{
    if (SwitchCtrl_Session::connectSwitch() == false)
        return false;
     if (CLI_Session::engage("login:") == false)
        return false;

    int n;
    if ((n = writeShell( "junoscript\n", 5)) < 0)
        goto _abort;
    if ((n= readShell( "<!-- session start", NULL, false, 1, 10)) < 0)
        goto _abort;
    if ((n= readShell( "-->", NULL, true, 1, 10)) < 0)
        goto _abort;
    if ((n = writeShell( "<?xml version=\"1.0\" encoding=\"us-ascii\"?> <junoscript version=\"1.0\" client=\"vlsr\" release=\"9.2R2\">\n", 5)) < 0)
        goto _abort;
    if ((n= readShell( "<!-- user", NULL, false, 1, 10)) < 0)
        goto _abort;
    if ((n= readShell( "-->", NULL, true, 1, 10)) < 0)
        goto _abort;

    return true;

_abort:

    LOG(2)( Log::MPLS, "VLSR: failed initiate JUNOScript communication with ", switchInetAddr);
    return false;
}

void SwitchCtrl_Session_JuniperEX3200::disconnectSwitch()
{
    CLI_Session::disengage("</junoscript>");
}


bool SwitchCtrl_Session_JuniperEX3200::preAction()
{
    if (!active || vendor!=JuniperEX3200 || !pipeAlive())
        return false;
    int n;
    DIE_IF_NEGATIVE(n= writeShell( "<rpc><lock-configuration /></rpc>", 5)) ;
    DIE_IF_NEGATIVE(n= readShellBuffer(bufScript, "</rpc-reply>", "</junoscript>", true, 1, 10)) ;
    JUNOScriptLockReplyParser lockReplyParser(bufScript);
    if (!lockReplyParser.loadAndVerifyScript())
        return false;   
    else if (!lockReplyParser.isSuccessful())
        return false;
    // Or try a few more times?

    return true;
}

bool SwitchCtrl_Session_JuniperEX3200::postAction()
{
    if (!active || vendor!=JuniperEX3200 || !pipeAlive())
        return false;
    int n;
    DIE_IF_NEGATIVE(n= writeShell( "<rpc><unlock-configuration /></rpc>", 5)) ;
    DIE_IF_NEGATIVE(n= readShellBuffer(bufScript, "</rpc-reply>", "</junoscript>", true, 1, 10)) ;
    JUNOScriptUnlockReplyParser unlockReplyParser(bufScript);
    if (!unlockReplyParser.loadAndVerifyScript())
    {
        //$$$$LOG::
        return false;
    }
    else if (!unlockReplyParser.isSuccessful())
    {
        //$$$$LOG::
        return false;
    }
    return true;
}

bool SwitchCtrl_Session_JuniperEX3200::postActionWithCommit()
{
    if (!active || vendor!=JuniperEX3200 || !pipeAlive())
        return false;
    int n;
    DIE_IF_NEGATIVE(n= writeShell( "<rpc><commit-configuration /></rpc>", 5)) ;
    DIE_IF_NEGATIVE(n= readShellBuffer(bufScript, "</rpc-reply>", "</junoscript>", true, 1, 10)) ;
    JUNOScriptCommitReplyParser commitReplyParser(bufScript);
    if (!commitReplyParser.loadAndVerifyScript())
    {
        //$$$$LOG::
    }
    else if (!commitReplyParser.isSuccessful())
    {
        //$$$$LOG::
    }
    
    DIE_IF_NEGATIVE(n= writeShell( "<rpc><unlock-configuration /></rpc>", 5)) ;
    DIE_IF_NEGATIVE(n= readShellBuffer(bufScript, "</rpc-reply>", "</junoscript>", true, 1, 10)) ;
    JUNOScriptUnlockReplyParser unlockReplyParser(bufScript);
    if (!unlockReplyParser.loadAndVerifyScript())
    {
        //$$$$LOG::
        return false;
    }
    else if (!unlockReplyParser.isSuccessful())
    {
        //$$$$LOG::
        return false;
    }

    return true;
}

bool SwitchCtrl_Session_JuniperEX3200::movePortToVLANAsUntagged(uint32 port, uint32 vlanID)
{
    uint32 bit;
    bool ret = false;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
        return ret; //don't touch the control port!

    int old_vlan = getVLANbyUntaggedPort(port);
    bit = convertUnifiedPort2JuniperEXBit(port);
    assert(bit < MAX_VLAN_PORT_BYTES*8);
    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, old_vlan);
    if (vpmUntagged)
        ResetPortBit(vpmUntagged->portbits, bit);
    vpmAll = getVlanPortMapById(vlanPortMapListAll, old_vlan);
    if (vpmAll)
        ResetPortBit(vpmAll->portbits, bit);
    if (old_vlan > 1) { //Remove untagged port from old VLAN
        ret &= deleteVLANPort_JUNOScript(port, old_vlan, false);
    }

    bit = convertUnifiedPort2JuniperEXBit(port);
    assert(bit < MAX_VLAN_PORT_BYTES*8);
    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if (vpmUntagged)
    SetPortBit(vpmUntagged->portbits, bit);
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
        SetPortBit(vpmAll->portbits, bit);
        ret &= addVLANPort_JUNOScript(port, vlanID, false);
    }

    return ret;
}

bool SwitchCtrl_Session_JuniperEX3200::movePortToVLANAsTagged(uint32 port, uint32 vlanID)
{
    uint32 bit;
    bool ret = false;
    vlanPortMap * vpmAll = NULL;

    if ((!active) || port==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
        return ret; //don't touch the control port!

    bit = convertUnifiedPort2JuniperEXBit(port);
    assert(bit < MAX_VLAN_PORT_BYTES*8);
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
       SetPortBit(vpmAll->portbits, bit);
       ret&=addVLANPort_JUNOScript(port, vlanID, true);
    }

    return ret;
}

bool SwitchCtrl_Session_JuniperEX3200::removePortFromVLAN(uint32 port, uint32 vlanID)
{
    uint32 bit;
    bool ret = false;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || port==SWITCH_CTRL_PORT)
    	return ret; //don't touch the control port!

    if (vlanID>=MIN_VLAN && vlanID<=MAX_VLAN){
        bit = convertUnifiedPort2JuniperEXBit(port);
        assert(bit < MAX_VLAN_PORT_BYTES*8);
        vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
        if (vpmUntagged)
            ResetPortBit(vpmUntagged->portbits, bit);
        vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
        if (vpmAll) {
            ResetPortBit(vpmAll->portbits, bit);
            ret &= deleteVLANPort_JUNOScript(port, vlanID, false);
            //ret &= deleteVLANPort_JUNOScript(port, vlanID, true);
        }
    } else {
        LOG(2) (Log::MPLS, "Trying to remove port from an invalid VLAN ", vlanID);
    }

    return ret;
}


bool SwitchCtrl_Session_JuniperEX3200::addVLANPort_JUNOScript(uint32 portID, uint32 vlanID, bool isTagged)
{
    int n;
    if (!preAction())
        return false;
    bool ret = false;
    JUNOScriptMovePortVlanComposer jsComposer(bufScript, LINELEN*3);
    JUNOScriptRpcReplyParser jsParser(bufScript);
    if (!(ret = jsComposer.setPortAndVlan(portID, vlanID, isTagged, false)))
        goto _out;
    DIE_IF_NEGATIVE(n = writeShell(jsComposer.getScript(), 5)) ;
    DIE_IF_NEGATIVE(n = writeShell("\n", 5)) ;

    DIE_IF_NEGATIVE(n= readShellBuffer(bufScript, "</rpc-reply>", "</junoscript>", true, 1, 10)) ;
    if (!(ret = jsParser.loadAndVerifyScript()))
        goto _out;
    else if (!(ret = jsParser.isSuccessful()))
        goto _out;

_out:
    if (ret == false)
    {
        //$$$$ LOG::
        return (false && postAction()); // without commit
    }
    return postActionWithCommit();
}

bool SwitchCtrl_Session_JuniperEX3200::deleteVLANPort_JUNOScript(uint32 portID, uint32 vlanID, bool isTagged)
{
    int n;
    if (!preAction())
        return false;
    bool ret = false;
    JUNOScriptMovePortVlanComposer jsComposer(bufScript, LINELEN*3);
    JUNOScriptRpcReplyParser jsParser(bufScript);
    if (!(ret = jsComposer.setPortAndVlan(portID, vlanID, isTagged, true)))
        goto _out;
    DIE_IF_NEGATIVE(n = writeShell(jsComposer.getScript(), 5)) ;
    DIE_IF_NEGATIVE(n = writeShell("\n", 5)) ;

    DIE_IF_NEGATIVE(n= readShellBuffer(bufScript, "</rpc-reply>", "</junoscript>", true, 1, 10)) ;
    if (!(ret = jsParser.loadAndVerifyScript()))
        goto _out;
    else if (!(ret = jsParser.isSuccessful()))
        goto _out;

_out:
    if (ret == false)
    {
        //$$$$ LOG::
        return (false && postAction()); // without commit
    }

    return postActionWithCommit();
}

/////--------QoS Functions------/////

//committed_rate and peak_rate are measued in Mbps; burst_size and peak_burst_size are in KB
bool SwitchCtrl_Session_JuniperEX3200::policeInputBandwidth_JUNOScript(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size)
{
    int n;
    int committed_rate_int = (int)committed_rate;
    if (committed_rate_int < 1 || !preAction())
        return false;
    bool ret = false;

    //$TODO:

_out:
    if (ret == false)
    {
        //$$$$ LOG::
        return (false && postAction()); // without commit
    }

    return postActionWithCommit();

}

////////-------vendor specific hook procedures------////////////

bool SwitchCtrl_Session_JuniperEX3200::hook_createVLAN(const uint32 vlanID)
{
    int n;
    bool ret = false;
    DIE_IF_EQUAL(vlanID, 0);
    DIE_IF_EQUAL(preAction(), false);

    JUNOScriptVlanComposer jsComposer(bufScript, LINELEN*3);
    JUNOScriptRpcReplyParser jsParser(bufScript);
    if (!(ret = jsComposer.setVlan(vlanID, false)))
        goto _out;
    DIE_IF_NEGATIVE(n = writeShell(jsComposer.getScript(), 5)) ;
    DIE_IF_NEGATIVE(n = writeShell("\n", 5)) ;

    DIE_IF_NEGATIVE(n= readShellBuffer(bufScript, "</rpc-reply>", "</junoscript>", true, 1, 10)) ;
    if (!(ret = jsParser.loadAndVerifyScript()))
        goto _out;
    else if (!(ret = jsParser.isSuccessful()))
        goto _out;

_out:
    if (ret == false)
    {
        //$$$$ LOG::
        return (false && postAction()); // without commit
    }

    return postActionWithCommit();
}

bool SwitchCtrl_Session_JuniperEX3200::hook_removeVLAN(const uint32 vlanID)
{
    int n;
    bool ret = false;
    DIE_IF_EQUAL(vlanID, 0);
    DIE_IF_EQUAL(preAction(), false);

    JUNOScriptVlanComposer jsComposer(bufScript, LINELEN*3);
    JUNOScriptRpcReplyParser jsParser(bufScript);
    if (!(ret = jsComposer.setVlan(vlanID, true)))
        goto _out;
    DIE_IF_NEGATIVE(n = writeShell(jsComposer.getScript(), 5)) ;
    DIE_IF_NEGATIVE(n = writeShell("\n", 5)) ;

    DIE_IF_NEGATIVE(n= readShellBuffer(bufScript, "</rpc-reply>", "</junoscript>", true, 1, 10)) ;
    if (!(ret = jsParser.loadAndVerifyScript()))
        goto _out;
    else if (!(ret = jsParser.isSuccessful()))
        goto _out;

_out:
    if (ret == false)
    {
        //$$$$ LOG::
        return (false && postAction()); // without commit
    }

    return postActionWithCommit();
}

bool SwitchCtrl_Session_JuniperEX3200::hook_isVLANEmpty(const vlanPortMap &vpm)
{
    vlanPortMap vpm_empty;
    memset(&vpm_empty, 0, sizeof(vlanPortMap));

    return (memcmp(&vpm.portbits, &vpm_empty.portbits, MAX_VLAN_PORT_BYTES) == 0);
}

void SwitchCtrl_Session_JuniperEX3200::hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars)
{
    memset(&vpm, 0, sizeof(vlanPortMap));
    if (vars->val.bitstring ){
        for (uint32 i = 0; i < vars->val_len && i < MAX_VLAN_PORT_BYTES; i++) {
            vpm.portbits[i] = vars->val.bitstring[i];
       }
    }
    vpm.vid = hook_convertVLANInterfaceToID((uint32)vars->name[vars->name_length - 1]);
}

bool SwitchCtrl_Session_JuniperEX3200::hook_createPortToIDRefTable(portRefIDList &portRefIdConvList)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    oid root[MAX_OID_LEN];
    char ref_str[100];
    portRefID ref_id;
    size_t anOID_len = MAX_OID_LEN;
    int status;
    bool running = true;
    size_t rootlen;
    uint32 tmp_port_id = 0; 
    uint32 tmp_slot_id = 0; 
    uint32 tmp_mod_id = 0; 

    if (!snmpEnabled())
        return false;

    status = read_objid(".1.3.6.1.2.1.2.2.1.2", anOID, &anOID_len);
    rootlen = anOID_len;
    memcpy(root, anOID, rootlen*sizeof(oid));
    portRefIdConvList.clear();
    while (running) {
            // Create the PDU for the data for our request.
            pdu = snmp_pdu_create(SNMP_MSG_GETNEXT);
            snmp_add_null_var(pdu, anOID, anOID_len);
            // Send the Request out.
            status = snmp_synch_response(snmpSessionHandle, pdu, &response);
            if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
                    for (vars = response->variables; vars; vars = vars->next_variable) {
                            if ((vars->name_length < rootlen) || (memcmp(anOID, vars->name, rootlen * sizeof(oid)) != 0)) {
                                    running = false;
                                    continue;
                            }

                            if (vars->val.string){
                                    strncpy(ref_str, (char*)vars->val.string, vars->val_len);
                                    ref_str[vars->val_len] = 0;
                            }
                            else
                                    ref_str[0] = 0;
                            if (ref_str[0] != 0) {
                                ref_id.ref_id = (int)vars->name[vars->name_length - 1];
                                tmp_port_id = 0;tmp_slot_id = 0; tmp_mod_id = 0; int tmp0; //recognize ge-0/0/0 instead of ge-0/0/0.0
                                if (sscanf(ref_str, "ge-%d/%d/%d.%d", &tmp_mod_id, &tmp_slot_id, &tmp_port_id, &tmp0) == 3) {
	                                      ref_id.port_bit = ref_id.ref_id; //port bit is zero-based
	                                      ref_id.port_id = (((tmp_mod_id&0xf) << 12) | ((tmp_slot_id&0xf) << 8) | (tmp_port_id&0xff));
	                                      portRefIdConvList.push_back(ref_id);
      				    }
                            }
                            if ((vars->type != SNMP_ENDOFMIBVIEW) &&
                                (vars->type != SNMP_NOSUCHOBJECT) &&
                                (vars->type != SNMP_NOSUCHINSTANCE)) {
                                memcpy((char *)anOID, (char *)vars->name, vars->name_length * sizeof(oid));
                                anOID_len = vars->name_length;
                            }
                            else {
                                running = 0;
                            }
                    }
            }
            else {
                running = false;
            }
            if(response) snmp_free_pdu(response);
    }


    if (portRefIdConvList.size() == 0)
        return false;
    return true;
}

bool SwitchCtrl_Session_JuniperEX3200::hook_createVlanInterfaceToIDRefTable(vlanRefIDList &vlanRefIdConvList)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    oid root[MAX_OID_LEN];
    char ref_str[100];
    vlanRefID ref_id;
    size_t anOID_len = MAX_OID_LEN;
    int status;
    bool running = true;
    size_t rootlen;

    if (!snmpEnabled())
        return false;

    status = read_objid(".1.3.6.1.2.1.17.7.1.4.3.1.1", anOID, &anOID_len); //--> interface mapping to determine port bits
    rootlen = anOID_len;
    memcpy(root, anOID, rootlen*sizeof(oid));
    vlanRefIdConvList.clear();
    while (running) {
            // Create the PDU for the data for our request.
            pdu = snmp_pdu_create(SNMP_MSG_GETNEXT);
            snmp_add_null_var(pdu, anOID, anOID_len);
            // Send the Request out.
            status = snmp_synch_response(snmpSessionHandle, pdu, &response);
            if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
                    for (vars = response->variables; vars; vars = vars->next_variable) {
                            if ((vars->name_length < rootlen) || (memcmp(anOID, vars->name, rootlen * sizeof(oid)) != 0)) {
                                    running = false;
                                    continue;
                            }

                            if (vars->val.string){
                                    strncpy(ref_str, (char*)vars->val.string, vars->val_len);
                                    ref_str[vars->val_len] = 0;
                            }
                            else
                                    ref_str[0] = 0;
                            if (ref_str[0] != 0) {
                                ref_id.ref_id = (int)vars->name[vars->name_length - 1];
                                if (sscanf(ref_str, "dynamic_vlan_%d", &ref_id.vlan_id) == 1)
                                    vlanRefIdConvList.push_back(ref_id);
                                else if (strncmp(ref_str, "default", 7) == 0)
                                {
                                    ref_id.vlan_id = 1;
                                    vlanRefIdConvList.push_back(ref_id);
                                }
                            }
                            if ((vars->type != SNMP_ENDOFMIBVIEW) &&
                                (vars->type != SNMP_NOSUCHOBJECT) &&
                                (vars->type != SNMP_NOSUCHINSTANCE)) {
                                memcpy((char *)anOID, (char *)vars->name, vars->name_length * sizeof(oid));
                                anOID_len = vars->name_length;
                            }
                            else {
                                running = 0;
                            }
                    }
            }
            else {
                running = false;
            }
            if(response) snmp_free_pdu(response);
    }

    if (vlanRefIdConvList.size() == 0)
        return false;
    return true;
}

bool SwitchCtrl_Session_JuniperEX3200::hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port)
{
    if (HasPortBit(vpm.portbits, convertUnifiedPort2JuniperEXBit(port)))
        return true;
    return false;
}

bool SwitchCtrl_Session_JuniperEX3200::hook_getPortListbyVLAN(PortList& portList, uint32  vlanID)
{
    uint32 bit;
    uint32 port;
    vlanPortMap* vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if(!vpmAll)
        return false;
    portList.clear();
    for (bit = 0; bit < sizeof(vpmAll->portbits)*8; bit++)
    {
        if (HasPortBit(vpmAll->portbits, bit))
        {
            port = convertJuniperEXBit2UnifiedPort(bit);
            portList.push_back(port);
        }
    }

    if (portList.size() == 0)
        return false;
    return true;
}

uint32 SwitchCtrl_Session_JuniperEX3200::hook_convertPortInterfaceToID(uint32 id)
{
    portRefIDList::Iterator it;
    for (it = portRefIdConvList.begin(); it != portRefIdConvList.end(); ++it)
    {
        if ((*it).ref_id == id)
            return (*it).port_id;
    }
 
    return 0;
}

uint32 SwitchCtrl_Session_JuniperEX3200::hook_convertPortIDToInterface(uint32 id)
{
    portRefIDList::Iterator it;
    for (it = portRefIdConvList.begin(); it != portRefIdConvList.end(); ++it)
    {
        if ((*it).port_id == id)
            return (*it).ref_id;
    }
 
    return 0;
}

uint32 SwitchCtrl_Session_JuniperEX3200::hook_convertVLANInterfaceToID(uint32 id)
{
    vlanRefIDList::Iterator it;
    for (it = vlanRefIdConvList.begin(); it != vlanRefIdConvList.end(); ++it)
    {
        if ((*it).ref_id == id)
            return (*it).vlan_id;
    }
 
    return 0;
}

uint32 SwitchCtrl_Session_JuniperEX3200::hook_convertVLANIDToInterface(uint32 id)
{
    vlanRefIDList::Iterator it;
    for (it = vlanRefIdConvList.begin(); it != vlanRefIdConvList.end(); ++it)
    {
        if ((*it).vlan_id == id)
            return (*it).ref_id;
    }
 
    return 0;
}

