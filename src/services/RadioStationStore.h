#pragma once
#include <Arduino.h>
#include <Preferences.h>

struct RadioStation {
  String name;
  String url;
  String genre;
  String country;
  String artwork;
  bool favorite = false;
};

class RadioStationStore {
public:
  static constexpr uint8_t MAX_STATIONS = 96;
private:
  RadioStation items[MAX_STATIONS];
  uint8_t used = 0;
  String lastErrorText;

  static String escField(const String& in) {
    String o; o.reserve(in.length() + 8);
    for (size_t i=0;i<in.length();++i) {
      const char c=in[i];
      if (c=='%' || c=='|' || c=='\n' || c=='\r') {
        char b[4]; snprintf(b,sizeof(b),"%%%02X",(unsigned char)c); o += b;
      } else o += c;
    }
    return o;
  }
  static String unescField(const String& in) {
    String o; o.reserve(in.length());
    for (size_t i=0;i<in.length();++i) {
      if (in[i]=='%' && i+2<in.length()) {
        char b[3]={in[i+1],in[i+2],0}; o += (char)strtoul(b,nullptr,16); i+=2;
      } else o += in[i];
    }
    return o;
  }
  static String part(const String& s, int n) {
    int from=0;
    for (int i=0;i<n;i++) { int p=s.indexOf('|',from); if(p<0) return String(); from=p+1; }
    int to=s.indexOf('|',from); if(to<0) to=s.length();
    return unescField(s.substring(from,to));
  }
  String encode(const RadioStation& s) const {
    return escField(s.name)+"|"+escField(s.url)+"|"+escField(s.genre)+"|"+escField(s.country)+"|"+(s.favorite?"1":"0")+"|"+escField(s.artwork);
  }
  RadioStation decode(const String& v) const {
    RadioStation s; s.name=part(v,0); s.url=part(v,1); s.genre=part(v,2); s.country=part(v,3); s.favorite=part(v,4)=="1"; s.artwork=part(v,5); return s;
  }
  void persistAll() {
    Preferences p; if(!p.begin("iq200-radio",false)) return;
    const uint8_t oldCount = min<uint8_t>(p.getUChar("count", 0), MAX_STATIONS);
    for(uint8_t i=0;i<used;i++) {
      char k[5]; snprintf(k,sizeof(k),"s%02u",i);
      p.putString(k,encode(items[i]));
    }
    // Remove only keys that belonged to the previous saved tail.
    // Never call remove() for all 96 slots: Preferences logs NOT_FOUND for
    // every missing key and wastes time in the web request handler.
    for(uint8_t i=used;i<oldCount;i++) {
      char k[5]; snprintf(k,sizeof(k),"s%02u",i);
      if (p.isKey(k)) p.remove(k);
    }
    p.putUChar("count",used);
    p.end();
  }
public:
  void begin() {
    used=0; Preferences p; if(!p.begin("iq200-radio",true)) return;
    uint8_t n=min<uint8_t>(p.getUChar("count",0),MAX_STATIONS);
    for(uint8_t i=0;i<n;i++) {
      char k[5]; snprintf(k,sizeof(k),"s%02u",i); String v=p.getString(k,"");
      RadioStation s=decode(v); if(s.name.length() && s.url.startsWith("http")) items[used++]=s;
    }
    p.end();
  }
  uint8_t count() const { return used; }
  const String& lastError() const { return lastErrorText; }
  const RadioStation* get(uint8_t i) const { return i<used?&items[i]:nullptr; }
  bool upsert(int index,const RadioStation& input) {
    RadioStation s=input; s.name.trim(); s.url.trim(); s.genre.trim(); s.country.trim(); s.artwork.trim();
    lastErrorText="";
    if(!s.name.length()) { lastErrorText="missing_name"; return false; }
    if(!s.url.startsWith("http://") && !s.url.startsWith("https://")) { lastErrorText="invalid_url"; return false; }
    if(s.name.length()>80 || s.url.length()>240 || s.genre.length()>40 || s.country.length()>20 || s.artwork.length()>240) { lastErrorText="field_too_long"; return false; }
    if(s.artwork.length() && !s.artwork.startsWith("http://") && !s.artwork.startsWith("https://")) { lastErrorText="invalid_artwork_url"; return false; }
    for(uint8_t i=0;i<used;i++) if((int)i!=index && items[i].url.equalsIgnoreCase(s.url)) { lastErrorText="duplicate_url"; return false; }
    if(index<0) { if(used>=MAX_STATIONS) { lastErrorText="store_full"; return false; } items[used++]=s; }
    else { if(index>=used) { lastErrorText="invalid_id"; return false; } items[index]=s; }
    persistAll(); return true;
  }
  bool removeAt(uint8_t index) {
    if(index>=used) return false; for(uint8_t i=index+1;i<used;i++) items[i-1]=items[i]; used--; persistAll(); return true;
  }
  void clear() { used=0; persistAll(); }
  uint8_t importText(const String& text,bool replace) {
    if(replace) used=0;
    uint8_t added=0; String pendingName;
    int start=0;
    while(start<(int)text.length() && used<MAX_STATIONS) {
      int nl=text.indexOf('\n',start); if(nl<0) nl=text.length();
      String line=text.substring(start,nl); line.trim(); start=nl+1;
      if(!line.length() || line=="#EXTM3U") continue;
      if(line.startsWith("#EXTINF:")) { int c=line.indexOf(','); pendingName=c>=0?line.substring(c+1):String("Station"); pendingName.trim(); continue; }
      RadioStation s;
      if(line.startsWith("http://") || line.startsWith("https://")) { s.url=line; s.name=pendingName.length()?pendingName:String("Station ")+String(used+1); pendingName=""; }
      else {
        // TSV / semicolon CSV / comma CSV: name<sep>url<sep>genre<sep>country.
        // TSV is preferred because station names and URLs can contain commas.
        char sep = '\0';
        if (line.indexOf('\t') >= 0) sep = '\t';
        else if (line.indexOf(';') >= 0) sep = ';';
        else if (line.indexOf(',') >= 0) sep = ',';
        if (!sep) continue;
        const int p1=line.indexOf(sep); if(p1<0) continue;
        const int p2=line.indexOf(sep,p1+1),p3=p2<0?-1:line.indexOf(sep,p2+1);
        s.name=line.substring(0,p1); s.url=line.substring(p1+1,p2<0?line.length():p2);
        if(p2>=0) s.genre=line.substring(p2+1,p3<0?line.length():p3);
        const int p4=p3<0?-1:line.indexOf(sep,p3+1);
        if(p3>=0) s.country=line.substring(p3+1,p4<0?line.length():p4);
        if(p4>=0) s.artwork=line.substring(p4+1);
        s.name.trim(); s.url.trim(); s.genre.trim(); s.country.trim(); s.artwork.trim();
        // Ignore a possible header row.
        String headerName=s.name; headerName.toLowerCase();
        String headerUrl=s.url; headerUrl.toLowerCase();
        if ((headerName=="name" || headerName=="station") && headerUrl=="url") continue;
      }
      if(s.name.length() && s.url.startsWith("http")) { items[used++]=s; added++; }
    }
    persistAll(); return added;
  }
};
