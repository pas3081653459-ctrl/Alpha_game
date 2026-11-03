# 《烬潮：归炉》 / AlphaGame

Windows 三人局域网 2.5D 合作动作闯关项目，使用 **Unreal Engine 5.6 / C++**。

当前是持续开发中的程序化美术原型源码。共享规则已实际编译测试；Unreal 工程尚未在本环境编译，不能据此称为可玩版本或已生成 EXE。

## 当前源码能力

- 有限纵深工坊战场：炉心、黄铜环、管道、青瓷柱梁、灯笼与远景塔楼；固定曝光与冷暖灯光。
- 按全队取景的平滑侧视镜头、轻量受击反馈与可关闭镜头震动。
- 默认补齐三人队伍的测试用 AI 同伴：跟随、攻击、技能、避险与救援；准备阶段可关闭。
- 沈砚、阿绫、闻灯三职业：不同基础属性、多部件人形轮廓和各三项 U/I/O 技能；配套衣甲、机关背包、灯环与武器。
- J 五段循环连招：正斩、反斩、上挑、突刺、下砸；支持空中连招、受限滞空、输入缓冲与受击取消。
- WASD 完整方向移动/转身；同方向双击后按住进入 2.5 倍奔跑，带风线特效；K 跳跃、L 朝面向翻滚。
- 服务端权威伤害、友伤过滤、闪避无敌、技能冷却与 U/I/O 能量。
- 能量初始 0：U 成功 +10，I 成功消耗 10，O 满 100 才可释放并消耗 100；普攻命中不加能量。
- 合作跟进仍可增加敌人架势伤害，但不再产生能量或自动强化普通技能。
- 持续按住 E 救援：距离、视线和独占目标检查；离距、受击、松手、死亡与断线中断；完成恢复 40% 生命。
- 准备选角 → 房主开局 → 三波敌人 → 双阶段铸炉 Boss → 胜败 → 重开。
- Boss 扇形横扫、直线炉火、落点重锤；基础网格地面预警与蓄力变色。
- 代码式 UMG 房间面板：创建、输入 IP 加入、选角、开局和重开；Tab 开关。
- 三人房主制、网络错误提示和返回本地准备场；控制台备用入口。
- 基于实际网格的斩击、聚怪、灯域、火星、破势和救援效果；暗青/黄铜 HUD。
- 可重复生成的原始细节/法线纹理，以及 UE 编辑器主材质生成脚本。

完整设计见 [GAME_DESIGN.md](Docs/GAME_DESIGN.md)，源码阶段记录见 [M1_PROGRESS.md](Docs/M1_PROGRESS.md)。

## 一个人测试三人合作

默认开启 **AI teammates**，一名真人补两名 AI、两名真人补一名 AI，三名真人不补位。AI 不占真人连接席位，并优先选择缺少的职业。

1. 完成下方 Unreal 构建后，启动一个独立游戏窗口即可，不需要创建局域网房间或再开客户端。
2. 在 Tab 房间面板确认 **AI teammates: ON**，选择职业，点击 **Start run**。
3. AI 会跟随、选择敌人、使用普通攻击和技能、尝试避开 Boss 预警，并救援倒地队友。头顶显示 AI 和当前行为。
4. Development 构建中，可在开局后输入控制台命令 `TestDownSelf` 测试 AI 救援。保护状态下需要等待后重试；该命令在 Shipping 不生效。
5. 需要纯单人对比时，重置回准备阶段，点击关闭补位，或执行 `SetBotFill 0`；`SetBotFill 1` 再次开启。

真人倒地而 AI 存活时不会立即判负；全队倒地才失败。真人可同样按 E 救援倒地的 AI。开局难度按真人与 AI 总人数计算，不是给单人额外赠送无敌队友。AI 使用相同伤害、冷却、占用与中断规则，不会瞬移、自行复活或绕过无敌。

当前为平坦场地的测试 AI，尚未验证实际移动/救援，也没有复杂导航或可靠通关保证；不能替代真实三机 LAN 测试。详细步骤见 [AI 同伴测试](Docs/COMPANION_TESTING.md)。

