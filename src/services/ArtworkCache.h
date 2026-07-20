#pragma once
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include "SDManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"

// v9.8-alpha37: reliable unified artwork resolver/cache.
// Core0 resolves and loads immutable JPEG/PNG bytes. Core1 only renders them.
class ArtworkCache {
public:
  enum State : uint8_t { EMPTY=0, JPEG_READY=1, PNG_READY=2, NOT_FOUND=3, LOAD_ERROR=4 };
  static constexpr uint8_t CAPACITY = 4;
  struct Entry {
    String key, path;
    uint8_t* data=nullptr;
    size_t size=0;
    State state=EMPTY;
    uint32_t used=0, attempt=0;
  };
  class ReadGuard {
  public:
    explicit ReadGuard(ArtworkCache& o, TickType_t timeout=portMAX_DELAY):o_(o),ok_(o_.lock(timeout)){}
    ~ReadGuard(){if(ok_)o_.unlock();}
    bool locked()const{return ok_;}
  private: ArtworkCache&o_; bool ok_;
  };

  static ArtworkCache& instance(){ static ArtworkCache c; return c; }
  bool begin(){ if(!mutex_) mutex_=xSemaphoreCreateMutex(); return mutex_!=nullptr; }

  bool prepareForTrack(const char* trackPath, bool force=false){
    if(!trackPath||!trackPath[0]) return false;
    begin();
    const String dir=directoryOf(trackPath);
    const String dirKey=albumKey(dir);
    const String trackKey=trackArtworkKey(dir, trackPath);

    if(!SDManager::backgroundAllowed()) {
      Serial.printf("[ART] deferred: SD busy/recovering track=%s\n", trackPath);
      return selectCachedForTrack(trackPath);
    }

    // Prefer a track-specific cached image, then a shared album/folder image.
    {
      ReadGuard g(*this); if(!g.locked()) return false;
      int i=findKey(trackKey);
      if(i<0) i=findKey(dirKey);
      if(i>=0 && !force){
        Entry&e=entries_[i];
        const bool retry=(e.state==NOT_FOUND||e.state==LOAD_ERROR)&&
                         ((uint32_t)(millis()-e.attempt)>=5000U);
        if(!retry){
          // alpha47: selecting the same already-current cached artwork must not
          // create a new generation. Navigation resolves ART before PLAY, and
          // the pre-play safety call invokes prepareForTrack() again. The old
          // unconditional generation increment forced a second JPEG decode.
          const bool selectionChanged = (current_ != i);
          current_=i; e.used=++clock_; hits_++;
          if(selectionChanged) generation_++;
          Serial.printf("[ART] cache hit slot=%d key=%s state=%u bytes=%u gen=%lu changed=%d\n",
                        i,e.key.c_str(),(unsigned)e.state,(unsigned)e.size,
                        (unsigned long)generation_, selectionChanged ? 1 : 0);
          return ready(e.state);
        }
      }
    }

    String found;
    bool trackSpecific=false;
    uint8_t* bytes=nullptr;
    size_t sz=0;
    State st=NOT_FOUND;

    SDManager::Guard sd(pdMS_TO_TICKS(350));
    if(!sd || !SDManager::backgroundAllowed()) {
      st=LOAD_ERROR;
    } else {
      found=resolveArtworkPath(dir, trackPath, trackSpecific);
      Serial.printf("[ART][RESOLVE] track=%s album=%s scope=%s selected=%s\n",
                    trackPath,dir.c_str(),trackSpecific?"TRACK":"ALBUM",
                    found.length()?found.c_str():"NONE");
      if(found.length()){
        File f=SD.open(found.c_str(),FILE_READ);
        if(f){
          const size_t n=(size_t)f.size();
          static constexpr size_t kSmallArtworkBytes=512U*1024U;
          static constexpr size_t kMaxArtworkBytes=6U*1024U*1024U;
          static constexpr size_t kChunk=8192U;
          if(n>=8 && n<=kMaxArtworkBytes){
            const bool large=n>kSmallArtworkBytes;
            bytes=(uint8_t*)heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
            if(!bytes && !large) bytes=(uint8_t*)heap_caps_malloc(n,MALLOC_CAP_8BIT);
            if(large) Serial.printf("[ART][LARGE] load begin bytes=%u psramFree=%u path=%s\n",
                                    (unsigned)n,(unsigned)ESP.getFreePsram(),found.c_str());
            if(bytes){
              size_t got=0;
              while(got<n){
                const size_t ask=min(kChunk,n-got);
                const size_t part=f.read(bytes+got,ask);
                if(part==0) break;
                got+=part;
                esp_task_wdt_reset();
                if((got&0x7FFFU)==0U) vTaskDelay(1);
              }
              const bool jpg=got==n&&bytes[0]==0xFF&&bytes[1]==0xD8;
              const bool png=got==n&&bytes[0]==0x89&&bytes[1]==0x50&&bytes[2]==0x4E&&bytes[3]==0x47&&
                             bytes[4]==0x0D&&bytes[5]==0x0A&&bytes[6]==0x1A&&bytes[7]==0x0A;
              if(jpg||png){
                sz=n; st=jpg?JPEG_READY:PNG_READY;
                if(large) Serial.printf("[ART][LARGE] load OK bytes=%u format=%s psramFree=%u\n",
                                        (unsigned)n,jpg?"JPEG":"PNG",(unsigned)ESP.getFreePsram());
              } else {
                Serial.printf("[ART] invalid signature got=%u head=%02X %02X %02X %02X path=%s\n",
                              (unsigned)got,bytes[0],bytes[1],bytes[2],bytes[3],found.c_str());
                heap_caps_free(bytes); bytes=nullptr; st=LOAD_ERROR;
              }
            } else {
              Serial.printf("[ART] allocation failed bytes=%u psramFree=%u path=%s\n",
                            (unsigned)n,(unsigned)ESP.getFreePsram(),found.c_str());
              st=LOAD_ERROR;
            }
          } else {
            Serial.printf("[ART] skipped image bytes=%u max=%u path=%s\n",
                          (unsigned)n,(unsigned)kMaxArtworkBytes,found.c_str());
            st=LOAD_ERROR;
          }
          f.close();
        } else {
          Serial.printf("[ART] open failed path=%s\n",found.c_str());
          st=LOAD_ERROR;
        }
      }
    }

    const String key=trackSpecific?trackKey:dirKey;
    int slot=0; String evicted;
    {
      ReadGuard g(*this);
      if(!g.locked()){if(bytes)heap_caps_free(bytes);return false;}
      slot=findKey(key); if(slot<0)slot=selectVictim();
      Entry&e=entries_[slot]; evicted=e.key; freeEntry(e);
      e.key=key; e.path=found; e.data=bytes; e.size=sz; e.state=st;
      e.used=++clock_; e.attempt=millis(); current_=slot; loads_++; generation_++;
    }
    if(evicted.length()&&evicted!=key) Serial.printf("[ART] evict slot=%d key=%s\n",slot,evicted.c_str());
    Serial.printf("[ART] load slot=%d key=%s result=%s bytes=%u path=%s entries=%u/%u gen=%lu\n",
                  slot,key.c_str(),st==JPEG_READY?"JPEG":st==PNG_READY?"PNG":st==NOT_FOUND?"CD_DISC":"ERROR",
                  (unsigned)sz,found.length()?found.c_str():"NONE",(unsigned)count(),(unsigned)CAPACITY,
                  (unsigned long)generation_);
    return ready(st);
  }

