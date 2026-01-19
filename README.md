README.md
=========

config-json2 使用说明
=====================

1. 模块定位与设计目标
--------------------

config-json2 是一个面向 **大规模、参数化 ns-3 仿真配置** 的 JSON 驱动模块，目标并非替代 ns-3 的脚本能力，而是：

- 强制 **仅通过 JSON 使用 config-json2**，不向用户暴露 model 或中间状态对象  
- 最大限度 **复用 ns-3 原生 Helper（Wifi / Mobility / Internet / Application 等）**  
- 通过 **分层（JsonDomain）+ 解耦（HandlerFn 注册）** 避免硬编码流水线  
- 使“JSON 表达能力”直接决定“仿真配置粒度HandlerFn”，而非额外抽象层  

config-json2 本质上是一个 **ns-3 仿真脚本的结构化生成与调度框架**。

------------------------------------------------------------

2. 总体流水线（抽象）
--------------------

json  
→ config-json2/model::ConfigJsonCore（抽象）  
→ ns3::(wifi / mobility / internet / application) Helper  
→ ns3::Object::AggregateObject()  
→ ns3::Simulator::Schedule()

设计要点：
- config-json2 不保存“长期存在的 model 状态”
- 所有参数仅在 HandlerFn 执行时生效
- 安装完成后，仿真完全由 ns-3 原生对象接管

------------------------------------------------------------

3. HandlerFn 设计（核心）
------------------------

3.1 HandlerFn 抽象定义

HandlerFn 是所有 JSON 处理逻辑的唯一抽象：

using HandlerFn = std::function<bool(const nlohmann::json&)>;

其语义为：

- 输入：某一 JsonDomain 下、某一 type 的 JSON 对象
- 输出：是否成功完成本 Handler 的 ns-3 安装工作
- 副作用：
  - 创建 / 配置 ns-3 Helper
  - 调用 Helper::Install
  - 聚合 ns3::Object（AggregateObject）
  - 注册 Simulator 事件（Schedule / ScheduleNow）

------------------------------------------------------------

3.2 HandlerFn 的职责边界

一个 HandlerFn **只能** 做以下事情：

1) 从 JSON 中解析参数（强类型、强字段名）  
2) 将参数映射到 ns-3 Helper / Attribute / Value  
3) 调用 ns-3 Helper 的 Set / Install 接口  
4) 必要时使用 ns3::Names 绑定对象标识  

一个 HandlerFn **不能** 做以下事情：

- 跨 JsonDomain 修改或依赖其他 Handler 的内部状态  
- 保存 JSON 或中间参数供后续阶段使用  
- 假设 JSON 的“隐式默认结构”  

------------------------------------------------------------

3.3 HandlerFn 与 JSON 的强耦合原则

- HandlerFn 与 JSON 结构是 **一一对应关系**
- JSON 的键名、值类型、语义必须与 HandlerFn 完全一致
- 推荐流程：
  1) 先设计 JSON 格式
  2) 再为该格式实现 HandlerFn
- config-json2 不提供“自动推断 / 反射 / 宽松匹配”

------------------------------------------------------------

4. 分层设计（JsonDomain）
-------------------------

JsonDomain 用于描述 **ns-3 安装顺序上的逻辑阶段**：

enum class JsonDomain
{
    Config,
    Simulator,
    Node,
    Link,
    Internet,
    Ipv4Network,
    Ipv6Network,
    Ipv4RoutingProtocol,
    Ipv6RoutingProtocol,
    Mobility,
    Application
};

设计原则：

- JsonDomain 顺序 ≈ 典型 ns-3 脚本的执行顺序
- 每一层只允许访问：
  - 当前层 JSON
  - ns-3 已创建的对象（通过 Names / NodeContainer 等）
- 禁止“反向依赖”（例如 Application 层影响 Link 层）

------------------------------------------------------------

5. Handler 注册机制
-------------------

config-json2 使用注册表而非硬编码 switch：

std::map<
    JsonDomain,
    std::map<std::string, HandlerFn>
> m_registry;

注册规则：

- JsonDomain：决定安装阶段
- string（type）：决定同层不同处理策略
- HandlerFn：具体处理逻辑

示例：

