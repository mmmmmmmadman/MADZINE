#include "plugin.hpp"

struct EnhancedTextLabel : TransparentWidget {
    std::string text;
    float fontSize;
    NVGcolor color;
    bool bold;
    
    EnhancedTextLabel(Vec pos, Vec size, std::string text, float fontSize = 12.f, 
                      NVGcolor color = nvgRGB(255, 255, 255), bool bold = true) {
        box.pos = pos;
        box.size = size;
        this->text = text;
        this->fontSize = fontSize;
        this->color = color;
        this->bold = bold;
    }
    
    void draw(const DrawArgs &args) override {
        nvgFontSize(args.vg, fontSize);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, color);
        
        if (bold) {
            float offset = 0.3f;
            nvgText(args.vg, box.size.x / 2.f - offset, box.size.y / 2.f, text.c_str(), NULL);
            nvgText(args.vg, box.size.x / 2.f + offset, box.size.y / 2.f, text.c_str(), NULL);
            nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f - offset, text.c_str(), NULL);
            nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f + offset, text.c_str(), NULL);
            nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f, text.c_str(), NULL);
        } else {
            nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f, text.c_str(), NULL);
        }
    }
};

struct WhiteBackgroundBox : Widget {
    WhiteBackgroundBox(Vec pos, Vec size) {
        box.pos = pos;
        box.size = size;
    }
    
    void draw(const DrawArgs &args) override {
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        nvgFillColor(args.vg, nvgRGB(255, 255, 255));
        nvgFill(args.vg);
        
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStrokeColor(args.vg, nvgRGBA(200, 200, 200, 255));
        nvgStroke(args.vg);
    }
};

struct ADGenerator : Module {
    enum ParamId {
        ATK_ALL_PARAM,
        DEC_ALL_PARAM,
        TRACK1_ATTACK_PARAM,
        TRACK1_DECAY_PARAM,
        TRACK1_CURVE_PARAM,
        TRACK2_ATTACK_PARAM,
        TRACK2_DECAY_PARAM,
        TRACK2_CURVE_PARAM,
        TRACK3_ATTACK_PARAM,
        TRACK3_DECAY_PARAM,
        TRACK3_CURVE_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        TRACK1_TRIG_INPUT,
        TRACK2_TRIG_INPUT,
        TRACK3_TRIG_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        TRACK1_OUTPUT,
        TRACK2_OUTPUT,
        TRACK3_OUTPUT,
        SUM_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        LIGHTS_LEN
    };

    // Auto-routing state
    bool autoRouteEnabled = false;
    
    // BPF state
    bool bpfEnabled[3] = {false, false, false}; // BPF enabled for each track
    float bpfCutoffs[3] = {200.0f, 1000.0f, 5000.0f}; // Default: Low, Mid, High
    float bpfGains[3] = {3.0f, 3.0f, 3.0f}; // BPF output gain compensation
    
    // BPF state variables for each track
    struct BandPassFilter {
        float lowpass = 0.0f;
        float highpass = 0.0f;
        float bandpass = 0.0f;
        
        void reset() {
            lowpass = 0.0f;
            highpass = 0.0f;
            bandpass = 0.0f;
        }
        
        float process(float input, float cutoff, float sampleRate) {
            float f = 2.0f * std::sin(M_PI * cutoff / sampleRate);
            f = clamp(f, 0.0f, 1.0f);
            
            lowpass += f * (input - lowpass);
            highpass = input - lowpass;
            bandpass += f * (highpass - bandpass);
            
            return bandpass;
        }
    };
    
    BandPassFilter bpfFilters[3];

    struct ADEnvelope {
        enum Phase {
            IDLE,
            ATTACK,
            DECAY
        };
        
        Phase phase = IDLE;
        float triggerOutput = 0.0f;
        float followerOutput = 0.0f;
        float attackTime = 0.01f;
        float decayTime = 1.0f;
        float phaseTime = 0.0f;
        float curve = 0.0f;
        
        // Envelope follower state
        float followerState = 0.0f;
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
        
        dsp::SchmittTrigger trigger;
        
        void reset() {
            phase = IDLE;
            triggerOutput = 0.0f;
            followerOutput = 0.0f;
            followerState = 0.0f;
            phaseTime = 0.0f;
        }
        
        float applyCurve(float x, float curvature) {
            x = clamp(x, 0.0f, 1.0f);
            
            if (curvature == 0.0f) {
                return x;
            }
            
            float k = curvature;
            float abs_x = std::abs(x);
            float denominator = k - 2.0f * k * abs_x + 1.0f;
            
            if (std::abs(denominator) < 1e-6f) {
                return x;
            }
            
            return (x - k * x) / denominator;
        }
        
