// sound-lines - a simple spectrogram-like output of a sampled audio clip
// Mitchell Lewis, 2-20-19


#include "al/core.hpp"
using namespace al;

#include "Gamma/DFT.h"
#include "Gamma/SamplePlayer.h"
using namespace gam;

#include <vector>
using namespace std;

#define N (513)  // number of particles
#define DISP_WIDTH (5.0)


// --------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------
//                                          APP START
// --------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------


struct AlloApp : App {
    ShaderProgram shader;
    Texture texture;
    
    Mesh pointMesh;          // holds positions

    gam::SamplePlayer<float, gam::ipl::Linear, gam::phsInc::Loop> samplePlayer;
    gam::STFT stft;
    AudioIO audio;

    vector<float> spectrum;
    
    // measures magnitude of each band and records it as a distance\
    // also keeps track of max value per band for normalization
    vector<float> bandDist;
    vector<float> max;
    
    vector<vector<float>> waves;
    float waveWidth = 1000;
    float waveHeight = 5;
    float xWidth = 10;
    float zWidth = 10;
    float waveRadius = 5;
    
    
    vector<int> bandWidths{ 2,2,2,2,2,3,3,3,3,4,4,5,6,7,8,10,12,15,19,23,28,38,53,75,90,94 }; // Bark Scale - # of stft bands per Bark band
    float heightScale = 10;
    bool analyzeInput = true; // if true: analyze input channel, else: analyze sample
    bool displayMode = false;  // true: show equal-width rectangle bands, false: show all bands
    
    void onCreate() override {
        if(!analyzeInput){
            samplePlayer.load("../sound/9.wav");
            samplePlayer.loop();
        }
        Sync::master().spu(audioIO().fps());
        
        int n = bandWidths.size();

        
        for(int i=0; i<n; i++){
            bandDist.push_back(0);
            max.push_back(0.1);
            
            vector<float> wave;
            for(int j=0; j<waveWidth; j++){
                wave.push_back(0);
            }
            waves.push_back(wave);
        }
        

    }

    void drawWaves(Graphics& g, vector<vector<float>> waves){
        
        
        for(int i=0; i<waves.size(); i++){
            Mesh m{Mesh::LINE_STRIP};
            float wavePercent = 1 - (float) (i+1) / waves.size();  //
            for(int j=0; j<waves[i].size(); j++){
                float valPercent = (float) j / waves[i].size();
                float val = waves[i][j];
                
                float x = (waveRadius + wavePercent*xWidth)*cos(valPercent*2*M_PI);
                float z = (waveRadius + wavePercent*xWidth)*sin(valPercent*2*M_PI);
                
                m.vertices().push_back( Vec3f(x, val*waveHeight, z) );
                m.color( HSV(wavePercent, val, val) );
                
            }
            g.meshColor();
            g.draw(m);
        }
        
    }
    

    
    void onSound(AudioIOData& io) override {
        vector<Vec3f>& vertex(pointMesh.vertices());
        while (io()) {
            float s;
            if(analyzeInput) s = io.in(0);
            else {
                s = samplePlayer();
                float f = samplePlayer.read(0);
                io.out(0) = f;
                io.out(1) = f;
            }
            if (stft(s)){
                int currentBand = 0;
                
                // for each band in Bark scale
                for(int i=0; i<bandWidths.size(); i++){
                    float barkSum = 0;
                    
                    // for each STFT band
                    for(int j=0; j<bandWidths[i]; j++){
                        barkSum += stft.bin(currentBand).norm() * 1000;
                        currentBand++;
                    }
                    
                    // calculate mean height of Bark band
                    float dist = barkSum / bandWidths[i];
                    
                    // update max if necessary
                    if(dist > max[i]) max[i] = dist;
                    float distRatio = dist/max[i];
                    
                    // set point distance to normalized band amplitude
                    float changeVal = (distRatio - bandDist[i]);
                    bandDist[i] += changeVal/20;
                    
                }
            }
        }
    }
    
    void onAnimate(double dt) override {
        for(int i=0; i<waves.size(); i++){
            waves[i].pop_back();
            waves[i].insert(waves[i].begin(), bandDist[i]);
        }
    }
    
    void onDraw(Graphics& g) override {
        g.clear(0);
        
        g.depthTesting(false);
        g.blending(true);
        g.blendModeTrans();
        
        drawWaves(g, waves);
        
    }
    
    void onKeyDown(const Keyboard& k) override {
        switch (k.key()) {
            case 'o':
                displayMode = !displayMode;
                
            
        }
        //
    }
    
    bool getAnalysisMode() { return analyzeInput; }
};



int main() {
    AlloApp app;
    if(app.getAnalysisMode()){
        app.initAudio(44100, 512, -1,-1,3);
    }
    else{
        app.initAudio();
    }
    app.start();
    
}
