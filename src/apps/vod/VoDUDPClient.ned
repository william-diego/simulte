//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

package lte.apps.vod;
import inet.applications.IUDPApp;


//
// Video-on-demand (VoD) client application.
//
simple VoDUDPClient like IUDPApp
{
    parameters:
        int localPort;
        double startStreamTime @unit("s") = default(0s);
        string startMetrics = default("off");
        string vod_trace_file;
        string bsePath = default("");
        string origVideoYuv = default("");
        string origVideoSvc = default("");
        string decPath = default("");
        string traceType = default("NS2");
        int playbackSize @unit("s") = default(2s);
        string avipluginPath = default("");
        int numFrame = default(0);
        int numPktPerFrame = default(0);
        @display("i=block/app");
        @class(VoDUDPClient);
        @statistic[VoDTptLayer0](title="VoD Tpt Layer 0"; unit=""; source="VoDTptLayer0"; record=lteRate);
        @statistic[VoDTptLayer1](title="VoD Tpt Layer 1"; unit=""; source="VoDTptLayer1"; record=lteRate);
        @statistic[VoDTptLayer2](title="VoD Tpt Layer 2"; unit=""; source="VoDTptLayer2"; record=lteRate);
        @statistic[VoDTptLayer3](title="VoD Tpt Layer 3"; unit=""; source="VoDTptLayer3"; record=lteRate);

        @statistic[VoDDelayLayer0](title="VoD Delay Layer 0"; unit=""; source="VoDDelayLayer0"; record=lteAvg);
        @statistic[VoDDelayLayer1](title="VoD Delay Layer 1"; unit=""; source="VoDDelayLayer1"; record=lteAvg);
        @statistic[VoDDelayLayer2](title="VoD Delay Layer 2"; unit=""; source="VoDDelayLayer2"; record=lteAvg);
        @statistic[VoDDelayLayer3](title="VoD Delay Layer 3"; unit=""; source="VoDDelayLayer3"; record=lteAvg);


    gates:
        input udpIn @labels(UDPControlInfo/up);
        output udpOut @labels(UDPControlInfo/down);
}

