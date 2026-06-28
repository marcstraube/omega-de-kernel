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

### First launch of a game

A brand-new game has no save yet, so the **first launch makes no backup** — there
is nothing to back up. The first backup appears on the **second launch**, once a
save exists.

## Restoring a backup

Using a PC / card reader, copy the wanted backup back over the live save:

```
/BACKUP/SAVER/<game>.savN   ->   /SAVER/<game>.sav
```

i.e. copy the chosen generation (`.sav0` = newest) and remove the trailing digit
so the filename matches the live save.
