# ETS2 G29 RPM LEDs

Native RPM/shift-light support for the Logitech G29 in Euro Truck Simulator 2.

[English](#english) · [Türkçe](#turkce)

---

<a id="english"></a>

## English

### About

ETS2 G29 RPM LEDs is a lightweight, in-process plugin built with the official
SCS Telemetry SDK. It reads the truck's live engine RPM and RPM limit, then
controls the five LEDs on a Logitech G29 directly through Windows HID.

No FanaLEDs, SimHub, companion application, background service, or Logitech
SDK is required. Logitech G HUB can remain installed and running for the wheel
driver, controls, and force feedback.

```text
ETS2 telemetry -> plugin DLL -> Windows HID -> Logitech G29 LEDs
```

### Features

- Single 64-bit DLL; no companion application
- Uses the truck's actual RPM limit instead of a fixed RPM value
- Five progressive RPM stages
- Limiter warning with flashing LEDs
- Automatic wheel reconnection attempts
- Three startup sweeps to confirm communication with the wheel
- Diagnostic messages in the standard ETS2 log
- Compatible with Logitech G HUB running in the background

### LED behavior

After the profile is selected and the plugin reaches the wheel, the LED bar
sweeps from green to red and back three times. ETS2 loads telemetry plugins
after profile selection, so the animation cannot run on the initial game menu.

During driving:

| Engine speed | LED behavior |
|---:|---|
| Below 55% of the RPM limit | Off |
| 55% | 1 LED |
| 64% | 2 LEDs |
| 73% | 3 LEDs |
| 82% | 4 LEDs |
| 90% | All 5 LEDs |
| 96% and above | All LEDs flash |

### Requirements

- Windows 10 or Windows 11, 64-bit
- 64-bit Euro Truck Simulator 2
- Logitech G29 in PC mode (`046D:C24F`)
- Logitech G HUB/driver configured for normal wheel and force-feedback use

This build targets the G29. The G920 and G923 use different USB protocols or
product IDs and are not currently supported.

### Installation

1. Exit Euro Truck Simulator 2 completely.
2. Download `ets2_g29_leds.dll` from the GitHub Releases section.
3. Open the ETS2 installation directory. In Steam, use **Library -> Euro Truck
   Simulator 2 -> Properties -> Installed Files -> Browse**.
4. Copy the DLL to:

   ```text
   Euro Truck Simulator 2\bin\win_x64\plugins\ets2_g29_leds.dll
   ```

5. Create the `plugins` directory if it does not exist.
6. Start ETS2 and accept the SCS SDK warning.
7. Select a profile. The three startup sweeps should begin when the plugin
   establishes communication with the G29.

The default Steam path is usually:

```text
C:\Program Files (x86)\Steam\steamapps\common\Euro Truck Simulator 2\bin\win_x64\plugins
```

### Troubleshooting

ETS2 writes plugin diagnostics to:

```text
Documents\Euro Truck Simulator 2\game.log.txt
```

Search for `G29 LED:` in that file.

| Log message | Meaning |
|---|---|
| `wheel detected; RPM LEDs enabled (report=...)` | The wheel was found and the first HID command succeeded. |
| `startup animation completed` | All three connection-test sweeps were sent successfully. |
| `wheel not found; will retry while driving` | Check the USB connection, G29 mode, power, and G HUB. |
| `output failed (report=..., WriteFile=..., HidD=...)` | Windows or the Logitech driver rejected both HID output methods. Include this line when reporting an issue. |
| `wheel reconnected` | The plugin recovered the connection after a previous failure. |

If the DLL does not appear in the log, confirm that it is inside
`bin\win_x64\plugins`, not the 32-bit `win_x86` directory.

### Building from source

The current build uses:

- Zig 0.17.0 for the Windows x86-64 C++ toolchain
- SCS Telemetry SDK 1.15 headers
- Windows system libraries `SetupAPI` and `HID`

The build script expects this layout:

```text
vendor\zig_toolchain\zig-x86_64-windows-0.17.0\zig.exe
vendor\scs_sdk\include\
src\g29_led_plugin.cpp
```

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

The resulting plugin is written to:

```text
dist\ets2_g29_leds.dll
```

The SCS SDK is distributed under its own license. Keep
`vendor/scs_sdk/sdk_license.txt` when redistributing its headers.

---

<a id="turkce"></a>

## Türkçe

### Proje hakkında

ETS2 G29 RPM LEDs, resmi SCS Telemetry SDK kullanılarak hazırlanmış hafif bir
oyun içi eklentidir. Kamyonun anlık motor devrini ve devir sınırını ETS2'den
okur, ardından Logitech G29 üzerindeki beş LED'i Windows HID üzerinden doğrudan
kontrol eder.

FanaLEDs, SimHub, yardımcı uygulama, arka plan servisi veya Logitech SDK
gerekmez. Direksiyon sürücüsü, kontroller ve force feedback için Logitech G HUB
kurulu ve açık kalabilir.

```text
ETS2 telemetrisi -> eklenti DLL'si -> Windows HID -> Logitech G29 LED'leri
```

### Özellikler

- Tek bir 64-bit DLL; yardımcı uygulama gerektirmez
- Sabit bir devir yerine kamyonun gerçek devir sınırını kullanır
- Beş kademeli devir göstergesi
- Devir kesiciye yaklaşınca yanıp sönen LED uyarısı
- Bağlantı koparsa direksiyona otomatik olarak yeniden bağlanmayı dener
- Direksiyon iletişimini doğrulayan üç başlangıç animasyonu
- Standart ETS2 log dosyasına tanılama mesajları yazar
- Logitech G HUB arka planda çalışırken kullanılabilir

### LED davranışı

Profil seçildikten ve eklenti direksiyona ulaştıktan sonra LED çubuğu yeşilden
kırmızıya ilerleyip geri döner. Bu hareket üç kez tekrarlanır. ETS2 telemetri
eklentilerini profil seçildikten sonra yüklediği için animasyon ilk oyun
menüsünde çalışamaz.

Sürüş sırasında:

| Motor devri | LED davranışı |
|---:|---|
| Devir sınırının %55'inden düşük | Kapalı |
| %55 | 1 LED |
| %64 | 2 LED |
| %73 | 3 LED |
| %82 | 4 LED |
| %90 | 5 LED'in tamamı |
| %96 ve üzeri | Tüm LED'ler yanıp söner |

### Gereksinimler

- 64-bit Windows 10 veya Windows 11
- Euro Truck Simulator 2'nin 64-bit sürümü
- PC modunda Logitech G29 (`046D:C24F`)
- Normal direksiyon ve force feedback kullanımı için yapılandırılmış Logitech G HUB/sürücü

Bu sürüm G29 içindir. G920 ve G923 farklı USB protokolleri veya ürün kimlikleri
kullandığından şu anda desteklenmemektedir.

### Kurulum

1. Euro Truck Simulator 2'yi tamamen kapatın.
2. GitHub **Releases** bölümünden `ets2_g29_leds.dll` dosyasını indirin.
3. ETS2 kurulum klasörünü açın. Steam'de **Kütüphane -> Euro Truck Simulator 2
   -> Özellikler -> Yüklü Dosyalar -> Göz At** yolunu kullanabilirsiniz.
4. DLL dosyasını şu konuma kopyalayın:

   ```text
   Euro Truck Simulator 2\bin\win_x64\plugins\ets2_g29_leds.dll
   ```

5. `plugins` klasörü yoksa oluşturun.
6. ETS2'yi başlatın ve SCS SDK uyarısını onaylayın.
7. Bir profil seçin. Eklenti G29 ile iletişim kurduğunda üç başlangıç animasyonu
   çalışmalıdır.

Steam'in varsayılan kurulum yolu genellikle şöyledir:

```text
C:\Program Files (x86)\Steam\steamapps\common\Euro Truck Simulator 2\bin\win_x64\plugins
```

### Sorun giderme

ETS2 eklenti mesajlarını şu dosyaya yazar:

```text
Belgeler\Euro Truck Simulator 2\game.log.txt
```

Dosyanın içinde `G29 LED:` ifadesini arayın.

| Log mesajı | Anlamı |
|---|---|
| `wheel detected; RPM LEDs enabled (report=...)` | Direksiyon bulundu ve ilk HID komutu başarıyla gönderildi. |
| `startup animation completed` | Üç bağlantı testi animasyonu başarıyla gönderildi. |
| `wheel not found; will retry while driving` | USB bağlantısını, G29 modunu, gücü ve G HUB'ı kontrol edin. |
| `output failed (report=..., WriteFile=..., HidD=...)` | Windows veya Logitech sürücüsü iki HID gönderim yöntemini de reddetti. Sorun bildirirken bu satırı ekleyin. |
| `wheel reconnected` | Eklenti önceki bağlantı hatasından sonra yeniden bağlandı. |

DLL logda hiç görünmüyorsa dosyanın 32-bit `win_x86` klasöründe değil,
`bin\win_x64\plugins` klasöründe bulunduğunu kontrol edin.

### Kaynak koddan derleme

Mevcut derleme sistemi şunları kullanır:

- Windows x86-64 C++ araç zinciri olarak Zig 0.17.0
- SCS Telemetry SDK 1.15 başlık dosyaları
- Windows sistem kitaplıkları `SetupAPI` ve `HID`

Derleme betiği şu klasör yapısını bekler:

```text
vendor\zig_toolchain\zig-x86_64-windows-0.17.0\zig.exe
vendor\scs_sdk\include\
src\g29_led_plugin.cpp
```

Çalıştırın:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

Oluşturulan eklenti şu konuma yazılır:

```text
dist\ets2_g29_leds.dll
```

SCS SDK kendi lisansı altında dağıtılır. Başlık dosyalarını yeniden dağıtırken
`vendor/scs_sdk/sdk_license.txt` dosyasını koruyun.

