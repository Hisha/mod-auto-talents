AutoTalentsUI 0.5.0
===================

Optional client UI for mod-auto-talents on WoW 3.3.5a.

Install:
1. Copy the AutoTalentsUI folder into:
   World of Warcraft/Interface/AddOns/
2. Start/restart the client.
3. Make sure "Auto Talents UI" is enabled on the AddOns screen.

Use:
- Visit a normal class trainer. The addon adds "Auto Talent Build - Spec 1" and,
  after Dual Talent Specialization is unlocked, "Auto Talent Build - Spec 2".
- /atui 1 and /atui 2 can also open the editor directly for testing.
- Left-click talents in the exact order you want them learned while leveling.
- "Undo Last" removes the most recently planned point so the leveling order stays deterministic.
- A saved personal build must contain all 71 level-80 talent points.
- Saving costs whatever the server administrator configured in mod_auto_talents.conf.

The addon does not learn/reset talents itself. It only submits the ordered plan to the
server. mod-auto-talents validates, stores, charges for, assigns, and applies the build.
