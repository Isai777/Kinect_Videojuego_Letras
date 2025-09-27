// main.c — C puro (sin lambdas), avance de nivel + centrado de cursor en ambos modos
// Requiere: SDL2, SDL2_image, SDL2_ttf, cJSON, Kinect10 SDK 1.8, kinect_shim.*

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include "kinect_shim.h"
#include <cjson/cJSON.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ====== Constantes ventana/UI ====== */
#define W 1280
#define H 720
#define SLOT_SIZE   90
#define LETTER_SIZE 90
#define GAP         16
#define MAX_LETTERS 24
#define MAX_SLOTS   16
#define MAX_PARTICLES 300

/* ====== Tipos ====== */
typedef struct { int x,y,w,h,active; } Rect;
typedef struct {
    int x,y,w,h,active;
    char glyph;
    SDL_Texture* tex;
    float scale, zlift, tScale, tZlift;
    int   snappedTo;
    int   homeX, homeY;
} Letter;

typedef struct { float x,y,vx,vy,life,maxlife; SDL_Color col; int alive; } Particle;

typedef struct {
    SDL_Color BG_TOP, BG_BOT, VIGNETTE;
    SDL_Color PANEL_GLASS, PANEL_EDGE, PANEL_LABEL;
    SDL_Color SLOT_FILL, SLOT_EDGE, SLOT_HILITE;
    SDL_Color LETTER_TILE, LETTER_FILL, LETTER_NEON, LETTER_NEON2;
    SDL_Color TEXT_MAIN, OK_PILL;
    SDL_Color PROG_BG, PROG_A, PROG_B, GLINT;
    SDL_Color SHADOW, CURSOR_A, CURSOR_B, CURSOR_C;
    SDL_Color FX_A, FX_B;
} Theme;

typedef struct { char clue[128]; char answer[32]; int answer_len; int letter_count; char letters[MAX_LETTERS]; } LevelDef;
typedef struct { LevelDef* defs; int count; } LevelPack;

/* ====== Globals UI/Temas ====== */
static Theme THEMES[3];
static SDL_Color C_BG_TOP, C_BG_BOT, C_VIGNETTE;
static SDL_Color C_PANEL_GLASS, C_PANEL_EDGE, C_PANEL_LABEL;
static SDL_Color C_SLOT_FILL, C_SLOT_EDGE, C_SLOT_HILITE;
static SDL_Color C_LETTER_TILE, C_LETTER_FILL, C_LETTER_NEON, C_LETTER_NEON2;
static SDL_Color C_TEXT_MAIN, C_OK_PILL;
static SDL_Color C_PROG_BG, C_PROG_A, C_PROG_B, C_GLINT;
static SDL_Color C_SHADOW, C_CURSOR_A, C_CURSOR_B, C_CURSOR_C;
static SDL_Color C_FX_A, C_FX_B;

/* ====== Globals de juego/UI ====== */
static SDL_Window*  g_win = NULL;
static SDL_Renderer* g_ren = NULL;
static TTF_Font* g_font_big = NULL;
static TTF_Font* g_font_lbl = NULL;

static LevelPack g_pack = {0};
static int g_levelIndex = 0;
static LevelDef g_cur;

static SDL_Texture* g_texPista = NULL;
static SDL_Rect g_rectPista = {0,26,0,0};

static SDL_Texture* g_texModoMouse = NULL;
static SDL_Texture* g_texModoKine  = NULL;
static SDL_Rect g_btnMode = {0,0,132,38};

static Rect   g_ranuras[MAX_SLOTS];
static float  g_slotGlow[MAX_SLOTS];
static SDL_Rect g_trayPanel = {0,0,0,0};
static Letter g_letras[MAX_LETTERS];
static int    g_nLetters = 0;
static int    g_slots = 0;

static Particle g_P[MAX_PARTICLES];

/* ====== Util ====== */
static int   point_in(Rect r, int px, int py){ return (px>=r.x && px<=r.x+r.w && py>=r.y && py<=r.y+r.h); }
static float lerp(float a,float b,float t){ return a + (b-a)*t; }
static float damp(float current, float target, float lambda, float dt){ return lerp(current, target, 1.0f - expf(-lambda*dt)); }
static float clamp01(float v){ return v<0?0:(v>1?1:v); }
static void  setc(SDL_Renderer* r, SDL_Color c){ SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a); }

