#include "config-json2-handler-default.h"

namespace ns3
{
namespace configjson2 {
using json = nlohmann::json;

WifiStandard
ParseWifiStandard(const std::string& s)
{
    static const std::unordered_map<std::string, WifiStandard> table = {
        {"WIFI_STANDARD_80211a", WIFI_STANDARD_80211a},
        {"WIFI_STANDARD_80211b", WIFI_STANDARD_80211b},
        {"WIFI_STANDARD_80211g", WIFI_STANDARD_80211g},
        {"WIFI_STANDARD_80211n", WIFI_STANDARD_80211n},
        {"WIFI_STANDARD_80211ac", WIFI_STANDARD_80211ac},
        {"WIFI_STANDARD_80211ax", WIFI_STANDARD_80211ax},
        {"WIFI_STANDARD_80211be", WIFI_STANDARD_80211be},
    };

    auto it = table.find(s);
    if (it == table.end())
    {
        throw std::invalid_argument("Unknown WifiStandard: " + s);
    }
    return it->second;
}

void
ConfigHandler(const json& jConfig, ConfigJsonHelper& helper)
{
    auto baseDir = helper.configPath.parent_path();

    /* ===============================
     * Load sub JSON files
     * =============================== */
    helper.handleJson[JsonDomain::Node] =
        ConfigJsonHelper::LoadJson(baseDir / jConfig.at("nodes").get<std::string>());

    helper.handleJson[JsonDomain::Link] =
        ConfigJsonHelper::LoadJson(baseDir / jConfig.at("links").get<std::string>());

    helper.handleJson[JsonDomain::Internet] =
        ConfigJsonHelper::LoadJson(baseDir / jConfig.at("internet").get<std::string>());

    helper.handleJson[JsonDomain::Ipv4Network] =
        ConfigJsonHelper::LoadJson(baseDir / jConfig.at("ipv4Network").get<std::string>());

    helper.handleJson[JsonDomain::Ipv6Network] =
        ConfigJsonHelper::LoadJson(baseDir / jConfig.at("ipv6Network").get<std::string>());

    helper.handleJson[JsonDomain::Ipv4RoutingProtocol] =
        ConfigJsonHelper::LoadJson(baseDir / jConfig.at("ipv4RoutingProtocol").get<std::string>());

    helper.handleJson[JsonDomain::Ipv6RoutingProtocol] =
        ConfigJsonHelper::LoadJson(baseDir / jConfig.at("ipv6RoutingProtocol").get<std::string>());

    helper.handleJson[JsonDomain::Mobility] =
        ConfigJsonHelper::LoadJson(baseDir / jConfig.at("mobility").get<std::string>());

    helper.handleJson[JsonDomain::Application] =
        ConfigJsonHelper::LoadJson(baseDir / jConfig.at("applications").get<std::string>());

    helper.handleJson[JsonDomain::Simulator] =
        ConfigJsonHelper::LoadJson(baseDir / jConfig.at("simulator").get<std::string>());
}

void
NodeHandler(const json& jNode, ConfigJsonHelper& helper)
{
    Ptr<Node> node = CreateObject<Node>();
    Names::Add("node" + std::to_string(jNode.at("nodeId").get<uint32_t>()), node);
}

void
SwitchHandler(const json& jNode, ConfigJsonHelper& helper)
{
    // 必需字段：nodeId
    uint32_t nodeId = jNode.at("nodeId").get<uint32_t>();
    Ptr<Node> node = Names::Find<Node>("node" + std::to_string(nodeId));
    NS_ASSERT_MSG(node, "Node not found: node" << nodeId);

    /* ===============================
     * Collect CSMA NetDevices
     * =============================== */
    NetDeviceContainer csmaDevices;
    for (uint32_t i = 0; i < node->GetNDevices(); ++i)
    {
        Ptr<NetDevice> dev = node->GetDevice(i);
        if (dev->GetInstanceTypeId() == CsmaNetDevice::GetTypeId())
        {
            csmaDevices.Add(dev);
        }
    }

    /* ===============================
     * Bridge (need >= 2 CSMA ports)
     * =============================== */
    if (csmaDevices.GetN() >= 2)
    {
        BridgeHelper bridge;
        bridge.Install(node, csmaDevices);
    }

    /* ===============================
     * Disable IPv4 interfaces (from 1)
     * =============================== */
    if (Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>())
    {
        for (uint32_t i = 1; i < ipv4->GetNInterfaces(); ++i)
        {
            ipv4->SetDown(i);
        }
    }

    /* ===============================
     * Disable IPv6 interfaces (from 1)
     * =============================== */
    if (Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>())
    {
        for (uint32_t i = 1; i < ipv6->GetNInterfaces(); ++i)
        {
            ipv6->SetDown(i);
        }
    }
}

void
WifiLinkHandler(const json& jLink, ConfigJsonHelper&)
{
    uint32_t linkId = jLink.at("linkId").get<uint32_t>();

    /* ---------- WifiHelper ---------- */
    WifiHelper wifi;
    wifi.SetStandard(ParseWifiStandard(jLink.at("wifiStandard").get<std::string>()));

    const auto& wm = jLink.at("wifiManager");
    wifi.SetRemoteStationManager(wm.at("type").get<std::string>(),
                                 "DataMode",
                                 StringValue(wm.at("dataMode").get<std::string>()),
                                 "ControlMode",
                                 StringValue(wm.at("controlMode").get<std::string>()));

    /* ---------- Channel ---------- */
    Ptr<Channel> channel;
    const auto& jChannel = jLink.at("channel");
    std::string channelType = jChannel.at("type").get<std::string>();

    if (channelType == "ns3::YansWifiChannel")
    {
        YansWifiChannelHelper ch;
        ch.SetPropagationDelay(jChannel.at("propagationDelay").get<std::string>());

        Ptr<PropagationLossModel> first = nullptr;
        Ptr<PropagationLossModel> prev = nullptr;

        for (const auto& loss : jChannel.at("propagationLoss"))
        {
            std::string type = loss.at("type").get<std::string>();
            Ptr<PropagationLossModel> cur;

            if (type == "ns3::FriisPropagationLossModel")
                cur = CreateObject<FriisPropagationLossModel>();
            else if (type == "ns3::LogDistancePropagationLossModel")
                cur = CreateObject<LogDistancePropagationLossModel>();
            else if (type == "ns3::NakagamiPropagationLossModel")
                cur = CreateObject<NakagamiPropagationLossModel>();
            else
                NS_FATAL_ERROR("Unsupported PropagationLossModel: " << type);

            if (!first)
                first = cur;
            if (prev)
                prev->SetNext(cur);
            prev = cur;
        }

        channel = ch.Create();
        DynamicCast<YansWifiChannel>(channel)->SetPropagationLossModel(first);
    }
    else if (channelType == "ns3::SingleModelSpectrumChannel" ||
             channelType == "ns3::MultiModelSpectrumChannel")
    {
        SpectrumChannelHelper ch;
        ch.SetChannel(channelType);
        ch.SetPropagationDelay(jChannel.at("propagationDelay").get<std::string>());

        for (const auto& loss : jChannel.at("propagationLoss"))
        {
            std::string type = loss.at("type").get<std::string>();
            Ptr<PropagationLossModel> m;

            if (type == "ns3::FriisPropagationLossModel")
                m = CreateObject<FriisPropagationLossModel>();
            else if (type == "ns3::LogDistancePropagationLossModel")
                m = CreateObject<LogDistancePropagationLossModel>();
            else
                NS_FATAL_ERROR("Unsupported PropagationLossModel: " << type);

            ch.AddPropagationLoss(m);
        }

        channel = ch.Create();
    }
    else
    {
        NS_FATAL_ERROR("Unsupported Wifi channel type: " << channelType);
    }

    Names::Add("link" + std::to_string(linkId) + "-channel", channel);

    /* ---------- NetDevices ---------- */
    for (const auto& jDev : jLink.at("netDevices"))
    {
        uint32_t nodeId = jDev.at("nodeId").get<uint32_t>();
        Ptr<Node> node = Names::Find<Node>("node" + std::to_string(nodeId));

        /* MAC */
        WifiMacHelper mac;
        const auto& jMac = jDev.at("wifiMac");
        mac.SetType(jMac.at("type").get<std::string>(),
                    "Ssid",
                    SsidValue(Ssid(jMac.at("ssid").get<std::string>())));

        /* PHY */
        Ptr<NetDevice> dev;
        const auto& jPhy = jDev.at("wifiPhy");

        if (channelType == "ns3::YansWifiChannel")
        {
            YansWifiPhyHelper phy;
            phy.SetChannel(DynamicCast<YansWifiChannel>(channel));
            phy.SetErrorRateModel(jLink.at("errorRateModel").get<std::string>());

            for (auto it = jPhy.begin(); it != jPhy.end(); ++it)
            {
                const std::string& k = it.key();
                const auto& v = it.value();
                if (k == "type")
                    continue;

                if (k == "channelSettings")
                    phy.Set("ChannelSettings", StringValue(v.get<std::string>()));
                else if (k == "txPowerStart")
                    phy.Set("TxPowerStart", DoubleValue(v.get<double>()));
                else if (k == "txPowerEnd")
                    phy.Set("TxPowerEnd", DoubleValue(v.get<double>()));
                else if (k == "rxSensitivity")
                    phy.Set("RxSensitivity", DoubleValue(v.get<double>()));
                else if (k == "ccaEdThreshold")
                    phy.Set("CcaEdThreshold", DoubleValue(v.get<double>()));
            }

            dev = wifi.Install(phy, mac, node).Get(0);
        }
        else
        {
            SpectrumWifiPhyHelper phy;
            phy.SetChannel(DynamicCast<SpectrumChannel>(channel));
            phy.SetErrorRateModel(jLink.at("errorRateModel").get<std::string>());

            for (auto it = jPhy.begin(); it != jPhy.end(); ++it)
            {
                const std::string& k = it.key();
                const auto& v = it.value();
                if (k == "type")
                    continue;

                if (k == "channelSettings")
                    phy.Set("ChannelSettings", StringValue(v.get<std::string>()));
                else if (k == "txPowerStart")
                    phy.Set("TxPowerStart", DoubleValue(v.get<double>()));
                else if (k == "txPowerEnd")
                    phy.Set("TxPowerEnd", DoubleValue(v.get<double>()));
                else if (k == "rxSensitivity")
                    phy.Set("RxSensitivity", DoubleValue(v.get<double>()));
                else if (k == "ccaEdThreshold")
                    phy.Set("CcaEdThreshold", DoubleValue(v.get<double>()));
            }

            dev = wifi.Install(phy, mac, node).Get(0);
        }

        Names::Add("node" + std::to_string(nodeId) + "-link" + std::to_string(linkId), dev);
    }
}

void
P2pLinkHandler(const json& jLink, ConfigJsonHelper& helper)
{
    // 必需字段
    uint32_t linkId = jLink.at("linkId").get<uint32_t>();
    PointToPointHelper p2p;

    /* ===============================
     * Queue (optional)
     * =============================== */
    if (jLink.contains("queue"))
    {
        const auto& jQueue = jLink.at("queue");
        for (auto it = jQueue.begin(); it != jQueue.end(); ++it)
        {
            const std::string& key = it.key();
            const auto& val = it.value();

            if (key == "type")
            {
                p2p.SetQueue(val.get<std::string>());
            }
        }
    }

    /* ===============================
     * Channel attributes (optional)
     * =============================== */
    if (jLink.contains("channel"))
    {
        const auto& jChannel = jLink.at("channel");
        for (auto it = jChannel.begin(); it != jChannel.end(); ++it)
        {
            const std::string& key = it.key();
            const auto& val = it.value();

            if (key == "type")
                continue;

            if (key == "delay")
            {
                p2p.SetChannelAttribute("Delay", TimeValue(Time(val.get<std::string>())));
            }
        }
    }

    /* ===============================
     * Device attributes (optional)
     * =============================== */
    if (jLink.contains("device"))
    {
        const auto& jDevice = jLink.at("device");
        for (auto it = jDevice.begin(); it != jDevice.end(); ++it)
        {
            const std::string& key = it.key();
            const auto& val = it.value();

            if (key == "type")
                continue;

            if (key == "dataRate")
            {
                p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate(val.get<std::string>())));
            }
            else if (key == "mtu")
            {
                p2p.SetDeviceAttribute("Mtu", UintegerValue(val.get<uint32_t>()));
            }
        }
    }

