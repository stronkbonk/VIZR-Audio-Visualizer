/*
 ██╗   ██╗██╗███████╗██████╗
 ██║   ██║██║╚══███╔╝██╔══██╗
 ██║   ██║██║  ███╔╝ ██████╔╝
 ╚██╗ ██╔╝██║ ███╔╝  ██╔══██╗
  ╚████╔╝ ██║███████╗██║  ██║
   ╚═══╝  ╚═╝╚══════╝╚═╝  ╚═╝

  VIZR — C++ Audio Visualizer  (SFML 3 + Dear ImGui)
  ─────────────────────────────────────────────────────
  Build with MSYS2 MinGW x64 terminal:
    g++ -std=c++17 -O2 vizr.cpp imgui/imgui.cpp imgui/imgui_draw.cpp \
        imgui/imgui_tables.cpp imgui/imgui_widgets.cpp               \
        imgui-sfml/imgui-SFML.cpp                                    \
        -I./imgui -I./imgui-sfml                                     \
        $(pkg-config --cflags --libs sfml-graphics sfml-audio sfml-window sfml-system) \
        -lopengl32 -lwinmm -lgdi32 -mwindows -o vizr.exe

  Controls:
    SPACE   play/pause   F/F11  fullscreen   <- ->  seek 5s   ESC  quit
*/

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <cmath>
#include <complex>
#include <vector>
namespace kissfft {
using cpx = std::complex<float>;
inline void fft(std::vector<cpx>& buf) {
    const size_t N = buf.size(); if (N<=1) return;
    std::vector<cpx> even(N/2), odd(N/2);
    for (size_t i=0;i<N/2;i++){even[i]=buf[2*i];odd[i]=buf[2*i+1];}
    fft(even); fft(odd);
    for (size_t k=0;k<N/2;k++){
        cpx t=std::polar(1.0f,-2.0f*(float)M_PI*k/N)*odd[k];
        buf[k]=even[k]+t; buf[k+N/2]=even[k]-t;
    }
}
inline std::vector<float> magnitude(const std::vector<float>& in, int fsz=2048){
    std::vector<cpx> buf(fsz);
    int n=std::min((int)in.size(),fsz);
    for (int i=0;i<n;i++){float w=0.5f*(1.0f-std::cos(2.0f*(float)M_PI*i/(fsz-1)));buf[i]={in[i]*w,0};}
    fft(buf);
    std::vector<float> mag(fsz/2);
    for (int i=0;i<fsz/2;i++){float m=std::abs(buf[i])/(fsz/2);mag[i]=std::max(0.0f,(20.0f*std::log10(m+1e-9f)+80.0f)/80.0f);}
    return mag;
}
}

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFML/Window.hpp>
#include <imgui.h>
#include <imgui-SFML.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <string>

struct Color3{float r,g,b;};
inline Color3 lerpColor(Color3 a,Color3 b,float t){t=std::clamp(t,0.0f,1.0f);return{a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t};}
inline sf::Color toSF(Color3 c,float a=1.0f){return sf::Color((uint8_t)std::clamp(c.r*255.0f,0.0f,255.0f),(uint8_t)std::clamp(c.g*255.0f,0.0f,255.0f),(uint8_t)std::clamp(c.b*255.0f,0.0f,255.0f),(uint8_t)std::clamp(a*255.0f,0.0f,255.0f));}
inline sf::Color lerpSF(Color3 a,Color3 b,float t,float al=1.0f){return toSF(lerpColor(a,b,t),al);}
static std::string fmtTime(float s){int m=(int)s/60,sec=(int)s%60;char buf[16];snprintf(buf,sizeof(buf),"%d:%02d",m,sec);return buf;}

static void drawLine(sf::RenderTarget& rt,sf::Vector2f a,sf::Vector2f b,sf::Color ca,sf::Color cb,float thick=1.0f){
    sf::Vector2f d=b-a;float len=std::sqrt(d.x*d.x+d.y*d.y);if(len<0.001f)return;
    sf::Vector2f p(-d.y/len,d.x/len);float h=thick*0.5f;
    sf::VertexArray q(sf::PrimitiveType::TriangleFan,4);
    q[0]={a+p*h,ca};q[1]={b+p*h,cb};q[2]={b-p*h,cb};q[3]={a-p*h,ca};rt.draw(q);
}
static void drawPoly(sf::RenderTarget& rt,const std::vector<sf::Vector2f>& pts,sf::Color fill,sf::Color edge){
    if(pts.size()<3)return;
    sf::VertexArray fan(sf::PrimitiveType::TriangleFan,pts.size()+1);
    float cx=0,cy=0;for(auto&p:pts){cx+=p.x;cy+=p.y;}
    fan[0]={sf::Vector2f(cx/pts.size(),cy/pts.size()),fill};
    for(size_t i=0;i<pts.size();i++)fan[i+1]={pts[i],sf::Color(fill.r,fill.g,fill.b,60)};
    rt.draw(fan);
    sf::VertexArray ol(sf::PrimitiveType::LineStrip,pts.size()+1);
    for(size_t i=0;i<=pts.size();i++)ol[i]={pts[i%pts.size()],edge};rt.draw(ol);
}