/* ====== Temas ====== */
static void apply_theme(int idx){
    Theme t = THEMES[idx%3];
    C_BG_TOP=t.BG_TOP; C_BG_BOT=t.BG_BOT; C_VIGNETTE=t.VIGNETTE;
    C_PANEL_GLASS=t.PANEL_GLASS; C_PANEL_EDGE=t.PANEL_EDGE; C_PANEL_LABEL=t.PANEL_LABEL;
    C_SLOT_FILL=t.SLOT_FILL; C_SLOT_EDGE=t.SLOT_EDGE; C_SLOT_HILITE=t.SLOT_HILITE;
    C_LETTER_TILE=t.LETTER_TILE; C_LETTER_FILL=t.LETTER_FILL; C_LETTER_NEON=t.LETTER_NEON; C_LETTER_NEON2=t.LETTER_NEON2;
    C_TEXT_MAIN=t.TEXT_MAIN; C_OK_PILL=t.OK_PILL;
    C_PROG_BG=t.PROG_BG; C_PROG_A=t.PROG_A; C_PROG_B=t.PROG_B; C_GLINT=t.GLINT;
    C_SHADOW=t.SHADOW; C_CURSOR_A=t.CURSOR_A; C_CURSOR_B=t.CURSOR_B; C_CURSOR_C=t.CURSOR_C;
    C_FX_A=t.FX_A; C_FX_B=t.FX_B;
}
static void init_themes(){
    // 0) Beige cálido
    THEMES[0] = (Theme){
        {232,218,197,255},{199,176,149,255},{ 65, 52, 39,110},
        {255,245,230, 96},{255,255,255, 48},{ 78, 67, 56,255},
        {250,244,235,255},{168,148,128,255},{255,255,255, 96},
        {255,255,255, 18},{255,251,245,255},{233,166, 69,255},{198,120, 90,255},
        { 58, 50, 42,255},{140,176, 90,235},
        {206,190,166,255},{222,176, 99,255},{198,120, 90,255},{255,255,255,170},
        {  0,  0,  0,110},{250,210,140, 52},{230,150,110, 92},{255,255,255,220},
        {233,166, 69,255},{198,120, 90,255}
    };
    // 1) Océano (azul mar)
    THEMES[1] = (Theme){
        {  4, 34, 84,255},{ 15,112,186,255},{  0,  0,  0,120},
        {240,248,255, 84},{255,255,255, 42},{ 40, 60, 80,255},
        {235,242,250,255},{130,160,200,255},{255,255,255, 90},
        {255,255,255, 16},{250,252,255,255},{ 80,190,255,255},{140,120,255,255},
        {236,244,255,255},{110,190,140,235},
        { 36, 54, 80,255},{ 80,190,255,255},{140,120,255,255},{255,255,255,160},
        {  0,  0,  0,110},{ 90,190,255, 44},{140,120,255, 84},{255,255,255,220},
        { 80,190,255,255},{140,120,255,255}
    };
    // 2) Atardecer (naranja)
    THEMES[2] = (Theme){
        {255,120, 40,255},{255,196,120,255},{  0,  0,  0,120},
        {255,240,230, 96},{255,255,255, 46},{ 85, 60, 40,255},
        {250,238,230,255},{210,150,120,255},{255,255,255, 96},
        {255,255,255, 18},{255,246,238,255},{255,140,100,255},{255, 96, 80,255},
        { 70, 50, 40,255},{120,200,120,235},
        { 90, 60, 40,255},{255,140,100,255},{255, 96, 80,255},{255,255,255,170},
        {  0,  0,  0,110},{255,190,120, 60},{255,120, 90, 90},{255,255,255,220},
        {255,140,100,255},{255, 96, 80,255}
    };
    apply_theme(0);
}

