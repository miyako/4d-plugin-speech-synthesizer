4d-plugin-speech-synthesizer
============================

Native Speech Synthesiser API

### Platform

| carbon | cocoa | win32 | win64 |
|:------:|:-----:|:---------:|:---------:|
|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|

## Syntax

```
SAY (voice;options{;path})
```

Parameter|Type|Description
------------|------------|----
voice|TEXT|
options|TEXT|
path|TEXT|location to create ``aiff`` file on Mac

```
VOICES LIST (voices)
```

Parameter|Type|Description
------------|------------|----
voices|ARRAY TEXT|

On Windows 10, the Cortana voices are returned in addition to [SAPI](https://msdn.microsoft.com/en-us/library/ms723627(v=vs.85).aspx) voices.

<img width="595" alt="2017-06-12 23 09 12" src="https://user-images.githubusercontent.com/1725068/27037977-99b1c0c4-4fc4-11e7-9af8-81045c91dfd7.png">
