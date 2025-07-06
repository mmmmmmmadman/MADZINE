#include "plugin.hpp"

struct Observer : Module {
    enum ParamIds {
        TIME_PARAM,
        TRIG_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        TRACK1_INPUT,
        TRACK2_INPUT,
        TRACK3_INPUT,
        TRACK4_INPUT,
        TRACK5_INPUT,
        TRACK6_INPUT,
        TRACK7_INPUT,
        TRACK8_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        NUM_OUTPUTS
    };
    enum LightIds {
        TRIG_LIGHT,
        NUM_LIGHTS
    };

    struct ScopePoint {
        float min = INFINITY;
        float max = -INFINITY;
    };

    static constexpr int SCOPE_BUFFER_SIZE = 256;
    
    ScopePoint scopeBuffer[8][SCOPE_BUFFER_SIZE];
    ScopePoint currentPoint[8];
    int bufferIndex = 0;
    int frameIndex = 0;
    
    dsp::SchmittTrigger triggers[16];

    Observer() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        
        const float maxTime = -std::log2(5e1f);
        const float minTime = -std::log2(5e-3f);
        const float defaultTime = -std::log2(5e-1f);
        configParam(TIME_PARAM, maxTime, minTime, defaultTime, "Time", " ms/screen", 1 / 2.f, 1000);
        
        configSwitch(TRIG_PARAM, 0.f, 1.f, 1.f, "Trigger", {"Enabled", "Disabled"});
        
        configLight(TRIG_LIGHT, "Trigger Light");
        
        configInput(TRACK1_INPUT, "Track 1");
        configInput(TRACK2_INPUT, "Track 2");
        configInput(TRACK3_INPUT, "Track 3");
        configInput(TRACK4_INPUT, "Track 4");
        configInput(TRACK5_INPUT, "Track 5");
        configInput(TRACK6_INPUT, "Track 6");
        configInput(TRACK7_INPUT, "Track 7");
        configInput(TRACK8_INPUT, "Track 8");
    }

    void process(const ProcessArgs& args) override {
        bool trig = !params[TRIG_PARAM].getValue();
        lights[TRIG_LIGHT].setBrightness(trig);

        if (bufferIndex >= SCOPE_BUFFER_SIZE) {
            bool triggered = false;

            if (!trig) {
                triggered = true;
            }
            else {
                for (int i = 0; i < 8; i++) {
                    if (inputs[TRACK1_INPUT + i].isConnected()) {
                        int trigChannels = inputs[TRACK1_INPUT + i].getChannels();
                        for (int c = 0; c < trigChannels; c++) {
                            float trigVoltage = inputs[TRACK1_INPUT + i].getVoltage(c);
                            if (triggers[c].process(rescale(trigVoltage, 0.f, 0.001f, 0.f, 1.f))) {
                                triggered = true;
                            }
                        }
                        break;
                    }
                }
            }

            if (triggered) {
                for (int c = 0; c < 16; c++) {
                    triggers[c].reset();
                }
                bufferIndex = 0;
                frameIndex = 0;
            }
        }

        if (bufferIndex < SCOPE_BUFFER_SIZE) {
            float deltaTime = dsp::exp2_taylor5(-params[TIME_PARAM].getValue()) / SCOPE_BUFFER_SIZE;
            int frameCount = (int) std::ceil(deltaTime * args.sampleRate);

            for (int i = 0; i < 8; i++) {
                float x = inputs[TRACK1_INPUT + i].getVoltage();
                currentPoint[i].min = std::min(currentPoint[i].min, x);
                currentPoint[i].max = std::max(currentPoint[i].max, x);
            }

            if (++frameIndex >= frameCount) {
                frameIndex = 0;
                for (int i = 0; i < 8; i++) {
                    scopeBuffer[i][bufferIndex] = currentPoint[i];
                }
                for (int i = 0; i < 8; i++) {
                    currentPoint[i] = ScopePoint();
                }
                bufferIndex++;
            }
        }
    }
};

struct ObserverScopeDisplay : LedDisplay {
    Observer* module;
    ModuleWidget* moduleWidget;
    
    ObserverScopeDisplay() {
        box.size = Vec(120, 300);
    }
    
