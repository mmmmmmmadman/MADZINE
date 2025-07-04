#include "plugin.hpp"

struct DensityParamQuantity : ParamQuantity {
    std::string getDisplayValueString() override {
        float value = getValue();
        int steps, primaryKnobs;
        
        if (value < 0.2f) {
            steps = 8 + (int)(value * 20);
            primaryKnobs = 2;
        } else if (value < 0.4f) {
            steps = 12 + (int)((value - 0.2f) * 40);
            primaryKnobs = 3;
        } else if (value < 0.6f) {
            steps = 20 + (int)((value - 0.4f) * 40);
            primaryKnobs = 4;
        } else {
            steps = 28 + (int)((value - 0.6f) * 50);
            primaryKnobs = 5;
        }
        steps = clamp(steps, 8, 48);
        
        return string::f("%d knobs, %d steps", primaryKnobs, steps);
    }
};

struct PPaTTTerning : Module {
    enum ParamId {
        K1_PARAM, K2_PARAM, K3_PARAM, K4_PARAM, K5_PARAM,
        STYLE_PARAM, DENSITY_PARAM, CHAOS_PARAM,
        CVD_ATTEN_PARAM, DELAY_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        CLOCK_INPUT, RESET_INPUT, CVD_CV_INPUT, INPUTS_LEN
    };
    enum OutputId {
        CV_OUTPUT, TRIG_OUTPUT, CV2_OUTPUT, TRIG2_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId { 
        STYLE_LIGHT_RED,
        STYLE_LIGHT_GREEN,
        STYLE_LIGHT_BLUE,
        DELAY_LIGHT_RED,
        DELAY_LIGHT_GREEN,
        DELAY_LIGHT_BLUE,
        LIGHTS_LEN 
    };

    dsp::SchmittTrigger clockTrigger, resetTrigger, styleTrigger, delayTrigger;
    dsp::PulseGenerator gateOutPulse, gate2OutPulse;
    
    int currentStep = 0, sequenceLength = 16, stepToKnobMapping[64];
    float previousVoltage = -999.0f;
    int styleMode = 1;
    
    static const int MAX_DELAY = 8;
    float cvHistory[MAX_DELAY];
    int historyIndex = 0, track2Delay = 1;
    
    static const int CVD_BUFFER_SIZE = 192000;
    float cvdBuffer[CVD_BUFFER_SIZE];
    int cvdWriteIndex = 0;
    float sampleRate = 44100.0f;
    
    PPaTTTerning() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        configParam(K1_PARAM, -10.0f, 10.0f, 0.0f, "K1", "V");
        configParam(K2_PARAM, -10.0f, 10.0f, 2.0f, "K2", "V");
        configParam(K3_PARAM, -10.0f, 10.0f, 4.0f, "K3", "V");
        configParam(K4_PARAM, -10.0f, 10.0f, 6.0f, "K4", "V");
        configParam(K5_PARAM, -10.0f, 10.0f, 8.0f, "K5", "V");
        
        configParam(STYLE_PARAM, 0.0f, 1.0f, 0.0f, "Style");
        configParam(DENSITY_PARAM, 0.0f, 1.0f, 0.5f, "Density");
        configParam(CHAOS_PARAM, 0.0f, 1.0f, 0.0f, "Chaos", "%", 0.f, 100.f);
        configParam(CVD_ATTEN_PARAM, 0.0f, 1.0f, 0.0f, "CVD Time/Attenuation", " ms", 0.f, 1000.f);
        configParam(DELAY_PARAM, 0.0f, 1.0f, 0.0f, "Delay");
        
        configInput(CLOCK_INPUT, "Clock");
        configInput(RESET_INPUT, "Reset");
        configInput(CVD_CV_INPUT, "CVD Time CV");
        configOutput(CV_OUTPUT, "CV");
        configOutput(TRIG_OUTPUT, "Trigger");
        configOutput(CV2_OUTPUT, "CV2 (Delay + CVD)");
        configOutput(TRIG2_OUTPUT, "Trigger 2 (Delay + CVD)");
        
        configLight(STYLE_LIGHT_RED, "Style Mode Red");
        configLight(STYLE_LIGHT_GREEN, "Style Mode Green");
        configLight(STYLE_LIGHT_BLUE, "Style Mode Blue");
        configLight(DELAY_LIGHT_RED, "Delay Red");
        configLight(DELAY_LIGHT_GREEN, "Delay Green");
        configLight(DELAY_LIGHT_BLUE, "Delay Blue");
        
        for (int i = 0; i < MAX_DELAY; i++) cvHistory[i] = 0.0f;
        for (int i = 0; i < CVD_BUFFER_SIZE; i++) cvdBuffer[i] = 0.0f;
        generateMapping();
    }
    
