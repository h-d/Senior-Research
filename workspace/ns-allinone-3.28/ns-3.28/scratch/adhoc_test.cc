/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "ns3/netanim-module.h"
#include <fstream>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/applications-module.h"

using namespace ns3;
using namespace dsr;

NS_LOG_COMPONENT_DEFINE("adhoc-network-test");

class AdHoc
{
	public:
		AdHoc();
		void Run (int nSinks, double transmissionPow, std::string CSVfileName, uint32_t protocol);
		uint32_t CommandSetup (int argc, char **argv);

	private:
		Ptr<Socket> SetupPacketReceive(Ipv4Address addr, Ptr<Node> node);
		void ReceivePacket (Ptr<Socket> socket);
		void Throughput();

	uint32_t port;
	uint32_t totalBytes;
	uint32_t packetsReceived;
	std::string m_CSVfileName;
	std::string protocolName;
	int num_Sinks;
	double transmP;
	bool Mobility;
	uint32_t m_protocol;
};

AdHoc::AdHoc ()
	:	port (9),
	 	totalBytes(8),
			packetsReceived(0),
			m_CSVfileName("ad-hoc-sim.csv"),
			Mobility(false),
			m_protocol (1)
{
}

static inline std::string PrintReceivedPacket (Ptr<Socket> socket, Ptr<Packet> packet, Address senderAddress)
{
	std::ostringstream oss;
	oss <<Simulator::Now().GetSeconds() << " " << socket->GetNode()->GetId();

	if (InetSocketAddress::IsMatchingType(senderAddress))
	{
		InetSocketAddress addr = InetSocketAddress::ConvertFrom(senderAddress);
		oss << " received a packet from " <<addr.GetIpv4();
	} 
	else
	{
		oss << "received a packet.";
	}	
	return oss.str();
}

uint32_t CalculatePacketLoss(uint32_t packetReceived)
{
	if (packetReceived < 1400)
		return (1400 - packetReceived);
	else 
		return 0;
}

void AdHoc::ReceivePacket(Ptr<Socket> socket)
{
	Ptr<Packet> packet;
	Address senderAddress;
	while ((packet = socket->RecvFrom(senderAddress)))
	{
		totalBytes+= packet->GetSize();
		packetsReceived +=1;
		NS_LOG_UNCOND(PrintReceivedPacket (socket, packet, senderAddress));
	}
}

void AdHoc::Throughput()
{
	double RetransRate = totalBytes * 8.0 / 1000;
	totalBytes = 0;

	std::ofstream out ("test.csv", std::ios::app);

	out << (Simulator::Now()).GetSeconds()<<"\n" << "Packet retransmission "
		  << RetransRate << " Kbps\n" << "N* of packets received " << packetsReceived 
		  << " p/s" << "\n"<< "N* of sinks "<<num_Sinks<<"\n"<<"Protocol used "
		  << protocolName << "\n" << "Power " << transmP << "\n" <<"Packet loss "
		  << CalculatePacketLoss(packetsReceived) << " p/s" << std::endl;

	out.close();
	packetsReceived = 0;
	Simulator::Schedule(Seconds(1.0), &AdHoc::Throughput, this);
}

Ptr<Socket> AdHoc::SetupPacketReceive (Ipv4Address addr, Ptr<Node> node)
{
	TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
	Ptr<Socket> sink = Socket::CreateSocket(node, tid);
	InetSocketAddress local = InetSocketAddress (addr, port);
	sink->Bind(local);
	sink->SetRecvCallback (MakeCallback (&AdHoc::ReceivePacket, this));

	return sink;
}

uint32_t AdHoc::CommandSetup (int argc, char **argv)
{
	CommandLine cmd;
	cmd.AddValue ("CSVfileName", "Name of CSV output file", m_CSVfileName);
	cmd.AddValue("Mobility tracing", "Enable mobility tracing", Mobility);
	cmd.AddValue("protocol", "1-AODV 2-DSR", m_protocol);
	cmd.Parse(argc, argv);
	return m_protocol;
}


//MAIN

int main (int argc, char *argv[])
{
	AdHoc experiment;
	int protocol = experiment.CommandSetup(argc, argv);

	std::string CSVfileName = "test.csv";

	//blank out last output file and write column headers
	std::ofstream out (CSVfileName.c_str());
	out << "SimulationScnd,ReceiveRate,PacketReceived,NumberOfSinks,RoutingProtocol,TransmissionPower"<<std::endl;
	out.close();

	int nSinks = 5;
	double transmissionPow = 7.5;
	
	experiment.Run(nSinks, transmissionPow, CSVfileName, protocol);
}