struct Theme{std::string name;Color3 a,b,bg;};
static const std::vector<Theme> THEMES={
    {"Neon",  {0.91f,1.00f,0.00f},{1.00f,0.24f,0.67f},{0.02f,0.02f,0.02f}},
    {"Cyber", {0.00f,0.94f,1.00f},{0.00f,0.27f,1.00f},{0.00f,0.02f,0.03f}},
    {"Fire",  {1.00f,0.42f,0.00f},{1.00f,0.00f,0.25f},{0.02f,0.00f,0.00f}},
    {"Matrix",{0.00f,1.00f,0.53f},{0.00f,0.80f,1.00f},{0.00f,0.02f,0.01f}},
    {"Purple",{0.80f,0.00f,1.00f},{1.00f,0.00f,0.67f},{0.02f,0.00f,0.03f}},
    {"Sunset",{1.00f,0.84f,0.00f},{1.00f,0.27f,0.00f},{0.02f,0.01f,0.00f}},
    {"Ice",   {0.53f,0.81f,1.00f},{0.80f,0.95f,1.00f},{0.00f,0.01f,0.02f}},
    {"Vapor", {1.00f,0.41f,0.71f},{0.67f,0.33f,1.00f},{0.02f,0.00f,0.02f}},
    {"Toxic", {0.55f,1.00f,0.00f},{0.00f,1.00f,0.55f},{0.01f,0.02f,0.00f}},
    {"Mono",  {1.00f,1.00f,1.00f},{0.40f,0.40f,0.40f},{0.01f,0.01f,0.01f}},
};

class AudioEngine{
public:
    sf::SoundBuffer buffer;
    sf::Sound sound{buffer};
    std::vector<float> samples,smoothFFT;
    unsigned int sampleRate=44100,numChannels=1;
    float duration=0.0f;
    std::string filename;
    bool loaded=false;
    static constexpr int FFT_SIZE=2048,N_BINS=512;

    bool load(const std::string& path){
        if(!buffer.loadFromFile(path))return false;
        sound.setBuffer(buffer);
        sampleRate=buffer.getSampleRate();
        numChannels=buffer.getChannelCount();
        duration=buffer.getDuration().asSeconds();
        loaded=true;
        const std::int16_t* raw=buffer.getSamples();
        size_t count=buffer.getSampleCount();
        samples.resize(count/numChannels);
        for(size_t i=0;i<samples.size();i++){
            float s=0;for(unsigned int c=0;c<numChannels;c++)s+=(float)raw[i*numChannels+c]/32768.0f;
            samples[i]=s/(float)numChannels;
        }
        smoothFFT.assign(N_BINS,0.0f);
        size_t sl=path.find_last_of("/\\");
        filename=(sl==std::string::npos)?path:path.substr(sl+1);
        return true;
    }
    void play(){if(loaded)sound.play();}
    void toggle(){
        if(!loaded)return;
        if(sound.getStatus()==sf::Sound::Status::Playing)sound.pause();else sound.play();
    }
    bool isPlaying()const{return sound.getStatus()==sf::Sound::Status::Playing;}
    float getPosition()const{return loaded?sound.getPlayingOffset().asSeconds():0.0f;}
    void seek(float s){if(!loaded)return;s=std::clamp(s,0.0f,duration-0.01f);sound.setPlayingOffset(sf::seconds(s));}

    void analyze(float sk,std::vector<float>& oFFT,std::vector<float>& oWave){
        oFFT.resize(N_BINS,0.0f);oWave.resize(1024,0.0f);
        if(!loaded)return;
        int start=(int)(getPosition()*sampleRate);
        int wLen=std::min(FFT_SIZE,(int)samples.size()-start);
        if(wLen<=0)return;
        int wv=std::min(1024,wLen);
        for(int i=0;i<wv;i++)oWave[i]=(start+i<(int)samples.size())?samples[start+i]:0.0f;
        std::vector<float> win(FFT_SIZE,0.0f);
        for(int i=0;i<wLen;i++)win[i]=samples[start+i];
        auto mag=kissfft::magnitude(win,FFT_SIZE);
        int sl=(int)mag.size();
        for(int i=0;i<N_BINS;i++){
            int s=i*sl/N_BINS,e=(i+1)*sl/N_BINS;float v=0;
            for(int j=s;j<e;j++)v+=mag[j];oFFT[i]=v/std::max(1,e-s);
        }
        for(int i=0;i<N_BINS;i++)smoothFFT[i]=smoothFFT[i]*sk+oFFT[i]*(1.0f-sk);
        oFFT=smoothFFT;
    }
};

struct VizContext{
    const std::vector<float>&fft,&wave;
    const Theme&theme;float gain,time;
    sf::RenderTarget&target;sf::FloatRect bounds;
};

// ── Visualizers ──────────────────────────────────────────────────────────────
static void vizBars(VizContext&ctx){
    auto&rt=ctx.target;auto&b=ctx.bounds;
    const int N=90;float bw=b.size.x/N;
    for(int i=0;i<N;i++){
        float v=std::clamp(ctx.fft[i*(int)ctx.fft.size()/N]*ctx.gain,0.0f,1.0f);
        float h=v*b.size.y*0.88f,x=b.position.x+i*bw,y=b.position.y+b.size.y-h;
        sf::Color c=lerpSF(ctx.theme.a,ctx.theme.b,(float)i/N);
        sf::RectangleShape bar({bw*0.82f,h});bar.setPosition({x+bw*0.09f,y});bar.setFillColor(c);rt.draw(bar);
        sf::RectangleShape cap({bw*0.82f,2.5f});cap.setPosition({x+bw*0.09f,y-2.5f});cap.setFillColor(sf::Color::White);rt.draw(cap);
        sf::RectangleShape ref({bw*0.82f,h*0.25f});ref.setPosition({x+bw*0.09f,b.position.y+b.size.y});ref.setFillColor(sf::Color(c.r,c.g,c.b,45));rt.draw(ref);
    }
}

