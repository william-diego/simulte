//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "LteMacEnbD2D.h"
#include "LteHarqBufferRx.h"
#include "LteMacBuffer.h"
#include "LteMacQueue.h"
#include "LteFeedbackPkt.h"
#include "LteSchedulerEnbDl.h"
#include "LteSchedulerEnbUl.h"
#include "LteSchedulingGrant.h"
#include "LteAllocationModule.h"
#include "LteAmc.h"
#include "UserTxParams.h"
#include "LteRac_m.h"
#include "LteCommon.h"
#include "AmcPilotD2D.h"
#include "D2DModeSwitchNotification_m.h"

Define_Module( LteMacEnbD2D);

/*********************
 * PUBLIC FUNCTIONS
 *********************/

LteMacEnbD2D::LteMacEnbD2D() :
    LteMacEnb()
{
}

LteMacEnbD2D::~LteMacEnbD2D()
{
}

void LteMacEnbD2D::initialize(int stage)
{
    LteMacEnb::initialize(stage);

    if (stage == 1)
    {
        usePreconfiguredTxParams_ = par("usePreconfiguredTxParams");
        Cqi d2dCqi = par("d2dCqi");
        if (usePreconfiguredTxParams_)
            check_and_cast<AmcPilotD2D*>(amc_->getPilot())->setPreconfiguredTxParams(d2dCqi);
    }
}

void LteMacEnbD2D::macHandleFeedbackPkt(cPacket *pkt)
{
    LteFeedbackPkt* fb = check_and_cast<LteFeedbackPkt*>(pkt);
    std::map<MacNodeId, LteFeedbackDoubleVector> fbMapD2D = fb->getLteFeedbackDoubleVectorD2D();

    // skip if no D2D CQI has been reported
    if (!fbMapD2D.empty())
    {
        //get Source Node Id<
        MacNodeId id = fb->getSourceNodeId();
        std::map<MacNodeId, LteFeedbackDoubleVector>::iterator mapIt;
        LteFeedbackDoubleVector::iterator it;
        LteFeedbackVector::iterator jt;

        // extract feedback for D2D links
        for (mapIt = fbMapD2D.begin(); mapIt != fbMapD2D.end(); ++mapIt)
        {
            MacNodeId peerId = mapIt->first;
            for (it = mapIt->second.begin(); it != mapIt->second.end(); ++it)
            {
                for (jt = it->begin(); jt != it->end(); ++jt)
                {
                    if (!jt->isEmptyFeedback())
                    {
                        amc_->pushFeedbackD2D(id, (*jt), peerId);
                    }
                }
            }
        }
    }
    LteMacEnb::macHandleFeedbackPkt(pkt);
}

void LteMacEnbD2D::bufferizeBsr(MacBsr* bsr, MacCid cid)
{
    PacketInfo vpkt(bsr->getSize(), bsr->getTimestamp());

    LteMacBufferMap::iterator it = bsrbuf_.find(cid);
    if (it == bsrbuf_.end())
    {
        // Queue not found for this cid: create
        LteMacBuffer* bsrqueue = new LteMacBuffer();

        bsrqueue->pushBack(vpkt);
        bsrbuf_[cid] = bsrqueue;

        EV << "LteBsrBuffers : Added new BSR buffer for node: "
           << MacCidToNodeId(cid) << " for Lcid: " << MacCidToLcid(cid)
           << " Current BSR size: " << bsr->getSize() << "\n";
    }
    else
    {
        // Found
        LteMacBuffer* bsrqueue = it->second;
        bsrqueue->pushBack(vpkt);

        EV << "LteBsrBuffers : Using old buffer for node: " << MacCidToNodeId(
            cid) << " for Lcid: " << MacCidToLcid(cid)
           << " Current BSR size: " << bsr->getSize() << "\n";
    }

    // signal backlog to Uplink scheduler
    enbSchedulerUl_->backlog(cid);
}

void LteMacEnbD2D::macPduUnmake(cPacket* pkt)
{
    LteMacPdu* macPkt = check_and_cast<LteMacPdu*>(pkt);
    while (macPkt->hasSdu())
    {
        // Extract and send SDU
        cPacket* upPkt = macPkt->popSdu();
        take(upPkt);

        // TODO: upPkt->info()
        EV << "LteMacBase: pduUnmaker extracted SDU" << endl;
        sendUpperPackets(upPkt);
    }

    while (macPkt->hasCe())
    {
        // Extract CE
        MacBsr* bsr = check_and_cast<MacBsr*>(macPkt->popCe());
        UserControlInfo* lteInfo = check_and_cast<UserControlInfo*>(macPkt->getControlInfo());
        LogicalCid lcid = lteInfo->getLcid();
        MacCid cid = idToMacCid(lteInfo->getSourceId(), lcid); // this way, different connections from the same UE (e.g. one UL and one D2D)
                                                               // obtain different CIDs. With the inverse operation, you can get
                                                               // the LCID and discover if the connection is UL or D2D
        bufferizeBsr(bsr, cid);
        delete bsr;
    }
    delete macPkt;
}

