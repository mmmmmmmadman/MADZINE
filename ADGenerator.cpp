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

    struct ADEnvelope {
        enum Phase {
            IDLE,
            ATTACK,
            DECAY
        };
        
        Phase phase = IDLE;
        float output = 0.0f;
        float attackTime = 0.01f;
        float decayTime = 1.0f;
        float phaseTime = 0.0f;
        float curve = 0.0f;
        
        dsp::SchmittTrigger trigger;
        
        void reset() {
            phase = IDLE;
            output = 0.0f;
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
        
        float process(float sampleTime, float triggerVoltage, float attack, float decay, float curveParam) {
            attackTime = std::pow(10.0f, (attack - 0.5f) * 6.0f);
            decayTime = std::pow(10.0f, (decay - 0.5f) * 6.0f);
            curve = curveParam;
            
            if (phase == IDLE && trigger.process(triggerVoltage)) {
                phase = ATTACK;
                phaseTime = 0.0f;
            }
            
            switch (phase) {
                case IDLE:
                    output = 0.0f;
                    break;
                    
                case ATTACK:
                    phaseTime += sampleTime;
                    if (phaseTime >= attackTime) {
                        phase = DECAY;
                        phaseTime = 0.0f;
                        output = 1.0f;
                    } else {
                        float t = phaseTime / attackTime;
                        output = applyCurve(t, curve);
                    }
                    break;
                    
                case DECAY:
                    phaseTime += sampleTime;
                    if (phaseTime >= decayTime) {
                        output = 0.0f;
                        phase = IDLE;
                        phaseTime = 0.0f;
                    } else {
                        float t = phaseTime / decayTime;
                        output = 1.0f - applyCurve(t, curve);
                    }
                    break;
            }
            
            output = clamp(output, 0.0f, 1.0f);
            return output * 10.0f;
        }
    };
    
    ADEnvelope envelopes[3];

    ADGenerator() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        for (int i = 0; i < 3; ++i) {
            configParam(TRACK1_ATTACK_PARAM + i * 3, 0.0f, 1.0f, 0.1f, string::f("Track %d Attack", i + 1), " s", 0.0f, 1.0f, std::pow(10.0f, -2.0f));
            configParam(TRACK1_DECAY_PARAM + i * 3, 0.0f, 1.0f, 0.3f, string::f("Track %d Decay", i + 1), " s", 0.0f, 1.0f, std::pow(10.0f, -2.0f));
            configParam(TRACK1_CURVE_PARAM + i * 3, -1.0f, 1.0f, 0.0f, string::f("Track %d Curve", i + 1));
            configInput(TRACK1_TRIG_INPUT + i, string::f("Track %d Trigger", i + 1));
            configOutput(TRACK1_OUTPUT + i, string::f("Track %d Output", i + 1));
        }
        
        configOutput(SUM_OUTPUT, "Sum Output");
    }

    void onReset() override {
        for (int i = 0; i < 3; ++i) {
            envelopes[i].reset();
        }
    }

    void process(const ProcessArgs& args) override {
        float sumOutput = 0.0f;
        
        for (int i = 0; i < 3; ++i) {
            float triggerVoltage = inputs[TRACK1_TRIG_INPUT + i].getVoltage();
            float attackParam = params[TRACK1_ATTACK_PARAM + i * 3].getValue();
            float decayParam = params[TRACK1_DECAY_PARAM + i * 3].getValue();
            float curveParam = params[TRACK1_CURVE_PARAM + i * 3].getValue();
            
            float envelopeOutput = envelopes[i].process(args.sampleTime, triggerVoltage, attackParam, decayParam, curveParam);
            
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

        float trackY[3] = {65, 147, 229};
        
        for (int i = 0; i < 3; ++i) {
            float y = trackY[i];
            float centerX = box.size.x / 2;

            addChild(new EnhancedTextLabel(Vec(1, y - 22), Vec(25, 10), "TRIG", 7.f, nvgRGB(200, 200, 200), true));
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
};

Model* modelADGenerator = createModel<ADGenerator, ADGeneratorWidget>("ADGenerator");