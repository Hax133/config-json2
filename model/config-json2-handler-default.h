/**
 * @file config-json-handle-default.h
 * @brief
 */

#ifndef CONFIG_JSON_HANDLER_DEFAULT_H
#define CONFIG_JSON_HANDLER_DEFAULT_H

#include "../helper/config-json2-helper.h"

#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/wifi-module.h"

#include <fstream>
#include <iostream>

namespace ns3
{
namespace configjson2
{
using json = nlohmann::json;
// Config
void ConfigHandler(const json& jConfig, ConfigJsonHelper& helper);

// Node
void NodeHandler(const json& jNode, ConfigJsonHelper& helper);
void SwitchHandler(const json& jNode, ConfigJsonHelper& helper);

// Link
void WifiLinkHandler(const json& jLink, ConfigJsonHelper& helper);
void P2pLinkHandler(const json& jLink, ConfigJsonHelper& helper);
void CsmaLinkHandler(const json& jLink, ConfigJsonHelper& helper);
// Internet
void InternetHandler(const json& jInternet, ConfigJsonHelper& helper);

// Network
void Ipv4NetworkHandler(const json& jNetwork, ConfigJsonHelper& helper);
void Ipv6NetworkHandler(const json& jNetwork, ConfigJsonHelper& helper);

// RoutingProtocol
void Ipv4StaticHandler(const json& jRouting, ConfigJsonHelper& helper);
void Ipv6StaticHandler(const json& jRouting, ConfigJsonHelper& helper);
void OlsrHandler(const json& jRoutingProtocol, ConfigJsonHelper& helper);

// Mobility
void ConstantPositionMobilityHandler(const json& jMobility, ConfigJsonHelper& helper);
void WaypointMobilityHandler(const json& jMobility, ConfigJsonHelper& helper);

// Application
void UdpEchoClientHandler(const json& jApplication, ConfigJsonHelper& helper);
void UdpEchoServerHandler(const json& jApplication, ConfigJsonHelper& helper);
void OnOffHandler(const json& jApplication, ConfigJsonHelper& helper);
void PacketSinkHandler(const json& jApplication, ConfigJsonHelper& helper);

// Simulator
void SimulatorHandler(const json& jSimulator, ConfigJsonHelper& helper);
} // namespace configjson2
} // namespace ns3

#endif