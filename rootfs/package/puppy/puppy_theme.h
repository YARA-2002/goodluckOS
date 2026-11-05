#pragma once
// Puppy external theme engine for GA36 (SDL2, no XML dependency).
// Uses editable INI files on HOME, not EmulationStation theme.xml files.
#include <SDL2/SDL.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

static constexpr const char* kPuppyThemesDir="/home/player/.config/puppy/themes";
static constexpr const char* kPuppyActiveTheme="/home/player/.config/puppy/active_theme.txt";

struct PuppyThemeStyle {
    std::string name="n64-unified";
    std::string title="GA36 Classic";
    std::string fontPath="/usr/share/fonts/Inter_24pt-Medium.ttf";
    std::string backgroundDir;
    std::string artFit="contain";
    int selectedFontSize=22,listFontSize=20,titleFontSize=34,smallFontSize=14;
    int systemPanelX=0,systemPanelY=52,systemPanelW=290,systemPanelH=382;
    int systemFocusY=235,systemSpacing=49,systemClipTop=68,systemClipBottom=405;
    int systemTextX=22,systemTextWidth=254;
    int gamePanelX=0,gamePanelY=52,gamePanelW=280,gamePanelH=382;
    int gameFocusY=235,gameSpacing=42,gameClipTop=66,gameClipBottom=405;
    int gameTextX=18,gameTextWidth=246;
    int artPanelX=285,artPanelY=65,artPanelW=340,artPanelH=348;
    int artBoxX=294,artBoxY=103,artBoxW=320,artBoxH=285;
    int headerH=52,footerY=434,headerTitleX=20,headerTitleY=13,footerTextY=452;
    SDL_Color background{24,31,58,255};
    SDL_Color panel{4,9,21,215};
    SDL_Color artPanel{2,8,20,190};
    SDL_Color header{6,12,24,192};
    SDL_Color footer{6,12,24,222};
    SDL_Color highlight{240,181,72,86};
    SDL_Color accent{255,210,96,255};
    SDL_Color text{255,255,255,255};
    SDL_Color selectedText{255,205,60,255};
};

