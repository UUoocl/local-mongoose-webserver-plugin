# Local Webserver OBS Studio Plugin

Serve local files over **HTTP directly inside OBS Studio**.  
Perfect for HTML/CSS/JS overlays, dashboards, widgets, and any Browser Source that needs local assets **no external server required**.

---

## ✨ Features

- 🖥️ **Built‑in local HTTP server** running inside OBS
- 📁 **Serves any folder** as a document root
- 🌐 **Browser Source friendly** (HTML/CSS/JS/images/fonts/JSON)
- 🧭 **Strictly Local-only** (locked to 127.0.0.1 for security)
- ⚙️ **Configurable port** (auto‑rebinding if busy)
- 🔄 **One‑click Browser Source refresh** (cache bust)
- 🎯 **Create/Update Browser Source in current scene**
- 💾 **Persistent settings** (doc root + port)
- 🧩 Clean, OBS‑style dock/panel UI

---

## 📦 Installation

### Windows (Installer)

1. Download the latest **Windows installer** from the Releases page:
   - **Releases → Assets → local-webserver-setup.exe**
2. Run the installer and choose your OBS Studio folder.

The installer places:

- `local-webserver.dll` → `OBS\obs-plugins\64bit\`
- locales → `OBS\data\obs-plugins\local-webserver\locale\`

### Manual Install (Windows)

1. Download the `.zip` from Releases.
2. Copy:
   ```
   obs-plugins/64bit/local-webserver.dll
   data/obs-plugins/local-webserver/locale/
   ```
   into your OBS installation directory.

### Manual Install (macOS)

1. Download the `.plugin` bundle.
2. Copy `local-webserver.plugin` to:
   `~/Library/Application Support/obs-studio/plugins/`
3. Restart OBS.

### Linux

Not packaged yet.  
If you want to help test builds, open an issue or ping me on Discord.

---

## 🚀 Quick Start

1. Open OBS
2. Go to **Tools → Local Webserver**
3. Click **Browse…** and select a folder to serve
4. Choose a port (default is fine)
5. Click **Start / Restart**
6. Click **Create Browser Source in current scene**

Your folder is now available at:

```
http://127.0.0.1:<port>/
```

Example:

```
http://127.0.0.1:8080/
```

Point any Browser Source to that URL and it will load your local overlay.

---

## 🧠 Typical Use Cases

- ✅ **Custom HTML overlays**
- ✅ Stream dashboards / control panels
- ✅ Local widget dev (hot reload by refresh)
- ✅ Serving JSON/images/fonts to overlays
- ✅ Testing web overlays without hosting

---

## 🔄 Refreshing the Browser Source

Use the **Refresh Browser Source** button in the dock to force a reload.  
This is helpful after editing local files, especially when OBS caches the page.

---

## ⚙️ Settings

The plugin stores:

- **Document root** (folder being served)
- **Port**

If the chosen port is unavailable, the server automatically binds to a free port and updates the UI.

---

## 🛠 Build From Source

### Requirements

- OBS Studio build environment (same as other OBS plugins)
- Qt 6
- CMake 3.28+

### Build (Windows)

```bash
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
```

### Build (macOS Apple Silicon)

```bash
# Configuration targeting arm64
cmake --preset macos
# Build and automatically ad-hoc sign
cmake --build --preset macos --config RelWithDebInfo
# Deploy to local OBS
./deploy_macos.sh
```

Artifacts will be staged under:

```
build_macos/RelWithDebInfo/ local-webserver.plugin/
```

---

## 🧩 Folder Structure Served

Whatever you pick as Document Root becomes `/` on the server.

Example:

```
my-overlay/
  index.html
  style.css
  app.js
  assets/
    logo.png
```

Loads as:

```
http://127.0.0.1:8080/index.html
http://127.0.0.1:8080/style.css
http://127.0.0.1:8080/app.js
http://127.0.0.1:8080/assets/logo.png
```

---

## 🐞 Troubleshooting

**Server says “Failed to start”**

- Another app is using the port.  
  Try another port or let the plugin auto‑rebind.

**Browser Source is blank**

- Make sure `index.html` exists in the chosen folder.
- Test in a normal browser: `http://127.0.0.1:<port>/`

**Refresh doesn’t seem to update**

- Some overlays cache aggressively.  
  Try refreshing twice or add cache‑busting query strings in your overlay if needed.

---

## 📣 Support / Community

- GitHub Issues: use this repo’s **Issues** tab
- Discord: https://discord.gg/2yD6B2PTuQ

---

## ☕ Support Development

If this plugin helps your stream, consider supporting future updates:

- Ko‑fi: https://ko-fi.com/mmltech
- PayPal: https://paypal.me/mmlTools

Any support keeps development moving. Thanks! ❤️

---

## 📄 License

MIT License free to use, modify, and share.  
If you build something cool with it, I’d love to see it!
