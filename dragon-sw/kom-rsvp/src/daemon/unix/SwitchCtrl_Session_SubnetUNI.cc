/****************************************************************************

UNI Based Switch/Subnet Control Module source file SwitchCtrl_Session_SubnetUNI.cc
Created by Xi Yang @ 01/9/2007
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Session_SubnetUNI.h"
#include "RSVP.h"
#include "RSVP_Global.h"
#include "RSVP_RoutingService.h"
#include "RSVP_NetworkServiceDaemon.h"
#include "RSVP_Message.h"
#include "RSVP_Log.h"

SwitchCtrl_Session_SubnetUNI_List* SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList = NULL;


void SwitchCtrl_Session_SubnetUNI::internalInit ()
{
    active = false;
    snmp_enabled = false; 
    rfc2674_compatible = false; 
    snmpSessionHandle = NULL; 
    uniSessionId = NULL; 
    uniState = 0;
}

void SwitchCtrl_Session_SubnetUNI::setSubnetUniData(SubnetUNI_Data& data, uint16 subuni_id, 
 uint16 tunnel_id, float bw, uint32 tna_ipv4, uint32 uni_cid_ipv4, uint32 uni_nid_ipv4, uint32 data_if,
 uint32 port, uint32 egress_label, uint32 upstream_label, char* cc_name)
{
    memset(&data, 0, sizeof(SubnetUNI_Data));
    data.subnet_id = subuni_id; 
    data.ethernet_bw = bw; 
    data.tna_ipv4 = tna_ipv4;
    data.egress_label = egress_label; 
    data.upstream_label = upstream_label;

    data.uni_cid_ipv4 = uni_cid_ipv4;
    data.uni_nid_ipv4 = uni_nid_ipv4;
    data.data_if_ipv4 = data_if;

    if (cc_name)
        strncpy((char*)data.control_channel_name, cc_name, CTRL_CHAN_NAME_LEN-1);
}       

void SwitchCtrl_Session_SubnetUNI::setSubnetUniSrc(uint16 id, uint16 tunnel_id, float bw,  uint32 tna, uint32 nid, uint32 port, uint32 egress_label, uint32 upstream_label, char* cc_name)
{
	assert (cc_name);
	const LogicalInterface* lif = RSVP_Global::rsvp->findInterfaceByName(String(cc_name));

	uint32 uni_c_id = lif ? lif->getLocalAddress().rawAddress() : tna; //?
	uint32 uni_n_id = nid;
	uint32 data_if = 0; //temp

	setSubnetUniData(subnetUniSrc, id, tunnel_id, bw, tna, uni_c_id, uni_n_id, data_if, port, egress_label, upstream_label, cc_name);
}

void SwitchCtrl_Session_SubnetUNI::setSubnetUniDest(uint16 id, uint16 tunnel_id, float bw,  uint32 tna, uint32 nid, uint32 port, uint32 egress_label, uint32 upstream_label, char* cc_name)
{
	assert (cc_name);
	const LogicalInterface* lif = RSVP_Global::rsvp->findInterfaceByName(String(cc_name));

	uint32 uni_c_id = lif ? lif->getLocalAddress().rawAddress() : tna; //?
	uint32 uni_n_id = nid;
	uint32 data_if = 0; //temp

	setSubnetUniData(subnetUniDest, id, tunnel_id, bw, tna, uni_c_id, uni_n_id, data_if, port, egress_label, upstream_label, cc_name);
}

const LogicalInterface* SwitchCtrl_Session_SubnetUNI::getControlInterface()
{
	SubnetUNI_Data* uniData = (isSource ? &subnetUniSrc : &subnetUniDest);
	const NetAddress nidAddress(uniData->uni_nid_ipv4);
	NetAddress gwAddress(0);
	if (uniData->control_channel_name[0] != 0)
		return RSVP_Global::rsvp->findInterfaceByName(String((char*)uniData->control_channel_name));
	else
		return RSVP_Global::rsvp->getRoutingService().getUnicastRoute( nidAddress, gwAddress );
}

SwitchCtrl_Session_SubnetUNI::~SwitchCtrl_Session_SubnetUNI() 
{
    deregisterRsvpApiClient();
    if (!uniSessionId)
        delete uniSessionId;
}


void SwitchCtrl_Session_SubnetUNI::uniRsvpSrcUpcall(const GenericUpcallParameter& upcallParam, void* uniClientData)
{
    //should never be called
}

void SwitchCtrl_Session_SubnetUNI::uniRsvpDestUpcall(const GenericUpcallParameter& upcallParam, void* uniClientData)
{
    //should never be called
}


void SwitchCtrl_Session_SubnetUNI::registerRsvpApiClient()
{
    //insert the UNI ApiClient logicalLif into rsvp lifList if it has not been added.
    assert (RSVP_API::apiLif);
    assert (getFileDesc() > 0);
    const String ifName(RSVP_Global::apiUniClientName);
    if (!RSVP_Global::rsvp->findInterfaceByName(String(RSVP_Global::apiUniClientName))) {
        RSVP_Global::rsvp->addApiClientInterface(RSVP_API::apiLif);
    }
    NetworkServiceDaemon::registerApiClient_Handle(getFileDesc());
    if (!SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList)
        SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList = new SwitchCtrl_Session_SubnetUNI_List;
    subnetUniApiClientList->push_front(this);
}

void SwitchCtrl_Session_SubnetUNI::deregisterRsvpApiClient()
{
    assert(SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList);
    SwitchCtrl_Session_SubnetUNI_List::Iterator it = subnetUniApiClientList->begin() ;
    for (; it != subnetUniApiClientList->end(); ++it) {
        if ((*it) == this) {
            subnetUniApiClientList->erase(it);
            break;
        }
    }
    if (subnetUniApiClientList->size() == 0) {
        NetworkServiceDaemon::deregisterApiClient_Handle(getFileDesc());
        delete SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList;
        SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList = NULL;
    }
}

void SwitchCtrl_Session_SubnetUNI::receiveAndProcessMessage(const Message& msg)
{
    //checking msg owner
    if (!isSessionOwner(msg))
        return;

    if (isSource)
        receiveAndProcessResv(msg);
    else
        receiveAndProcessPath(msg);
}

bool SwitchCtrl_Session_SubnetUNI::isSessionOwner(const Message& msg)
{
    const SESSION_Object* session_obj = &msg.getSESSION_Object();
    const LSP_TUNNEL_IPv4_SENDER_TEMPLATE_Object* sender_obj;
    const FlowDescriptorList* fdList;
    SubnetUNI_Data* uniData;

    if (isSource) {
        if ( !(session_obj->getDestAddress().rawAddress() == subnetUniSrc.uni_nid_ipv4 && session_obj->getTunnelId() == subnetUniSrc.tunnel_id) )
            return false;
        uniData = &subnetUniSrc;
    }
    else if ( !(session_obj->getDestAddress().rawAddress() == subnetUniDest.uni_cid_ipv4 && session_obj->getTunnelId() == subnetUniDest.tunnel_id) )
    {
        return false;
        uniData = &subnetUniDest;
    }

    if (msg.getMsgType() == Message::Path)
    {
        sender_obj = &msg.getSENDER_TEMPLATE_Object();
        if (sender_obj->getSrcAddress().rawAddress() == uniData->uni_cid_ipv4
            || sender_obj->getSrcAddress().rawAddress() == 0 || sender_obj->getSrcAddress().rawAddress() == 0x100007f)
            return true;
    }
    else if  (msg.getMsgType() == Message::Resv)
    {
        fdList = &msg.getFlowDescriptorList();
        FlowDescriptorList::ConstIterator it = fdList->begin();
        for (; it != fdList->end(); ++it)
        {
             if ((*it).filterSpecList.size()>0 && (*(*it).filterSpecList.begin()).getSrcAddress().rawAddress()  == uniData->uni_cid_ipv4)
                return true;
        }
    }

    return false;
}

void SwitchCtrl_Session_SubnetUNI::initUniRsvpApiSession()
{
    uniSessionId = new RSVP_API::SessionId();
    if (isSource)
        *uniSessionId = createSession( NetAddress(subnetUniSrc.uni_nid_ipv4), subnetUniSrc.tunnel_id,subnetUniSrc.uni_cid_ipv4, SwitchCtrl_Session_SubnetUNI::uniRsvpSrcUpcall);
    else
        *uniSessionId = createSession( NetAddress(subnetUniDest.uni_cid_ipv4), subnetUniDest.tunnel_id, subnetUniDest.uni_nid_ipv4, SwitchCtrl_Session_SubnetUNI::uniRsvpDestUpcall);
    active = true;

    uniState = Message::InitAPI;
}

void SwitchCtrl_Session_SubnetUNI::createRsvpUniPath()
{
    if (!active || !uniSessionId)
        initUniRsvpApiSession();
    if (!isSource)
    	return;

    SONET_SDH_SENDER_TSPEC_Object *stb = NULL;
    GENERALIZED_UNI_Object *uni = NULL;
    LABEL_SET_Object* labelSet = NULL;
    SESSION_ATTRIBUTE_Object* ssAttrib = NULL;
    UPSTREAM_LABEL_Object* upLabel = NULL;
    GENERALIZED_LABEL_REQUEST_Object *lr = NULL;
    //IPv4_IF_ID_RSVP_HOP_Object  $$$$ based on control_channel_name ???
    //==> revise RSVP_AP::createSender

    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(subnetUniSrc.ethernet_bw);
    if (sonet_tb1)
        stb = new SENDER_TSPEC_Object(*sonet_tb1);

    uni = new GENERALIZED_UNI_Object (subnetUniSrc.tna_ipv4, subnetUniDest.tna_ipv4, 
                    subnetUniDest.logical_port, subnetUniDest.egress_label, 
                    subnetUniDest.logical_port, subnetUniDest.upstream_label);

    /* $$$$ we may need this later on ...
    labelSet = new LABEL_SET_Object();
    for (int i=0;i<para->labelSetSize;i++)
     	labelSet->addSubChannel(para->labelSet[i]);
    */

    ssAttrib = new SESSION_ATTRIBUTE_Object(sessionName); //$$$$

    upLabel = new UPSTREAM_LABEL_Object(subnetUniSrc.upstream_label);

    lr = new LABEL_REQUEST_Object ( LABEL_REQUEST_Object::L_ANSI_SDH, 
    							       LABEL_REQUEST_Object::S_TDM,
                                                        LABEL_REQUEST_Object::G_SONET_SDH);

    //$$$$ subnetUniSrc.tunnel_id = subnetUniSrc.subnet_id;

    //NetAddress srcAddress(subnetUniSrc.uni_cid_ipv4);
    //createSender( *uniSessionId, srcAddress, subnetUniSrc.tunnel_id, *stb, *lr, NULL, uni, labelSet, ssAttrib, upLabel, 50);
    createSender( *uniSessionId, subnetUniSrc.tunnel_id, *stb, *lr, NULL, uni, labelSet, ssAttrib, upLabel, 50);

    if (uni) uni->destroy();
    if (labelSet) labelSet->destroy();
    if (lr) delete lr;
    if (stb) delete stb;
    if (ssAttrib) delete ssAttrib;
    if (upLabel) delete upLabel;

    if (uniState == Message::InitAPI)
        uniState = Message::Path;

    return;
}