  bool selectCachedForTrack(const char* trackPath){
    if(!trackPath||!trackPath[0]) return false;
    begin();
    const String dir=directoryOf(trackPath);
    const String trackKey=trackArtworkKey(dir,trackPath);
    const String dirKey=albumKey(dir);
    ReadGuard g(*this); if(!g.locked()) return false;
    int i=findKey(trackKey); if(i<0)i=findKey(dirKey); if(i<0)return false;
    Entry&e=entries_[i]; current_=i; e.used=++clock_; hits_++; generation_++;
    Serial.printf("[ART] cached select slot=%d key=%s state=%u bytes=%u gen=%lu\n",
                  i,e.key.c_str(),(unsigned)e.state,(unsigned)e.size,(unsigned long)generation_);
    return ready(e.state);
  }

  void clear(){ReadGuard g(*this);if(!g.locked())return;for(auto&e:entries_)freeEntry(e);current_=-1;generation_++;clears_++;Serial.printf("[ART] cache cleared gen=%lu\n",(unsigned long)generation_);}
  void printInfo()const{Serial.printf("[ART] cache entries=%u/%u current=%d loads=%lu hits=%lu clears=%lu bytes=%u gen=%lu\n",(unsigned)count(),(unsigned)CAPACITY,current_,(unsigned long)loads_,(unsigned long)hits_,(unsigned long)clears_,(unsigned)totalBytes(),(unsigned long)generation_);for(uint8_t i=0;i<CAPACITY;i++){const Entry&e=entries_[i];if(e.state!=EMPTY)Serial.printf("[ART] slot=%u state=%u bytes=%u used=%lu key=%s path=%s%s\n",i,(unsigned)e.state,(unsigned)e.size,(unsigned long)e.used,e.key.c_str(),e.path.length()?e.path.c_str():"NONE",(int)i==current_?" *":"");}}
  State state()const{return current_>=0?entries_[current_].state:EMPTY;}
  const uint8_t* data()const{return current_>=0?entries_[current_].data:nullptr;}
  size_t size()const{return current_>=0?entries_[current_].size:0;}
  uint32_t generation()const{return generation_;}
  const String& albumKey()const{return current_>=0?entries_[current_].key:empty_;}
  const String& imagePath()const{return current_>=0?entries_[current_].path:empty_;}
  uint32_t loads()const{return loads_;} uint32_t cacheHits()const{return hits_;}
  uint8_t count()const{uint8_t n=0;for(const auto&e:entries_)if(e.state!=EMPTY)n++;return n;}
  size_t totalBytes()const{size_t n=0;for(const auto&e:entries_)n+=e.size;return n;}
  bool lock(TickType_t t=portMAX_DELAY){if(!mutex_)begin();return mutex_&&xSemaphoreTake(mutex_,t)==pdTRUE;}
  void unlock(){if(mutex_)xSemaphoreGive(mutex_);}

private:
  ArtworkCache()=default;
  ~ArtworkCache(){for(auto&e:entries_)freeEntry(e);}
  ArtworkCache(const ArtworkCache&)=delete; ArtworkCache&operator=(const ArtworkCache&)=delete;
  static bool ready(State s){return s==JPEG_READY||s==PNG_READY;}
  static String directoryOf(const char*p){String v(p?p:"");int s=v.lastIndexOf('/');return s<=0?String("/"):v.substring(0,s);}
  static String fileNameOf(const String&p){int s=p.lastIndexOf('/');return s>=0?p.substring(s+1):p;}
  static String stemOf(const String&p){String n=fileNameOf(p);int d=n.lastIndexOf('.');if(d>0)n=n.substring(0,d);return n;}
  static String asciiLower(String s){for(size_t i=0;i<s.length();++i){char c=s[i];if(c>='A'&&c<='Z')s.setCharAt(i,c-'A'+'a');}return s;}
  static bool isImageName(const String&name){String l=asciiLower(name);return l.endsWith(".jpg")||l.endsWith(".jpeg")||l.endsWith(".png");}
  static bool isIgnoredImage(const String&name){String l=asciiLower(name);return l.startsWith(".")||l.indexOf("thumb")>=0||l.indexOf("thumbnail")>=0||l.indexOf("small")>=0||l.indexOf("icon")>=0;}
  static String joinPath(const String&dir,const String&name){String p=dir.length()?dir:String("/");if(!p.endsWith("/"))p+="/";p+=name;return p;}
  static String albumKey(const String&dir){return dir.length()?dir:String("/");}
  static String trackArtworkKey(const String&dir,const char*trackPath){return albumKey(dir)+String("|track:")+stemOf(String(trackPath?trackPath:""));}