void AdHoc::Run (int nSinks, double transmissionPow, std::string CSVfileName, uint32_t protocol)
{
	Packet::EnablePrinting();
	num_Sinks = nSinks;
	transmP = transmissionPow;
	m_protocol = protocol;

	int nWifis = 10;

	double totalTime = 10.0;
	std::string rate ("1Mbps");
	std::string phyMode ("DsssRate11Mbps");
	std::string tr_name ("adhoc-network-test");
	int nodeSpeed = 20; //in m/s
	int nodePause = 0; //in s
	protocolName = "protocol";

	Config::SetDefault ("ns3::OnOffApplication::PacketSize", StringValue("64")); //set the packet size
	Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue(rate)); //set the packet rate

	//Set Non-unicast rate to unicast mode
	Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

	NodeContainer adhocNodes;
	adhocNodes.Create(nWifis);

	//Set up wifi phy and channel using helpers
	WifiHelper wifi;
	wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

	YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
	YansWifiChannelHelper wifiChannel;
	wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
	wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
	wifiPhy.SetChannel(wifiChannel.Create());

	//Add MAC and disable rate control
	WifiMacHelper wifiMac;
	wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));
	
	wifiPhy.Set("TxPowerStart",DoubleValue(transmissionPow));
	wifiPhy.Set("TxPowerEnd", DoubleValue(transmissionPow));

	wifiMac.SetType("ns3::AdhocWifiMac"); //Set Adhoc network type
	NetDeviceContainer adhocDevices = wifi.Install(wifiPhy, wifiMac, adhocNodes);

	MobilityHelper mobilityAdhoc;
	int64_t streamIndex = 0; //Used to get consistent mobility across scenarios

	ObjectFactory pos;
	pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
	pos.Set("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
	pos.Set("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));

	Ptr<PositionAllocator> taPositionAlloc = pos.Create()->GetObject<PositionAllocator>();
	streamIndex += taPositionAlloc->AssignStreams (streamIndex);

	std::stringstream ssSpeed;
	ssSpeed << "ns3::UniformRandomVariable[Min=0.0|Max="<<nodeSpeed<<"]";
	std::stringstream ssPause;
	ssPause << "ns3::ConstantRandomVariable[Constant="<<nodePause<<"]";
	mobilityAdhoc.SetMobilityModel ("ns3::RandomWaypointMobilityModel", //Mobility mode //Random
			"Speed", StringValue (ssSpeed.str()),
			"Pause", StringValue (ssPause.str()),
			"PositionAllocator", PointerValue(taPositionAlloc));
	mobilityAdhoc.SetPositionAllocator(taPositionAlloc);
	mobilityAdhoc.Install(adhocNodes);
	streamIndex+=mobilityAdhoc.AssignStreams(adhocNodes, streamIndex);
	NS_UNUSED(streamIndex); //From this point, streamIndex is unused

	AodvHelper aodv;
	DsrMainHelper dsrMain;
	Ipv4ListRoutingHelper list;
	InternetStackHelper internet;

	if (m_protocol ==  1)
	{
		list.Add (aodv, 100);
		protocolName = "AODV";
		internet.Install(adhocNodes);
		std::cout<<"Using AODV Protocol";
	} else if (m_protocol == 2)
	{
		DsrHelper dsr;
		protocolName = "DSR";
		internet.Install(adhocNodes);
		dsrMain.Install(dsr, adhocNodes);
		std::cout<<"Using DSR Protocol";
	}
	

	NS_LOG_INFO("assigning IP address");
	Ipv4AddressHelper addressAdhoc;
	addressAdhoc.SetBase ("10.1.1.0","255.255.255.0");
	Ipv4InterfaceContainer adhocInterfaces;
	adhocInterfaces = addressAdhoc.Assign (adhocDevices);

	OnOffHelper onoff1 ("ns3::UdpSocketFactory",Address()); //Set a UDP communication
	onoff1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
	onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
	for (int i = 0; i < nSinks; i++)
	{
		Ptr<Socket> sink = SetupPacketReceive (adhocInterfaces.GetAddress(i), adhocNodes.Get(i));
		AddressValue remoteAddress (InetSocketAddress (adhocInterfaces.GetAddress(i),port));
		onoff1.SetAttribute("Remote", remoteAddress);

		Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
		ApplicationContainer temp = onoff1.Install (adhocNodes.Get (i + nSinks));
		temp.Start (Seconds (var->GetValue(0.0,1.0)));

	}

	Ptr<Socket> sink = SetupPacketReceive(adhocInterfaces.GetAddress(5), adhocNodes.Get(5));
	AddressValue remoteAddress (InetSocketAddress (adhocInterfaces.GetAddress(5), port));

	Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
	ApplicationContainer temp = onoff1.Install(adhocNodes.Get(0));
	temp.Start(Seconds (var->GetValue (0.0,1.0)));
	temp.Stop(Seconds(totalTime));

	for (int i = nSinks+1; i < nWifis; i++)
	{
		Ptr<Socket> sink = SetupPacketReceive (adhocInterfaces.GetAddress (i), adhocNodes.Get(i));

		AddressValue remoteAddress (InetSocketAddress (adhocInterfaces.GetAddress (i), port));
		onoff1.SetAttribute ("Remote", remoteAddress);

		Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
		ApplicationContainer temp = onoff1.Install (adhocNodes.Get (nWifis - i));
		temp.Start(Seconds (var->GetValue (0.0,1.0)));
		temp.Stop(Seconds(totalTime));
	}

	NS_LOG_INFO("Run Simulation.");

	AdHoc::Throughput();

	AnimationInterface anim ("animation.xml");
	Simulator::Stop(Seconds(totalTime));
	Simulator::Run();

	Simulator::Destroy();
}
