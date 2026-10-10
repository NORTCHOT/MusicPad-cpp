# MusicPad C++ Daemon

Кроссплатформенный демон-сервер для удаленного управления медиаплеером и системной громкостью с Android-приложения.

## Архитектура

```
[Android клиент] <--- WebSocket (JSON) ---> [MusicPad C++ Daemon]
                                                    |
                       +----------------------------+----------------------------+
                       |                                                         |
             [WindowsMediaBackend]                                      [LinuxMediaBackend]
      - Windows.Media.Control (WinRT / GSMTC)                       - MPRIS2 over DBus
      - IAudioEndpointVolume (CoreAudio WASAPI)                     - PulseAudio / PipeWire
```

- **WebSocket Server:** Построен на `IXWebSocket` (с поддержкой `ws://0.0.0.0:PORT` для локальной сети).
- **Сериализация:** `nlohmann/json`.
- **Поддержка платформ:**
  - **Windows:** Нативная интеграция через WinRT C++/WinRT (`Windows.Media.Control` для метаданных треков, обложек и медиа-команд; WASAPI `IAudioEndpointVolume` с мгновенными уведомлениями `IAudioEndpointVolumeCallback` при изменении громкости).
  - **Linux:** Скелет и заготовка `LinuxMediaBackend` под MPRIS2 и PulseAudio.
  - **Mock:** `MockMediaBackend` с генерацией тестовых треков, симуляцией воспроизведения и обложек для отладки.

## Сборка

### Требования
- CMake 3.20+
- Компилятор с поддержкой C++20 (GCC 13+, Clang 16+, MSVC 2022)
- На Windows (MinGW/UCRT64): пакет `mingw-w64-ucrt-x86_64-cppwinrt`

### Команды сборки
```bash
cmake -B build -G Ninja
cmake --build build
```

Исполняемый файл: `build/musicpad_daemon.exe`

## Запуск

### Обычный режим (системный плеер Windows)
```bash
./build/musicpad_daemon.exe
```

При запуске в консоль выводятся все доступные локальные IP-адреса для ввода в Android-приложении:
```
Connect from your phone using one of these addresses:
  -> ws://192.168.1.100:51234
  -> ws://127.0.0.1:51234
```

### Режим симуляции (Mock)
Для тестирования без запущенных плееров:
```bash
./build/musicpad_daemon.exe --mock
```

### Смена порта
```bash
./build/musicpad_daemon.exe --port 55555
```

## Протокол WebSocket

Сервер слушает WebSocket на порту `51234`.

### Исходящие сообщения от сервера

#### 1. Статус трека (`now_playing`)
Отправляется сразу при подключении, при смене трека, паузе/воспроизведении, изменении позиции или громкости:
```json
{
  "type": "now_playing",
  "data": {
    "hasMedia": true,
    "title": "Starboy",
    "artist": "The Weeknd ft. Daft Punk",
    "album": "Starboy",
    "artwork": "data:image/jpeg;base64,...",
    "artworkHash": "13a150e319893bad",
    "positionMs": 45000,
    "durationMs": 230000,
    "isPlaying": true,
    "volume": 65,
    "isMuted": false,
    "source": "Spotify.exe",
    "capabilities": {
      "canPlayPause": true,
      "canNext": true,
      "canPrev": true,
      "canSeek": true,
      "canSetVolume": true
    }
  }
}
```
*Оптимизация трафика:* Поле `artwork` передается полным base64 только при подключении нового клиента, изменении обложки (`artworkHash`) или явном запросе `getArtwork`. На регулярных тиках воспроизведения поле `artwork` пустое, а `artworkHash` сохраняется, чтобы не нагружать Wi-Fi мегабайтами изображений каждую секунду.

### Входящие команды от клиента

- **Воспроизведение / Пауза:**
  - `{"cmd": "play"}`
  - `{"cmd": "pause"}`
  - `{"cmd": "toggle"}`
- **Переключение треков:**
  - `{"cmd": "next"}`
  - `{"cmd": "prev"}`
- **Перемотка (Seek):**
  - `{"cmd": "seek", "position": 60000}` *(в миллисекундах)*
- **Громкость:**
  - `{"cmd": "setVolume", "volume": 75}` *(0 - 100)*
  - `{"cmd": "setMute", "mute": true}`
- **Запрос статуса:**
  - `{"cmd": "getStatus"}`
- **Запрос обложки:**
  - `{"cmd": "getArtwork"}`
- **Проверка связи:**
  - `{"cmd": "ping"}` -> возвращает `{"type": "pong"}`