        float processEnvelopeFollower(float triggerVoltage, float sampleTime, float attackTime, float releaseTime, float curve) {
            // Faster response coefficients for better audio tracking
            attackCoeff = 1.0f - std::exp(-sampleTime / std::max(0.0005f, attackTime * 0.1f)); // 10x faster attack
            releaseCoeff = 1.0f - std::exp(-sampleTime / std::max(0.001f, releaseTime * 0.5f)); // 2x faster release
            
            // Clamp coefficients to safe range
            attackCoeff = clamp(attackCoeff, 0.0f, 1.0f);
            releaseCoeff = clamp(releaseCoeff, 0.0f, 1.0f);
            
            // Rectify input signal and preserve amplitude
            float rectified = std::abs(triggerVoltage) / 10.0f; // Normalize to 0-1 but keep amplitude info
            rectified = clamp(rectified, 0.0f, 1.0f);
            
            // Apply attack/release filtering with curve shaping
            float targetCoeff;
            if (rectified > followerState) {
                // Attack phase - apply curve to the coefficient like in trigger envelope
                float progress = attackCoeff;
                targetCoeff = applyCurve(progress, curve);
            } else {
                // Release phase - apply curve to the coefficient
                float progress = releaseCoeff;
                targetCoeff = applyCurve(progress, curve);
            }
            
            targetCoeff = clamp(targetCoeff, 0.0f, 1.0f);
            
            // Update follower state to track input amplitude
            followerState += (rectified - followerState) * targetCoeff;
            followerState = clamp(followerState, 0.0f, 1.0f);
            
            return followerState;
        }
        
        float processTriggerEnvelope(float triggerVoltage, float sampleTime, float attack, float decay, float curve) {
            // Only process triggers above 9.5V threshold
            bool isHighVoltage = (std::abs(triggerVoltage) > 9.5f);
            
            // Trigger detection with voltage threshold
            if (phase == IDLE && isHighVoltage && trigger.process(triggerVoltage)) {
                phase = ATTACK;
                phaseTime = 0.0f;
            }
            
            switch (phase) {
                case IDLE:
                    triggerOutput = 0.0f;
                    break;
                    
                case ATTACK:
                    phaseTime += sampleTime;
                    if (phaseTime >= attack) {
                        phase = DECAY;
                        phaseTime = 0.0f;
                        triggerOutput = 1.0f;
                    } else {
                        float t = phaseTime / attack;
                        triggerOutput = applyCurve(t, curve);
                    }
                    break;
                    
                case DECAY:
                    phaseTime += sampleTime;
                    if (phaseTime >= decay) {
                        triggerOutput = 0.0f;
                        phase = IDLE;
                        phaseTime = 0.0f;
                    } else {
                        float t = phaseTime / decay;
                        triggerOutput = 1.0f - applyCurve(t, curve);
                    }
                    break;
            }
            
            return clamp(triggerOutput, 0.0f, 1.0f);
        }
        
        float process(float sampleTime, float triggerVoltage, float attack, float decay, float curveParam, float atkAll, float decAll) {
            float atkOffset = atkAll * 0.5f;
            float decOffset = decAll * 0.5f;
            
            attackTime = std::pow(10.0f, (attack - 0.5f) * 6.0f) + atkOffset;
            decayTime = std::pow(10.0f, (decay - 0.5f) * 6.0f) + decOffset;
            
            attackTime = std::max(0.001f, attackTime);
            decayTime = std::max(0.001f, decayTime);
            
            curve = curveParam;
            
            // Process both envelope types
            float triggerEnv = processTriggerEnvelope(triggerVoltage, sampleTime, attackTime, decayTime, curve);
            float followerEnv = processEnvelopeFollower(triggerVoltage, sampleTime, attackTime, decayTime, curve);
            
            // Output hybrid result (trigger now only activates for high voltage)
            float output = std::max(triggerEnv, followerEnv);
            
            return output * 10.0f;
        }
    };
    
    ADEnvelope envelopes[3];

    ADGenerator() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        configParam(ATK_ALL_PARAM, -1.0f, 1.0f, 0.0f, "Attack All");
        configParam(DEC_ALL_PARAM, -1.0f, 1.0f, 0.0f, "Decay All");
        
