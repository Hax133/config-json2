/**
 * @file config-json-helper.h
 * @brief Helper to construct a ConfigJson object from configuration files.
 */

#ifndef CONFIG_JSON_HELPER_H
#define CONFIG_JSON_HELPER_H

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"

#include <boost/filesystem.hpp>
#include <memory>
#include <nlohmann/json.hpp>

namespace ns3
{
namespace configjson2
{
using json = nlohmann::json;
using HandlerFn = std::function<void(const nlohmann::json&)>;

// 决定分发粒度
enum class JsonDomain
{
    Config,
    Node,
    Link,
    Internet,
    Ipv4Network,
    Ipv6Network,
    Ipv4RoutingProtocol,
    Ipv6RoutingProtocol,
    Mobility,
    Application,
    Simulator
};

class ConfigJsonCore
{
  public:
    virtual void Install(boost::filesystem::path configPath) = 0;
    void Register(JsonDomain, std::string, HandlerFn);
    HandlerFn GetRegistry(JsonDomain domain, const std::string& type) const;

  protected:
    std::map<JsonDomain, std::map<std::string, HandlerFn>> m_registry;
};

class ConfigJsonHelper : public ConfigJsonCore
{
  public:
    static ConfigJsonHelper Default();
    void Install(boost::filesystem::path configPath) override;
    static json LoadJson(boost::filesystem::path path);
    // 必要变量存储，helper存储并维护，fn只读
    boost::filesystem::path configPath;
    std::map<JsonDomain, json> handleJson;
    // 常用变量存储，helper存储并维护，fn只读
    uint32_t currentNodeId = UINT32_MAX;
    uint32_t currentLinkId = UINT32_MAX;
    JsonDomain status = JsonDomain::Config; // 当前阶段
    // 额外变量存储，helper作存储，fn维护
    bool enableGlobalRouting = false;
    std::unique_ptr<Ipv4ListRoutingHelper> ipv4List;
    std::unique_ptr<Ipv6ListRoutingHelper> ipv6List;
};
} // namespace configjson2

} // namespace ns3

#endif // CONFIG_JSON_HELPER_H