void SwitchCtrl_Session_SubnetUNI::createRsvpUniResv(const SONET_SDH_SENDER_TSPEC_Object& sendTSpec, const LSP_TUNNEL_IPv4_FILTER_SPEC_Object& senderTemplate)
{
    if (!active || !uniSessionId)
        return;

    SONET_SDH_FLOWSPEC_Object* flowspec = new FLOWSPEC_Object((const SONET_TSpec&)(sendTSpec));
    FlowDescriptorList fdList;
    fdList.push_back( flowspec );
    fdList.back().filterSpecList.push_back( senderTemplate );
    //IPv4_IF_ID_RSVP_HOP_Object? ==> revise RSVP_AP::createReservation

    createReservation( *uniSessionId, false, FF, fdList, NULL);

    if (uniState == Message::InitAPI)
        uniState = Message::Resv;

    return;
}

void SwitchCtrl_Session_SubnetUNI::receiveAndProcessPath(const Message & msg)
{
    if (!active)
        return;

    //change session states ...
    //notify main session indirectly
    uniState = msg.getMsgType();

    switch (msg.getMsgType())
    {
    case Message::Path:
    case Message::PathResv:

        createRsvpUniResv(msg.getSENDER_TSPEC_Object(), msg.getSENDER_TEMPLATE_Object());

        break;
    case Message::InitAPI:

        assert( *(*uniSessionId) );
        //refreshSession( **(*uniSessionId), RSVP_Global::defaultApiRefresh );

        break;
    default:
        //unprocessed RSVP messages
        break;
    }

    return;
}

void SwitchCtrl_Session_SubnetUNI::receiveAndProcessResv(const Message & msg)
{
    if (!active)
        return;

    //change session states ...
    //notify main session indirectly
    uniState = msg.getMsgType();
    
    switch (msg.getMsgType())
    {
    case Message::Resv:
    case Message::ResvConf:

        break;
    case Message::PathErr:

        break;
    case Message::ResvTear:

        break;
    case Message::InitAPI:

        assert( *(*uniSessionId) );
        //refreshSession( **(*uniSessionId), RSVP_Global::defaultApiRefresh );

	break;
    default:
        break;
    }

    return;
}

void SwitchCtrl_Session_SubnetUNI::releaseRsvpPath()
{
    if (!active || !uniSessionId)
        return;

    assert( *(*uniSessionId) );
    releaseSession(**(*uniSessionId));

    if (isSource)
        uniState = Message::PathTear;
    else
        uniState = Message::ResvTear; 

    return;
}

void SwitchCtrl_Session_SubnetUNI::refreshUniRsvpSession()
{
    if (!active)
        return;
    
    //do nothing?
}


