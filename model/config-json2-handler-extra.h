/**
 * @file config-json-handler-extra.h
 * @brief
 */

#ifndef CONFIG_JSON_HANDLER_EXTRA_H
#define CONFIG_JSON_HANDLER_EXTRA_H

#include "../helper/config-json2-helper.h"

#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/csma-module.h"
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
// Config

// Node

// Link
// Internet

// Network

// RoutingProtocol

// Mobility
void GazeboMobilityHandler(const json& jMobility, ConfigJsonHelper& helper);
// Application

// Simulator
} // namespace configjson2
} // namespace ns3

#endif