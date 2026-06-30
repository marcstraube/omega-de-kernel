# Hard reset on launch

When you launch a game — whether from NOR or the SD list, GBA or emulator — the
kernel can either **soft-reset** into it (the quick default) or perform a full
**hardware reset** first — a clean cold boot, as if you had just powered the
console on. A hard reset clears more of the machine state, which some games or
add-on setups prefer.

## The L button (stock behaviour)

Out of the box, a hard reset happens **only when you hold the L button** as the
game launches; a normal launch soft-resets. This is unchanged.

## Setting: make hard reset the default

On the **second settings page**, the **HARD RESET** row turns this into a
persistent choice that the **L** button then *inverts*:

| `hard_reset` | Launch without L | Launch holding **L** |
| --- | --- | --- |
| **Disabled** (default) | Soft reset (normal) | **Hard reset** |
| **Enabled** | **Hard reset** | Soft reset (normal) |

So with the setting **off** you get the stock behaviour (L forces a hard reset);
with it **on** every launch hard-resets, and holding **L** gives you a normal
launch for that one game.

- Press **A** on the row, use **LEFT/RIGHT** to pick the checkbox or the **SET**
  button, **A** on the checkbox to toggle Enabled/Disabled, then **SET** to save.
- The change takes effect on the **next launch**.

It can also be set on a PC through the settings file as `hard_reset = on` /
`off` (see `SETTINGS_FILE.md`).

## Credits and differences from SimpleLight

The launch-time reset toggle was ported from the
[SimpleLight DE fork](https://github.com/Sterophonick/omega-de-kernel)
(original author: Veikkos) — both Apache-2.0, like this kernel.

This version wires the toggle into our settings infrastructure: a tagged,
default-correcting store slot (so a value carried over from another fork or a
fresh card never enables it by accident) and the human-readable
`SETTINGS.TXT` layer, alongside this documentation.
