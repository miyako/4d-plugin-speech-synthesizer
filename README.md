4d-plugin-speech-synthesizer
============================

Native Speech Synthesiser API

### Platform

| carbon | cocoa | win32 | win64 |
|:------:|:-----:|:---------:|:---------:|
|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|

## Syntax

```
SAY (text;options{;path})
```

Parameter|Type|Description
------------|------------|----
text|TEXT|text to speak
options|TEXT|options in JSON format; supported attributes are: ``rate``, ``volume``, ``voice``
path|TEXT|location to create file

```
VOICES LIST (voices)
```

If ``path``is specified, a ``WAV`` file is created on Windows. The format is hard coded, as shown below.

```c
WAVEFORMATEX format;
const GUID guid = SPDFID_WaveFormatEx;

//SPSF_16kHz16BitMono;
format.cbSize = 0;
format.nAvgBytesPerSec = 32000;
format.nBlockAlign = 2;
format.nChannels = 1;
format.nSamplesPerSec = 16000;
format.wBitsPerSample = 16;
format.wFormatTag =WAVE_FORMAT_PCM;
```

On Windows, if the text begins with a ``<`` it is parsed as XML. See [SPEAKFLAGS](https://msdn.microsoft.com/en-us/library/ee431843(v=vs.85).aspx) for details.

On Windows, [``rate``](https://msdn.microsoft.com/en-us/library/ms719798(v=vs.85).aspx) must be between ``-10`` and ``10``.

On Windows, [``volume``](https://msdn.microsoft.com/en-us/library/ms719811(v=vs.85).aspx) must be between ``0`` and ``100``.

On Windows, the speach is synchronous.

Parameter|Type|Description
------------|------------|----
voices|ARRAY TEXT|

On Windows 10, the Cortana voices are returned in addition to [SAPI](https://msdn.microsoft.com/en-us/library/ms723627(v=vs.85).aspx) voices.

<img width="595" alt="2017-06-12 23 09 12" src="https://user-images.githubusercontent.com/1725068/27037977-99b1c0c4-4fc4-11e7-9af8-81045c91dfd7.png">