/* ====== Dibujo ====== */
static void drawGradientV(SDL_Renderer* r, int w, int h, SDL_Color top, SDL_Color bot){
    for (int y=0; y<h; ++y){
        float t = (float)y/(float)(h-1);
        Uint8 R=(Uint8)(top.r + t*(bot.r-top.r));
        Uint8 G=(Uint8)(top.g + t*(bot.g-top.g));
        Uint8 B=(Uint8)(top.b + t*(bot.b-top.b));
        SDL_SetRenderDrawColor(r,R,G,B,255);
        SDL_RenderDrawLine(r,0,y,w,y);
    }
}
static void drawFilledCircle(SDL_Renderer* r,int cx,int cy,int rad,SDL_Color c){
    setc(r,c);
    for(int dy=-rad;dy<=rad;++dy){
        int w=(int)floorf(sqrtf((float)(rad*rad - dy*dy)));
        SDL_RenderDrawLine(r,cx-w,cy+dy,cx+w,cy+dy);
    }
}
static void drawRoundedRectFill(SDL_Renderer* r, SDL_Rect rr, int rad, SDL_Color c){
    if (rad<=0){ setc(r,c); SDL_RenderFillRect(r,&rr); return; }
    if (rad*2>rr.w) rad=rr.w/2; if (rad*2>rr.h) rad=rr.h/2;
    setc(r,c);
    SDL_Rect core={rr.x+rad,rr.y,rr.w-2*rad,rr.h}; SDL_RenderFillRect(r,&core);
    SDL_Rect l={rr.x,rr.y+rad,rad,rr.h-2*rad}, d={rr.x+rr.w-rad,rr.y+rad,rad,rr.h-2*rad};
    SDL_RenderFillRect(r,&l); SDL_RenderFillRect(r,&d);
    for(int dx=-rad;dx<=rad;++dx){
        int h=(int)floorf(sqrtf((float)(rad*rad - dx*dx)));
        SDL_RenderDrawLine(r, rr.x+rad-dx, rr.y+rad-h, rr.x+rad-dx, rr.y+rad);
        SDL_RenderDrawLine(r, rr.x+rr.w-rad+dx, rr.y+rad-h, rr.x+rr.w-rad+dx, rr.y+rad);
        SDL_RenderDrawLine(r, rr.x+rad-dx, rr.y+rr.h-rad, rr.x+rad-dx, rr.y+rr.h-rad+h);
        SDL_RenderDrawLine(r, rr.x+rr.w-rad+dx, rr.y+rr.h-rad, rr.x+rr.w-rad+dx, rr.y+rr.h-rad+h);
    }
}
static void drawRoundedRectOutline(SDL_Renderer* r, SDL_Rect rr, int rad, SDL_Color c){
    setc(r,c);
    for (int x=rr.x+rad; x<rr.x+rr.w-rad; ++x){ SDL_RenderDrawPoint(r,x,rr.y); SDL_RenderDrawPoint(r,x,rr.y+rr.h-1); }
    for (int y=rr.y+rad; y<rr.y+rr.h-rad; ++y){ SDL_RenderDrawPoint(r,rr.x,y); SDL_RenderDrawPoint(r,rr.x+rr.w-1,y); }
    for (int a=0;a<=90;++a){
        float rf=(float)rad,t=(float)a*(float)(M_PI/180.0);
        int dx=(int)(cosf(t)*rf), dy=(int)(sinf(t)*rf);
        SDL_RenderDrawPoint(r, rr.x+rad-dx, rr.y+rad-dy);
        SDL_RenderDrawPoint(r, rr.x+rr.w-rad+dx, rr.y+rad-dy);
        SDL_RenderDrawPoint(r, rr.x+rad-dx, rr.y+rr.h-rad+dy);
        SDL_RenderDrawPoint(r, rr.x+rr.w-rad+dx, rr.y+rr.h-rad+dy);
    }
}
static void drawShadow(SDL_Renderer* r, SDL_Rect rr, int rad,int offx,int offy){
    SDL_Rect s={rr.x+offx,rr.y+offy,rr.w,rr.h}; drawRoundedRectFill(r,s,rad,C_SHADOW);
}
static void drawVignette(SDL_Renderer* r, int w, int h){
    int rad=(int)(sqrtf((float)(w*w+h*h))*0.5f);
    SDL_Color c=C_VIGNETTE;
    for(int k=0;k<5;k++){
        Uint8 a=(Uint8)(c.a/(k+2)); SDL_Color cc={c.r,c.g,c.b,a};
        drawFilledCircle(r,0,0,rad-k*30,cc);
        drawFilledCircle(r,w,0,rad-k*30,cc);
        drawFilledCircle(r,0,h,rad-k*30,cc);
        drawFilledCircle(r,w,h,rad-k*30,cc);
    }
}
static void drawNeonOutline(SDL_Renderer* r, SDL_Rect rr, int rad){
    drawRoundedRectOutline(r, rr, rad, C_LETTER_NEON);
    SDL_Rect rr2={rr.x-1,rr.y-1,rr.w+2,rr.h+2};
    drawRoundedRectOutline(r, rr2, rad+1, C_LETTER_NEON2);
}
static void drawProgress(SDL_Renderer* r, SDL_Rect bar, float t){
    drawRoundedRectFill(r, bar, 8, C_PROG_BG);
    int fillW=(int)(bar.w*clamp01(t));
    if(fillW>0){
        for(int x=0;x<fillW;++x){
            float u=(float)x/(float)(bar.w-1);
            Uint8 R=(Uint8)(C_PROG_A.r+u*(C_PROG_B.r-C_PROG_A.r));
            Uint8 G=(Uint8)(C_PROG_A.g+u*(C_PROG_B.g-C_PROG_A.g));
            Uint8 B=(Uint8)(C_PROG_A.b+u*(C_PROG_B.b-C_PROG_A.b));
            SDL_SetRenderDrawColor(r,R,G,B,255);
            SDL_RenderDrawLine(r,bar.x+x,bar.y,bar.x+x,bar.y+bar.h);
        }
    }
    static float phase=0.0f; phase+=0.02f; if(phase>1) phase-=1;
    int gx=bar.x+(int)((bar.w-1)*phase*clamp01(t));
    SDL_Rect gl={gx-18,bar.y-2,36,bar.h+4}; drawRoundedRectFill(r,gl,8,C_GLINT);
}
static void drawCursorGlow(SDL_Renderer* r,int cx,int cy){
    drawFilledCircle(r,cx,cy,26,C_CURSOR_A);
    drawFilledCircle(r,cx,cy,18,C_CURSOR_B);
    drawFilledCircle(r,cx,cy,12,C_CURSOR_C);
}

/* ====== Recursos ====== */
static SDL_Texture* load_png(SDL_Renderer* ren,const char* path){ return IMG_LoadTexture(ren,path); }
static SDL_Texture* render_text(SDL_Renderer* ren, TTF_Font* font,const char* text, SDL_Color col){
    if(!font) return NULL; SDL_Surface* s=TTF_RenderUTF8_Blended(font,text,col); if(!s) return NULL;
    SDL_Texture* t=SDL_CreateTextureFromSurface(ren,s); SDL_FreeSurface(s); return t;
}
static char* file_read_all(const char* path, long* outSize){
    FILE* f=fopen(path,"rb"); if(!f) return NULL; fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    char* buf=(char*)malloc(sz+1); if(!buf){ fclose(f); return NULL; }
    fread(buf,1,sz,f); fclose(f); buf[sz]=0; if(outSize) *outSize=sz; return buf;
}

