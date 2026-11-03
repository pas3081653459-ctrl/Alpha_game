# AlphaGame development

- Product: Windows, 1–3 player LAN cooperative 2.5D action game. Engine baseline: Unreal Engine 5.6, C++ and procedural art; engine/runtime gates remain mandatory.
- Continue the prioritized roadmap in `Docs/PROJECT_PLAN.md` autonomously when asked to develop. The user has authorized delegation: split independent work among role-specific agents, coordinate public interfaces, and review the integrated result. Do not leave a permission question between routine development stages.
- Every cloud task already has an isolated checkout. Use the current checkout; do not create extra worktrees unless requested. Preserve unrelated user work.
- Do not mark a stage accepted merely because source exists. Run `python3 Scripts/check_project.py`, `python3 Scripts/run_rules_tests.py`, and `python3 Scripts/run_rules_tests.py --sanitize` for relevant changes. For visual changes also run `python3 Scripts/run_presentation_tests.py` and `python3 Scripts/run_presentation_tests.py --sanitize`. Windows UHT/UBT, UE Automation, packaged rendering and real three-device LAN are separate required gates.
- If Unreal is unavailable, report the actual blocker and finish independent work. Do not create mock engine headers or treat portable tests as engine compilation. Do not claim background work continues after a turn ends.
- Keep targeting, damage, cooldowns, revival, phase transitions and resource rewards authoritative. Client RPCs submit intent, never damage values or arbitrary target transforms.
- Keep shared portable rules in `Source/AlphaGame/Rules`; test behavior in `Scripts/test_rules.cpp`. UE tests live in `Source/AlphaGame/Tests`. Update the Windows report validation when changing test discovery.
- Document behavior and limitations in `README.md`, `Docs/M1_PROGRESS.md`, and `Docs/VALIDATION.md`. Match explanations to the implementation, including generated-material fallback and unsupported mid-run reconnection.

- Test companions are now explicitly requested: default fill to three during preparation, keep human connection seats separate, use authoritative shared combat/rescue, and never change a live encounter population. Run `python3 Scripts/run_companion_tests.py` and `python3 Scripts/run_companion_tests.py --sanitize` for roster/AI rule changes. Preserve the distinction between test AI and complex navigation or guaranteed AI completion.