    /* ===============================
     * Nodes (required)
     * =============================== */
    NodeContainer nodes;
    for (const auto& dev : jLink.at("netDevices"))
    {
        uint32_t nodeId = dev.at("nodeId").get<uint32_t>();
        nodes.Add(Names::Find<Node>("node" + std::to_string(nodeId)));
    }

    /* ===============================
     * Install
     * =============================== */
    NetDeviceContainer devices = p2p.Install(nodes);

    /* ===============================
     * Name NetDevices
     * =============================== */
    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        Ptr<NetDevice> dev = devices.Get(i);
        std::string nodeName = Names::FindName(dev->GetNode());
        Names::Add(nodeName + "-link" + std::to_string(linkId), dev);
    }
}

void
CsmaLinkHandler(const json& jLink, ConfigJsonHelper& helper)
{
    // 必需字段
    uint32_t linkId = jLink.at("linkId").get<uint32_t>();
    CsmaHelper csma;

    /* ===============================
     * Queue (optional)
     * =============================== */
    if (jLink.contains("queue"))
    {
        const auto& jQueue = jLink.at("queue");
        for (auto it = jQueue.begin(); it != jQueue.end(); ++it)
        {
            const std::string& key = it.key();
            const auto& val = it.value();

            if (key == "type")
            {
                csma.SetQueue(val.get<std::string>());
            }
        }
    }

    /* ===============================
     * Channel attributes (optional)
     * =============================== */
    if (jLink.contains("channel"))
    {
        const auto& jChannel = jLink.at("channel");
        for (auto it = jChannel.begin(); it != jChannel.end(); ++it)
        {
            const std::string& key = it.key();
            const auto& val = it.value();

            if (key == "type")
                continue;

            if (key == "dataRate")
            {
                csma.SetChannelAttribute("DataRate",
                                         DataRateValue(DataRate(val.get<std::string>())));
            }
            else if (key == "delay")
            {
                csma.SetChannelAttribute("Delay", TimeValue(Time(val.get<std::string>())));
            }
        }
    }

    /* ===============================
     * Device attributes (optional)
     * =============================== */
    if (jLink.contains("device"))
    {
        const auto& jDevice = jLink.at("device");
        for (auto it = jDevice.begin(); it != jDevice.end(); ++it)
        {
            const std::string& key = it.key();
            const auto& val = it.value();

            if (key == "type")
                continue;

            if (key == "mtu")
            {
                csma.SetDeviceAttribute("Mtu", UintegerValue(val.get<uint32_t>()));
            }
        }
    }

    /* ===============================
     * Nodes (required)
     * =============================== */
    NodeContainer nodes;
    for (const auto& dev : jLink.at("netDevices"))
    {
        uint32_t nodeId = dev.at("nodeId").get<uint32_t>();
        nodes.Add(Names::Find<Node>("node" + std::to_string(nodeId)));
    }

    /* ===============================
     * Install
     * =============================== */
    NetDeviceContainer devices = csma.Install(nodes);

    /* ===============================
     * Name NetDevices
     * =============================== */
    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        Ptr<NetDevice> dev = devices.Get(i);
        std::string nodeName = Names::FindName(dev->GetNode());
        Names::Add(nodeName + "-link" + std::to_string(linkId), dev);
    }
}