static void vizWaveform(VizContext&ctx){
    auto&rt=ctx.target;auto&b=ctx.bounds;
    int N=(int)ctx.wave.size();if(N<2)return;
    float cy=b.position.y+b.size.y*0.5f;
    sf::VertexArray va(sf::PrimitiveType::LineStrip,N),va2(sf::PrimitiveType::LineStrip,N);
    for(int i=0;i<N;i++){
        float x=b.position.x+(float)i/(N-1)*b.size.x;
        float y=cy-ctx.wave[i]*ctx.gain*b.size.y*0.42f;
        float amp=std::abs(ctx.wave[i]);
        va[i]={sf::Vector2f(x,y),lerpSF(ctx.theme.a,ctx.theme.b,amp)};
        va2[i]={sf::Vector2f(x,2*cy-y),lerpSF(ctx.theme.a,ctx.theme.b,amp,0.2f)};
    }
    rt.draw(va2);rt.draw(va);
}

static void vizMirror(VizContext&ctx){
    auto&rt=ctx.target;auto&b=ctx.bounds;
    const int N=70;float cx=b.position.x+b.size.x*0.5f,cy=b.position.y+b.size.y*0.5f,bw=b.size.x*0.5f/N;
    for(int i=0;i<N;i++){
        float v=std::clamp(ctx.fft[i*(int)ctx.fft.size()/(N*2)]*ctx.gain,0.0f,1.0f);
        float h=v*b.size.y*0.45f;sf::Color c=lerpSF(ctx.theme.a,ctx.theme.b,(float)i/N);
        for(int side:{-1,1}){
            float x=(side==1)?cx+i*bw:cx-(i+1)*bw;
            sf::RectangleShape b1({bw*0.85f,h});b1.setPosition({x+bw*0.075f,cy-h});b1.setFillColor(c);rt.draw(b1);
            sf::RectangleShape b2({bw*0.85f,h});b2.setPosition({x+bw*0.075f,cy});b2.setFillColor(sf::Color(c.r,c.g,c.b,120));rt.draw(b2);
        }
    }
    sf::RectangleShape ln({b.size.x,1.5f});ln.setPosition({b.position.x,cy});ln.setFillColor(toSF(ctx.theme.a,0.5f));rt.draw(ln);
}

static void vizRadial(VizContext&ctx){
    auto&rt=ctx.target;auto&b=ctx.bounds;
    float cx=b.position.x+b.size.x*0.5f,cy=b.position.y+b.size.y*0.5f;
    float R=std::min(b.size.x,b.size.y)*0.42f,base=R*0.28f;
    for(int i=0;i<200;i++){
        float v=std::clamp(ctx.fft[i*(int)ctx.fft.size()/200]*ctx.gain,0.0f,1.0f);
        float a=(float)i/200*2.0f*(float)M_PI-(float)M_PI/2;
        float r1=base+v*(R-base);
        drawLine(rt,{cx+base*(float)cos(a),cy+base*(float)sin(a)},{cx+r1*(float)cos(a),cy+r1*(float)sin(a)},lerpSF(ctx.theme.a,ctx.theme.b,(float)i/200,0.9f),sf::Color::White,1.5f+v*2.0f);
    }
    sf::CircleShape ring(base);ring.setOrigin({base,base});ring.setPosition({cx,cy});
    ring.setFillColor(sf::Color::Transparent);ring.setOutlineThickness(1.0f);ring.setOutlineColor(toSF(ctx.theme.a,0.25f));rt.draw(ring);
}

static void vizRadialFilled(VizContext&ctx){
    auto&rt=ctx.target;auto&b=ctx.bounds;
    float cx=b.position.x+b.size.x*0.5f,cy=b.position.y+b.size.y*0.5f;
    float R=std::min(b.size.x,b.size.y)*0.44f,base=R*0.22f;
    for(int pass=0;pass<2;pass++){
        float sc=(pass==0)?1.0f:0.68f;Color3 col=(pass==0)?ctx.theme.a:ctx.theme.b;
        sf::VertexArray fan(sf::PrimitiveType::TriangleFan,258);
        fan[0]={sf::Vector2f(cx,cy),toSF(col,0.3f)};
        for(int i=0;i<=256;i++){
            float v=std::clamp(ctx.fft[(i%256)*(int)ctx.fft.size()/256]*ctx.gain*sc,0.0f,1.0f);
            float a=(float)(i%256)/256*2.0f*(float)M_PI-(float)M_PI/2;
            float r=base+v*(R*sc-base);
            fan[i+1]={sf::Vector2f(cx+r*(float)cos(a),cy+r*(float)sin(a)),lerpSF(col,ctx.theme.b,v,0.5f)};
        }
        rt.draw(fan);
    }
    sf::CircleShape dot(12.0f);dot.setOrigin({12,12});dot.setPosition({cx,cy});dot.setFillColor(toSF(ctx.theme.a));rt.draw(dot);
}

static void vizLissajous(VizContext&ctx){
    auto&rt=ctx.target;auto&b=ctx.bounds;
    float cx=b.position.x+b.size.x*0.5f,cy=b.position.y+b.size.y*0.5f;
    float rx=b.size.x*0.44f,ry=b.size.y*0.44f;
    int N=(int)ctx.wave.size()/2;if(N<2)return;
    sf::VertexArray va(sf::PrimitiveType::LineStrip,N);
    for(int i=0;i<N;i++)va[i]={sf::Vector2f(cx+ctx.wave[i]*ctx.gain*rx,cy+ctx.wave[i+N]*ctx.gain*ry),lerpSF(ctx.theme.a,ctx.theme.b,(float)i/N)};
    rt.draw(va);
}

