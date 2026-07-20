# IQ200 OS v7.1.1 SMALL PROGRESS X10THINK

Base: v7.1 RT Audio Stabilizer.

Changes:
- Reduced progress percent text from large TextSize(3) to TextSize(2).
- Progress percent color changed to LIGHTGREY.
- Footer/version labels updated to v7.1.1.
- RT Audio architecture unchanged: Core0 audio task, Core1 UI.

Purpose:
- Less visual weight on the progress percentage.
- Slightly lower TFT redraw cost.
- Keep stable WAV playback from v7.1.