static std::string puppyTrim(std::string s){
    size_t a=s.find_first_not_of(" \t\r\n");
    if(a==std::string::npos)return {};
    size_t b=s.find_last_not_of(" \t\r\n");
    return s.substr(a,b-a+1);
}
static std::vector<std::string> discoverPuppyThemes(){
    std::vector<std::string> themes;
    std::error_code ec;
    const std::filesystem::path root(kPuppyThemesDir);
    if(!std::filesystem::is_directory(root,ec))return {"n64-unified"};
    for(std::filesystem::directory_iterator it(root,ec),end;!ec&&it!=end;it.increment(ec)){
        if(!it->is_directory(ec))continue;
        const auto dir=it->path();
        if(std::filesystem::is_regular_file(dir/"theme.ini",ec)
           ||std::filesystem::is_directory(dir/"backgrounds",ec))
            themes.push_back(dir.filename().string());
    }
    std::sort(themes.begin(),themes.end());
    if(themes.empty())themes.push_back("n64-unified");
    return themes;
}
static std::string readPuppyActiveTheme(){
    std::ifstream f(kPuppyActiveTheme);
    std::string name;
    if(f&&std::getline(f,name))return puppyTrim(name);
    return {};
}
static void writePuppyActiveTheme(const std::string& name){
    std::ofstream out(kPuppyActiveTheme,std::ios::trunc);
    if(out)out<<name<<"\n";
}
static SDL_Color puppyParseColor(std::string s,SDL_Color fallback){
    s=puppyTrim(s);
    if(!s.empty()&&s.front()=='#')s.erase(0,1);
    if(s.size()!=6&&s.size()!=8)return fallback;
    for(char c:s)if(!std::isxdigit((unsigned char)c))return fallback;
    try{
        unsigned long v=std::stoul(s,nullptr,16);
        if(s.size()==6)return SDL_Color{
            (Uint8)((v>>16)&255),(Uint8)((v>>8)&255),(Uint8)(v&255),255};
        return SDL_Color{(Uint8)((v>>24)&255),(Uint8)((v>>16)&255),
                         (Uint8)((v>>8)&255),(Uint8)(v&255)};
    }catch(...){return fallback;}
}
static PuppyThemeStyle loadPuppyTheme(const std::string& name){
    PuppyThemeStyle t;
    t.name=name;
    const auto themeRoot=std::filesystem::path(kPuppyThemesDir)/name;
    t.backgroundDir=(themeRoot/"backgrounds").lexically_normal().string();
    std::ifstream in(themeRoot/"theme.ini");
    std::map<std::string,std::string> options;
    std::string line,group;
    while(std::getline(in,line)){
        line=puppyTrim(line);
        if(line.empty()||line[0]=='#'||line[0]==';')continue;
        if(line.front()=='['&&line.back()==']'){
            group=puppyTrim(line.substr(1,line.size()-2));
            continue;
        }
        size_t p=line.find('=');
        if(p==std::string::npos)continue;
        options[group+"."+puppyTrim(line.substr(0,p))]=puppyTrim(line.substr(p+1));
    }
    auto get=[&](const std::string& g,const std::string& key){
        auto it=options.find(g+"."+key);
        return it==options.end()?std::string():it->second;
    };
    auto getInt=[&](const std::string& g,const std::string& k,int original,int lo,int hi){
        std::string v=get(g,k);
        if(v.empty())return original;
        try{
            size_t end=0;
            long long n=std::stoll(v,&end,10);
            return end==v.size()?std::clamp((int)std::clamp(n,(long long)lo,(long long)hi),lo,hi):original;
        }catch(...){return original;}
    };
    auto getColor=[&](const std::string& g,const std::string& k,SDL_Color original){
        return puppyParseColor(get(g,k),original);
    };
    std::string title=get("theme","name");
    if(!title.empty())t.title=title;
    std::string font=get("theme","font");
    if(!font.empty()){
        std::filesystem::path p(font);
        t.fontPath=(p.is_absolute()?p:(themeRoot/p)).lexically_normal().string();
    }
    std::string backgrounds=get("theme","backgrounds");
    if(!backgrounds.empty()){
        std::filesystem::path p(backgrounds);
        t.backgroundDir=(p.is_absolute()?p:(themeRoot/p)).lexically_normal().string();
    }
    std::string fit=get("art","fit");
    if(fit=="cover"||fit=="contain")t.artFit=fit;
    t.selectedFontSize=getInt("fonts","selected",t.selectedFontSize,10,54);
    t.listFontSize=getInt("fonts","list",t.listFontSize,10,48);
    t.titleFontSize=getInt("fonts","title",t.titleFontSize,10,56);
    t.smallFontSize=getInt("fonts","small",t.smallFontSize,8,32);
#define THEME_INT(g,key,member,lo,hi) t.member=getInt(g,key,t.member,lo,hi)
    THEME_INT("systems","panel_x",systemPanelX,-640,640);
    THEME_INT("systems","panel_y",systemPanelY,0,480);
    THEME_INT("systems","panel_w",systemPanelW,0,640);
    THEME_INT("systems","panel_h",systemPanelH,0,480);
    THEME_INT("systems","focus_y",systemFocusY,0,480);
    THEME_INT("systems","spacing",systemSpacing,16,150);
    THEME_INT("systems","clip_top",systemClipTop,0,480);
    THEME_INT("systems","clip_bottom",systemClipBottom,0,480);
    THEME_INT("systems","text_x",systemTextX,0,620);
    THEME_INT("systems","text_width",systemTextWidth,20,640);
    THEME_INT("games","panel_x",gamePanelX,-640,640);
    THEME_INT("games","panel_y",gamePanelY,0,480);
    THEME_INT("games","panel_w",gamePanelW,0,640);
    THEME_INT("games","panel_h",gamePanelH,0,480);
    THEME_INT("games","focus_y",gameFocusY,0,480);
    THEME_INT("games","spacing",gameSpacing,16,150);
    THEME_INT("games","clip_top",gameClipTop,0,480);
    THEME_INT("games","clip_bottom",gameClipBottom,0,480);
    THEME_INT("games","text_x",gameTextX,0,620);
    THEME_INT("games","text_width",gameTextWidth,20,640);
    THEME_INT("art","panel_x",artPanelX,-640,640);
    THEME_INT("art","panel_y",artPanelY,0,480);
    THEME_INT("art","panel_w",artPanelW,0,640);
    THEME_INT("art","panel_h",artPanelH,0,480);
    THEME_INT("art","box_x",artBoxX,0,620);
    THEME_INT("art","box_y",artBoxY,0,460);
    THEME_INT("art","box_w",artBoxW,1,640);
    THEME_INT("art","box_h",artBoxH,1,480);
    THEME_INT("bars","header_height",headerH,0,160);
    THEME_INT("bars","footer_y",footerY,0,480);
    THEME_INT("bars","header_text_x",headerTitleX,0,620);
    THEME_INT("bars","header_text_y",headerTitleY,0,460);
    THEME_INT("bars","footer_text_y",footerTextY,0,470);
#undef THEME_INT
    t.background=getColor("colors","background",t.background);
    t.panel=getColor("colors","panel",t.panel);
    t.artPanel=getColor("colors","art_panel",t.artPanel);
    t.header=getColor("colors","header",t.header);
    t.footer=getColor("colors","footer",t.footer);
    t.highlight=getColor("colors","highlight",t.highlight);
    t.accent=getColor("colors","accent",t.accent);
    t.text=getColor("colors","text",t.text);
    t.selectedText=getColor("colors","selected_text",t.selectedText);
    return t;
}
