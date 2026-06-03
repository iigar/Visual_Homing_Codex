# Raspberry Pi Backup And Restore

This document captures the current Pi-side backup and restore workflow.

The source code is stored in GitHub. The files under `artifacts/` are local runtime state and are intentionally not committed, so they must be backed up separately.

## What To Back Up

Current important Pi artifacts:

- `artifacts/visual_homing_live_route.vhrs`
- `artifacts/logs/test-core-pi-20260603T184605Z.log`
- `artifacts/route_keyframes/`
- `artifacts/active_camera_profile.txt`

These preserve the validated route, the successful live telemetry health dry-run log, route keyframes, and active camera profile selection.

## Create Backup On Pi

Run from the repository root on the Pi:

```bash
cd ~/Visual_Homing_Codex

mkdir -p ~/visual_homing_backups

tar -czf ~/visual_homing_backups/visual_homing_pi_backup_20260603T1847Z.tar.gz \
  artifacts/visual_homing_live_route.vhrs \
  artifacts/logs/test-core-pi-20260603T184605Z.log \
  artifacts/route_keyframes \
  artifacts/active_camera_profile.txt
```

Verify the backup:

```bash
ls -lh ~/visual_homing_backups/visual_homing_pi_backup_20260603T1847Z.tar.gz
tar -tzf ~/visual_homing_backups/visual_homing_pi_backup_20260603T1847Z.tar.gz | head -50
```

Expected archive contents include:

```text
artifacts/visual_homing_live_route.vhrs
artifacts/logs/test-core-pi-20260603T184605Z.log
artifacts/route_keyframes/
artifacts/route_keyframes/075.pgm
artifacts/route_keyframes/025.pgm
artifacts/route_keyframes/050.pgm
artifacts/route_keyframes/start.pgm
artifacts/route_keyframes/end.pgm
artifacts/active_camera_profile.txt
```

## Copy Backup To Windows

From Windows PowerShell:

```powershell
scp pi@jtzero:~/visual_homing_backups/visual_homing_pi_backup_20260603T1847Z.tar.gz D:\LLM\ChatGPT\Codex\Visual-Homing\
```

Adjust the destination path if needed.

## Restore On A Fresh Pi OS SD Card

Clone the repository:

```bash
cd ~
git clone https://github.com/iigar/Visual_Homing_Codex.git
cd ~/Visual_Homing_Codex
```

Install dependencies and validate the Pi build:

```bash
./scripts/bootstrap-pi.sh
```

This should install the required packages, configure/build the core, and run CTest.

Copy the backup archive back to the Pi, for example into:

```bash
mkdir -p ~/visual_homing_backups
```

Then restore artifacts:

```bash
cd ~/Visual_Homing_Codex
tar -xzf ~/visual_homing_backups/visual_homing_pi_backup_20260603T1847Z.tar.gz
```

Verify restored state:

```bash
ls -lh artifacts/visual_homing_live_route.vhrs
ls -lh artifacts/logs/test-core-pi-20260603T184605Z.log
cat artifacts/active_camera_profile.txt
```

Expected active profile:

```text
imx219-visible-wide
```

## Important Notes

`git pull` or `git clone` restores only code and documentation. It does not restore `artifacts/`.

The restored `VHRS` route remains useful only if the physical setup is still comparable: same camera, lens, mount angle, location, lighting, route direction, and approximate start/end positions. If any of those changed materially, record a new route before the next bench or ground test.

Live MAVLink command output remains blocked. The restored state includes a validated dry-run route and telemetry-health log, not permission to fly.