    void onSampleRateChange() override {
        sampleRate = APP->engine->getSampleRate();
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "track2Delay", json_integer(track2Delay));
        json_object_set_new(rootJ, "styleMode", json_integer(styleMode));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* t2 = json_object_get(rootJ, "track2Delay");
        if (t2) {
            track2Delay = clamp((int)json_integer_value(t2), 0, 5);
        }
        
        json_t* styleJ = json_object_get(rootJ, "styleMode");
        if (styleJ) {
            styleMode = clamp((int)json_integer_value(styleJ), 0, 2);
        }
    }

    void generateMapping() {
        int style = styleMode;
        float density = params[DENSITY_PARAM].getValue();
        float chaos = params[CHAOS_PARAM].getValue();
        
        if (density < 0.2f) {
            sequenceLength = 8 + (int)(density * 20);
        } else if (density < 0.4f) {
            sequenceLength = 12 + (int)((density - 0.2f) * 40);
        } else if (density < 0.6f) {
            sequenceLength = 20 + (int)((density - 0.4f) * 40);
        } else {
            sequenceLength = 28 + (int)((density - 0.6f) * 50);
        }
        sequenceLength = clamp(sequenceLength, 8, 48);
        
        if (chaos > 0.0f) {
            float chaosRange = chaos * sequenceLength * 0.5f;
            float randomOffset = (random::uniform() - 0.5f) * 2.0f * chaosRange;
            sequenceLength += (int)randomOffset;
            sequenceLength = clamp(sequenceLength, 4, 64);
        }
        
        int primaryKnobs = (density < 0.2f) ? 2 : (density < 0.4f) ? 3 : (density < 0.6f) ? 4 : 5;
        
        for (int i = 0; i < 64; i++) stepToKnobMapping[i] = 0;
        
        switch (style) {
            case 0:
                for (int i = 0; i < sequenceLength; i++) {
                    stepToKnobMapping[i] = i % primaryKnobs;
                }
                break;
            case 1: {
                int minimalistPattern[32] = {0,1,2,0,1,2,3,4,3,4,0,1,2,0,1,2,3,4,3,4,1,3,2,4,0,2,1,3,0,4,2,1};
                for (int i = 0; i < sequenceLength; i++) {
                    stepToKnobMapping[i] = minimalistPattern[i % 32] % primaryKnobs;
                }
                break;
            }
            case 2: {
                int jumpPattern[5] = {0, 2, 4, 1, 3};
                for (int i = 0; i < sequenceLength; i++) {
                    stepToKnobMapping[i] = jumpPattern[i % 5] % primaryKnobs;
                }
                break;
            }
        }
        
        if (primaryKnobs < 5) {
            int insertInterval = sequenceLength / (5 - primaryKnobs + 1);
            for (int unusedKnob = primaryKnobs; unusedKnob < 5; unusedKnob++) {
                int insertPos = insertInterval * (unusedKnob - primaryKnobs + 1);
                if (insertPos < sequenceLength) stepToKnobMapping[insertPos] = unusedKnob;
            }
        }
        
        if (density > 0.8f) {
            int changeInterval = clamp(sequenceLength / 8, 3, 8);
            for (int i = changeInterval; i < sequenceLength; i += changeInterval) {
                stepToKnobMapping[i] = (stepToKnobMapping[i] + 2) % 5;
            }
        }
        
        if (chaos > 0.3f) {
            int chaosSteps = (int)(chaos * sequenceLength * 0.3f);
            for (int i = 0; i < chaosSteps; i++) {
                int randomStep = random::u32() % sequenceLength;
                stepToKnobMapping[randomStep] = random::u32() % 5;
            }
        }
    }

    void process(const ProcessArgs& args) override {
        if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
            currentStep = 0;
            generateMapping();
            previousVoltage = -999.0f;
            for (int i = 0; i < MAX_DELAY; i++) cvHistory[i] = 0.0f;
            for (int i = 0; i < CVD_BUFFER_SIZE; i++) cvdBuffer[i] = 0.0f;
            historyIndex = 0;
            cvdWriteIndex = 0;
        }
        
        if (styleTrigger.process(params[STYLE_PARAM].getValue())) {
            styleMode = (styleMode + 1) % 3;
            generateMapping();
        }
        
        if (delayTrigger.process(params[DELAY_PARAM].getValue())) {
            track2Delay = (track2Delay + 1) % 6;
        }
        
        lights[STYLE_LIGHT_RED].setBrightness(styleMode == 0 ? 1.0f : 0.0f);
        lights[STYLE_LIGHT_GREEN].setBrightness(styleMode == 1 ? 1.0f : 0.0f);
        lights[STYLE_LIGHT_BLUE].setBrightness(styleMode == 2 ? 1.0f : 0.0f);
        
        float delayBrightness = (track2Delay == 0) ? 0.0f : (float)track2Delay / 5.0f;
        lights[DELAY_LIGHT_RED].setBrightness(delayBrightness);
        lights[DELAY_LIGHT_GREEN].setBrightness(0.0f);
        lights[DELAY_LIGHT_BLUE].setBrightness(delayBrightness);
        
        bool clockTriggered = clockTrigger.process(inputs[CLOCK_INPUT].getVoltage());
        if (clockTriggered) {
            int activeKnob = stepToKnobMapping[currentStep];
            float voltage = params[K1_PARAM + activeKnob].getValue();
            cvHistory[historyIndex] = voltage;
            
            currentStep = (currentStep + 1) % sequenceLength;
            generateMapping();
            
            int newActiveKnob = stepToKnobMapping[currentStep];
            float newVoltage = params[K1_PARAM + newActiveKnob].getValue();
            
            if (newVoltage != previousVoltage) gateOutPulse.trigger(0.01f);
            previousVoltage = newVoltage;
            
            int delay1Index = (historyIndex - track2Delay + 1 + MAX_DELAY) % MAX_DELAY;
            int delay2Index = (historyIndex - track2Delay + MAX_DELAY) % MAX_DELAY;
            if (track2Delay > 0 && cvHistory[delay1Index] != cvHistory[delay2Index]) gate2OutPulse.trigger(0.01f);
            
            historyIndex = (historyIndex + 1) % MAX_DELAY;
        }
        
        int activeKnob = stepToKnobMapping[currentStep];
        outputs[CV_OUTPUT].setVoltage(params[K1_PARAM + activeKnob].getValue());
        outputs[TRIG_OUTPUT].setVoltage(gateOutPulse.process(args.sampleTime) ? 10.0f : 0.0f);
        
        int shiftRegisterIndex = (historyIndex - track2Delay + MAX_DELAY) % MAX_DELAY;
        float shiftRegisterCV = (track2Delay == 0) ? outputs[CV_OUTPUT].getVoltage() : cvHistory[shiftRegisterIndex];
        
        float delayTimeMs = 0.0f;
        float knobValue = params[CVD_ATTEN_PARAM].getValue();
        
        if (!inputs[CVD_CV_INPUT].isConnected()) {
            delayTimeMs = knobValue * 1000.0f;
        } else {
            float cvdCV = clamp(inputs[CVD_CV_INPUT].getVoltage(), 0.0f, 10.0f);
            delayTimeMs = (cvdCV / 10.0f) * knobValue * 1000.0f;
        }
        
        if (delayTimeMs <= 0.001f) {
            outputs[CV2_OUTPUT].setVoltage(shiftRegisterCV);
        } else {
            cvdBuffer[cvdWriteIndex] = shiftRegisterCV;
            cvdWriteIndex = (cvdWriteIndex + 1) % CVD_BUFFER_SIZE;
            
            int delaySamples = (int)(delayTimeMs * sampleRate / 1000.0f);
            delaySamples = clamp(delaySamples, 0, CVD_BUFFER_SIZE - 1);
            
            int readIndex = (cvdWriteIndex - delaySamples + CVD_BUFFER_SIZE) % CVD_BUFFER_SIZE;
            float delayedCV = cvdBuffer[readIndex];
            
            outputs[CV2_OUTPUT].setVoltage(delayedCV);
        }
        
        outputs[TRIG2_OUTPUT].setVoltage(gate2OutPulse.process(args.sampleTime) ? 10.0f : 0.0f);
    }
};

