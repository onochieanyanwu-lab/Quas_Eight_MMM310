# Q8 Strip

Channel strip VST3 (JUCE 8): input trim, transformer + discrete preamp, high-pass,
3-band LC EQ derived from the Quad Eight MM-310 BL schematic, compressor, line amp, output trim.

## Build (same flow as your other plugin)
1. Push this folder to a GitHub repo.
2. Actions tab -> "Build VST3 (Windows)" -> download the `Q8Strip-VST3` artifact.
3. Copy `Q8 Strip.vst3` to `C:\Program Files\Common Files\VST3\`, rescan plugins in FL Studio.

## Notes
- EQ: centre frequencies, tuning capacitors and the A2 stage resistors are read from the MM-310 BL schematic.
  Inductor loss (Q 12/15/15) and the preamp/compressor are modelled, not read from a schematic.
- 2x oversampling around the nonlinear stages; reports ~latency to the host.
- Circuit reference: `Q8_circuit_reference.svg`.
