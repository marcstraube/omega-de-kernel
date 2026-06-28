# Save backups

The kernel can keep rotating backups of your game saves, so a corrupted or
lost save can be recovered.

## How it works

- The backup is made **as a game starts**: whenever you start a game that
  **already has a save**, the kernel copies that save into `/BACKUP/SAVER/`
  *before* loading it. This snapshots the save as it was before this play
  session (corruption recovery). While the copy runs you briefly see
  **"Backing up save..."** on screen.
- Backups rotate per game: `<game>.sav0` is the newest, `<game>.sav1` is older,
  and so on up to the configured count.
- The live save itself stays in `/SAVER/<game>.sav` and is never modified by the
  backup — the files under `/BACKUP/SAVER/` are extra copies.
- `/BACKUP` and `/BACKUP/SAVER` are created automatically on the first backup.
- If a backup cannot be written (e.g. the SD card is full or failing), you see
  **"Backup failed!"** and your existing backups are kept unchanged — a failed
  backup never overwrites or rotates the good ones.

## Setting: number of generations

On the **second settings page**, the **BACKUP** row sets how many backups to
keep per game:

- `0` = off. No new backups are made. **Existing backups are left untouched.**
- `1`–`9` = keep that many generations. **Default is `4`.**
- Press **A** to step through the values (`0 → 1 → … → 9 → 0`), then **SET** to
  save.

### Lowering the count

If you lower the count (for example from `9` to `4`), the now-excess backups
(`.sav4` and higher) are removed the **next time that game is backed up**. Games
you do not launch again keep their extra backups until you next play them.

### A game with no save yet

A backup needs an existing save. The **very first time you play a brand-new
game there is nothing to back up** — the save is created during that session.
From then on, every start of that game backs it up. A game that already has a
save is backed up right on its next start.

## Restoring a backup

Using a PC / card reader, copy the wanted backup back over the live save:

```
/BACKUP/SAVER/<game>.savN   ->   /SAVER/<game>.sav
```

i.e. copy the chosen generation (`.sav0` = newest) and remove the trailing digit
so the filename matches the live save.

## Credits and differences from SimpleLight

The rotating save-backup concept and the original `Copy_file` /
`Backup_savefile` were ported from the
[SimpleLight DE fork](https://github.com/Sterophonick/omega-de-kernel)
(original author: Veikkos) — both Apache-2.0, like this kernel.

This version extends the original with:

- automatic creation of the `/BACKUP` parent directory (the original made no
  backups when `/BACKUP` did not already exist);
- a configurable number of generations (`0`–`9`, default `4`) instead of a
  fixed count;
- an **atomic** copy-to-temp-then-publish, so a failed backup (full or broken
  SD) never destroys existing backups (the original rotated *before* copying and
  ignored copy failures);
- a robust default that survives settings carried over from another fork;
- pruning of excess generations when the count is lowered;
- on-screen **"Backing up save..."** and **"Backup failed!"** feedback;
- bounds-safe path building, and this documentation.