void
InternetHandler(const json& jInternet, ConfigJsonHelper& helper)
{
    InternetStackHelper stack;

    /* ===============================
     * Global routing (optional)
     * =============================== */
    bool enableGlobalRouting = false;
    enableGlobalRouting = jInternet.at("enableGlobalRouting").get<bool>();

    if (enableGlobalRouting)
    {
        helper.enableGlobalRouting = true;
        stack.SetRoutingHelper(Ipv4GlobalRoutingHelper());
        stack.InstallAll();
        Simulator::ScheduleNow(&Ipv4GlobalRoutingHelper::PopulateRoutingTables);
        return;
    }

    /* ===============================
     * Per-node routing configuration
     * =============================== */
    const auto& jIpv4RoutingProtocols = helper.handleJson[JsonDomain::Ipv4RoutingProtocol];
    const auto& jIpv6RoutingProtocols = helper.handleJson[JsonDomain::Ipv6RoutingProtocol];

    std::set<uint32_t> allNodeIds;
    for (const auto& j : jIpv4RoutingProtocols)
        allNodeIds.insert(j.at("nodeId").get<uint32_t>());
    for (const auto& j : jIpv6RoutingProtocols)
        allNodeIds.insert(j.at("nodeId").get<uint32_t>());

    for (uint32_t nodeId : allNodeIds)
    {
        Ptr<Node> node = Names::Find<Node>("node" + std::to_string(nodeId));
        NS_ASSERT(node);

        helper.ipv4List = std::make_unique<Ipv4ListRoutingHelper>();
        helper.ipv6List = std::make_unique<Ipv6ListRoutingHelper>();

        /* ---------- IPv4 routing ---------- */
        for (const auto& j : jIpv4RoutingProtocols)
        {
            if (j.at("nodeId").get<uint32_t>() != nodeId)
                continue;

            for (const auto& routeConf : j.at("ipv4RoutingList"))
            {
                auto fn = helper.GetRegistry(JsonDomain::Ipv4RoutingProtocol,
                                             routeConf.at("type").get<std::string>());
                fn(routeConf);
            }
        }

        /* ---------- IPv6 routing ---------- */
        for (const auto& j : jIpv6RoutingProtocols)
        {
            if (j.at("nodeId").get<uint32_t>() != nodeId)
                continue;

            for (const auto& routeConf : j.at("ipv6RoutingList"))
            {
                auto fn = helper.GetRegistry(JsonDomain::Ipv6RoutingProtocol,
                                             routeConf.at("type").get<std::string>());
                fn(routeConf);
            }
        }

        /* ---------- Install stack ---------- */
        Ipv4ListRoutingHelper ipv4list = *helper.ipv4List;
        Ipv6ListRoutingHelper ipv6list = *helper.ipv6List;
        stack.SetRoutingHelper(ipv4list);
        stack.SetRoutingHelper(ipv6list);
        stack.Install(node);
        helper.ipv4List.reset(new Ipv4ListRoutingHelper());
        helper.ipv6List.reset(new Ipv6ListRoutingHelper());
    }
}

