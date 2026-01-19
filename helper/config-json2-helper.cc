#include "config-json2-helper.h"

#include "../model/config-json2-handler-default.h"

namespace ns3
{
namespace configjson2
{
NS_LOG_COMPONENT_DEFINE("ConfigJson2");

void
ConfigJsonCore::Register(JsonDomain domain, std::string type, HandlerFn function)
{
    m_registry[domain][type] = function;
}

HandlerFn
ConfigJsonCore::GetRegistry(JsonDomain domain, const std::string& type) const
{
    auto itDomain = m_registry.find(domain);
    if (itDomain == m_registry.end())
    {
        return nullptr;
    }

    auto itHandler = itDomain->second.find(type);
    if (itHandler == itDomain->second.end())
    {
        return nullptr;
    }

    return itHandler->second;
}

ConfigJsonHelper
ConfigJsonHelper::Default()
{
    ConfigJsonHelper configHelper;
    /* ---------- 占位函数 ---------- */
    auto noop = [](const json&) {};
    /* ---------- Config ---------- */
    configHelper.Register(JsonDomain::Config, "default", [&configHelper](const json& j) {
        ConfigHandler(j, configHelper);
    });
    /* ---------- Node ---------- */
    configHelper.Register(JsonDomain::Node, "default", [&configHelper](const json& j) {
        NodeHandler(j, configHelper);
    });
    configHelper.Register(JsonDomain::Node, "switch", [&configHelper](const json& j) {
        SwitchHandler(j, configHelper);
    });
    configHelper.Register(JsonDomain::Node, "gateway", noop);
    configHelper.Register(JsonDomain::Node, "router", noop);
    configHelper.Register(JsonDomain::Node, "terminal", noop);
    configHelper.Register(JsonDomain::Node, "adhoc", noop);

    /* ---------- Link ---------- */
    configHelper.Register(JsonDomain::Link, "wifi", [&configHelper](const json& j) {
        WifiLinkHandler(j, configHelper);
    });
    configHelper.Register(JsonDomain::Link, "p2p", [&configHelper](const json& j) {
        P2pLinkHandler(j, configHelper);
    });
    configHelper.Register(JsonDomain::Link, "csma", [&configHelper](const json& j) {
        CsmaLinkHandler(j, configHelper);
    });

    /* ---------- Internet ---------- */
    configHelper.Register(JsonDomain::Internet, "default", [&configHelper](const json& j) {
        InternetHandler(j, configHelper);
    });

    /* ---------- Network ---------- */
    configHelper.Register(JsonDomain::Ipv4Network, "default", [&configHelper](const json& j) {
        Ipv4NetworkHandler(j, configHelper);
    });
    configHelper.Register(JsonDomain::Ipv6Network, "default", [&configHelper](const json& j) {
        Ipv6NetworkHandler(j, configHelper);
    });

    /* ---------- Routing ---------- */
    configHelper.Register(JsonDomain::Ipv4RoutingProtocol,
                          "static",
                          [&configHelper](const json& j) { Ipv4StaticHandler(j, configHelper); });
    configHelper.Register(JsonDomain::Ipv6RoutingProtocol,
                          "static",
                          [&configHelper](const json& j) { Ipv6StaticHandler(j, configHelper); });
    configHelper.Register(JsonDomain::Ipv4RoutingProtocol, "olsr", [&configHelper](const json& j) {
        OlsrHandler(j, configHelper);
    });

    /* ---------- Mobility ---------- */
    configHelper.Register(
        JsonDomain::Mobility,
        "ConstantPositionMobilityModel",
        [&configHelper](const json& j) { ConstantPositionMobilityHandler(j, configHelper); });
    configHelper.Register(
        JsonDomain::Mobility,
        "WaypointMobilityModel",
        [&configHelper](const json& j) { WaypointMobilityHandler(j, configHelper); });

    /* ---------- Application ---------- */
    configHelper.Register(JsonDomain::Application, "UdpEchoClient", [&configHelper](const json& j) {
        UdpEchoClientHandler(j, configHelper);
    });
    configHelper.Register(JsonDomain::Application, "UdpEchoServer", [&configHelper](const json& j) {
        UdpEchoServerHandler(j, configHelper);
    });
    configHelper.Register(JsonDomain::Application, "OnOff", [&configHelper](const json& j) {
        OnOffHandler(j, configHelper);
    });
    configHelper.Register(JsonDomain::Application, "PacketSink", [&configHelper](const json& j) {
        PacketSinkHandler(j, configHelper);
    });

    /* ---------- Simulator ---------- */
    configHelper.Register(JsonDomain::Simulator, "default", [&configHelper](const json& j) {
        SimulatorHandler(j, configHelper);
    });
    return configHelper;
}

json
ConfigJsonHelper::LoadJson(boost::filesystem::path path)
{
    std::ifstream ifs(path.string());
    if (!ifs.is_open())
    {
        throw std::runtime_error("ConfigJsonHelper: cannot open config file: " + path.string());
    }
    json j;
    ifs >> j;
    return j;
}

void
ConfigJsonHelper::Install(boost::filesystem::path jsonPath)
{
    try
    {
        /* ===============================
         * 0. Load Json File
         * =============================== */
        NS_LOG_DEBUG("[0%] Install Stage 0/10: Loading Json File");
        status = JsonDomain::Config;
        configPath = jsonPath;
        handleJson[JsonDomain::Config] = LoadJson(configPath);
        m_registry.at(JsonDomain::Config).at("default")(handleJson[JsonDomain::Config]);

        /* ===============================
         * 1. Create Nodes
         * =============================== */
        status = JsonDomain::Node;
        NS_LOG_DEBUG("[10%] Install Stage 1/10: Create Nodes");
        for (const auto& jNode : handleJson[JsonDomain::Node])
        {
            currentNodeId = jNode.at("nodeId").get<uint32_t>();
            auto fn = GetRegistry(JsonDomain::Node, "default");
            fn(jNode);
        }

        /* ===============================
         * 2. Install Links
         * =============================== */
        status = JsonDomain::Link;
        NS_LOG_DEBUG("[20%] Install Stage 2/10: Install Links");
        for (const auto& jLink : handleJson[JsonDomain::Link])
        {
            std::string type = jLink.at("type").get<std::string>();
            currentLinkId = jLink.at("linkId").get<uint32_t>();
            auto fn = GetRegistry(JsonDomain::Link, type);
            fn(jLink);
        }
        /* ===============================
         * 3. Internet Stack
         * =============================== */
        status = JsonDomain::Internet;
        NS_LOG_DEBUG("[30%] Install Stage 3/10: Internet Stack");
        {
            auto fn = GetRegistry(JsonDomain::Internet, "default");
            fn(handleJson[JsonDomain::Internet]);
        }
        /* ===============================
         * 4. IPv4 / IPv6 Network
         * =============================== */
        status = JsonDomain::Ipv4Network;
        NS_LOG_DEBUG("[40%] Install Stage 4/10: IPv4 / IPv6 Network");
        for (const auto& j : handleJson[JsonDomain::Ipv4Network])
        {
            auto fn = GetRegistry(JsonDomain::Ipv4Network, "default");
            fn(j);
        }
        status = JsonDomain::Ipv6Network;
        for (const auto& j : handleJson[JsonDomain::Ipv6Network])
        {
            auto fn = GetRegistry(JsonDomain::Ipv6Network, "default");
            fn(j);
        }
        /* ===============================
         * 5. IPv4 / IPv6 Routing extra-config
         * =============================== */
        NS_LOG_DEBUG("[50%] Install Stage 5/10: IPv4 / IPv6 Routing (Extra Config)");
        if (!enableGlobalRouting)
        {
            status = (JsonDomain::Ipv4RoutingProtocol);
            for (const auto& jProto : handleJson[JsonDomain::Ipv4RoutingProtocol])
            {
                currentNodeId = jProto.at("nodeId").get<uint32_t>();
                for (const auto& jRouting : jProto.at("ipv4RoutingList"))
                {
                    auto fn = GetRegistry(JsonDomain::Ipv4RoutingProtocol,
                                          jRouting.at("type").get<std::string>());
                    fn(jRouting);
                }
            }
            status = (JsonDomain::Ipv6RoutingProtocol);
            for (const auto& jProto : handleJson[JsonDomain::Ipv6RoutingProtocol])
            {
                currentNodeId = jProto.at("nodeId").get<uint32_t>();
                for (const auto& jRouting : jProto.at("ipv6RoutingList"))
                {
                    auto fn = GetRegistry(JsonDomain::Ipv6RoutingProtocol,
                                          jRouting.at("type").get<std::string>());
                    fn(jRouting);
                }
            }
        }
        /* ===============================
         * 6. Node Roles
         * =============================== */
        status = JsonDomain::Node;
        NS_LOG_DEBUG("[60%] Install Stage 6/10: Node Roles");
        for (const auto& jNode : handleJson[JsonDomain::Node])
        {
            if (!jNode.contains("role"))
                continue;

            currentNodeId = jNode.at("nodeId").get<uint32_t>();
            auto fn = GetRegistry(JsonDomain::Node, jNode.at("role").get<std::string>());
            fn(jNode);
        }
        /* ===============================
         * 7. Mobility
         * =============================== */
        status = JsonDomain::Mobility;
        NS_LOG_DEBUG("[70%] Install Stage 7/10: Mobility");
        for (const auto& jMob : handleJson[JsonDomain::Mobility])
        {
            currentNodeId = jMob.at("nodeId").get<uint32_t>();
            auto fn = GetRegistry(JsonDomain::Mobility, jMob.at("type").get<std::string>());
            fn(jMob);
        }
        /* ===============================
         * 8. Application
         * =============================== */
        status = JsonDomain::Application;
        NS_LOG_DEBUG("[80%] Install Stage 8/10: Application");
        for (const auto& jApp : handleJson[JsonDomain::Application])
        {
            currentNodeId = jApp.at("nodeId").get<uint32_t>();
            auto fn = GetRegistry(JsonDomain::Application, jApp.at("type").get<std::string>());
            fn(jApp);
        }

        /* ===============================
         * 9. Simulator
         * =============================== */
        NS_LOG_DEBUG("[90%] Install Stage 9/10: Simulator");
        {
            status = JsonDomain::Simulator;
            auto fn = GetRegistry(JsonDomain::Simulator, "default");
            fn(handleJson[JsonDomain::Simulator]);
        }
        NS_LOG_DEBUG("[100%] Install Stage 10/10: Finish");
    }
    catch (const std::exception& e)
    {
        NS_FATAL_ERROR("ConfigJson Install failed: " << e.what());
    }
}
} // namespace configjson2
} // namespace ns3