    void drawWave(const DrawArgs& args, int track, NVGcolor color) {
        if (!module) return;
        
        nvgSave(args.vg);
        
        float trackHeight = box.size.y / 8.0f;
        float trackY = track * trackHeight;
        
        Rect b = Rect(Vec(0, trackY), Vec(box.size.x, trackHeight));
        nvgScissor(args.vg, RECT_ARGS(b));
        nvgBeginPath(args.vg);
        
        for (int i = 0; i < Observer::SCOPE_BUFFER_SIZE; i++) {
            const Observer::ScopePoint& point = module->scopeBuffer[track][i];
            float max = point.max;
            if (!std::isfinite(max))
                max = 0.f;

            Vec p;
            p.x = (float)i / (Observer::SCOPE_BUFFER_SIZE - 1);
            p.y = (max) * -0.05f + 0.5f;
            p = b.interpolate(p);
            
            if (i == 0)
                nvgMoveTo(args.vg, p.x, p.y);
            else
                nvgLineTo(args.vg, p.x, p.y);
        }
        
        nvgStrokeColor(args.vg, color);
        nvgStrokeWidth(args.vg, 1.5f);
        nvgLineCap(args.vg, NVG_ROUND);
        nvgStroke(args.vg);
        nvgResetScissor(args.vg);
        nvgRestore(args.vg);
    }
    
    void drawBackground(const DrawArgs& args) {
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        nvgFillColor(args.vg, nvgRGB(20, 20, 20));
        nvgFill(args.vg);
        
        nvgStrokeColor(args.vg, nvgRGBA(255, 255, 255, 30));
        nvgStrokeWidth(args.vg, 0.5f);
        
        float trackHeight = box.size.y / 8.0f;
        
        for (int i = 0; i < 8; i++) {
            float trackY = i * trackHeight;
            
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, trackY);
            nvgLineTo(args.vg, box.size.x, trackY);
            nvgStroke(args.vg);
            
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0, trackY + trackHeight / 2);
            nvgLineTo(args.vg, box.size.x, trackY + trackHeight / 2);
            nvgStrokeColor(args.vg, nvgRGBA(255, 255, 255, 15));
            nvgStroke(args.vg);
            nvgStrokeColor(args.vg, nvgRGBA(255, 255, 255, 30));
        }
        
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0, box.size.y);
        nvgLineTo(args.vg, box.size.x, box.size.y);
        nvgStroke(args.vg);
        
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStrokeColor(args.vg, nvgRGB(100, 100, 100));
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        nvgStroke(args.vg);
    }
    
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;
        
        drawBackground(args);
        
        if (!module || !moduleWidget) return;
        
        for (int i = 0; i < 8; i++) {
            PortWidget* inputPort = moduleWidget->getInput(Observer::TRACK1_INPUT + i);
            CableWidget* cable = APP->scene->rack->getTopCable(inputPort);
            NVGcolor trackColor = cable ? cable->color : nvgRGB(255, 255, 255);
            
            drawWave(args, i, trackColor);
        }
    }
    
    void onDragMove(const event::DragMove& e) override {
        ModuleWidget* mw = getAncestorOfType<ModuleWidget>();
        if (!mw) return;
        
        ParamWidget* timeParam = mw->getParam(Observer::TIME_PARAM);
        if (!timeParam) return;
        
        ParamQuantity* pq = timeParam->getParamQuantity();
        if (!pq) return;
        
        float sensitivity = 0.01f;
        float deltaValue = -e.mouseDelta.y * sensitivity;
        pq->setValue(pq->getValue() + deltaValue);
        e.consume(this);
    }
};

struct ObserverWidget : ModuleWidget {
    ObserverWidget(Observer* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "Observer.png")));
        
        box.size = Vec(8 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
        
        addParam(createParamCentered<VCVButton>(Vec(100, 13), module, Observer::TRIG_PARAM));
        addChild(createLightCentered<MediumLight<RedLight>>(Vec(100, 13), module, Observer::TRIG_LIGHT));
        
        ObserverScopeDisplay* scopeDisplay = new ObserverScopeDisplay();
        scopeDisplay->box.pos = Vec(0, 30);
        scopeDisplay->box.size = Vec(120, 300);
        scopeDisplay->module = module;
        scopeDisplay->moduleWidget = this;
        addChild(scopeDisplay);
        
        addParam(createParamCentered<Trimpot>(Vec(0, 0), module, Observer::TIME_PARAM));
        
        addInput(createInputCentered<PJ301MPort>(Vec(15, 343), module, Observer::TRACK1_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(45, 343), module, Observer::TRACK2_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(75, 343), module, Observer::TRACK3_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(105, 343), module, Observer::TRACK4_INPUT));
        
        addInput(createInputCentered<PJ301MPort>(Vec(15, 368), module, Observer::TRACK5_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(45, 368), module, Observer::TRACK6_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(75, 368), module, Observer::TRACK7_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(105, 368), module, Observer::TRACK8_INPUT));
    }
};

Model* modelObserver = createModel<Observer, ObserverWidget>("Observer");