void
Ipv4NetworkHandler(const json& jNetwork, ConfigJsonHelper& helper)
{
    // ===== Required fields =====
    std::string subnet = jNetwork.at("subnet").get<std::string>();
    std::string mask = jNetwork.at("mask").get<std::string>();
    std::string base = jNetwork.at("base").get<std::string>();

    /* ===== Fixed IPv4 addresses (optional) ===== */
    if (jNetwork.contains("fixed"))
    {
        for (const auto& f : jNetwork.at("fixed"))
        {
            const auto& devId = f.at("netDeviceId");

            Ptr<Node> node =
                Names::Find<Node>("node" + std::to_string(devId.at("nodeId").get<uint32_t>()));
            Ptr<NetDevice> dev = Names::Find<NetDevice>(
                "node" + std::to_string(devId.at("nodeId").get<uint32_t>()) + "-link" +
                std::to_string(devId.at("linkId").get<uint32_t>()));

            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_ASSERT(ipv4);

            int32_t interface = ipv4->AddInterface(dev);

            Ipv4InterfaceAddress addr(Ipv4Address(f.at("ipv4Address").get<std::string>().c_str()),
                                      Ipv4Mask(mask.c_str()));

            ipv4->AddAddress(interface, addr);
            ipv4->SetUp(interface);
        }
    }

    /* ===== Automatic IPv4 assignment (optional) ===== */
    if (jNetwork.contains("netDeviceIds"))
    {
        Ipv4AddressHelper address;
        address.SetBase(subnet.c_str(), mask.c_str(), base.c_str());

        NetDeviceContainer devs;
        for (const auto& devId : jNetwork.at("netDeviceIds"))
        {
            Ptr<NetDevice> dev = Names::Find<NetDevice>(
                "node" + std::to_string(devId.at("nodeId").get<uint32_t>()) + "-link" +
                std::to_string(devId.at("linkId").get<uint32_t>()));
            devs.Add(dev);
        }

        address.Assign(devs);
    }
}

void
Ipv6NetworkHandler(const json& jNetwork, ConfigJsonHelper& helper)
{
    std::string subnet = jNetwork.at("subnet").get<std::string>();
    uint8_t prefixLen = jNetwork.at("prefixLength").get<uint8_t>();

    /* ===== Fixed IPv6 addresses (optional) ===== */
    if (jNetwork.contains("fixed"))
    {
        for (const auto& f : jNetwork.at("fixed"))
        {
            const auto& devId = f.at("netDeviceId");

            Ptr<Node> node =
                Names::Find<Node>("node" + std::to_string(devId.at("nodeId").get<uint32_t>()));
            Ptr<NetDevice> dev = Names::Find<NetDevice>(
                "node" + std::to_string(devId.at("nodeId").get<uint32_t>()) + "-link" +
                std::to_string(devId.at("linkId").get<uint32_t>()));

            Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
            NS_ASSERT(ipv6);

            int32_t interface = ipv6->AddInterface(dev);

            Ipv6InterfaceAddress addr(Ipv6Address(f.at("ipv6Address").get<std::string>().c_str()),
                                      prefixLen);

            ipv6->AddAddress(interface, addr);
            ipv6->SetUp(interface);
        }
    }

    /* ===== Automatic IPv6 assignment (optional) ===== */
    if (jNetwork.contains("netDeviceIds"))
    {
        Ipv6AddressHelper address;
        address.SetBase(Ipv6Address(subnet.c_str()), prefixLen);

        NetDeviceContainer devs;
        for (const auto& devId : jNetwork.at("netDeviceIds"))
        {
            Ptr<NetDevice> dev = Names::Find<NetDevice>(
                "node" + std::to_string(devId.at("nodeId").get<uint32_t>()) + "-link" +
                std::to_string(devId.at("linkId").get<uint32_t>()));
            devs.Add(dev);
        }

        address.Assign(devs);
    }
}

void
Ipv4StaticHandler(const json& jRouting, ConfigJsonHelper& helper)
{
    uint32_t nodeId = jRouting.at("nodeId").get<uint32_t>();
    Ptr<Node> node = Names::Find<Node>("node" + std::to_string(nodeId));
    NS_ASSERT(node);

    /* ===============================
     * Internet stage: register helper
     * =============================== */
    if (helper.status == JsonDomain::Internet)
    {
        Ipv4StaticRoutingHelper ipv4Static;
        helper.ipv4List->Add(ipv4Static, jRouting.at("priority").get<uint32_t>());
    }
    /* ===============================
     * Per-node routing configuration
     * =============================== */
    else if (helper.status == JsonDomain::Ipv4RoutingProtocol)
    {
        if (!jRouting.contains("routes"))
            return;

        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        NS_ASSERT(ipv4);

        Ipv4StaticRoutingHelper ipv4Static;
        Ptr<Ipv4StaticRouting> staticRouting = ipv4Static.GetStaticRouting(ipv4);
        for (const auto& route : jRouting.at("routes"))
        {
            const std::string network = route.at("ipv4Address").get<std::string>();
            const std::string mask = route.at("mask").get<std::string>();
            const std::string nextHop = route.at("nextHop").get<std::string>();
            uint32_t nextLinkId = route.at("nextLinkId").get<uint32_t>();
            Ptr<NetDevice> dev = Names::Find<NetDevice>("node" + std::to_string(nodeId) + "-link" +
                                                        std::to_string(nextLinkId));
            uint32_t nextIf = ipv4->GetInterfaceForDevice(dev);
            staticRouting->AddNetworkRouteTo(network.c_str(),
                                             mask.c_str(),
                                             nextHop.c_str(),
                                             nextIf);
        }
    }
}

