#include "ns3/config-json2-module.h"

using namespace ns3;
using ns3::configjson2::ConfigJsonHelper;
using ns3::configjson2::JsonDomain;
using json = nlohmann::json;
int
main(int argc, char* argv[])
{
    CommandLine cmd;
    std::string configPath = "contrib/config-json2/examples/json-example/config.json";
    cmd.AddValue("config", "Path to config.json", configPath);
    cmd.Parse(argc, argv);
    LogComponentEnable("ConfigJson2", LOG_LEVEL_DEBUG);
    // 1. 构建默认 ConfigJsonHelper
    ConfigJsonHelper configHelper = ConfigJsonHelper::Default();
    // 2. 手动注册额外的 handler
    configHelper.Register(JsonDomain::Mobility, "gazebo", [&configHelper](const json& j) {
        GazeboMobilityHandler(j, configHelper);
    });
    // 3. 执行安装
    configHelper.Install(configPath);
    // 4. 启动仿真
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