static std::deque<std::vector<float>> spectroHist;
static void vizSpectrogram(VizContext&ctx){
    std::vector<float> row(120);
    for(int i=0;i<120;i++)row[i]=std::clamp(ctx.fft[i*(int)ctx.fft.size()/120]*ctx.gain,0.0f,1.0f);
    spectroHist.push_back(row);if((int)spectroHist.size()>200)spectroHist.pop_front();
    auto&rt=ctx.target;auto&b=ctx.bounds;
    float cw=b.size.x/200,rh=b.size.y/120;
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    for(int c=0;c<(int)spectroHist.size();c++)for(int r=0;r<120;r++){
        float v=spectroHist[c][r];sf::Color col=lerpSF(ctx.theme.b,ctx.theme.a,v,v);
        float x0=b.position.x+c*cw,x1=x0+cw+1,y0=b.position.y+(119-r)*rh,y1=y0+rh+1;
        va.append({{x0,y0},col});va.append({{x1,y0},col});va.append({{x1,y1},col});
        va.append({{x0,y0},col});va.append({{x1,y1},col});va.append({{x0,y1},col});
    }
    rt.draw(va);
}

static std::deque<std::vector<float>> waterfallHist;
static void vizWaterfall(VizContext&ctx){
    std::vector<float> row(ctx.fft.begin(),ctx.fft.begin()+std::min(256,(int)ctx.fft.size()));
    waterfallHist.push_back(row);if((int)waterfallHist.size()>55)waterfallHist.pop_front();
    auto&rt=ctx.target;auto&b=ctx.bounds;int R=(int)waterfallHist.size();
    for(int j=0;j<R;j++){
        float al=(float)(j+1)/R,yO=b.position.y+b.size.y-j*(b.size.y/60.0f);
        const auto&d=waterfallHist[j];int N=(int)d.size();
        sf::VertexArray fill(sf::PrimitiveType::TriangleStrip);
        for(int i=0;i<N;i++){
            float x=b.position.x+(float)i/(N-1)*b.size.x;
            float v=std::clamp(d[i]*ctx.gain,0.0f,1.0f),y=yO-v*(b.size.y/5.0f)*al;
            sf::Color c=lerpSF(ctx.theme.a,ctx.theme.b,v,al*0.7f);
            fill.append({sf::Vector2f(x,y),c});fill.append({sf::Vector2f(x,yO),sf::Color(0,0,0,0)});
        }
        rt.draw(fill);
    }
}

static void vizCircularWave(VizContext&ctx){
    auto&rt=ctx.target;auto&b=ctx.bounds;
    float cx=b.position.x+b.size.x*0.5f,cy=b.position.y+b.size.y*0.5f;
    float R=std::min(b.size.x,b.size.y)*0.38f;
    int N=std::min(1024,(int)ctx.wave.size());if(N<2)return;
    sf::VertexArray va(sf::PrimitiveType::LineStrip,N+1);
    for(int i=0;i<=N;i++){
        int ii=i%N;float a=(float)ii/N*2.0f*(float)M_PI-(float)M_PI/2;
        float r=R+ctx.wave[ii]*ctx.gain*R*0.45f;
        va[i]={sf::Vector2f(cx+r*(float)cos(a),cy+r*(float)sin(a)),lerpSF(ctx.theme.a,ctx.theme.b,std::abs(ctx.wave[ii]))};
    }
    rt.draw(va);
}

static void vizBlob(VizContext&ctx){
    auto&rt=ctx.target;auto&b=ctx.bounds;
    float cx=b.position.x+b.size.x*0.5f,cy=b.position.y+b.size.y*0.5f;
    float R=std::min(b.size.x,b.size.y)*0.40f;
    const int N=128;std::vector<float> radii(N);
    for(int i=0;i<N;i++)radii[i]=std::clamp(ctx.fft[i*(int)ctx.fft.size()/(N*2)]*ctx.gain,0.0f,1.0f);
    for(int s=0;s<3;s++){std::vector<float>tmp=radii;for(int i=0;i<N;i++)radii[i]=0.25f*tmp[(i-1+N)%N]+0.5f*tmp[i]+0.25f*tmp[(i+1)%N];}
    for(int layer=2;layer>=0;layer--){
        float sc=1.0f-layer*0.15f;Color3 lc=lerpColor(ctx.theme.a,ctx.theme.b,(float)layer/2);
        std::vector<sf::Vector2f> pts(N);
        for(int i=0;i<N;i++){float a=(float)i/N*2.0f*(float)M_PI;float r=(0.22f+radii[i]*0.78f)*R*sc;pts[i]={cx+r*(float)cos(a),cy+r*(float)sin(a)};}
        drawPoly(rt,pts,toSF(lc,60),toSF(lc,200));
    }
}