struct DelayParamQuantity : ParamQuantity {
    std::string getDisplayValueString() override {
        PPaTTTerning* module = dynamic_cast<PPaTTTerning*>(this->module);
        if (!module) return "1 step";
        
        int delay = module->track2Delay;
        if (delay == 0) return "No delay";
        if (delay == 1) return "1 step";
        return string::f("%d steps", delay);
    }
    
    std::string getLabel() override {
        return "Delay";
    }
};

struct StyleParamQuantity : ParamQuantity {
    std::string getDisplayValueString() override {
        PPaTTTerning* module = dynamic_cast<PPaTTTerning*>(this->module);
        if (!module) return "Minimalism";
        
        switch (module->styleMode) {
            case 0: return "Sequential";
            case 1: return "Minimalism";
            case 2: return "Jump";
            default: return "Minimalism";
        }
    }
    
    std::string getLabel() override {
        return "Mode";
    }
};

struct PPaTTTerningWidget : ModuleWidget {
    PPaTTTerningWidget(PPaTTTerning* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "PPaTTTerning.png")));
        box.size = Vec(4 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
        float centerX = box.size.x / 2;
        
        addInput(createInputCentered<PJ301MPort>(Vec(centerX - 15, 55), module, PPaTTTerning::CLOCK_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(centerX + 15, 55), module, PPaTTTerning::RESET_INPUT));
        
        addParam(createParamCentered<RoundBlackKnob>(Vec(centerX - 15, 97), module, PPaTTTerning::K1_PARAM));
        addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(Vec(centerX + 15, 97), module, PPaTTTerning::STYLE_LIGHT_RED));
        addParam(createParamCentered<VCVButton>(Vec(centerX + 15, 97), module, PPaTTTerning::STYLE_PARAM));
        
        addParam(createParamCentered<RoundBlackKnob>(Vec(centerX - 15, 142), module, PPaTTTerning::K2_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(Vec(centerX + 15, 142), module, PPaTTTerning::DENSITY_PARAM));
        
        addParam(createParamCentered<RoundBlackKnob>(Vec(centerX - 15, 187), module, PPaTTTerning::K3_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(Vec(centerX + 15, 187), module, PPaTTTerning::CHAOS_PARAM));
        
        addParam(createParamCentered<RoundBlackKnob>(Vec(centerX - 15, 232), module, PPaTTTerning::K4_PARAM));
        addOutput(createOutputCentered<PJ301MPort>(Vec(centerX + 15, 232), module, PPaTTTerning::CV_OUTPUT));
        
        addParam(createParamCentered<RoundBlackKnob>(Vec(centerX - 15, 277), module, PPaTTTerning::K5_PARAM));
        addOutput(createOutputCentered<PJ301MPort>(Vec(centerX + 15, 277), module, PPaTTTerning::TRIG_OUTPUT));
        
        addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(Vec(15, 315), module, PPaTTTerning::DELAY_LIGHT_RED));
        addParam(createParamCentered<VCVButton>(Vec(15, 315), module, PPaTTTerning::DELAY_PARAM));
        
        addOutput(createOutputCentered<PJ301MPort>(Vec(15, 345), module, PPaTTTerning::CV2_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(45, 345), module, PPaTTTerning::TRIG2_OUTPUT));
        
        addParam(createParamCentered<Trimpot>(Vec(15, 370), module, PPaTTTerning::CVD_ATTEN_PARAM));
        addInput(createInputCentered<PJ301MPort>(Vec(45, 370), module, PPaTTTerning::CVD_CV_INPUT));
    }

    void appendContextMenu(Menu* menu) override {
        PPaTTTerning* module = getModule<PPaTTTerning>();
        if (!module) return;

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Pattern Mode"));
        
        menu->addChild(createCheckMenuItem("Sequential", "",
            [=]() { return module->styleMode == 0; },
            [=]() { 
                module->styleMode = 0;
                module->generateMapping();
            }
        ));
        
        menu->addChild(createCheckMenuItem("Minimalism", "",
            [=]() { return module->styleMode == 1; },
            [=]() { 
                module->styleMode = 1;
                module->generateMapping();
            }
        ));
        
        menu->addChild(createCheckMenuItem("Jump", "",
            [=]() { return module->styleMode == 2; },
            [=]() { 
                module->styleMode = 2;
                module->generateMapping();
            }
        ));
        
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Delay Settings"));
        
        std::vector<std::string> delayNames = {"No delay", "1 step", "2 steps", "3 steps", "4 steps", "5 steps"};
        
        for (int i = 0; i < 6; ++i) {
            menu->addChild(createCheckMenuItem(delayNames[i], "",
                [=]() { return module->track2Delay == i; },
                [=]() { module->track2Delay = i; }
            ));
        }
        
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("CVD Time Presets"));
        
        struct CVDTimeChoice {
            std::string name;
            float value;
        };
        
        std::vector<CVDTimeChoice> choices = {
            {"0ms (Off)", 0.0f},
            {"1ms", 0.001f},
            {"5ms", 0.005f},
            {"10ms", 0.01f},
            {"25ms", 0.025f},
            {"50ms", 0.05f},
            {"100ms", 0.1f},
            {"250ms", 0.25f},
            {"500ms", 0.5f},
            {"1s (Max)", 1.0f}
        };
        
        for (const auto& choice : choices) {
            menu->addChild(createMenuItem(choice.name, "",
                [=]() { 
                    module->params[PPaTTTerning::CVD_ATTEN_PARAM].setValue(choice.value);
                }
            ));
        }
    }
};

Model* modelPPaTTTerning = createModel<PPaTTTerning, PPaTTTerningWidget>("PPaTTTerning");