  static bool isCanonicalStem(const String&stem){
    const String n=asciiLower(stem);
    return n=="cover"||n=="folder"||n=="front"||n=="album"||n=="albumart"||n=="album art"||
           n=="artwork"||n=="art"||n=="disc"||n=="cd"||n=="booklet"||
           stem=="Обложка"||stem=="обложка"||stem=="ОБЛОЖКА"||
           stem=="Обкладинка"||stem=="обкладинка"||stem=="ОБКЛАДИНКА";
  }

  static int artworkNameScore(const String&imageName,const String&trackPath,bool&trackSpecific){
    const String stem=stemOf(imageName);
    const String n=asciiLower(stem);
    const String trackStem=stemOf(trackPath);
    const String t=asciiLower(trackStem);
    int score=0; trackSpecific=false;
    if(isCanonicalStem(stem))score+=3000;
    if(n.indexOf("cover")>=0||n.indexOf("folder")>=0||n.indexOf("front")>=0||
       n.indexOf("album")>=0||n.indexOf("artwork")>=0||n.indexOf("booklet")>=0||
       stem.indexOf("Облож")>=0||stem.indexOf("облож")>=0||
       stem.indexOf("Обклад")>=0||stem.indexOf("обклад")>=0)score+=1400;
    if(n==t){score+=2800;trackSpecific=true;}
    else if(n.length()>=4&&(t.indexOf(n)>=0||n.indexOf(t)>=0)){score+=900;trackSpecific=true;}
    if(n.startsWith("00")||n.startsWith("01"))score+=120;
    if(n.indexOf("back")>=0||n.indexOf("rear")>=0||n.indexOf("inside")>=0||n.indexOf("booklet")>=0)score-=700;
    return score;
  }