static void vizStarburst(VizContext&ctx){
    auto&rt=ctx.target;auto&b=ctx.bounds;
    float cx=b.position.x+b.size.x*0.5f,cy=b.position.y+b.size.y*0.5f,R=std::min(b.size.x,b.size.y)*0.46f;
    for(int i=0;i<72;i++){
        float v=std::clamp(ctx.fft[i*(int)ctx.fft.size()/72]*ctx.gain,0.0f,1.0f),a=(float)i/72*2.0f*(float)M_PI;
        sf::Color c=lerpSF(ctx.theme.a,ctx.theme.b,(float)i/72,0.9f);
        drawLine(rt,{cx,cy},{cx+v*R*(float)cos(a),cy+v*R*(float)sin(a)},c,sf::Color::White,1.5f+v*2.5f);
        for(float off:{-0.15f,0.15f}){float a2=a+off;sf::Color c2(c.r,c.g,c.b,100);drawLine(rt,{cx+v*R*0.5f*(float)cos(a),cy+v*R*0.5f*(float)sin(a)},{cx+v*R*0.72f*(float)cos(a2),cy+v*R*0.72f*(float)sin(a2)},c2,c2,0.8f);}
    }
    sf::CircleShape dot(8.0f);dot.setOrigin({8,8});dot.setPosition({cx,cy});dot.setFillColor(toSF(ctx.theme.a));rt.draw(dot);
}

static void vizBars3D(VizContext&ctx){
    auto&rt=ctx.target;auto&b=ctx.bounds;
    const int N=50;float bw=b.size.x/N*0.92f,dep=bw*0.55f;
    for(int i=N-1;i>=0;i--){
        float v=std::clamp(ctx.fft[i*(int)ctx.fft.size()/N]*ctx.gain,0.0f,1.0f);
        float h=v*b.size.y*0.82f,x=b.position.x+i*(b.size.x/N),y=b.position.y+b.size.y;
        Color3 c=lerpColor(ctx.theme.a,ctx.theme.b,(float)i/N);
        Color3 ct=lerpColor(c,{1,1,1},0.3f),cs=lerpColor(c,{0,0,0},0.4f);
        sf::VertexArray f(sf::PrimitiveType::TriangleFan,4);
        f[0]={{x,y},toSF(c,0.9f)};f[1]={{x+bw,y},toSF(c,0.9f)};f[2]={{x+bw,y-h},toSF(c,0.9f)};f[3]={{x,y-h},toSF(c,0.9f)};rt.draw(f);
        sf::VertexArray t(sf::PrimitiveType::TriangleFan,4);
        t[0]={{x,y-h},toSF(ct)};t[1]={{x+bw,y-h},toSF(ct)};t[2]={{x+bw+dep,y-h+dep*0.5f},toSF(ct)};t[3]={{x+dep,y-h+dep*0.5f},toSF(ct)};rt.draw(t);
        sf::VertexArray s(sf::PrimitiveType::TriangleFan,4);
        s[0]={{x+bw,y},toSF(cs,0.85f)};s[1]={{x+bw+dep,y+dep*0.5f},toSF(cs,0.85f)};s[2]={{x+bw+dep,y-h+dep*0.5f},toSF(cs,0.85f)};s[3]={{x+bw,y-h},toSF(cs,0.85f)};rt.draw(s);
    }
}

static float tunnelA=0.0f;
static void vizTunnel(VizContext&ctx){
    auto&rt=ctx.target;auto&b=ctx.bounds;
    float cx=b.position.x+b.size.x*0.5f,cy=b.position.y+b.size.y*0.5f;
    float maxR=std::min(b.size.x,b.size.y)*0.47f;tunnelA+=0.012f;
    float bass=std::clamp(ctx.fft[0]*ctx.gain,0.0f,1.0f);
    for(int r=20;r>=1;r--){
        float v=std::clamp(ctx.fft[r*(int)ctx.fft.size()/40]*ctx.gain,0.0f,1.0f);
        float rad=(float)r/20*maxR*(1.0f+bass*0.07f);
        float rot=tunnelA*((r%2==0)?1.0f:-1.0f)*((float)r/20);
        float al=(1.0f-(float)r/20)*0.75f*(0.3f+v*0.7f);
        Color3 c=lerpColor(ctx.theme.a,ctx.theme.b,(float)r/20);
        sf::VertexArray ring(sf::PrimitiveType::LineStrip,9);
        for(int s=0;s<=8;s++){float a=(float)(s%8)/8*2.0f*(float)M_PI+rot;float w=1.0f+v*0.12f*(float)sin(s*3.0f+ctx.time*2.0f);ring[s]={sf::Vector2f(cx+rad*w*(float)cos(a),cy+rad*w*(float)sin(a)),toSF(c,al)};}
        rt.draw(ring);
    }
}

static float helixT=0.0f;
static void vizDNA(VizContext&ctx){
    auto&rt=ctx.target;auto&b=ctx.bounds;helixT+=0.035f;
    const int N=140;float amp=b.size.y*0.35f,cy=b.position.y+b.size.y*0.5f;
    sf::VertexArray s1(sf::PrimitiveType::LineStrip,N),s2(sf::PrimitiveType::LineStrip,N);
    std::vector<float> env(N);
    for(int i=0;i<N;i++){
        env[i]=std::clamp(ctx.fft[i*(int)ctx.fft.size()/N]*ctx.gain,0.0f,1.0f);
        float x=b.position.x+(float)i/(N-1)*b.size.x,a=(float)i/N*4.0f*(float)M_PI+helixT,aw=(0.35f+env[i]*0.65f)*amp;
        s1[i]={sf::Vector2f(x,cy+aw*(float)sin(a)),lerpSF(ctx.theme.a,ctx.theme.b,env[i])};
        s2[i]={sf::Vector2f(x,cy+aw*(float)sin(a+(float)M_PI)),lerpSF(ctx.theme.b,ctx.theme.a,env[i])};
    }
    rt.draw(s1);rt.draw(s2);
    for(int r=0;r<28;r++){
        int i=r*N/28;float x=b.position.x+(float)i/(N-1)*b.size.x,a=(float)i/N*4.0f*(float)M_PI+helixT,aw=(0.35f+env[i]*0.65f)*amp;
        float y1=cy+aw*(float)sin(a),y2=cy+aw*(float)sin(a+(float)M_PI);
        drawLine(rt,{x,y1},{x,y2},lerpSF(ctx.theme.a,ctx.theme.b,env[i],0.55f),lerpSF(ctx.theme.a,ctx.theme.b,env[i],0.55f),1.0f+env[i]*1.5f);
        sf::CircleShape d1(3.5f+env[i]*3.0f),d2(3.5f+env[i]*3.0f);
        d1.setOrigin({d1.getRadius(),d1.getRadius()});d1.setPosition({x,y1});d1.setFillColor(toSF(ctx.theme.a));rt.draw(d1);
        d2.setOrigin({d2.getRadius(),d2.getRadius()});d2.setPosition({x,y2});d2.setFillColor(toSF(ctx.theme.b));rt.draw(d2);
    }
}

