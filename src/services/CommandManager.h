#pragma once
#include <Arduino.h>

// IQ200 OS v9.6-alpha2: central command metadata registry + Web Command Center.
// Execution remains delegated to the existing proven handlers; this service
// owns discovery, categorized help, alias visibility and command search.
class CommandManager {
public:
  struct Entry {
    const char* category;
    const char* command;
    const char* aliases;
    const char* description;
  };

private:
  static const Entry* entries(size_t& count) {
    static const Entry k[] = {
      {"General", "help [term]", "h, ?", "Show all commands or filter by category/name"},
      {"General", "commands", "", "Show compact categorized command list"},
      {"General", "apropos <term>", "", "Search command names, aliases and descriptions"},
      {"General", "home", "back", "Open Home screen"},
      {"General", "status", "st", "Show system status"},
      {"General", "diag", "", "Open diagnostics screen"},
      {"General", "reboot", "rb", "Restart ESP32-S3"},

      {"Playback", "play", "p, mplay, wav", "Play selected track"},
      {"Playback", "playwav", "", "Force WAV fallback test"},
      {"Playback", "stop", "s, mstop", "Stop playback (Player screen only)"},
      {"Playback", "next", "plnext", "Next track / navigation preview"},
      {"Playback", "prev", "plprev", "Previous track / navigation preview"},
      {"Playback", "player", "now", "Open Player screen"},
      {"Playback", "autoplay on|off|status", "", "Configure autoplay"},
      {"Playback", "repeat off|one|all", "", "Set repeat mode"},
      {"Playback", "tone", "", "Play hardware test tone"},
      {"Playback", "volume <0..100>", "", "Set output volume"},

      {"Navigation", "nav status", "", "Show preview navigation statistics"},
      {"Navigation", "nav preview on|off", "", "Enable or disable deferred preview"},
      {"Navigation", "nav delay <200..1500>", "", "Set preview commit delay in ms"},

      {"Playlist", "pl", "list", "Show playlist/media summary"},
      {"Playlist", "scan", "rescan, dbscan", "Build media database"},
      {"Playlist", "pladd", "", "Add test/current item"},
      {"Playlist", "plclear", "", "Clear playlist"},

      {"Queue", "queue", "q", "Show queue"},
      {"Queue", "queueadd", "", "Add current track"},
      {"Queue", "queueclear", "", "Clear queue"},
      {"Queue", "queuenext|queueprev", "", "Navigate queue"},
      {"Queue", "queuesave|queueload", "", "Persist or restore queue"},
      {"Queue", "queueshuffle", "qshuffle", "Toggle smart shuffle"},
      {"Queue", "queuerepeat0|1|2", "", "Set queue repeat mode"},

      {"Library", "library", "lib", "Open library"},
      {"Library", "artists", "", "Browse artists"},
      {"Library", "albums", "", "Browse albums"},
      {"Library", "genres", "", "Browse genres"},
      {"Library", "folders", "", "Browse folders"},
      {"Library", "recent", "", "Show recently played"},
      {"Library", "most", "", "Show most played"},
      {"Library", "favorites", "fav", "Open favorites"},
      {"Library", "searchui", "search", "Open search screen"},
      {"Library", "find <text>", "findall", "Search indexed library"},
      {"Library", "libstats", "", "Show library statistics"},
      {"Library", "libbuild", "", "Rebuild library indexes"},

      {"Database", "db", "", "Show database information"},
      {"Database", "dbload", "", "Load active playlist from DB"},
      {"Database", "dbclear", "", "Clear database"},
      {"Database", "dbtest", "", "Run database self-test"},
      {"Database", "update", "upd, dbupdate", "Fast DB validation/update"},

      {"Resume", "resume", "", "Show resume state"},
      {"Resume", "resumesave", "rsave", "Save resume state"},
      {"Resume", "resumeload", "rload", "Load resume state"},
      {"Resume", "resumeclear", "", "Clear resume state"},

      {"Appearance", "theme list|status", "", "List themes or show active theme"},
      {"Appearance", "theme bluepro|emerald|amber|purple|ice|oled|carbon|matrix", "", "Apply and save Theme Pack 1.0"},
      {"Appearance", "vu status", "", "Show VU settings"},
      {"Appearance", "art info", "art, art cache", "Show Artwork PSRAM LRU cache"},
      {"Appearance", "art reload", "", "Reload current track artwork on Core0"},
      {"Appearance", "art clear", "", "Clear Artwork PSRAM cache"},
      {"Appearance", "player theme modern|dual|artwork|classic", "", "Set Player 2.0 layout and save to NVS"},
      {"Appearance", "vu style rect|dot|thin|line", "", "Set VU style"},
      {"Appearance", "vu fps <10..30>", "", "Set VU refresh rate"},
      {"Appearance", "vuseg <4..24>", "", "Set VU segment count"},
      {"Appearance", "vu peak on|off", "", "Enable or disable peak hold"},
      {"Appearance", "vu hold <50..1500>", "", "Set peak hold time"},
      {"Appearance", "vu decay <1..10>", "", "Set peak decay"},
      {"Appearance", "vu save", "", "Persist VU configuration"},

      {"Diagnostics", "perf", "", "Show performance dashboard"},
      {"Diagnostics", "ui", "ui status", "Show UI profiler"},
      {"Diagnostics", "ui reset", "", "Reset UI profiler"},
      {"Diagnostics", "bb on|off|status", "bb", "Control Black Box recorder"},
      {"Diagnostics", "bb dump [count]", "", "Dump recent Black Box records"},
      {"Diagnostics", "bb clear", "", "Clear Black Box"},
      {"Diagnostics", "log on|off|status", "log", "Control process logging"},
      {"Diagnostics", "log rate <250..10000>", "", "Set process log interval"},
      {"Diagnostics", "log reset", "", "Reset process/SD counters"},
      {"Diagnostics", "health", "hl", "Open health monitor"},
      {"Diagnostics", "tasks", "", "Open task monitor"},
      {"Diagnostics", "scheduler", "", "Open scheduler monitor"},
      {"Diagnostics", "pipeline", "pipe", "Show media pipeline"},

      {"Hardware", "psram", "", "Show PSRAM information"},
      {"Hardware", "display", "", "Run display test"},
      {"Hardware", "enc", "", "Show encoder state"},
      {"Hardware", "audio", "", "Open audio screen"},
      {"Hardware", "sd", "", "Open SD screen"},
      {"Hardware", "sd status", "sd stats", "Show SD clock, retries and recovery counters"},
      {"Hardware", "sd speed 16|12|10", "sd 16|12|10", "Set and save SD SPI clock; stop playback first"},
      {"Hardware", "files", "", "Open file browser"},
      {"Hardware", "index", "ls", "Build/show root index"},

      {"Network", "wifi", "", "Open Wi-Fi screen"},
      {"Network", "wifi scan", "", "Scan nearby Wi-Fi networks"},
      {"Network", "wifi status|ip", "", "Show Wi-Fi mode, IP, RSSI and saved profile"},
      {"Network", "wifi connect <ssid> <password>", "", "Connect STA and save profile"},
      {"Network", "wifi save <ssid> <password>", "", "Save profile without connecting"},
      {"Network", "wifi apsta <ssid> <password>", "", "Run AP and STA together"},
      {"Network", "wifi load", "wifi sta", "Connect using saved profile"},
      {"Network", "wifi disconnect", "", "Disconnect STA without erasing profile"},
      {"Network", "wifi auto on|off", "", "Enable or disable boot AutoConnect"},
      {"Network", "wifi fallback on|off", "", "Enable or disable fallback AP"},
      {"Network", "wifi forget", "", "Erase saved Wi-Fi profile"},
      {"Network", "wifi boot|stats", "", "Run or inspect Wi-Fi boot policy"},
      {"Network", "web", "web on|off", "Control Web UI"},
      {"Network", "net ap|off", "", "Control access point"},
      {"Network", "ota info|sd", "", "OTA information/update from SD"},
      {"Network", "radio", "", "Open internet radio"},

      {"System", "services", "", "Open service monitor"},
      {"System", "apps", "", "Open app manager"},
      {"System", "windows", "", "Open window manager"},
      {"System", "coreos", "", "Open CoreOS screen"},
      {"System", "render", "", "Open renderer screen"},
      {"System", "settings", "", "Open settings"},
      {"System", "stability", "stab", "Show stability information"},
      {"System", "polish", "", "Show commercial polish status"},
      {"System", "burnin start|stop", "", "Control burn-in test"}
    };
    count = sizeof(k) / sizeof(k[0]);
    return k;
  }