void
Ipv6StaticHandler(const json& jRouting, ConfigJsonHelper& helper)
{
    uint32_t nodeId = jRouting.at("nodeId").get<uint32_t>();
    Ptr<Node> node = Names::Find<Node>("node" + std::to_string(nodeId));
    NS_ASSERT(node);

    /* ===============================
     * Internet stage: register helper
     * =============================== */
    if (helper.status == JsonDomain::Internet)
    {
        Ipv6StaticRoutingHelper ipv6Static;
        helper.ipv6List->Add(ipv6Static, jRouting.at("priority").get<uint32_t>());
    }
    /* ===============================
     * Per-node routing configuration
     * =============================== */
    else if (helper.status == JsonDomain::Ipv6RoutingProtocol)
    {
        if (!jRouting.contains("routes"))
            return;

        Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
        NS_ASSERT(ipv6);

        Ipv6StaticRoutingHelper ipv6Static;
        Ptr<Ipv6StaticRouting> staticRouting = ipv6Static.GetStaticRouting(ipv6);

        for (const auto& route : jRouting.at("routes"))
        {
            const std::string network = route.at("dest").get<std::string>();
            uint32_t prefixLength = route.at("prefixLength").get<uint32_t>();
            const std::string nextHop = route.at("nextHop").get<std::string>();
            uint32_t nextLinkId = route.at("nextLinkId").get<uint32_t>();

            Ptr<NetDevice> dev = Names::Find<NetDevice>("node" + std::to_string(nodeId) + "-link" +
                                                        std::to_string(nextLinkId));

            uint32_t nextIf = ipv6->GetInterfaceForDevice(dev);

            staticRouting->AddNetworkRouteTo(network.c_str(),
                                             prefixLength,
                                             nextHop.c_str(),
                                             nextIf);
        }
    }
}

void
OlsrHandler(const json& jRouting, ConfigJsonHelper& helper)
{
    uint32_t nodeId = jRouting.at("nodeId").get<uint32_t>();
    Ptr<Node> node = Names::Find<Node>("node" + std::to_string(nodeId));
    NS_ASSERT(node);

    /* ===============================
     * Internet stage: register helper
     * =============================== */
    if (helper.status == JsonDomain::Internet)
    {
        OlsrHelper olsr;

        for (auto it = jRouting.begin(); it != jRouting.end(); ++it)
        {
            const std::string& key = it.key();
            const auto& val = it.value();

            if (key == "helloInterval")
                olsr.Set("HelloInterval", TimeValue(Time(val.get<std::string>())));
            else if (key == "tcInterval")
                olsr.Set("TcInterval", TimeValue(Time(val.get<std::string>())));
            else if (key == "hnaInterval")
                olsr.Set("HnaInterval", TimeValue(Time(val.get<std::string>())));
            else if (key == "willingness")
                olsr.Set("Willingness", EnumValue(val.get<uint32_t>()));
        }

        helper.ipv4List->Add(olsr, jRouting.at("priority").get<uint32_t>());
    }
    /* ===============================
     * Per-node HNA configuration
     * =============================== */
    else if (helper.status == JsonDomain::Ipv4RoutingProtocol)
    {
        if (!jRouting.contains("hnaNetworks"))
            return;

        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        NS_ASSERT(ipv4);

        Ptr<olsr::RoutingProtocol> olsrRouting =
            Ipv4RoutingHelper::GetRouting<olsr::RoutingProtocol>(ipv4->GetRoutingProtocol());

        NS_ASSERT(olsrRouting);
        for (const auto& route : jRouting.at("hnaNetworks"))
        {
            const std::string network = route.at("network").get<std::string>();
            const std::string mask = route.at("mask").get<std::string>();

            olsrRouting->AddHostNetworkAssociation(network.c_str(), mask.c_str());
        }
    }
}

void
ConstantPositionMobilityHandler(const json& jMobility, ConfigJsonHelper& helper)
{
    uint32_t nodeId = jMobility.at("nodeId").get<uint32_t>();
    Ptr<Node> node = Names::Find<Node>("node" + std::to_string(nodeId));
    NS_ASSERT(node);

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    const auto& jPos = jMobility.at("position");
    for (auto it = jPos.begin(); it != jPos.end(); ++it)
    {
        const std::string& key = it.key();
        const auto& val = it.value();

        if (key == "x")
            x = val.get<double>();
        else if (key == "y")
            y = val.get<double>();
        else if (key == "z")
            z = val.get<double>();
    }
    MobilityHelper mob;
    mob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mob.Install(node);

    node->GetObject<MobilityModel>()->SetPosition(Vector(x, y, z));
}

void
WaypointMobilityHandler(const json& jMobility, ConfigJsonHelper& helper)
{
    uint32_t nodeId = jMobility.at("nodeId").get<uint32_t>();
    Ptr<Node> node = Names::Find<Node>("node" + std::to_string(nodeId));
    NS_ASSERT(node);

    /* ---------- 初始位置 ---------- */
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    const auto& jPos = jMobility.at("position");
    for (auto it = jPos.begin(); it != jPos.end(); ++it)
    {
        const std::string& key = it.key();
        const auto& val = it.value();

        if (key == "x")
            x = val.get<double>();
        else if (key == "y")
            y = val.get<double>();
        else if (key == "z")
            z = val.get<double>();
    }

    MobilityHelper mob;
    mob.SetMobilityModel("ns3::WaypointMobilityModel");
    mob.Install(node);

    node->GetObject<MobilityModel>()->SetPosition(Vector(x, y, z));

    /* ---------- Waypoints ---------- */
    Ptr<WaypointMobilityModel> wpm = node->GetObject<WaypointMobilityModel>();
    NS_ASSERT(wpm);

    const auto& waypoints = jMobility.at("waypoints");
    for (const auto& p : waypoints)
    {
        double t = 0.0;
        double wx = 0.0;
        double wy = 0.0;
        double wz = 0.0;

        for (auto it = p.begin(); it != p.end(); ++it)
        {
            const std::string& key = it.key();
            const auto& val = it.value();

            if (key == "time")
                t = val.get<double>();
            else if (key == "x")
                wx = val.get<double>();
            else if (key == "y")
                wy = val.get<double>();
            else if (key == "z")
                wz = val.get<double>();
        }

        wpm->AddWaypoint(Waypoint(Seconds(t), Vector(wx, wy, wz)));
    }
}

