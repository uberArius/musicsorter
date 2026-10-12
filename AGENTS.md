# MusicSorter — zasady projektu

- Natywny C++20, Qt Widgets/Multimedia/Concurrent. Nie dodawaj QML, web UI ani Electron.
- Oryginały muzyki są nietykalne. Sortowanie zawsze kopiuje, nigdy nie usuwa,
  nie przenosi i nie nadpisuje. Zachowaj atomowe zatwierdzanie kopii bez nadpisania.
- Core: geometria, stany, konfiguracja, skanowanie, kopiowanie i cache.
  music_audio: AudioRepository (pełne skompresowane pakiety + indeks w RAM),
  CompressedAudio/PacketDecoder (FFmpeg 7.1.2), AudioEngine (stały QAudioSink),
  AudioController i WaveformGenerator. Nie wracaj do otwierania pliku przez
  QMediaPlayer przy każdym hover. music_ui: okno, drzewo,
  siatka i ustawienia. Karty rysuje jeden QAbstractScrollArea, bez QWidget per utwór.
- I/O i dekodowanie w tle; wyniki muszą mieć kontrolę generacji i kontekstu QObject.
  Anulowanie przy zmianie katalogu/zamykaniu, 2–8 równoległych analiz plus miejsce
  dla pilnego odsłuchu. Najnowszy seek zastępuje poprzedni. Callback audio nie
  wykonuje I/O ani dekodowania. Cache RAM wyrzuca całe utwory, nigdy fragmenty.
- Angielski interfejs; QSaveFile do konfiguracji w APPDATA, cache w LOCALAPPDATA.
- Weryfikacja: `./build.ps1`, raporty `build/core-results.txt`, `build/ui-results.txt`,
  `build/audio-results.txt`, `build/audio-benchmark.json`. Testy seek porównują PCM
  również dla VBR i po usunięciu własnego pliku testowego. Benchmark obejmuje
  100 minutowych MP3; wynik software-first-PCM nie jest pomiarem akustycznym.
  Testy audio mogą wymagać uprawnień poza piaskownicą. Nie deklaruj odsłuchu,
  testu czystego systemu ani wszystkich DPI bez rzeczywistej weryfikacji.
- Pakowanie: `./package.ps1`; zależności i licencje są częścią dystrybucji.
  Przy zmianie Qt zaktualizuj wersje FFmpeg i metadane licencji.