static void vizScope(VizContext&ctx){
    auto&rt=ctx.target;auto&b=ctx.bounds;
    float cy=b.position.y+b.size.y*0.5f;int N=(int)ctx.wave.size();if(N<2)return;
    for(int gx=0;gx<=8;gx++){float x=b.position.x+(float)gx/8*b.size.x;sf::VertexArray gl(sf::PrimitiveType::Lines,2);gl[0]={{x,b.position.y},sf::Color(20,30,20)};gl[1]={{x,b.position.y+b.size.y},sf::Color(20,30,20)};rt.draw(gl);}
    for(int gy=0;gy<=6;gy++){float y=b.position.y+(float)gy/6*b.size.y;sf::VertexArray gl(sf::PrimitiveType::Lines,2);gl[0]={{b.position.x,y},sf::Color(20,30,20)};gl[1]={{b.position.x+b.size.x,y},sf::Color(20,30,20)};rt.draw(gl);}
    std::array<std::pair<float,float>,4> passes={{{8.0f,0.025f},{4.0f,0.07f},{2.0f,0.45f},{0.8f,1.0f}}};
    for(auto[lw,al]:passes){
        sf::VertexArray va(sf::PrimitiveType::LineStrip,N);
        for(int i=0;i<N;i++){float x=b.position.x+(float)i/(N-1)*b.size.x,y=cy-ctx.wave[i]*ctx.gain*b.size.y*0.42f;va[i]={sf::Vector2f(x,y),lerpSF(ctx.theme.a,ctx.theme.b,std::abs(ctx.wave[i]),al)};}
        rt.draw(va);
    }
}

struct Particle{sf::Vector2f pos,vel;float life,size;Color3 color;};
static std::vector<Particle> particles;
static void vizParticles(VizContext&ctx){
    auto&rt=ctx.target;auto&b=ctx.bounds;
    float cx=b.position.x+b.size.x*0.5f,cy=b.position.y+b.size.y*0.5f;
    float bass=0;for(int i=0;i<8;i++)bass+=ctx.fft[i];bass=std::clamp(bass/8.0f*ctx.gain,0.0f,1.0f);
    if(bass>0.45f)for(int s=0;s<5;s++){float a=(float)(rand()%628)/100.0f,spd=(2.0f+(float)(rand()%300)/100.0f)*bass;particles.push_back({{cx+(float)(rand()%80-40),cy+(float)(rand()%80-40)},{(float)cos(a)*spd,(float)sin(a)*spd-0.5f},1.0f,2.0f+(float)(rand()%4)*bass,(rand()%2==0)?ctx.theme.a:ctx.theme.b});}
    particles.erase(std::remove_if(particles.begin(),particles.end(),[](const Particle&p){return p.life<=0;}),particles.end());
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    for(auto&p:particles){p.pos+=p.vel;p.vel.y+=0.04f;p.life-=0.016f;float al=p.life;sf::Color c=toSF(p.color,al);float s=p.size*al;va.append({p.pos+sf::Vector2f(-s,-s),c});va.append({p.pos+sf::Vector2f(s,-s),c});va.append({p.pos+sf::Vector2f(s,s),c});va.append({p.pos+sf::Vector2f(-s,-s),c});va.append({p.pos+sf::Vector2f(s,s),c});va.append({p.pos+sf::Vector2f(-s,s),c});}
    rt.draw(va);if((int)particles.size()>2000)particles.erase(particles.begin(),particles.begin()+200);
}

static void vizHeatgrid(VizContext&ctx){
    auto&rt=ctx.target;auto&b=ctx.bounds;
    const int C=32,R=20;float cw=b.size.x/C,rh=b.size.y/R;
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    for(int r=0;r<R;r++)for(int c=0;c<C;c++){
        int idx=(c*R+r)*(int)ctx.fft.size()/(C*R);
        float v=std::clamp(ctx.fft[std::min(idx,(int)ctx.fft.size()-1)]*ctx.gain,0.0f,1.0f);
        float x0=b.position.x+c*cw+1,x1=x0+cw-2,y0=b.position.y+(R-1-r)*rh+1,y1=y0+rh-2;
        sf::Color col=lerpSF(ctx.theme.b,ctx.theme.a,v,0.2f+v*0.8f);
        va.append({{x0,y0},col});va.append({{x1,y0},col});va.append({{x1,y1},col});
        va.append({{x0,y0},col});va.append({{x1,y1},col});va.append({{x0,y1},col});
    }
    rt.draw(va);
}