/* ====== Niveles/JSON ====== */
static int load_levels_json(const char* path, LevelPack* out){
    long sz=0; char* json=file_read_all(path,&sz); if(!json){ printf("No pude leer %s\n", path); return 0; }
    cJSON* root=cJSON_Parse(json); if(!root){ free(json); printf("JSON invalido\n"); return 0; }
    cJSON* arr=cJSON_GetObjectItem(root,"levels"); if(!cJSON_IsArray(arr)){ cJSON_Delete(root); free(json); printf("Sin levels[]\n"); return 0; }
    int n=cJSON_GetArraySize(arr); if(n<=0){ cJSON_Delete(root); free(json); printf("levels vacio\n"); return 0; }
    LevelDef* defs=(LevelDef*)calloc(n,sizeof(LevelDef)); int ok=0;
    for(int i=0;i<n;i++){
        cJSON* L=cJSON_GetArrayItem(arr,i);
        cJSON* clue=cJSON_GetObjectItem(L,"clue");
        cJSON* answer=cJSON_GetObjectItem(L,"answer");
        cJSON* letters=cJSON_GetObjectItem(L,"letters");
        if(!cJSON_IsString(clue)||!cJSON_IsString(answer)||!cJSON_IsArray(letters)) continue;
        LevelDef d; memset(&d,0,sizeof(d));
        strncpy(d.clue, clue->valuestring, sizeof(d.clue)-1);
        strncpy(d.answer, answer->valuestring, sizeof(d.answer)-1);
        d.answer_len=(int)strlen(d.answer);
        int lc=cJSON_GetArraySize(letters); if(lc>MAX_LETTERS) lc=MAX_LETTERS;
        for(int j=0;j<lc;j++){ cJSON* it=cJSON_GetArrayItem(letters,j);
            if(cJSON_IsString(it) && it->valuestring && it->valuestring[0]) d.letters[d.letter_count++] = it->valuestring[0];
        }
        if(d.answer_len>0 && d.letter_count>0) defs[ok++]=d;
    }
    cJSON_Delete(root); free(json);
    if(ok==0){ free(defs); return 0; }
    out->defs=defs; out->count=ok; return 1;
}

/* ====== Partículas ====== */
static void particles_clear(void){ for(int i=0;i<MAX_PARTICLES;i++) g_P[i].alive=0; }
static void particles_spawn(float x,float y,int count, SDL_Color a, SDL_Color b){
    for(int n=0;n<count;n++){
        for(int i=0;i<MAX_PARTICLES;i++) if(!g_P[i].alive){
            float ang = ((float)rand()/RAND_MAX)*(float)(2*M_PI);
            float spd = 80.0f + ((float)rand()/RAND_MAX)*160.0f;
            g_P[i].x=x; g_P[i].y=y; g_P[i].vx=cosf(ang)*spd; g_P[i].vy=sinf(ang)*spd;
            g_P[i].life=g_P[i].maxlife=0.55f + ((float)rand()/RAND_MAX)*0.35f;
            g_P[i].col = (n%2==0)? a : b;
            g_P[i].alive=1; break;
        }
    }
}
static void particles_update_draw(SDL_Renderer* r, float dt){
    for(int i=0;i<MAX_PARTICLES;i++) if(g_P[i].alive){
        g_P[i].life -= dt; if(g_P[i].life<=0){ g_P[i].alive=0; continue; }
        g_P[i].x += g_P[i].vx*dt; g_P[i].y += g_P[i].vy*dt;
        float a = g_P[i].life/g_P[i].maxlife;
        SDL_Color c=g_P[i].col; c.a=(Uint8)(180*a);
        int rad=(int)(2+2*(1.0f-a));
        setc(r,c);
        for(int dy=-rad;dy<=rad;++dy){ int w=(int)floorf(sqrtf((float)(rad*rad - dy*dy))); SDL_RenderDrawLine(r,(int)g_P[i].x-w,(int)g_P[i].y+dy,(int)g_P[i].x+w,(int)g_P[i].y+dy); }
    }
}