        for (int i = 0; i < 3; ++i) {
            configParam(TRACK1_ATTACK_PARAM + i * 3, 0.0f, 1.0f, 0.1f, string::f("Track %d Attack", i + 1), " s", 0.0f, 1.0f, std::pow(10.0f, -2.0f));
            configParam(TRACK1_DECAY_PARAM + i * 3, 0.0f, 1.0f, 0.3f, string::f("Track %d Decay", i + 1), " s", 0.0f, 1.0f, std::pow(10.0f, -2.0f));
            configParam(TRACK1_CURVE_PARAM + i * 3, -1.0f, 1.0f, 0.0f, string::f("Track %d Curve", i + 1));
            configInput(TRACK1_TRIG_INPUT + i, string::f("Track %d Input", i + 1));
            configOutput(TRACK1_OUTPUT + i, string::f("Track %d Output", i + 1));
        }
        
        configOutput(SUM_OUTPUT, "Sum Output");
    }

    void onReset() override {
        for (int i = 0; i < 3; ++i) {
            envelopes[i].reset();
            bpfFilters[i].reset();
        }
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "autoRouteEnabled", json_boolean(autoRouteEnabled));
        
        // Save BPF enabled states
        json_t* bpfEnabledJ = json_array();
        for (int i = 0; i < 3; ++i) {
            json_array_append_new(bpfEnabledJ, json_boolean(bpfEnabled[i]));
        }
        json_object_set_new(rootJ, "bpfEnabled", bpfEnabledJ);
        
        // Save BPF cutoff frequencies
        json_t* bpfCutoffsJ = json_array();
        for (int i = 0; i < 3; ++i) {
            json_array_append_new(bpfCutoffsJ, json_real(bpfCutoffs[i]));
        }
        json_object_set_new(rootJ, "bpfCutoffs", bpfCutoffsJ);
        
        // Save BPF gains
        json_t* bpfGainsJ = json_array();
        for (int i = 0; i < 3; ++i) {
            json_array_append_new(bpfGainsJ, json_real(bpfGains[i]));
        }
        json_object_set_new(rootJ, "bpfGains", bpfGainsJ);
        
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* autoRouteJ = json_object_get(rootJ, "autoRouteEnabled");
        if (autoRouteJ) {
            autoRouteEnabled = json_boolean_value(autoRouteJ);
        }
        
        // Load BPF enabled states
        json_t* bpfEnabledJ = json_object_get(rootJ, "bpfEnabled");
        if (bpfEnabledJ) {
            for (int i = 0; i < 3; ++i) {
                json_t* enabledJ = json_array_get(bpfEnabledJ, i);
                if (enabledJ) {
                    bpfEnabled[i] = json_boolean_value(enabledJ);
                }
            }
        }
        
        // Load BPF cutoff frequencies
        json_t* bpfCutoffsJ = json_object_get(rootJ, "bpfCutoffs");
        if (bpfCutoffsJ) {
            for (int i = 0; i < 3; ++i) {
                json_t* cutoffJ = json_array_get(bpfCutoffsJ, i);
                if (cutoffJ) {
                    bpfCutoffs[i] = json_real_value(cutoffJ);
                }
            }
        }
        
        // Load BPF gains
        json_t* bpfGainsJ = json_object_get(rootJ, "bpfGains");
        if (bpfGainsJ) {
            for (int i = 0; i < 3; ++i) {
                json_t* gainJ = json_array_get(bpfGainsJ, i);
                if (gainJ) {
                    bpfGains[i] = json_real_value(gainJ);
                }
            }
        }
    }

    void process(const ProcessArgs& args) override {
        float sumOutput = 0.0f;
        float atkAll = params[ATK_ALL_PARAM].getValue();
        float decAll = params[DEC_ALL_PARAM].getValue();
        
        // Get input signals with auto-routing logic
        float inputSignals[3];
        
        if (autoRouteEnabled) {
            // Auto-route: Input 1 signal goes to all tracks
            float input1Signal = inputs[TRACK1_TRIG_INPUT].getVoltage();
            inputSignals[0] = input1Signal;
            inputSignals[1] = input1Signal;
            inputSignals[2] = input1Signal;
        } else {
            // Normal mode: each track uses its own input
            inputSignals[0] = inputs[TRACK1_TRIG_INPUT].getVoltage();
            inputSignals[1] = inputs[TRACK2_TRIG_INPUT].getVoltage();
            inputSignals[2] = inputs[TRACK3_TRIG_INPUT].getVoltage();
        }
        
        for (int i = 0; i < 3; ++i) {
            // Apply band pass filter to input signal (if enabled)
            float processedSignal = inputSignals[i];
            if (bpfEnabled[i]) {
                processedSignal = bpfFilters[i].process(inputSignals[i], bpfCutoffs[i], args.sampleRate);
            }
            
            float attackParam = params[TRACK1_ATTACK_PARAM + i * 3].getValue();
            float decayParam = params[TRACK1_DECAY_PARAM + i * 3].getValue();
            float curveParam = params[TRACK1_CURVE_PARAM + i * 3].getValue();
            
            float envelopeOutput = envelopes[i].process(args.sampleTime, processedSignal, attackParam, decayParam, curveParam, atkAll, decAll);
            
            // Apply gain compensation to envelope output (not input signal)
            if (bpfEnabled[i]) {
                envelopeOutput *= bpfGains[i];
            }
            
            outputs[TRACK1_OUTPUT + i].setVoltage(envelopeOutput);
            
            sumOutput += envelopeOutput * 0.33f;
        }
        
        sumOutput = clamp(sumOutput, 0.0f, 10.0f);
        outputs[SUM_OUTPUT].setVoltage(sumOutput);
    }
};