static void vizScopeLiss(VizContext&ctx){
    auto&b=ctx.bounds;float half=b.size.x*0.5f;
    VizContext cL=ctx;cL.bounds={b.position,{half-2,b.size.y}};
    VizContext cR=ctx;cR.bounds={{b.position.x+half+2,b.position.y},{half-2,b.size.y}};
    vizScope(cL);vizLissajous(cR);
    sf::RectangleShape div({2.0f,b.size.y});div.setPosition({b.position.x+half-1,b.position.y});div.setFillColor(sf::Color(40,40,40));ctx.target.draw(div);
}

using VizFn=std::function<void(VizContext&)>;
static const std::vector<std::pair<std::string,VizFn>> VIZ_LIST={
    {"Spectrum Bars",vizBars},{"Waveform",vizWaveform},{"Mirror Bars",vizMirror},
    {"Radial Spikes",vizRadial},{"Radial Filled",vizRadialFilled},{"Lissajous",vizLissajous},
    {"Spectrogram",vizSpectrogram},{"Waterfall",vizWaterfall},{"Circular Wave",vizCircularWave},
    {"Blob",vizBlob},{"Starburst",vizStarburst},{"3D Bars",vizBars3D},
    {"Tunnel Rings",vizTunnel},{"DNA Helix",vizDNA},{"Oscilloscope",vizScope},
    {"Particles",vizParticles},{"Heat Grid",vizHeatgrid},{"Scope+Lissajous",vizScopeLiss},
};