void LteMacEnbD2D::sendGrants(LteMacScheduleList* scheduleList)
{
    EV << NOW << " LteMacEnbD2D::sendGrants " << endl;

    while (!scheduleList->empty())
    {
        LteMacScheduleList::iterator it, ot;
        it = scheduleList->begin();

        Codeword cw = it->first.second;
        Codeword otherCw = MAX_CODEWORDS - cw;

        MacCid cid = it->first.first;
        LogicalCid lcid = MacCidToLcid(cid);
        MacNodeId nodeId = MacCidToNodeId(cid);

        unsigned int granted = it->second;
        unsigned int codewords = 0;

        // removing visited element from scheduleList.
        scheduleList->erase(it);

        if (granted > 0)
        {
            // increment number of allocated Cw
            ++codewords;
        }
        else
        {
            // active cw becomes the "other one"
            cw = otherCw;
        }

        std::pair<unsigned int, Codeword> otherPair(nodeId, otherCw);

        if ((ot = (scheduleList->find(otherPair))) != (scheduleList->end()))
        {
            // increment number of allocated Cw
            ++codewords;

            // removing visited element from scheduleList.
            scheduleList->erase(ot);
        }

        if (granted == 0)
            continue; // avoiding transmission of 0 grant (0 grant should not be created)

        // get the direction of the grant, depending on which connection has been scheduled by the eNB
        Direction dir = (lcid == D2D_MULTI_SHORT_BSR) ? D2D_MULTI : ((lcid == D2D_SHORT_BSR) ? D2D : UL);

        EV << NOW << " LteMacEnbD2D::sendGrants Node[" << getMacNodeId() << "] - "
           << granted << " blocks to grant for user " << nodeId << " on "
           << codewords << " codewords. CW[" << cw << "\\" << otherCw << "] dir[" << dirToA(dir) << "]" << endl;

        // TODO Grant is set aperiodic as default
        LteSchedulingGrant* grant = new LteSchedulingGrant("LteGrant");
        grant->setDirection(dir);
        grant->setCodewords(codewords);

        // set total granted blocks
        grant->setTotalGrantedBlocks(granted);

        UserControlInfo* uinfo = new UserControlInfo();
        uinfo->setSourceId(getMacNodeId());
        uinfo->setDestId(nodeId);
        uinfo->setFrameType(GRANTPKT);

        grant->setControlInfo(uinfo);

        // get and set the user's UserTxParams
        const UserTxParams& ui = getAmc()->computeTxParams(nodeId, dir);
        UserTxParams* txPara = new UserTxParams(ui);
        // FIXME: possible memory leak
        grant->setUserTxParams(txPara);

        // acquiring remote antennas set from user info
        const std::set<Remote>& antennas = ui.readAntennaSet();
        std::set<Remote>::const_iterator antenna_it, antenna_et = antennas.end();
        const unsigned int logicalBands = deployer_->getNumBands();

        //  HANDLE MULTICW
        for (; cw < codewords; ++cw)
        {
            unsigned int grantedBytes = 0;

            for (Band b = 0; b < logicalBands; ++b)
            {
                unsigned int bandAllocatedBlocks = 0;

                for (antenna_it = antennas.begin(); antenna_it != antenna_et; ++antenna_it)
                {
                    bandAllocatedBlocks += enbSchedulerUl_->readPerUeAllocatedBlocks(nodeId, *antenna_it, b);
                }

                grantedBytes += amc_->computeBytesOnNRbs(nodeId, b, cw,
                    bandAllocatedBlocks, dir);
            }

            grant->setGrantedCwBytes(cw, grantedBytes);
            EV << NOW << " LteMacEnbD2D::sendGrants - granting " << grantedBytes << " on cw " << cw << endl;
        }

        RbMap map;

        enbSchedulerUl_->readRbOccupation(nodeId, map);

        grant->setGrantedBlocks(map);

        // send grant to PHY layer
        sendLowerPackets(grant);
    }
}

void LteMacEnbD2D::storeRxHarqBufferMirror(MacNodeId id, LteHarqBufferRxD2DMirror* mirbuff)
{
    // TODO optimize

    if ( harqRxBuffersD2DMirror_.find(id) != harqRxBuffersD2DMirror_.end() )
        delete harqRxBuffersD2DMirror_[id];
    harqRxBuffersD2DMirror_[id] = mirbuff;
}

HarqRxBuffersMirror* LteMacEnbD2D::getRxHarqBufferMirror()
{
    return &harqRxBuffersD2DMirror_;
}

void LteMacEnbD2D::deleteRxHarqBufferMirror(MacNodeId id)
{
    HarqRxBuffersMirror::iterator it = harqRxBuffersD2DMirror_.begin() , et=harqRxBuffersD2DMirror_.end();
    for(; it != et;)
    {
        // get current nodeIDs
        MacNodeId senderId = it->second->peerId_; // Transmitter
        MacNodeId destId = it->first;             // Receiver

        if (senderId == id || destId == id)
        {
            it = harqRxBuffersD2DMirror_.erase(it);
        }
        else
        {
            ++it;
        }
    }

}

void LteMacEnbD2D::sendModeSwitchNotification(MacNodeId srcId, MacNodeId dstId, LteD2DMode oldMode, LteD2DMode newMode)
{
    Enter_Method("sendModeSwitchNotification");

    D2DModeSwitchNotification* switchPkt = new D2DModeSwitchNotification("D2DModeSwitchNotification");
    switchPkt->setPeerId(dstId);
    switchPkt->setOldMode(oldMode);
    switchPkt->setNewMode(newMode);

    UserControlInfo* uinfo = new UserControlInfo();
    uinfo->setSourceId(nodeId_);
    uinfo->setDestId(srcId);
    uinfo->setFrameType(D2DMODESWITCHPKT);
    switchPkt->setControlInfo(uinfo);

    // send pkt to PHY layer
    sendLowerPackets(switchPkt);
}
