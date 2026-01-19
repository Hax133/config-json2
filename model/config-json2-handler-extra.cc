#include "config-json2-handler-extra.h"

namespace ns3
{
namespace configjson2
{
using json = nlohmann::json;

void
GazeboMobilityHandler(const json& jMobility, ConfigJsonHelper& helper)
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

    /* ---------- MobilityModel ---------- */
    MobilityHelper mob;
    mob.SetMobilityModel("ns3::WaypointMobilityModel");
    mob.Install(node);

    node->GetObject<MobilityModel>()->SetPosition(Vector(x, y, z));
}

} // namespace configjson2
} // namespace ns3