## Windows 准备与构建

1. 安装 Unreal Engine **5.6**，以及引擎要求的 Visual Studio 2022 C++ 工具链与 Windows SDK。
2. 克隆仓库。加入 `.uasset`、模型和声音资源前安装 Git LFS，并执行 `git lfs install`。
3. PowerShell 构建，EngineRoot 按本机安装位置填写：

```powershell
.\Scripts\Build-Windows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.6' -Action Build
```

4. 首次运行先生成美术材质（下节说明），再打开 `AlphaGame.uproject`，用独立窗口运行。默认 Entry 是引擎自带空地图，场地在游戏开始时生成，因此编辑视口中没有正式场景。

构建成功后也可以直接启动独立游戏：

```powershell
$Editor = 'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe'
& $Editor "$PWD\AlphaGame.uproject" /Engine/Maps/Entry -game -windowed -ResX=1280 -ResY=720 -log
```

## 生成美术材质

两张原创纹理已位于 `SourceArt/Surfaces/`。UE 主材质需要在引擎环境生成；Windows 一键执行：

```powershell
.\Scripts\Build-Windows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.6' -Action Art
```

该步骤先编译编辑器目标，再调用 `Scripts/create_art_assets.py` 导入纹理、创建参数材质，并核对本次保存报告和三个 `.uasset` 文件。`Package` 动作也会先执行该步骤。未在当前云环境运行。

插件已限定为编辑器用途：PythonScriptPlugin、EditorScriptingUtilities。只允许刷新带生成器标签的资产，遇到同名手工资源会停止。手工修改生成资源前应复制一份。修改后重启已经运行过游戏的编辑器，避免旧材质回退缓存。

Mac 上也可用 UE5.6 编辑器 Python 执行同一个 `Scripts/create_art_assets.py`，但这里的 `.ps1` 包装只适用于 Windows，Mac 路径未实测。

生成前，几何和颜色使用引擎默认材质回退，**没有完整金属度、粗糙度、法线与发光效果**。当前提交包含原始 PNG 和生成脚本，不包含声称已烘焙的 `.uasset`。详细说明见 [美术实现](Docs/ART_IMPLEMENTATION.md)。

## 单人或三人局域网流程

1. 启动后显示房间面板。单人可以直接选角并开始；联机房主先点 **Create LAN room**，这会重新加载准备场。
2. 房主确认本机局域网 IPv4，允许应用通过 Windows 专用网络防火墙；默认游戏端口为 UDP 7777。
3. 两名客户端填入房主 IP，例如 `192.168.1.10:7777`，点击 **Join address**。面板中不预填地址，以免把示例当成真实房主。
4. 各人选择职业；房主点击 **Start run**。客户端初次准备面板随开局关闭，之后可按 Tab 打开。
5. 消灭三波敌人和 Boss，或全队倒地失败；房主在面板中重置回准备阶段，再次开局。

战斗和间歇期间拒绝新加入，包括掉线重连。玩家需要等到准备或结算阶段再加入。人数缩放固定在开局，掉线不会让 Boss 回血。未实现房间自动发现、房主迁移或战斗状态恢复。

| 操作 | 键鼠 | 手柄 |
|---|---|---|
| 移动与面向 | WASD | 左摇杆 |
| 奔跑（2.5 倍） | 同方向双击，第二次按住 | 当前使用键盘双击入口 |
| 跳跃 | K | 下侧面键 |
| 五段循环普攻 | J | 左侧面键（上侧面键兼容同一连段） |
| 面向翻滚 | L | 右侧面键 |
| 常用技能：成功 +10 能量 | U | 右肩 |
| 小技能：消耗 10 能量 | I | 右扳机 |
| 终极：满能量消耗 100 | O | 左扳机 |
| 救援 | 持续按 E | 持续按左肩 |
| 房间面板 | Tab | Start |