  static bool containsIgnoreCase(const char* text, const String& needle) {
    String s(text ? text : "");
    s.toLowerCase();
    return s.indexOf(needle) >= 0;
  }

public:
  void begin() {}

  size_t count() const {
    size_t n = 0;
    entries(n);
    return n;
  }

  const Entry* get(size_t index) const {
    size_t n = 0;
    const Entry* list = entries(n);
    return index < n ? &list[index] : nullptr;
  }

  void print(Stream& out, const String& rawFilter = String()) const {
    String filter = rawFilter;
    filter.trim();
    filter.toLowerCase();
    size_t count = 0;
    const Entry* list = entries(count);
    String lastCategory;
    uint16_t shown = 0;
    out.println("========== IQ200 COMMAND MANAGER ==========");
    if (filter.length()) { out.print("Filter: "); out.println(filter); }
    for (size_t i = 0; i < count; ++i) {
      const Entry& e = list[i];
      if (filter.length() && !containsIgnoreCase(e.category, filter) &&
          !containsIgnoreCase(e.command, filter) && !containsIgnoreCase(e.aliases, filter) &&
          !containsIgnoreCase(e.description, filter)) continue;
      String cat(e.category);
      if (cat != lastCategory) {
        if (shown) out.println();
        out.print('['); out.print(cat); out.println(']');
        lastCategory = cat;
      }
      out.print("  "); out.print(e.command);
      if (e.aliases && e.aliases[0]) { out.print("  (aliases: "); out.print(e.aliases); out.print(')'); }
      out.print("\n      "); out.println(e.description);
      shown++;
    }
    if (!shown) out.println("No matching commands. Try: help, help playback, apropos theme");
    out.println("===========================================");
  }

  void printCompact(Stream& out) const {
    size_t count = 0;
    const Entry* list = entries(count);
    String category;
    for (size_t i = 0; i < count; ++i) {
      String cat(list[i].category);
      if (cat != category) {
        if (category.length()) out.println();
        category = cat;
        out.print(category); out.print(": ");
      } else out.print(" | ");
      String cmd(list[i].command);
      int sp = cmd.indexOf(' ');
      if (sp > 0) cmd = cmd.substring(0, sp);
      out.print(cmd);
    }
    out.println();
  }

  bool handleDiscovery(const String& line, Stream& out) const {
    if (line == "h" || line == "?" || line == "help") { print(out); return true; }
    if (line == "commands") { printCompact(out); return true; }
    if (line.startsWith("help ")) { print(out, line.substring(5)); return true; }
    if (line.startsWith("apropos ")) { print(out, line.substring(8)); return true; }
    return false;
  }
};
