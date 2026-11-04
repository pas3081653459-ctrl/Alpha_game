# 三章高低地形改造

## 设计与实现

保留先前章节各自的平面轮廓与真实负空间，先为路线建立连续高度剖面，再以剖面切分生成可见地板与碰撞盒。不是仅摆放无法走上去的装饰高台，也不增加需要队友 AI 跳跃才能通过的硬性关卡门槛。

| 章节 | 路线节奏 | 高度 |
| --- | --- | --- |
| 灰渡码头 | 平坦入口 → 货运上坡 → 高位卸货台 → 下坡栈桥 → 低位泊船场 | 入口 0cm、最高 120cm、后场 40cm |
| 赤炉工坊 | 平坦入口 → 原料坡道 → 中层作业台 → 上升输送通道 → 炉心高台 | 入口 0cm、中层 100cm、炉心 220cm |
| 灯河水道 | 平坦入口 → 石阶缓坡 → 河岸平台 → 拱起桥面 → 两翼庭院 | 入口 0cm、河岸 60cm、桥顶 180cm、庭院 90cm |

- `AlphaLayoutRules::ElevationProfile` 保存每章 X/Z 剖面，坡度最高 0.4（约 21.8°）。三个出生点与 NPC 接近区仍为 0cm 平面。
- `SurfaceHeight` 为刷新、目标提示、地面技能表现和视觉辅助提供统一地面高度；数据没有每帧堆分配。
- `TerrainTiles` 将原平面地板并集在坡度变化点切开。`AlphaRouteGeometry` 用完全相同的旋转、尺寸、位置创建可见实例和物理盒，顶部平面与高度函数一致。
- 不保留重叠的隐藏平面地板；桥旁水域仍然是实际无地板的负空间，边界墙继续限制掉落。
- 路线边饰和挡土立面随高台升高；地面箭头、路线目标与 Boss 刷新高度已同步。Boss 地面预警按每个端点的地形高度 +10cm 放置，跨坡度变化点的长线段切分，避免预警埋入上坡或桥顶。普通敌人及可破坏物本来即使用共享高度函数。
- 原码头 30cm 小货台被完整连续的 120cm 路线平台替代。
- AI 仍使用现有 XY 可见图寻路，由 CharacterMovement 沿可行走坡面升降；没有将整套 AI 切换成 NavMesh，也不声称复杂跳台导航已完成。

## 验证

已执行：

```bash
python3 Scripts/run_level_layout_tests.py
python3 Scripts/run_level_layout_tests.py --sanitize
python3 Scripts/run_rules_tests.py
python3 Scripts/run_rules_tests.py --sanitize
python3 Scripts/run_presentation_tests.py
python3 Scripts/run_presentation_tests.py --sanitize
python3 Scripts/check_project.py
```

地形便携测试 8106 项通过（原 5440 项），覆盖高度连续、坡度上限、出生点安全、坡面插值一致、逐点无重复覆盖、负空间保持以及现有路线和敌人刷新安全。基础规则 222 项、表现规则 320 项及各自 UBSan 均通过。项目检查通过。以上不能代替 UE 碰撞、动画、材质和联网验证。

现有 UE 测试 `AlphaGame.LevelLayout.Collision.ConnectedRouteAndRealNegativeSpace` 已补充三章坡面/高台采样射线，校验实际命中高度等于规则高度；云环境未安装 UE，尚未执行。

Mac/Windows 实机重点：三人出生和 NPC 对话不受影响；从入口走至每章终点和返回；两名 AI 能跟随越过坡顶；坡面可攻击、翻滚、跳跃并落地；Boss 不埋地；桥边仍有阻挡；坡面接缝没有卡胶囊或明显悬浮。

## 范围限制

当前高低落差通过全宽连续坡面构建；没有上下重叠的多层空间、升降台、需要跳跃的断桥或悬崖绕行。这样能先让现有三人协作、敌人路线和剧情节点稳定运行。边缘装饰为无碰撞表现，不改变权威伤害判定。UE 实际物理走坡及三机 LAN 仍待验证。
