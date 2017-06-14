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

### Windows

If ``path``is specified, a ``WAV`` file is created. The format is hard coded, as shown below.

```c
WAVEFORMATEX format;
const GUID guid = SPDFID_WaveFormatEx;

format.cbSize = 0;
format.nAvgBytesPerSec = 44100;
format.nBlockAlign = 2;
format.nChannels = 1;
format.nSamplesPerSec = 22050;
format.wBitsPerSample = 16;
format.wFormatTag =WAVE_FORMAT_PCM;
```

If the text begins with a ``<`` it is parsed as XML. See [SPEAKFLAGS](https://msdn.microsoft.com/en-us/library/ee431843(v=vs.85).aspx) for details.

[``rate``](https://msdn.microsoft.com/en-us/library/ms719798(v=vs.85).aspx) must be between ``-10`` and ``10``.

[``volume``](https://msdn.microsoft.com/en-us/library/ms719811(v=vs.85).aspx) must be between ``0`` and ``100``.

The speach is synchronous.

### macOS

If ``path``is specified, an ``AIFF`` file is created. 

The quality is the same as on Windows, namely, mono, 16 bit per sample, sample rate 22050. 

You can convert it to ``WAV`` with ``afconvert``.

```sh
/usr/bin/afconvert \
{srcPath} {dstPath} -d LEI16@16000 -f WAVE -l Mono
```

[``rate``](https://developer.apple.com/documentation/appkit/nsspeechsynthesizer/1448450-rate?language=objc) is the number of words per minute.

[``volume``](https://developer.apple.com/documentation/appkit/nsspeechsynthesizer/1448501-volume?language=objc) must be between ``0.0`` and ``1.0``.

Tthe speach is **a**synchronous.

Parameter|Type|Description
------------|------------|----
voices|ARRAY TEXT|

On Windows 10, the Cortana voices are returned in addition to [SAPI](https://msdn.microsoft.com/en-us/library/ms723627(v=vs.85).aspx) voices.

<img width="595" alt="2017-06-12 23 09 12" src="https://user-images.githubusercontent.com/1725068/27037977-99b1c0c4-4fc4-11e7-9af8-81045c91dfd7.png">
