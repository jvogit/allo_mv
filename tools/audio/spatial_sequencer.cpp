#include "al/app/al_DistributedApp.hpp"
#include "al/app/al_GUIDomain.hpp"
#include "al/io/al_File.hpp"
#include "al/io/al_Imgui.hpp"
#include "al/io/al_Toml.hpp"

#include "al/graphics/al_Shapes.hpp"
#include "al/io/al_PersistentConfig.hpp"
#include "al/math/al_Spherical.hpp"
#include "al/scene/al_DistributedScene.hpp"
#include "al/sound/al_Lbap.hpp"
#include "al/sound/al_Speaker.hpp"
#include "al/sound/al_SpeakerAdjustment.hpp"
#include "al/sphere/al_AlloSphereSpeakerLayout.hpp"
#include "al/sphere/al_SphereUtils.hpp"
#include "al/ui/al_FileSelector.hpp"
#include "al/ui/al_ParameterGUI.hpp"
#include "al_ext/soundfile/al_SoundfileBuffered.hpp"

#include "Gamma/Analysis.h"
#include "Gamma/scl.h"

using namespace al;

struct MappedAudioFile {
  std::unique_ptr<SoundFileBuffered> soundfile;
  std::vector<size_t> outChannelMap;
  std::string fileInfoText;
  std::string fileName;
  float gain;
  bool mute{false};
};

struct AudioObjectData {
  std::string rootPath;
  uint16_t audioSampleRate;
  uint16_t audioBlockSize;
  Mesh *mesh;
};

class Meter {
public:
  void init(const Speakers &sl) {
    addCube(mMesh);
    mSl = sl;
    //    mMesh.update();
  }

  void processSound(AudioIOData &io) {

    if (tempValues.size() != io.channelsOut()) {
      tempValues.resize(io.channelsOut());
      values.resize(io.channelsOut());
    }
    io.frame(0);
    while (io()) {
      for (int i = 0; i < io.channelsOut(); i++) {
        tempValues[i] = 0.0f;
        for (int samp = 0; samp < io.framesPerBuffer(); samp++) {
          tempValues[i] += io.out(i);
        }
        tempValues[i] = tempValues[i] / io.framesPerBuffer();
        if (values[i] > tempValues[i]) {
          values[i] = values[i] - 0.003 * (values[i] - tempValues[i]);
        } else {
          values[i] = tempValues[i];
        }
      }
    }
  }

  void draw(Graphics &g) {
    g.polygonLine();
    int index = 0;
    auto spkrIt = mSl.begin();
    g.color(1);
    for (const auto &v : values) {
      if (spkrIt != mSl.end()) {
        // FIXME assumes speakers are sorted by device channel index
        if (spkrIt->deviceChannel == index) {
          g.pushMatrix();
          g.scale(1 / 5.0f);
          g.translate(spkrIt->vecGraphics());
          g.scale(0.1 + v * 5);
          g.draw(mMesh);
          g.popMatrix();
          spkrIt++;
        }
      } else {
        spkrIt = mSl.begin();
      }
      index++;
    }
  }

private:
  Mesh mMesh;
  std::vector<float> values;
  std::vector<float> tempValues;
  Speakers mSl;
};

class AudioObject : public PositionedVoice {
public:
  // Trigger Params
  ParameterString file{"audioFile", ""};            // in seconds
  ParameterString automation{"automationFile", ""}; // in seconds

  // Variable params
  Parameter gain{"gain", "", 1.0, 0.0, 4.0};
  ParameterBool mute{"mute", "", 0.0};

  // Internal
  Parameter env{"env", "", 1.0, 0.00001, 10};

  void init() override {
    registerTriggerParameters(file, automation, gain);
    registerParameters(env);             // Propagate from audio rendering node
    registerParameters(parameterPose()); // Update position in secondary nodes

    mSequencer << parameterPose();
    mPresetHandler << parameterPose();
    mSequencer << mPresetHandler; // For morphing
  }

  void onProcess(AudioIOData &io) override {
    float buffer[2048 * 60];
    int numChannels = soundfile.channels();
    assert(io.framesPerBuffer() < INT32_MAX);
    auto framesRead =
        soundfile.read(buffer, static_cast<int>(io.framesPerBuffer()));
    int outIndex = 0;
    size_t inChannel = 0;
    if (!mute) {
      for (size_t sample = 0; sample < framesRead; sample++) {
        io.outBuffer(outIndex)[sample] +=
            gain * buffer[sample * numChannels + inChannel];
        mEnvFollow(buffer[sample * numChannels + inChannel]);
      }
    }
  }