/* ====== Letras helpers ====== */
static void free_letter_textures(Letter* L, int n){ for(int i=0;i<n;i++){ if(L[i].tex){ SDL_DestroyTexture(L[i].tex); L[i].tex=NULL; } } }
static SDL_Texture* make_letter_tile(SDL_Renderer* ren, TTF_Font* font, int size, char ch){
    char buf[4]={ch,0,0,0};
    SDL_Texture* gtxt = font ? render_text(ren,font,buf,(SDL_Color){70,60,50,255}) : NULL;
    SDL_Texture* tile = SDL_CreateTexture(ren,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,size,size);
    SDL_SetTextureBlendMode(tile,SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(ren,tile);
    drawRoundedRectFill(ren,(SDL_Rect){0,0,size,size},18,C_LETTER_FILL);
    if(gtxt){
        int tw,th; SDL_QueryTexture(gtxt,NULL,NULL,&tw,&th);
        SDL_Rect dst={(size-tw)/2,(size-th)/2,tw,th};
        SDL_RenderCopy(ren,gtxt,NULL,&dst);
        SDL_DestroyTexture(gtxt);
    }
    SDL_SetRenderTarget(ren,NULL);
    return tile;
}

/* ====== Construcción de nivel (C puro) ====== */
static void refresh_hint(void){
    if(g_texPista){ SDL_DestroyTexture(g_texPista); g_texPista=NULL; }
    g_texPista = render_text(g_ren, g_font_big?g_font_big:g_font_lbl, g_cur.clue, C_TEXT_MAIN);
    if(g_texPista){ SDL_QueryTexture(g_texPista,NULL,NULL,&g_rectPista.w,&g_rectPista.h); g_rectPista.x=(W-g_rectPista.w)/2; g_rectPista.y=26; }
}
static void rebuild_level(void){
    // ranuras
    g_slots = g_cur.answer_len; if(g_slots>MAX_SLOTS) g_slots=MAX_SLOTS;
    int totalW=g_slots*SLOT_SIZE+(g_slots-1)*GAP, startX=(W-totalW)/2, centerY=H/2+40;
    for(int i=0;i<g_slots;i++){ g_ranuras[i]=(Rect){ startX+i*(SLOT_SIZE+GAP), centerY-SLOT_SIZE/2, SLOT_SIZE, SLOT_SIZE, 1 }; g_slotGlow[i]=0; }

    // bandeja
    int cols=3, rows=(g_cur.letter_count+cols-1)/cols; if(rows<2) rows=2; if(rows>4) rows=4;
    int panelW=12+cols*LETTER_SIZE+(cols-1)*GAP+12, panelH=12+rows*LETTER_SIZE+(rows-1)*GAP+12;
    g_trayPanel=(SDL_Rect){ 28, H-(28+panelH), panelW, panelH };

    // letras
    free_letter_textures(g_letras, g_nLetters);
    g_nLetters = g_cur.letter_count; if(g_nLetters>MAX_LETTERS) g_nLetters=MAX_LETTERS;
    memset(g_letras,0,sizeof(g_letras));
    for(int i=0;i<g_nLetters;i++){
        g_letras[i]=(Letter){0,0,LETTER_SIZE,LETTER_SIZE,1,g_cur.letters[i],NULL,1,0,1,0,-1,0,0};
    }
    for(int i=0;i<g_nLetters;i++){
        int cols2=3; int r=i/cols2, c=i%cols2;
        g_letras[i].x=g_trayPanel.x+12+c*(LETTER_SIZE+GAP);
        g_letras[i].y=g_trayPanel.y+12+r*(LETTER_SIZE+GAP);
        g_letras[i].homeX=g_letras[i].x; g_letras[i].homeY=g_letras[i].y;
        char path[256]; snprintf(path,sizeof(path),"assets/L_%c.png", g_letras[i].glyph);
        SDL_Texture* t=load_png(g_ren,path);
        if(!t){ t = make_letter_tile(g_ren, g_font_big, LETTER_SIZE, g_letras[i].glyph); }
        g_letras[i].tex=t;
    }
    refresh_hint();
    particles_clear();
}

/* ====== MAIN ====== */
int main(int argc, char** argv){
    (void)argc; (void)argv;
    srand(1234);
    init_themes(); int themeIndex=0;

    /* Kinect */
    if (!ks_init()){ printf("No se pudo iniciar Kinect.\n"); return 1; }
    ks_set_smoothing(0.5f,0.5f,0.0f,0.05f,0.04f);

    /* SDL */
    if (SDL_Init(SDL_INIT_VIDEO)!=0){ printf("SDL_Init: %s\n", SDL_GetError()); ks_shutdown(); return 1; }
    int imgFlags=IMG_INIT_PNG; if((IMG_Init(imgFlags)&imgFlags)!=imgFlags){ printf("IMG_Init: %s\n", IMG_GetError()); }
    if (TTF_Init()!=0){ printf("TTF_Init: %s\n", TTF_GetError()); }

    g_win=SDL_CreateWindow("PRUEBA - Niveles + cursor centrado",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W,H,0);
    g_ren=SDL_CreateRenderer(g_win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(g_ren,W,H);

    /* Fuentes */
    g_font_big=TTF_OpenFont("assets/font.ttf",36);
    g_font_lbl=TTF_OpenFont("assets/font.ttf",20);

    /* Niveles */
    if(!load_levels_json("assets/levels.json",&g_pack)){
        // Fallback
        g_pack.count=1;
        g_pack.defs=(LevelDef*)calloc(1,sizeof(LevelDef));
        strcpy(g_pack.defs[0].clue,"PISTA: forma 'COSA'");
        strcpy(g_pack.defs[0].answer,"COSA");
        g_pack.defs[0].answer_len=4;
        g_pack.defs[0].letters[0]='C'; g_pack.defs[0].letters[1]='O'; g_pack.defs[0].letters[2]='S'; g_pack.defs[0].letters[3]='A';
        g_pack.defs[0].letters[4]='X'; g_pack.defs[0].letters[5]='Z'; g_pack.defs[0].letter_count=6;
    }
    g_levelIndex=0;
    g_cur = g_pack.defs[g_levelIndex];

    /* Pista inicial + botón */
    refresh_hint();
    g_texModoMouse=render_text(g_ren, g_font_lbl, "MODO: MOUSE", C_PANEL_LABEL);
    g_texModoKine =render_text(g_ren, g_font_lbl, "MODO: KINECT", C_PANEL_LABEL);

    rebuild_level();

    /* Estado runtime */
    int running=1, cx=W/2, cy=H/2;
    int use_mouse=0; SDL_ShowCursor(SDL_DISABLE);
    float hx=0,hy=0,hz=0,hz_at_pick=0;
    int dragging=-1, offx=0, offy=0, hoverIdx=-1;
    Uint32 hover_start=0, lastTicks=SDL_GetTicks();
    const Uint32 HOLD_MS=260; const float RELEASE_DELTA_Z=-0.15f; Uint32 rehoverStart=0;
    Uint32 btnHoverStart=0;
    int capturing=0;

    // Avance de nivel
    int levelCompleting=0; Uint32 levelNextAt=0;

    // Centro al volver a Kinect
    int forceKinectCenter=0; Uint32 kinectCenterUntil=0; float ripple=0.0f;

    while(running){
        Uint32 now=SDL_GetTicks(); float dt=(now-lastTicks)/1000.0f; if(dt>0.033f) dt=0.033f; lastTicks=now;

        /* Eventos */
        SDL_Event e;
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT) running=0;
            if(e.type==SDL_KEYDOWN){
                if(e.key.keysym.sym==SDLK_ESCAPE) running=0;
                if(e.key.keysym.sym==SDLK_t){ themeIndex=(themeIndex+1)%3; apply_theme(themeIndex); refresh_hint(); }
            }
            if(use_mouse){
                if(e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT){
                    int mx=e.button.x,my=e.button.y;
                    Rect b={g_btnMode.x,g_btnMode.y,g_btnMode.w,g_btnMode.h,1};
                    if(point_in(b,mx,my)){
                        // Mouse -> Kinect
                        use_mouse=0; SDL_ShowCursor(SDL_DISABLE);
                        dragging=-1; hover_start=rehoverStart=0; btnHoverStart=0;
                        forceKinectCenter=1; kinectCenterUntil=now+450; cx=W/2; cy=H/2; ripple=1.0f;
                        continue;
                    }
                    for(int i=g_nLetters-1;i>=0;i--){
                        Rect lr={g_letras[i].x,g_letras[i].y,g_letras[i].w,g_letras[i].h,1};
                        if(point_in(lr,mx,my)){
                            dragging=i; offx=mx-g_letras[i].x; offy=my-g_letras[i].y;
                            g_letras[i].tScale=1.1f; g_letras[i].tZlift=-10.0f;
                            SDL_CaptureMouse(SDL_TRUE); capturing=1; break;
                        }
                    }
                }
                if(e.type==SDL_MOUSEMOTION){
                    if(dragging>=0){ g_letras[dragging].x = e.motion.x - offx; g_letras[dragging].y = e.motion.y - offy; }
                }
                if(e.type==SDL_MOUSEBUTTONUP && e.button.button==SDL_BUTTON_LEFT){
                    if(dragging>=0){
                        int cxL=g_letras[dragging].x+g_letras[dragging].w/2;
                        int cyL=g_letras[dragging].y+g_letras[dragging].h/2;
                        g_letras[dragging].snappedTo=-1;
                        for(int r=0;r<g_slots;r++){
                            Rect rr=g_ranuras[r]; Rect ex={rr.x-14,rr.y-14,rr.w+28,rr.h+28,1};
                            if(point_in(ex,cxL,cyL)){ g_letras[dragging].x=rr.x; g_letras[dragging].y=rr.y; g_letras[dragging].snappedTo=r;
                                particles_spawn(rr.x+rr.w/2, rr.y+rr.h/2, 18, C_FX_A, C_FX_B); break; }
                        }
                        g_letras[dragging].tScale=1.0f; g_letras[dragging].tZlift=0.0f; dragging=-1;
                    }
                    if(capturing){ SDL_CaptureMouse(SDL_FALSE); capturing=0; }
                }
            }
        } /* fin eventos */

        /* Cursor */
        if(use_mouse){
            SDL_GetMouseState(&cx,&cy);
        }else{
            if(forceKinectCenter && now<kinectCenterUntil){ cx=W/2; cy=H/2; }
            else{
                forceKinectCenter=0;
                if(ks_update()){ ks_get_right_hand(&hx,&hy,&hz); cx=(int)(hx*W); cy=(int)(hy*H); }
            }
        }

        /* Botón modo (abajo-derecha) */
        int tw=0,th=0; SDL_Texture* tMode = use_mouse?g_texModoKine:g_texModoMouse;
        if(tMode) SDL_QueryTexture(tMode,NULL,NULL,&tw,&th);
        g_btnMode.w=(tw>0?tw+28:132); g_btnMode.h=(th>0?th+14:38);
        g_btnMode.x=W-18-g_btnMode.w; g_btnMode.y=H-18-g_btnMode.h;
        Rect btnR={g_btnMode.x,g_btnMode.y,g_btnMode.w,g_btnMode.h,1};

        if(!use_mouse){
            if(point_in(btnR,cx,cy)){
                if(btnHoverStart==0) btnHoverStart=now;
                if(now-btnHoverStart>=HOLD_MS){
                    // Kinect -> Mouse
                    use_mouse=1; SDL_ShowCursor(SDL_ENABLE);
                    dragging=-1; hover_start=rehoverStart=0; btnHoverStart=0;
                    cx=W/2; cy=H/2; SDL_WarpMouseInWindow(g_win,cx,cy); ripple=1.0f;
                }
            }else btnHoverStart=0;
        }

        /* Interacción Kinect (mano) */
        if(!use_mouse){
            hoverIdx=-1;
            for(int i=0;i<g_nLetters;i++){
                Rect lr={g_letras[i].x,g_letras[i].y,g_letras[i].w,g_letras[i].h,1};
                if(point_in(lr,cx,cy)){ hoverIdx=i; break; }
            }
            if(dragging<0 && hoverIdx>=0){
                if(hover_start==0) hover_start=now;
                if(now-hover_start>=HOLD_MS){
                    dragging=hoverIdx; hover_start=0; rehoverStart=0;
                    offx=cx-g_letras[dragging].x; offy=cy-g_letras[dragging].y;
                    hz_at_pick=hz; g_letras[dragging].tScale=1.12f; g_letras[dragging].tZlift=-10.0f;
                }
            } else if(hoverIdx<0) hover_start=0;

            if(dragging>=0){
                g_letras[dragging].x=cx-offx; g_letras[dragging].y=cy-offy;
                if((hz-hz_at_pick)<=RELEASE_DELTA_Z){
                    int cxL=g_letras[dragging].x+g_letras[dragging].w/2;
                    int cyL=g_letras[dragging].y+g_letras[dragging].h/2;
                    g_letras[dragging].snappedTo=-1;
                    for(int r=0;r<g_slots;r++){
                        Rect rr=g_ranuras[r]; Rect ex={rr.x-14,rr.y-14,rr.w+28,rr.h+28,1};
                        if(point_in(ex,cxL,cyL)){ g_letras[dragging].x=rr.x; g_letras[dragging].y=rr.y; g_letras[dragging].snappedTo=r;
                            particles_spawn(rr.x+rr.w/2, rr.y+rr.h/2, 18, C_FX_A, C_FX_B); break; }
                    }
                    g_letras[dragging].tScale=1.0f; g_letras[dragging].tZlift=0.0f; dragging=-1; rehoverStart=0;
                } else {
                    Rect curR={g_letras[dragging].x,g_letras[dragging].y,g_letras[dragging].w,g_letras[dragging].h,1};
                    if(point_in(curR,cx,cy)){
                        if(rehoverStart==0) rehoverStart=now;
                        if(now-rehoverStart>250){
                            int cxL=g_letras[dragging].x+g_letras[dragging].w/2;
                            int cyL=g_letras[dragging].y+g_letras[dragging].h/2;
                            g_letras[dragging].snappedTo=-1;
                            for(int r=0;r<g_slots;r++){
                                Rect rr=g_ranuras[r]; Rect ex={rr.x-14,rr.y-14,rr.w+28,rr.h+28,1};
                                if(point_in(ex,cxL,cyL)){ g_letras[dragging].x=rr.x; g_letras[dragging].y=rr.y; g_letras[dragging].snappedTo=r;
                                    particles_spawn(rr.x+rr.w/2, rr.y+rr.h/2, 18, C_FX_A, C_FX_B); break; }
                            }
                            g_letras[dragging].tScale=1.0f; g_letras[dragging].tZlift=0.0f; dragging=-1; rehoverStart=0;
                        }
                    } else rehoverStart=0;
                }
            }
        }

        /* Animación y progreso */
        for(int i=0;i<g_nLetters;i++){
            float tgt=(hoverIdx==i && dragging<0)?1.05f:1.0f;
            g_letras[i].tScale=tgt;
            g_letras[i].scale=damp(g_letras[i].scale,g_letras[i].tScale,10.0f,dt);
            g_letras[i].zlift=damp(g_letras[i].zlift,g_letras[i].tZlift,10.0f,dt);
        }

        int correct=0, filled=0;
        int cxChk=(dragging>=0)?(g_letras[dragging].x+g_letras[dragging].w/2):cx;
        int cyChk=(dragging>=0)?(g_letras[dragging].y+g_letras[dragging].h/2):cy;

        for(int r=0;r<g_slots;r++){
            Rect rr=g_ranuras[r]; Rect ex={rr.x-16,rr.y-16,rr.w+32,rr.h+32,1};
            float tgtGlow=point_in(ex,cxChk,cyChk)?1.0f:0.0f;
            g_slotGlow[r]=damp(g_slotGlow[r],tgtGlow,8.0f,dt);

            for(int i=0;i<g_nLetters;i++) if(g_letras[i].snappedTo==r){
                filled++;
                if(toupper((unsigned char)g_letras[i].glyph)==toupper((unsigned char)g_cur.answer[r])) correct++;
                break;
            }
        }
        float prog = g_slots>0 ? (float)correct/(float)g_slots : 0.0f;

        // Avance de nivel
        if(!levelCompleting && g_slots>0 && correct==g_slots && filled==g_slots){
            levelCompleting=1; levelNextAt=now+800;
            for(int r=0;r<g_slots;r++){ SDL_Rect rr2={g_ranuras[r].x,g_ranuras[r].y,g_ranuras[r].w,g_ranuras[r].h};
                particles_spawn(rr2.x+rr2.w/2, rr2.y+rr2.h/2, 24, C_FX_A, C_FX_B);
            }
            particles_spawn(W/2, H/2, 60, C_LETTER_NEON, C_LETTER_NEON2);
        }
        if(levelCompleting && now>=levelNextAt){
            levelCompleting=0;
            g_levelIndex = (g_levelIndex+1) % g_pack.count;
            g_cur = g_pack.defs[g_levelIndex];
            rebuild_level();
        }

        /* ---------- Render ---------- */
        drawGradientV(g_ren,W,H,C_BG_TOP,C_BG_BOT);
        drawVignette(g_ren,W,H);

        if(g_texPista){
            SDL_Rect pill={g_rectPista.x-18,g_rectPista.y-10,g_rectPista.w+36,g_rectPista.h+20};
            drawShadow(g_ren,pill,18,0,6);
            drawRoundedRectFill(g_ren,pill,18,C_PANEL_GLASS);
            SDL_Rect hi={pill.x+6,pill.y+6,pill.w-12,8}; drawRoundedRectFill(g_ren,hi,4,C_PANEL_EDGE);
            SDL_RenderCopy(g_ren,g_texPista,NULL,&g_rectPista);
            SDL_Rect bar={ (W-520)/2, pill.y+pill.h+12, 520, 18 }; drawProgress(g_ren,bar,prog);
        }

        // Botón modo
        drawShadow(g_ren,g_btnMode,14,0,6);
        drawRoundedRectFill(g_ren,g_btnMode,14,C_PANEL_GLASS);
        SDL_Rect hiB={g_btnMode.x+6,g_btnMode.y+6,g_btnMode.w-12,6}; drawRoundedRectFill(g_ren,hiB,4,C_PANEL_EDGE);
        if(tMode){ /* placeholder */ }
        {
            SDL_Texture* t = use_mouse?g_texModoKine:g_texModoMouse; int tw2=0,th2=0;
            if(t){ SDL_QueryTexture(t,NULL,NULL,&tw2,&th2);
                SDL_Rect dst={ g_btnMode.x+(g_btnMode.w-tw2)/2, g_btnMode.y+(g_btnMode.h-th2)/2, tw2, th2 };
                SDL_RenderCopy(g_ren,t,NULL,&dst);
            }
        }
        if(point_in((Rect){g_btnMode.x,g_btnMode.y,g_btnMode.w,g_btnMode.h,1},cx,cy)) drawNeonOutline(g_ren,g_btnMode,14);

        // Ranuras
        for(int i=0;i<g_slots;i++){
            SDL_Rect rr3={g_ranuras[i].x,g_ranuras[i].y,g_ranuras[i].w,g_ranuras[i].h};
            drawShadow(g_ren,rr3,18,0,6);
            drawRoundedRectFill(g_ren,rr3,18,C_SLOT_FILL);
            SDL_Rect inner={rr3.x+4,rr3.y+4,rr3.w-8,8}; drawRoundedRectFill(g_ren,inner,6,C_SLOT_HILITE);
            drawRoundedRectOutline(g_ren,rr3,18,C_SLOT_EDGE);
            if(g_slotGlow[i]>0.01f){
                SDL_Color g=C_LETTER_NEON; g.a=(Uint8)(80*g_slotGlow[i]);
                SDL_Rect rr2={rr3.x-2,rr3.y-2,rr3.w+4,rr3.h+4}; drawRoundedRectOutline(g_ren,rr2,20,g);
            }
        }

        // Bandeja
        drawShadow(g_ren,g_trayPanel,22,0,8);
        drawRoundedRectFill(g_ren,g_trayPanel,22,C_PANEL_GLASS);
        SDL_Rect topHi={g_trayPanel.x+8,g_trayPanel.y+8,g_trayPanel.w-16,8}; drawRoundedRectFill(g_ren,topHi,4,C_PANEL_EDGE);
        if(g_font_lbl){
            SDL_Texture* t=render_text(g_ren,g_font_lbl,"LETRAS",C_PANEL_LABEL);
            if(t){ int twl=0,thl=0; SDL_QueryTexture(t,NULL,NULL,&twl,&thl);
                SDL_Rect dst={g_trayPanel.x+12,g_trayPanel.y-4-thl,twl,thl}; SDL_RenderCopy(g_ren,t,NULL,&dst); SDL_DestroyTexture(t);
            }
        }

        // Letras
        int hoverDrawIdx=-1;
        for(int i=0;i<g_nLetters;i++){
            float pulse=0.015f*sinf(now/200.0f + i)*((hoverIdx==i && dragging<0)?2.5f:1.0f);
            float s=g_letras[i].scale + pulse;
            int rw=(int)(g_letras[i].w*s), rh=(int)(g_letras[i].h*s);
            int cxL=g_letras[i].x+g_letras[i].w/2, cyL=g_letras[i].y+g_letras[i].h/2+(int)g_letras[i].zlift;
            SDL_Rect lr={cxL-rw/2,cyL-rh/2,rw,rh};
            if(g_letras[i].scale>1.02f || g_letras[i].zlift<-1.0f){
                setc(g_ren,(SDL_Color){0,0,0,82}); SDL_Rect sh={lr.x,lr.y+lr.h+8,lr.w,10}; SDL_RenderFillRect(g_ren,&sh);
            }
            drawRoundedRectFill(g_ren,lr,18,C_LETTER_TILE);
            if(g_letras[i].tex) SDL_RenderCopy(g_ren,g_letras[i].tex,NULL,&lr);
            else                drawRoundedRectFill(g_ren,lr,18,C_LETTER_FILL);
            if(point_in((Rect){g_letras[i].x,g_letras[i].y,g_letras[i].w,g_letras[i].h,1},cx,cy) && dragging<0) hoverDrawIdx=i;
        }
        if(hoverDrawIdx>=0){
            int i=hoverDrawIdx;
            int rw=(int)(g_letras[i].w*g_letras[i].scale), rh=(int)(g_letras[i].h*g_letras[i].scale);
            int cxL=g_letras[i].x+g_letras[i].w/2, cyL=g_letras[i].y+g_letras[i].h/2+(int)g_letras[i].zlift;
            SDL_Rect lr={cxL-rw/2,cyL-rh/2,rw,rh}; drawNeonOutline(g_ren,lr,18);
        }

        // Partículas
        particles_update_draw(g_ren,dt);

        // Cursor Kinect + ripple
        if(!use_mouse){
            drawCursorGlow(g_ren,cx,cy);
            if(ripple>0.0f){ float t=1.0f-ripple; int rad=(int)(t*220.0f);
                Uint8 a=(Uint8)(ripple*160);
                SDL_Color ring={C_LETTER_NEON.r,C_LETTER_NEON.g,C_LETTER_NEON.b,a};
                drawRoundedRectOutline(g_ren,(SDL_Rect){W/2-rad,H/2-rad,rad*2,rad*2},rad/3,ring);
                ripple-=dt*1.6f; if(ripple<0) ripple=0;
            }
        }

        SDL_RenderPresent(g_ren);
    }

    /* Limpieza */
    free_letter_textures(g_letras,g_nLetters);
    if(g_texPista) SDL_DestroyTexture(g_texPista);
    if(g_texModoMouse) SDL_DestroyTexture(g_texModoMouse);
    if(g_texModoKine)  SDL_DestroyTexture(g_texModoKine);
    if(g_pack.defs) free(g_pack.defs);
    if(g_font_big) TTF_CloseFont(g_font_big);
    if(g_font_lbl) TTF_CloseFont(g_font_lbl);
    SDL_DestroyRenderer(g_ren); SDL_DestroyWindow(g_win);
    TTF_Quit(); IMG_Quit(); SDL_Quit(); ks_shutdown();
    return 0;
}