面板目前以鼠标操作为主，完整手柄焦点导航与中文 UI 字体待补。W/S 同样会改变面向，斜向输入归一化；每段攻击动作期间保持该段起手朝向，避免武器画面与判定分离。完整规则见 [新操作说明](Docs/PLAYER_CONTROLS.md)。

控制台 `~` 备用命令：`HostGame`、`JoinGame 192.168.1.10:7777`、`SelectHero 0`（可选 0/1/2）、`StartRun`、`RestartRound`。`CameraShakeStrength 0` 可关闭镜头震动，范围 0～1。职业切换仅允许准备或结算；开始与重开仅房主/单人可执行。

## 验证与打包

当前云环境可运行：

```bash
python3 Scripts/check_project.py
python3 Scripts/run_rules_tests.py
python3 Scripts/run_rules_tests.py --sanitize
python3 Scripts/run_presentation_tests.py
python3 Scripts/run_presentation_tests.py --sanitize
python3 Scripts/run_companion_tests.py
python3 Scripts/run_companion_tests.py --sanitize
python3 Scripts/run_player_control_tests.py
python3 Scripts/run_player_control_tests.py --sanitize
```

- 工程一致性检查通过。
- 标准 C++17 战斗规则 **222/222 通过**、镜头/效果规则 **270/270 通过**，两套 UBSan 检查也通过；另有 AI 决策与技能体积规则 **784/784 通过**（含 UBSan），新增操作/能量规则 **1157/1157 通过**（含 UBSan）。
- 这些规则被服务端战斗、救援、技能、人数缩放和 Boss 命中实际调用。
- `.github/workflows/portable-checks.yml` 已配置同类 CI；本轮未查询 GitHub 托管运行结果。

Windows 引擎环境执行 UE 自动化与打包：

```powershell
.\Scripts\Build-Windows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.6' -Action Test
.\Scripts\Build-Windows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.6' -Action Package
```

目前有 **30 项 UE Automation 测试源码**，覆盖战斗、连招、空中取消、方向/奔跑、救援、能量、Boss、AI 同伴和队伍生命周期。脚本从源码提取完整测试名，要求本次报告中逐项成功，拒绝零测试、漏项、失败或未完成。报告位于 `Artifacts/Tests/`，Development 包位于 `Artifacts/Windows/`。

**Unreal 编译、这 30 项 UE 测试、PowerShell、渲染、打包和三台 Windows 设备联机均未执行。**当前 Linux 云机器没有 Unreal Editor/UnrealBuildTool，测试结果不能替代这些门槛。

## 已知范围

场景和角色已由基础几何组合为程序化美术，但仍没有正式手工模型、骨骼动画、声音、正式骨骼连段动画、局部顿帧或六关内容。技能效果与 Boss 预警均采用可保留于 Shipping 的实际网格，效果通过缩小与亮度衰减消失，不是透明材质或 Niagara。实际打包可见性待验收。沈砚格挡技能是短时保护与反击爆发原型，尚无成功防反识别。

当前敌人没有正式战术进攻名额，缺少存档、画质设置、正式大厅与版本匹配检查。不要用 Shipping 发布未验收内容。

## 项目管理

- [任务分工与优先级](Docs/PROJECT_PLAN.md)
- [源码阶段交付](Docs/M1_PROGRESS.md)
- [验收步骤](Docs/ACCEPTANCE.md)
- [验证证据与阻塞](Docs/VALIDATION.md)

按路线自动推进独立开发工作；Windows 编译和真实局域网是下一道外部验收门槛。各云任务使用现有隔离目录，无需额外创建 Git worktree。

## 新操作的难度基准

普通敌人生命55、架势35、单次伤害8、移速210；普通攻击前摇0.30秒、间隔0.85秒。Boss生命为单人360/双人540/三人720（包含AI），三种攻击伤害12/14/20，预警和恢复延长。空中连招停止输入、第五段结束、受击或其他动作取消后恢复下落，同次离地最多悬停3.2秒，不能无限续空。当前仍需 UE 实机确认手感与网络表现。