struct ADGeneratorWidget : ModuleWidget {
    ADGeneratorWidget(ADGenerator* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/SwingLFO.svg")));
        
        box.size = Vec(4 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

        addChild(new EnhancedTextLabel(Vec(0, 1), Vec(box.size.x, 20), "ADGene", 12.f, nvgRGB(255, 200, 0), true));
        addChild(new EnhancedTextLabel(Vec(0, 13), Vec(box.size.x, 20), "MADZINE", 10.f, nvgRGB(255, 200, 0), false));

        float centerX = box.size.x / 2;
        
        addChild(new EnhancedTextLabel(Vec(2, 30), Vec(25, 10), "ATK ALL", 6.f, nvgRGB(255, 255, 255), true));
        addParam(createParamCentered<Trimpot>(Vec(centerX - 15, 50), module, ADGenerator::ATK_ALL_PARAM));
        
        addChild(new EnhancedTextLabel(Vec(33, 30), Vec(25, 10), "DEC ALL", 6.f, nvgRGB(255, 255, 255), true));
        addParam(createParamCentered<Trimpot>(Vec(centerX + 15, 50), module, ADGenerator::DEC_ALL_PARAM));

        float trackY[3] = {85, 167, 249};
        
        for (int i = 0; i < 3; ++i) {
            float y = trackY[i];
            float centerX = box.size.x / 2;

            addChild(new EnhancedTextLabel(Vec(1, y - 22), Vec(25, 10), "INPUT", 6.f, nvgRGB(200, 200, 200), true));
            addInput(createInputCentered<PJ301MPort>(Vec(centerX - 15, y), module, ADGenerator::TRACK1_TRIG_INPUT + i));

            addChild(new EnhancedTextLabel(Vec(32, y - 22), Vec(25, 10), "CURV", 7.f, nvgRGB(200, 200, 200), true));
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerX + 15, y), module, ADGenerator::TRACK1_CURVE_PARAM + i * 3));

