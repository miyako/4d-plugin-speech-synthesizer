# Ownership regressions

These are ASan/UBSan tests for 4D native ownership in the plugin's commands and in `support/4DPlugin-JSON.cpp`. The tests build the real plugin sources and the full, unchanged bundled `4D Plugin API`. They use a synthetic `Call4D` engine that tracks every engine string, picture, handle and object.

No speech is produced:
- `PA_RunInMainProcess` is answered by the harness, which never runs `speech::say`.
- `NSSpeechSynthesizer` `+availableVoices` returns synthetic names.
- Starting to speak aborts the harness.

```sh
# fixed tree: every test must pass
tests/ownership/run.sh "Speech Synthesizer" fixed

# immutable baseline: defect/hardening tests must fail, controls must pass
mkdir /tmp/base && git archive <base-sha> | tar -x -C /tmp/base
tests/ownership/run.sh "/tmp/base/Speech Synthesizer" baseline
```

This only covers macOS. The Windows (SAPI) paths and the real 4D runtime are not exercised.