  void onProcess(Graphics &g) override {
    auto &mesh = *static_cast<AudioObjectData *>(userData())->mesh;
    if (isPrimary()) {
      env = mEnvFollow.value();
    }
    g.scale(0.5);
    g.scale(0.1 + gain + env * 10);
    g.color(c);
    g.polygonLine();
    g.draw(mesh);
  }

  void onTriggerOn() override {
    auto objData = static_cast<AudioObjectData *>(userData());

    if (isPrimary()) {
      auto &rootPath = objData->rootPath;
      soundfile.open(File::conformPathToOS(rootPath) + file.get());
      if (!soundfile.opened()) {
        std::cerr << "ERROR: opening audio file: "
                  << File::conformPathToOS(rootPath) + file.get() << std::endl;
      }

      float seqStep = (float)objData->audioBlockSize / objData->audioSampleRate;
      mSequencer.setSequencerStepTime(seqStep);

      mSequencer.playSequence(File::conformPathToOS(rootPath) +
                              automation.get());
    }
    auto colorIndex = automation.get()[0] - 'A';
    c = HSV(colorIndex / 6.0f, 1.0f, 1.0f);
  }

  void onFree() override { soundfile.close(); }

private:
  PresetSequencer mSequencer;
  PresetHandler mPresetHandler{""};
  SoundFileBuffered soundfile;
  Color c;

  gam::EnvFollow<> mEnvFollow;
};

class SpatialSequencer : public DistributedApp {
public:
  std::string rootDir{""};

  DistributedScene scene{"spatial_sequencer"};

  Trigger play{"play", ""};
  PersistentConfig config;

  void setPath(std::string path) {
    rootDir = path;
    mSequencer.setDirectory(path);
  }

  void onInit() override {
    // Prepare scene shared data
    mObjectData.mesh = &this->mObjectMesh;
    mObjectData.rootPath = rootDir;
    mObjectData.audioSampleRate = audioIO().framesPerSecond();
    mObjectData.audioBlockSize = audioIO().framesPerBuffer();
    scene.setDefaultUserData(&mObjectData);

    if (al::sphere::isSimulatorMachine()) {
    }
    auto sl = al::AlloSphereSpeakerLayout();
    mSpatializer = scene.setSpatializer<Lbap>(sl);

    audioIO().channelsOut(60);
    audioIO().print();

    mSequencer << scene;

    registerDynamicScene(scene);
    scene.registerSynthClass<AudioObject>(); // Allow AudioObject in sequences
    scene.allocatePolyphony<AudioObject>(16);

    // Prepare GUI
    if (isPrimary()) {
      auto guiDomain = GUIDomain::enableGUI(defaultWindowDomain());
      auto &gui = guiDomain->newGUI();
      gui << play << mSequencer << audioDomain()->parameters()[0];
      gui.drawFunction = [&]() {
        if (ParameterGUI::drawAudioIO(audioIO())) {
          scene.prepare(audioIO());
          mObjectData.audioSampleRate = audioIO().framesPerSecond();
          mObjectData.audioBlockSize = audioIO().framesPerBuffer();
        }
      };
    }

    // Parameter callbacks
    play.registerChangeCallback([&](auto v) {
      if (v == 1.0) {
        mSequencer.playSequence("session");
      }
    });
  }

  void onCreate() override {
    // Prepare mesh
    addSphere(mObjectMesh);
    mObjectMesh.scale(0.1f);
    mObjectMesh.update();
    mMeter.init(mSpatializer->speakerLayout());
  }

  void onAnimate(double dt) override { mSequencer.update(dt); }

  void onDraw(Graphics &g) override {
    g.clear(0, 0, 0);
    g.pushMatrix();
    if (isPrimary()) {
      // For simulator view from outside
      g.translate(0.5, 0, -4);
    }
    {
      g.pushMatrix();
      g.polygonLine();
      g.scale(10);
      g.color(0.5);
      g.draw(mObjectMesh);
      g.popMatrix();
    }
    mMeter.draw(g);

    mSequencer.render(g);
    g.popMatrix();
  }

  void onSound(AudioIOData &io) override {
    mSequencer.render(io);
    mMeter.processSound(io);
  }

  void onExit() override {}

private:
  VAOMesh mObjectMesh;

  SynthSequencer mSequencer{TimeMasterMode::TIME_MASTER_CPU};
  AudioObjectData mObjectData;
  SpeakerDistanceGainAdjustmentProcessor gainAdjustment;
  Meter mMeter;
  std::shared_ptr<Spatializer> mSpatializer;
};

int main() {
  SpatialSequencer app;

  app.setPath("Morris Allosphere piece");

  app.start();
  return 0;
}