            addChild(new EnhancedTextLabel(Vec(1, y + 13), Vec(25, 10), "ATK", 7.f, nvgRGB(200, 200, 200), true));
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerX - 15, y + 35), module, ADGenerator::TRACK1_ATTACK_PARAM + i * 3));

            addChild(new EnhancedTextLabel(Vec(32, y + 13), Vec(25, 10), "DEC", 7.f, nvgRGB(200, 200, 200), true));
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerX + 15, y + 35), module, ADGenerator::TRACK1_DECAY_PARAM + i * 3));
        }
        
        addChild(new WhiteBackgroundBox(Vec(0, 310), Vec(60, 70)));
        
        addOutput(createOutputCentered<PJ301MPort>(Vec(15, 333), module, ADGenerator::TRACK1_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(45, 333), module, ADGenerator::TRACK2_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(15, 368), module, ADGenerator::TRACK3_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(45, 368), module, ADGenerator::SUM_OUTPUT));
        
        addChild(new EnhancedTextLabel(Vec(9, 312), Vec(12, 10), "1", 7.f, nvgRGB(255, 133, 133), true));
        addChild(new EnhancedTextLabel(Vec(39, 312), Vec(12, 10), "2", 7.f, nvgRGB(255, 133, 133), true));
        addChild(new EnhancedTextLabel(Vec(9, 347), Vec(12, 10), "3", 7.f, nvgRGB(255, 133, 133), true));
        addChild(new EnhancedTextLabel(Vec(36, 347), Vec(16, 10), "SUM", 7.f, nvgRGB(255, 133, 133), true));
    }

    void appendContextMenu(Menu* menu) override {
        ADGenerator* module = dynamic_cast<ADGenerator*>(this->module);
        if (!module) return;

        menu->addChild(new MenuSeparator);

        // Auto-route option
        MenuItem* autoRouteItem = createCheckMenuItem("Auto-route Input 1 to all tracks", "",
            [=]() { return module->autoRouteEnabled; },
            [=]() { module->autoRouteEnabled = !module->autoRouteEnabled; }
        );
        menu->addChild(autoRouteItem);

        menu->addChild(new MenuSeparator);

        // BPF cutoff frequency sliders
        menu->addChild(createMenuLabel("Band Pass Filter"));

        for (int i = 0; i < 3; ++i) {
            // BPF Enable/Disable
            MenuItem* enableItem = createCheckMenuItem(string::f("Track %d BPF Enable", i + 1), "",
                [=]() { return module->bpfEnabled[i]; },
                [=]() { module->bpfEnabled[i] = !module->bpfEnabled[i]; }
            );
            menu->addChild(enableItem);
            
            // Track frequency label and slider (only show if enabled)
            if (module->bpfEnabled[i]) {
                menu->addChild(createMenuLabel(string::f("Track %d Frequency", i + 1)));
                
                // Frequency slider
                struct BPFQuantity : Quantity {
                    ADGenerator* module;
                    int trackIndex;
                    
                    BPFQuantity(ADGenerator* m, int track) : module(m), trackIndex(track) {}
                    
                    void setValue(float value) override {
                        if (module) {
                            // Clamp to valid range
                            value = clamp(value, 20.0f, 8000.0f);
                            module->bpfCutoffs[trackIndex] = value;
                        }
                    }
                    
                    float getValue() override {
                        if (module) {
                            return clamp(module->bpfCutoffs[trackIndex], 20.0f, 8000.0f);
                        }
                        return 1000.0f;
                    }
                    
                    float getMinValue() override { return 20.0f; }
                    float getMaxValue() override { return 8000.0f; }
                    float getDefaultValue() override { 
                        if (trackIndex == 0) return 200.0f;
                        if (trackIndex == 1) return 1000.0f;
                        return 5000.0f;
                    }
                    
                    float getDisplayValue() override { return getValue(); }
                    void setDisplayValue(float displayValue) override { setValue(displayValue); }
                    std::string getLabel() override { return "Frequency"; }
                    std::string getUnit() override { return " Hz"; }
                    int getDisplayPrecision() override { return 0; }
                    
                    std::string getDisplayValueString() override {
                        return string::f("%.0f", getValue());
                    }
                };
                
                ui::Slider* frequencySlider = new ui::Slider;
                frequencySlider->box.size.x = 200.0f;
                frequencySlider->quantity = new BPFQuantity(module, i);
                menu->addChild(frequencySlider);
                
                // Output gain slider
                menu->addChild(createMenuLabel(string::f("Track %d Output Gain", i + 1)));
                
                struct BPFGainQuantity : Quantity {
                    ADGenerator* module;
                    int trackIndex;
                    
                    BPFGainQuantity(ADGenerator* m, int track) : module(m), trackIndex(track) {}
                    
                    void setValue(float value) override {
                        if (module) {
                            value = clamp(value, 0.1f, 10.0f);
                            module->bpfGains[trackIndex] = value;
                        }
                    }
                    
                    float getValue() override {
                        if (module) {
                            return clamp(module->bpfGains[trackIndex], 0.1f, 10.0f);
                        }
                        return 3.0f;
                    }
                    
                    float getMinValue() override { return 0.1f; }
                    float getMaxValue() override { return 10.0f; }
                    float getDefaultValue() override { return 3.0f; }
                    
                    float getDisplayValue() override { return getValue(); }
                    void setDisplayValue(float displayValue) override { setValue(displayValue); }
                    std::string getLabel() override { return "Output Gain"; }
                    std::string getUnit() override { return "x"; }
                    int getDisplayPrecision() override { return 1; }
                    
                    std::string getDisplayValueString() override {
                        return string::f("%.1f", getValue());
                    }
                };
                
                ui::Slider* gainSlider = new ui::Slider;
                gainSlider->box.size.x = 200.0f;
                gainSlider->quantity = new BPFGainQuantity(module, i);
                menu->addChild(gainSlider);
            }
        }
    }
};

Model* modelADGenerator = createModel<ADGenerator, ADGeneratorWidget>("ADGenerator");