  static String resolveArtworkPathInDir(const String&dir,const char*trackPath,bool&trackSpecific,bool canonicalOnly=false){
    trackSpecific=false;
    const String trackStem=stemOf(String(trackPath?trackPath:""));
    String best; int bestScore=-100000; bool bestTrackSpecific=false; int imageCount=0;
    File d=SD.open(dir.c_str());
    if(d&&d.isDirectory()){
      File f=d.openNextFile();
      while(f){
        if(!f.isDirectory()){
          const String name=fileNameOf(String(f.name()));
          if(isImageName(name)&&!isIgnoredImage(name)){
            imageCount++;
            bool candidateTrackSpecific=false;
            int score=artworkNameScore(name,String(trackPath?trackPath:""),candidateTrackSpecific);
            if (canonicalOnly && score < 1200) { f.close(); f=d.openNextFile(); continue; }
            const size_t fileSize=(size_t)f.size();
            if(fileSize<1024U)score-=2000;
            if(fileSize>6U*1024U*1024U)score-=2000;
            Serial.printf("[ART][RESOLVE] candidate=%s bytes=%u score=%d scope=%s\n",
                          name.c_str(),(unsigned)fileSize,score,candidateTrackSpecific?"TRACK":"ALBUM");
            if(score>bestScore){bestScore=score;best=joinPath(dir,name);bestTrackSpecific=candidateTrackSpecific;}
          }
        }
        f.close(); f=d.openNextFile();
      }
      d.close();
    }
    if(imageCount==0)return String();
    // A lone image is always the folder cover. With multiple images, choose the
    // strongest candidate; generic unknown names are still accepted rather than
    // hiding all artwork, while rear/thumbnail images are penalized.
    if(imageCount==1){trackSpecific=false;return best;}
    if(bestScore>-1000){trackSpecific=bestTrackSpecific;return best;}
    return String();
  }

  static String parentDirectoryOf(const String&dir){
    if(dir.length()<=1)return String("/");
    int s=dir.lastIndexOf('/');
    return s<=0?String("/"):dir.substring(0,s);
  }

  static String resolveArtworkPath(const String&dir,const char*trackPath,bool&trackSpecific){
    String found=resolveArtworkPathInDir(dir,trackPath,trackSpecific,false);
    if(found.length())return found;
    const String parent=parentDirectoryOf(dir);
    if(parent!=dir && parent!="/"){
      bool parentTrackSpecific=false;
      found=resolveArtworkPathInDir(parent,trackPath,parentTrackSpecific,true);
      if(found.length()){
        trackSpecific=false;
        Serial.printf("[ART][RESOLVE] parent fallback dir=%s selected=%s\n",parent.c_str(),found.c_str());
      }
    }
    return found;
  }

  int findKey(const String&k)const{for(uint8_t i=0;i<CAPACITY;i++)if(entries_[i].state!=EMPTY&&entries_[i].key==k)return i;return -1;}
  int selectVictim()const{for(uint8_t i=0;i<CAPACITY;i++)if(entries_[i].state==EMPTY)return i;uint8_t v=0;for(uint8_t i=1;i<CAPACITY;i++)if(entries_[i].used<entries_[v].used)v=i;return v;}
  static void freeEntry(Entry&e){if(e.data)heap_caps_free(e.data);e.data=nullptr;e.size=0;e.key="";e.path="";e.state=EMPTY;e.used=0;e.attempt=0;}
  SemaphoreHandle_t mutex_=nullptr;
  Entry entries_[CAPACITY];
  int8_t current_=-1;
  uint32_t clock_=0;
  volatile uint32_t generation_=0,loads_=0,hits_=0,clears_=0;
  String empty_;
};