Register(JsonDomain::Link, "wifi", HandleWifiLink);
Register(JsonDomain::Link, "p2p", HandleP2pLink);

------------------------------------------------------------

6. ConfigJsonCore
-----------------

ConfigJsonCore 是与 JSON 结构无关的抽象执行器：

virtual bool Install(boost::filesystem::path configPath) = 0;

职责：

- 按 JsonDomain 顺序推进安装流程
- 在每一层：
  - 遍历该层 JSON 对象
  - 读取 type 字段
  - 查表获取 HandlerFn
  - 调用 HandlerFn

ConfigJsonCore 不关心：
- JSON 如何拆分
- JSON 来自单文件还是多文件

------------------------------------------------------------

7. ConfigJsonHelper（基于 config.json 的实现）
----------------------------------------------

ConfigJsonHelper 是一种 **具体 JSON 组织方式的实现**。

假设：
- 存在一个统一入口 config.json
- config.json 仅负责分发子 JSON（nodes / links / mobility / ...）

HandleConfig 的职责：

- 解析 config.json
- 加载或拆分子 JSON
- 按 JsonDomain 调用对应 HandlerFn

------------------------------------------------------------

8. 默认 HandlerFn（官方模块）
-----------------------------

默认 HandlerFn 存放于：

config-json/model/config-json-handle-default/

其特点：

- 一一对应 ns-3 官方模块
- 不引入跨层状态
- 不隐藏 ns-3 的真实行为

------------------------------------------------------------

9. 扩展 HandlerFn（第三方模块）
-------------------------------

第三方模块或自定义协议应：

- 在 config-json/model/config-json-handle-extra/ 中实现 HandlerFn
- 通过 Register 显式注册

示例（GPSR）：

Register(JsonDomain::Ipv4RoutingProtocol, "gpsr", HandleGpsr);

JSON 示例：

{
  "type": "gpsr",
  "helloInterval": "1s",
  "neighborTimeout": "3s"
}

------------------------------------------------------------

10. 关键约定与设计取舍
----------------------

1) string → ns-3 类型  
   - 必须使用完整 TypeId 名称，如 "ns3::FriisPropagationLossModel"

2) PositionAllocator 取舍  
   - 不提供通用 PositionAllocator Handler
   - 位置逻辑由 Mobility Handler 全权负责

3) PropagationLossModel 参数配置  
   - Link Handler 只负责安装 Channel
   - 参数修改在后处理阶段完成

4) 对象命名（ns3::Names）

Node        : nodeX  
NetDevice   : nodeX-linkY  
Channel     : linkY-channel  

------------------------------------------------------------

11. 使用说明（examples）
------------------------

11.1 examples 目录结构

config-json2/examples/
├── json-example/
│   ├── config.json
│   ├── nodes.json
│   ├── links.json
│   ├── mobility.json
│   ├── applications.json
│   └── ...
└── config-json2-loader.cc

------------------------------------------------------------

11.2 config-json2-loader.cc 的使用方法
-------------------------------------

config-json2-loader.cc 是一个**最小可运行的 ns-3 仿真入口脚本**，用于演示
如何在 ns-3 中加载并执行一组 config-json2 描述的 JSON 配置。
该脚本直接放入 scratch 目录运行。

------------------------------------------------------------
方式一：使用默认 JSON 路径
在 ns-3 根目录下执行：
./ns3 run scratch/config-json2-loader.cc
此时脚本将使用源码中默认的：
contrib/config-json2/examples/json-example/config.json
------------------------------------------------------------

方式二：通过命令行指定 JSON 路径
当你有多组实验 JSON，或不希望修改脚本源码时，可使用命令行参数：
./ns3 run "scratch/config-json2-loader.cc -- \
    --configPath=/absolute/or/relative/path/to/config.json"

------------------------------------------------------------
该 loader 脚本的职责仅限于：

- 解析命令行参数
- 构建 ConfigJsonHelper
- 注册扩展 Handler
- 调用 Install 并启动 Simulator
- 有限的、不易整合进config-json2的HandlerFn函数的一些处理

特别注意命名空间的问题，由于要和老config-json模块公用类名（ConfigJsonHelper），额外需要指明命名空间成员