void
UdpEchoClientHandler(const json& jApplication, ConfigJsonHelper& helper)
{
    uint32_t nodeId = jApplication.at("nodeId").get<uint32_t>();
    Ptr<Node> node = Names::Find<Node>("node" + std::to_string(nodeId));
    NS_ASSERT(node);

    std::string start;
    std::string stop;

    for (auto it = jApplication.begin(); it != jApplication.end(); ++it)
    {
        if (it.key() == "startTime")
            start = it.value().get<std::string>();
        else if (it.key() == "stopTime")
            stop = it.value().get<std::string>();
    }

    const auto& jSocket = jApplication.at("socket");

    std::string socketType;
    uint16_t port = 0;
    uint32_t remoteNodeId = 0;
    bool hasLinkId = false;
    uint32_t linkId = 0;

    for (auto it = jSocket.begin(); it != jSocket.end(); ++it)
    {
        const std::string& key = it.key();
        const auto& val = it.value();

        if (key == "type")
            socketType = val.get<std::string>();
        else if (key == "port")
            port = val.get<uint16_t>();
        else if (key == "netDeviceId")
        {
            for (auto nit = val.begin(); nit != val.end(); ++nit)
            {
                if (nit.key() == "nodeId")
                    remoteNodeId = nit.value().get<uint32_t>();
                else if (nit.key() == "linkId")
                {
                    hasLinkId = true;
                    linkId = nit.value().get<uint32_t>();
                }
            }
        }
    }

    uint32_t maxPackets = 0;
    std::string interval;
    uint32_t packetSize = 0;

    for (auto it = jApplication.begin(); it != jApplication.end(); ++it)
    {
        const std::string& key = it.key();
        const auto& val = it.value();

        if (key == "maxPackets")
            maxPackets = val.get<uint32_t>();
        else if (key == "interval")
            interval = val.get<std::string>();
        else if (key == "packetSize")
            packetSize = val.get<uint32_t>();
    }

    Ptr<Application> app;

    Ptr<Node> remote = Names::Find<Node>("node" + std::to_string(remoteNodeId));
    NS_ASSERT(remote);

    if (socketType == "ipv4")
    {
        Ptr<Ipv4> ipv4 = remote->GetObject<Ipv4>();
        int32_t iface = 1;

        if (hasLinkId)
        {
            Ptr<NetDevice> dev = Names::Find<NetDevice>("node" + std::to_string(remoteNodeId) +
                                                        "-link" + std::to_string(linkId));
            iface = ipv4->GetInterfaceForDevice(dev);
        }

        Ipv4Address dst = ipv4->GetAddress(iface, 0).GetLocal();
        UdpEchoClientHelper client(InetSocketAddress(dst, port));
        client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        client.SetAttribute("Interval", TimeValue(Time(interval)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        app = client.Install(node).Get(0);
    }
    else if (socketType == "ipv6")
    {
        Ptr<Ipv6> ipv6 = remote->GetObject<Ipv6>();
        int32_t iface = 1;

        if (hasLinkId)
        {
            Ptr<NetDevice> dev = Names::Find<NetDevice>("node" + std::to_string(remoteNodeId) +
                                                        "-link" + std::to_string(linkId));
            iface = ipv6->GetInterfaceForDevice(dev);
        }

        Ipv6Address dst = ipv6->GetAddress(iface, 0).GetAddress();
        UdpEchoClientHelper client(Inet6SocketAddress(dst, port));
        client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        client.SetAttribute("Interval", TimeValue(Time(interval)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        app = client.Install(node).Get(0);
    }

    app->SetStartTime(Time(start));
    app->SetStopTime(Time(stop));
    uint32_t appId = jApplication.at("applicationId").get<uint32_t>();
    Names::Add("node" + std::to_string(nodeId) + "-app" + std::to_string(appId), app);
}

void
UdpEchoServerHandler(const json& jApplication, ConfigJsonHelper& helper)
{
    uint32_t nodeId = jApplication.at("nodeId").get<uint32_t>();
    Ptr<Node> node = Names::Find<Node>("node" + std::to_string(nodeId));
    NS_ASSERT(node);

    std::string start;
    std::string stop;
    uint16_t port = 0;

    for (auto it = jApplication.begin(); it != jApplication.end(); ++it)
    {
        if (it.key() == "startTime")
            start = it.value().get<std::string>();
        else if (it.key() == "stopTime")
            stop = it.value().get<std::string>();
        else if (it.key() == "socket")
        {
            for (auto sit = it.value().begin(); sit != it.value().end(); ++sit)
            {
                if (sit.key() == "port")
                    port = sit.value().get<uint16_t>();
            }
        }
    }

    UdpEchoServerHelper server(port);
    Ptr<Application> app = server.Install(node).Get(0);
    app->SetStartTime(Time(start));
    app->SetStopTime(Time(stop));
    uint32_t appId = jApplication.at("applicationId").get<uint32_t>();
    Names::Add("node" + std::to_string(nodeId) + "-app" + std::to_string(appId), app);
}

void
OnOffHandler(const json& jApplication, ConfigJsonHelper& helper)
{
    uint32_t nodeId = jApplication.at("nodeId").get<uint32_t>();
    Ptr<Node> node = Names::Find<Node>("node" + std::to_string(nodeId));
    NS_ASSERT(node);

    std::string start;
    std::string stop;
    std::string dataRate;
    uint32_t packetSize = 0;

    for (auto it = jApplication.begin(); it != jApplication.end(); ++it)
    {
        const std::string& key = it.key();
        const auto& val = it.value();

        if (key == "startTime")
            start = val.get<std::string>();
        else if (key == "stopTime")
            stop = val.get<std::string>();
        else if (key == "dataRate")
            dataRate = val.get<std::string>();
        else if (key == "packetSize")
            packetSize = val.get<uint32_t>();
    }

    const auto& jSocket = jApplication.at("socket");

    std::string socketType;
    uint16_t port = 0;
    uint32_t remoteNodeId = 0;
    bool hasLinkId = false;
    uint32_t linkId = 0;

    for (auto it = jSocket.begin(); it != jSocket.end(); ++it)
    {
        const std::string& key = it.key();
        const auto& val = it.value();

        if (key == "type")
            socketType = val.get<std::string>();
        else if (key == "port")
            port = val.get<uint16_t>();
        else if (key == "netDeviceId")
        {
            for (auto nit = val.begin(); nit != val.end(); ++nit)
            {
                if (nit.key() == "nodeId")
                    remoteNodeId = nit.value().get<uint32_t>();
                else if (nit.key() == "linkId")
                {
                    hasLinkId = true;
                    linkId = nit.value().get<uint32_t>();
                }
            }
        }
    }

    Ptr<Application> app;
    Ptr<Node> remote = Names::Find<Node>("node" + std::to_string(remoteNodeId));
    NS_ASSERT(remote);

    if (socketType == "ipv4")
    {
        Ptr<Ipv4> ipv4 = remote->GetObject<Ipv4>();
        int32_t iface = 1;

        if (hasLinkId)
        {
            Ptr<NetDevice> dev = Names::Find<NetDevice>("node" + std::to_string(remoteNodeId) +
                                                        "-link" + std::to_string(linkId));
            iface = ipv4->GetInterfaceForDevice(dev);
        }

        Ipv4Address dst = ipv4->GetAddress(iface, 0).GetLocal();
        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(dst, port));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
        onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
        app = onoff.Install(node).Get(0);
    }
    else if (socketType == "ipv6")
    {
        Ptr<Ipv6> ipv6 = remote->GetObject<Ipv6>();
        int32_t iface = 1;

        if (hasLinkId)
        {
            Ptr<NetDevice> dev = Names::Find<NetDevice>("node" + std::to_string(remoteNodeId) +
                                                        "-link" + std::to_string(linkId));
            iface = ipv6->GetInterfaceForDevice(dev);
        }

        Ipv6Address dst = ipv6->GetAddress(iface, 0).GetAddress();
        OnOffHelper onoff("ns3::UdpSocketFactory", Inet6SocketAddress(dst, port));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
        onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
        app = onoff.Install(node).Get(0);
    }

    app->SetStartTime(Time(start));
    app->SetStopTime(Time(stop));
    uint32_t appId = jApplication.at("applicationId").get<uint32_t>();
    Names::Add("node" + std::to_string(nodeId) + "-app" + std::to_string(appId), app);
}

void
PacketSinkHandler(const json& jApplication, ConfigJsonHelper& helper)
{
    uint32_t nodeId = jApplication.at("nodeId").get<uint32_t>();
    Ptr<Node> node = Names::Find<Node>("node" + std::to_string(nodeId));
    NS_ASSERT(node);

    std::string start;
    std::string stop;
    std::string protocol;

    for (auto it = jApplication.begin(); it != jApplication.end(); ++it)
    {
        const std::string& key = it.key();
        const auto& val = it.value();

        if (key == "startTime")
            start = val.get<std::string>();
        else if (key == "stopTime")
            stop = val.get<std::string>();
        else if (key == "protocol")
            protocol = val.get<std::string>();
    }

    const auto& jSocket = jApplication.at("socket");

    std::string socketType;
    uint16_t port = 0;

    for (auto it = jSocket.begin(); it != jSocket.end(); ++it)
    {
        const std::string& key = it.key();
        const auto& val = it.value();

        if (key == "type")
            socketType = val.get<std::string>();
        else if (key == "port")
            port = val.get<uint16_t>();
    }

    Ptr<Application> app;

    if (socketType == "ipv4")
    {
        PacketSinkHelper sink(protocol, InetSocketAddress(Ipv4Address::GetAny(), port));
        app = sink.Install(node).Get(0);
    }
    else if (socketType == "ipv6")
    {
        PacketSinkHelper sink(protocol, Inet6SocketAddress(Ipv6Address::GetAny(), port));
        app = sink.Install(node).Get(0);
    }

    app->SetStartTime(Time(start));
    app->SetStopTime(Time(stop));
    uint32_t appId = jApplication.at("applicationId").get<uint32_t>();
    Names::Add("node" + std::to_string(nodeId) + "-app" + std::to_string(appId), app);
}

LogLevel
ParseLogLevel(const std::string& s)
{
    static const std::unordered_map<std::string, LogLevel> table = {
        {"LOG_NONE", LOG_NONE},

        {"LOG_ERROR", LOG_ERROR},
        {"LOG_LEVEL_ERROR", LOG_LEVEL_ERROR},

        {"LOG_WARN", LOG_WARN},
        {"LOG_LEVEL_WARN", LOG_LEVEL_WARN},

        {"LOG_DEBUG", LOG_DEBUG},
        {"LOG_LEVEL_DEBUG", LOG_LEVEL_DEBUG},

        {"LOG_INFO", LOG_INFO},
        {"LOG_LEVEL_INFO", LOG_LEVEL_INFO},

        {"LOG_FUNCTION", LOG_FUNCTION},
        {"LOG_LEVEL_FUNCTION", LOG_LEVEL_FUNCTION},

        {"LOG_LOGIC", LOG_LOGIC},
        {"LOG_LEVEL_LOGIC", LOG_LEVEL_LOGIC},

        {"LOG_ALL", LOG_ALL},
        {"LOG_LEVEL_ALL", LOG_LEVEL_ALL},

        {"LOG_PREFIX_FUNC", LOG_PREFIX_FUNC},
        {"LOG_PREFIX_TIME", LOG_PREFIX_TIME},
        {"LOG_PREFIX_NODE", LOG_PREFIX_NODE},
        {"LOG_PREFIX_LEVEL", LOG_PREFIX_LEVEL},
        {"LOG_PREFIX_ALL", LOG_PREFIX_ALL},
    };

    auto it = table.find(s);
    if (it == table.end())
    {
        throw std::invalid_argument("Unknown LogLevel: " + s);
    }
    return it->second;
}

void
DefaultSink(Ptr<PcapFileWrapper> file, Ptr<const Packet> packet)
{
    if (!file || !packet)
    {
        return;
    }
    file->Write(Simulator::Now(), packet);
}

void
WifiPcapSink(Ptr<PcapFileWrapper> file,
             Ptr<const Packet> packet,
             uint16_t channelFreqMhz,
             WifiTxVector txVector,
             MpduInfo mpduInfo,
             SignalNoiseDbm signalNoise,
             uint16_t staId)
{
    if (!file || !packet)
    {
        return;
    }
    file->Write(Simulator::Now(), packet);
}

void
EnablePcapAuto(const std::string& prefix, uint32_t linkId, bool promiscuous = false)
{
    PcapHelper pcapHelper;

    Ptr<Channel> channel = Names::Find<Channel>("link" + std::to_string(linkId) + "-channel");
    NS_ASSERT_MSG(channel, "Channel not found: channel" << linkId);

    uint32_t nDev = channel->GetNDevices();
    for (uint32_t i = 0; i < nDev; ++i)
    {
        Ptr<NetDevice> dev = channel->GetDevice(i);
        Ptr<Node> node = dev->GetNode();
        uint32_t nodeId = node->GetId();

        std::string filename =
            prefix + "-link" + std::to_string(linkId) + "-node" + std::to_string(nodeId) + ".pcap";

        // === CSMA ===
        if (dev->GetObject<CsmaNetDevice>())
        {
            Ptr<PcapFileWrapper> file =
                pcapHelper.CreateFile(filename, std::ios::out, PcapHelper::DLT_EN10MB);

            std::string traceName = promiscuous ? "PromiscSniffer" : "Sniffer";
            dev->TraceConnectWithoutContext(traceName, MakeBoundCallback(&DefaultSink, file));
            continue;
        }

        // === P2P ===
        if (dev->GetObject<PointToPointNetDevice>())
        {
            Ptr<PcapFileWrapper> file =
                pcapHelper.CreateFile(filename, std::ios::out, PcapHelper::DLT_PPP);

            dev->TraceConnectWithoutContext("PromiscSniffer",
                                            MakeBoundCallback(&DefaultSink, file));
            continue;
        }

        // === Wi-Fi ===
        Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice>();

        Ptr<WifiPhy> phy = nullptr;
        if (wifi)
            phy = wifi->GetPhy();

        if (phy)
        {
            Ptr<PcapFileWrapper> file =
                pcapHelper.CreateFile(filename, std::ios::out, PcapHelper::DLT_IEEE802_11);

            phy->TraceConnectWithoutContext("MonitorSnifferRx",
                                            MakeBoundCallback(&WifiPcapSink, file));
            continue;
        }
    }
}

void
SimulatorHandler(const json& jSimulator, ConfigJsonHelper& helper)
{
    const std::string simName = jSimulator.at("simName").get<std::string>();
    const Time duration = Time(jSimulator.at("duration").get<std::string>());

    Simulator::Stop(duration);

    for (auto it = jSimulator.begin(); it != jSimulator.end(); ++it)
    {
        const std::string& key = it.key();
        const auto& val = it.value();

        // ===== RNG =====
        if (key == "seed")
        {
            RngSeedManager::SetSeed(val.get<uint32_t>());
        }
        else if (key == "run")
        {
            RngSeedManager::SetRun(val.get<uint64_t>());
        }

        // ===== LOG =====
        else if (key == "log")
        {
            NS_ASSERT(val.is_array());
            for (const auto& item : val)
            {
                const std::string component = item.at("component").get<std::string>();
                const std::string level = item.at("level").get<std::string>();
                LogComponentEnable(component, ParseLogLevel(level));
            }
        }

        // ===== PCAP =====
        else if (key == "pcapLinkId")
        {
            NS_ASSERT(val.is_array());
            for (const auto& linkId : val)
            {
                EnablePcapAuto(simName, linkId.get<uint32_t>());
            }
        }

        // ===== FLOW MONITOR =====
        else if (key == "flowMonitorTimes")
        {
            NS_ASSERT(val.is_array());

            auto flow = new (
                FlowMonitorHelper); // 由于FlowMonitorHelper的析构会同时析构monitor和classifier，必须保障其生存期

            Ptr<FlowMonitor> monitor = flow->InstallAll();

            Ptr<FlowClassifier> classifier = flow->GetClassifier();

            for (const auto& t : val)
            {
                Time printTime = Time(t.get<std::string>());
                if (printTime == duration)
                {
                    printTime -= NanoSeconds(100);
                }
                Simulator::Schedule(printTime, [flow, monitor, printTime]() {
                    if (!monitor)
                        return;

                    monitor->CheckForLostPackets();

                    Ptr<Ipv4FlowClassifier> classifier =
                        DynamicCast<Ipv4FlowClassifier>(flow->GetClassifier());
                    if (!classifier)
                        return;

                    const auto& stats = monitor->GetFlowStats();
                    std::cout << "=== FlowMonitor @" << printTime.GetSeconds() << "s ===\n";
                    std::cout << "检测到的流量数量: " << stats.size() << std::endl;

                    for (const auto& [flowId, stat] : stats)
                    {
                        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
                        std::cout << "Flow " << flowId << ": " << t.sourceAddress << ":"
                                  << t.sourcePort << " -> " << t.destinationAddress << ":"
                                  << t.destinationPort << " Tx: " << stat.txPackets
                                  << " Rx: " << stat.rxPackets << std::endl;
                    }
                });
            }
        }
    }
}
}
} // namespace ns3