int main(int argc,char**argv){
    sf::RenderWindow window(sf::VideoMode({1400u,860u}),"VIZR - Audio Visualizer");
    window.setFramerateLimit(60);
    (void)ImGui::SFML::Init(window);

    ImGui::StyleColorsDark();
    ImGuiStyle&style=ImGui::GetStyle();
    style.WindowRounding=0;style.FrameRounding=3;style.FrameBorderSize=1;
    auto*C=style.Colors;
    C[ImGuiCol_WindowBg]     ={0.07f,0.07f,0.07f,1};C[ImGuiCol_FrameBg]      ={0.12f,0.12f,0.12f,1};
    C[ImGuiCol_Button]       ={0.14f,0.14f,0.14f,1};C[ImGuiCol_ButtonHovered]={0.20f,0.20f,0.00f,1};
    C[ImGuiCol_ButtonActive] ={0.91f,1.00f,0.00f,.8f};C[ImGuiCol_SliderGrab] ={0.91f,1.00f,0.00f,1};
    C[ImGuiCol_Header]       ={0.20f,0.20f,0.00f,1};C[ImGuiCol_HeaderHovered]={0.30f,0.30f,0.00f,1};
    C[ImGuiCol_HeaderActive] ={0.91f,1.00f,0.00f,.6f};

    AudioEngine audio;
    int vizIdx=0,themeIdx=0;float gain=1.5f,smoothK=0.75f,runTime=0.0f;
    bool fullscreen=false;char pathBuf[512]={};
    std::vector<float> fft(512,0.0f),wave(1024,0.0f);
    if(argc>1){audio.load(argv[1]);audio.play();strncpy(pathBuf,argv[1],511);}

    sf::Clock deltaClock;const float SW=230.0f,BH=90.0f;

    while(window.isOpen()){
        while(const std::optional<sf::Event> ev=window.pollEvent()){
            ImGui::SFML::ProcessEvent(window,*ev);
            if(ev->is<sf::Event::Closed>())window.close();
            if(const auto*kp=ev->getIf<sf::Event::KeyPressed>()){
                switch(kp->code){
                    case sf::Keyboard::Key::Space:audio.toggle();break;
                    case sf::Keyboard::Key::F:case sf::Keyboard::Key::F11:
                        fullscreen=!fullscreen;
                        window.create(fullscreen?sf::VideoMode::getDesktopMode():sf::VideoMode({1400u,860u}),"VIZR - Audio Visualizer",fullscreen?sf::State::Fullscreen:sf::State::Windowed);
                        window.setFramerateLimit(60);(void)ImGui::SFML::Init(window);break;
                    case sf::Keyboard::Key::Right:audio.seek(audio.getPosition()+5.0f);break;
                    case sf::Keyboard::Key::Left: audio.seek(audio.getPosition()-5.0f);break;
                    case sf::Keyboard::Key::Escape:window.close();break;
                    default:break;
                }
            }
        }
        sf::Time dt=deltaClock.restart();runTime+=dt.asSeconds();
        ImGui::SFML::Update(window,dt);
        if(audio.loaded)audio.analyze(smoothK,fft,wave);

        auto wsz=window.getSize();float vizW=(float)wsz.x-SW,vizH=(float)wsz.y-BH;
        window.clear(sf::Color(10,10,10));

        {const Theme&th=THEMES[themeIdx];sf::RectangleShape bg({vizW,vizH});bg.setPosition({SW,0});bg.setFillColor(sf::Color((uint8_t)(th.bg.r*255),(uint8_t)(th.bg.g*255),(uint8_t)(th.bg.b*255)));window.draw(bg);}

        VizContext ctx{fft,wave,THEMES[themeIdx],gain,runTime,window,{{SW+8,8},{vizW-16,vizH-16}}};
        VIZ_LIST[vizIdx].second(ctx);

        // Sidebar
        ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({SW,(float)wsz.y-BH});
        ImGui::Begin("##sb",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollbar);
        ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.91f,1.0f,0.0f,1.0f));ImGui::SetWindowFontScale(2.0f);ImGui::Text("VIZ");ImGui::SameLine(0,0);
        ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1.0f,0.24f,0.67f,1.0f));ImGui::Text("R");ImGui::PopStyleColor(2);
        ImGui::SetWindowFontScale(1.0f);ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.3f,0.3f,0.3f,1.0f));ImGui::Text("AUDIO VISUALIZER");ImGui::PopStyleColor();ImGui::Separator();
        ImGui::Spacing();ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.6f,0.6f,0.6f,1.0f));ImGui::Text("LOAD FILE");ImGui::PopStyleColor();
        ImGui::SetNextItemWidth(-1);ImGui::InputText("##p",pathBuf,sizeof(pathBuf));
        if(ImGui::Button("  Open File  ",{-1,0})){
            system("powershell -Command \"Add-Type -AssemblyName System.Windows.Forms;$f=New-Object System.Windows.Forms.OpenFileDialog;$f.Filter='Audio|*.wav;*.ogg;*.flac';if($f.ShowDialog() -eq 'OK'){$f.FileName}\" > C:\\Windows\\Temp\\vizr_path.txt 2>&1");
            FILE*f=fopen("C:\\Windows\\Temp\\vizr_path.txt","r");
            if(f){char picked[512]={};if(fgets(picked,512,f)){int l=strlen(picked);while(l>0&&(picked[l-1]=='\n'||picked[l-1]=='\r'))picked[--l]=0;if(strlen(picked)>0){strncpy(pathBuf,picked,511);if(audio.load(pathBuf))audio.play();}}fclose(f);}
        }
        ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.3f,0.3f,0.3f,1.0f));ImGui::Text("VISUALIZER");ImGui::PopStyleColor();
        ImGui::BeginChild("##vl",{-1,280},false);
        for(int i=0;i<(int)VIZ_LIST.size();i++){bool sel=(i==vizIdx);if(sel)ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.91f,1.0f,0.0f,1.0f));if(ImGui::Selectable(VIZ_LIST[i].first.c_str(),sel)){vizIdx=i;spectroHist.clear();waterfallHist.clear();particles.clear();}if(sel)ImGui::PopStyleColor();}
        ImGui::EndChild();ImGui::Separator();ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.3f,0.3f,0.3f,1.0f));ImGui::Text("THEME");ImGui::PopStyleColor();
        ImGui::BeginChild("##tl",{-1,145},false);
        for(int i=0;i<(int)THEMES.size();i++){const Theme&th=THEMES[i];ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(th.a.r,th.a.g,th.a.b,1.0f));if(ImGui::Selectable(th.name.c_str(),i==themeIdx))themeIdx=i;ImGui::PopStyleColor();}
        ImGui::EndChild();ImGui::End();

        // Bottom bar
        ImGui::SetNextWindowPos({0,(float)wsz.y-BH});ImGui::SetNextWindowSize({(float)wsz.x,BH});
        ImGui::Begin("##bb",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollbar);
        ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.0f,0.94f,1.0f,1.0f));ImGui::Text("%s",audio.loaded?audio.filename.c_str():"-- no file loaded --");ImGui::PopStyleColor();
        std::string ts=fmtTime(audio.getPosition())+" / "+fmtTime(audio.duration);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x*0.5f-60);ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.5f,0.5f,0.5f,1.0f));ImGui::Text("%s",ts.c_str());ImGui::PopStyleColor();
        float prog=(audio.loaded&&audio.duration>0)?audio.getPosition()/audio.duration:0.0f;
        float pbw=(float)wsz.x-SW-12;
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram,ImVec4(0.91f,1.0f,0.0f,1.0f));ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(0.1f,0.1f,0.1f,1.0f));
        ImGui::SetCursorPosX(SW+6);ImGui::ProgressBar(prog,{pbw,6.0f},"");
        if(ImGui::IsItemClicked()){float rx=ImGui::GetMousePos().x-(SW+6);audio.seek(std::clamp(rx/pbw,0.0f,1.0f)*audio.duration);}
        ImGui::PopStyleColor(2);
        ImGui::SetCursorPosX(SW+6);
        if(ImGui::Button(audio.isPlaying()?"  || PAUSE  ":"  > PLAY  "))audio.toggle();ImGui::SameLine();
        if(ImGui::Button("  << -5s  "))audio.seek(audio.getPosition()-5.0f);ImGui::SameLine();
        if(ImGui::Button("  >> +5s  "))audio.seek(audio.getPosition()+5.0f);ImGui::SameLine(0,20);
        ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.4f,0.4f,0.4f,1.0f));ImGui::Text("GAIN");ImGui::PopStyleColor();ImGui::SameLine();ImGui::SetNextItemWidth(110);ImGui::SliderFloat("##g",&gain,0.3f,4.0f,"%.2f");ImGui::SameLine(0,15);
        ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.4f,0.4f,0.4f,1.0f));ImGui::Text("SMOOTH");ImGui::PopStyleColor();ImGui::SameLine();ImGui::SetNextItemWidth(110);ImGui::SliderFloat("##s",&smoothK,0.0f,0.97f,"%.2f");ImGui::SameLine(0,15);
        if(ImGui::Button(fullscreen?"  [] WINDOWED  ":"  [] FULLSCREEN  ")){
            fullscreen=!fullscreen;
            window.create(fullscreen?sf::VideoMode::getDesktopMode():sf::VideoMode({1400u,860u}),"VIZR - Audio Visualizer",fullscreen?sf::State::Fullscreen:sf::State::Windowed);
            window.setFramerateLimit(60);(void)ImGui::SFML::Init(window);
        }
        ImGui::End();

        ImGui::SFML::Render(window);window.display();
    }
    ImGui::SFML::Shutdown();return 0;
}
