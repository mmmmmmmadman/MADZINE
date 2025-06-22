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

struct UFOWidget : Widget {
    UFOWidget(Vec pos, Vec size) {
        box.pos = pos;
        box.size = size;
    }
    
    void draw(const DrawArgs &args) override {
        float centerX = box.size.x / 2.0f;
        float centerY = box.size.y / 2.0f;
        
        // Save current transform
        nvgSave(args.vg);
        
        // Rotate everything by 15 degrees
        nvgTranslate(args.vg, centerX, centerY);
        nvgRotate(args.vg, 15.0f * M_PI / 180.0f);
        nvgTranslate(args.vg, -centerX, -centerY);
        
        nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
        
        // UFO Body (main disc - outline only)
        nvgBeginPath(args.vg);
        nvgEllipse(args.vg, centerX, centerY, 12.0f, 4.0f);
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStroke(args.vg);
        
        // UFO Dome (outline only)
        nvgBeginPath(args.vg);
        nvgEllipse(args.vg, centerX, centerY - 2.0f, 6.0f, 3.0f);
        nvgStrokeWidth(args.vg, 0.6f);
        nvgStroke(args.vg);
        
        // Small lights on UFO (white dots)
        nvgStrokeWidth(args.vg, 1.0f);
        for (int i = 0; i < 5; ++i) {
            float angle = i * 2.0f * M_PI / 5.0f;
            float lightX = centerX + 8.0f * cosf(angle);
            float lightY = centerY + 2.0f * sinf(angle);
            
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, lightX, lightY, 1.0f);
            nvgStroke(args.vg);
        }
        
        // Light beams (tilted with UFO)
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, centerX - 8.0f, centerY + 4.0f);
        nvgLineTo(args.vg, centerX - 12.0f, centerY + 12.0f);
        nvgLineTo(args.vg, centerX + 12.0f, centerY + 12.0f);
        nvgLineTo(args.vg, centerX + 8.0f, centerY + 4.0f);
        nvgClosePath(args.vg);
        nvgStrokeWidth(args.vg, 0.5f);
        nvgStroke(args.vg);
        
        // Restore transform
        nvgRestore(args.vg);
    }
};

struct CowHeadWidget : Widget {
    CowHeadWidget(Vec pos, Vec size) {
        box.pos = pos;
        box.size = size;
    }
    
    void draw(const DrawArgs &args) override {
        float centerX = box.size.x / 2.0f;
        float centerY = box.size.y / 2.0f;
        
        // Cow head (main oval)
        nvgBeginPath(args.vg);
        nvgEllipse(args.vg, centerX, centerY, 10.0f, 8.0f);
        nvgFillColor(args.vg, nvgRGB(240, 235, 230));
        nvgFill(args.vg);
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStrokeColor(args.vg, nvgRGB(180, 175, 170));
        nvgStroke(args.vg);
        
        // Cow spots
        nvgBeginPath(args.vg);
        nvgEllipse(args.vg, centerX - 3, centerY - 2, 2.5f, 2.0f);
        nvgFillColor(args.vg, nvgRGB(60, 60, 60));
        nvgFill(args.vg);
        
        nvgBeginPath(args.vg);
        nvgEllipse(args.vg, centerX + 4, centerY + 1, 2.0f, 1.5f);
        nvgFillColor(args.vg, nvgRGB(60, 60, 60));
        nvgFill(args.vg);
        
        // Cow ears
        nvgBeginPath(args.vg);
        nvgEllipse(args.vg, centerX - 8, centerY - 3, 3.0f, 4.0f);
        nvgFillColor(args.vg, nvgRGB(220, 215, 210));
        nvgFill(args.vg);
        
        nvgBeginPath(args.vg);
        nvgEllipse(args.vg, centerX + 8, centerY - 3, 3.0f, 4.0f);
        nvgFillColor(args.vg, nvgRGB(220, 215, 210));
        nvgFill(args.vg);
        
        // Cow horns
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, centerX - 6, centerY - 6);
        nvgLineTo(args.vg, centerX - 8, centerY - 10);
        nvgLineTo(args.vg, centerX - 4, centerY - 8);
        nvgClosePath(args.vg);
        nvgFillColor(args.vg, nvgRGB(200, 180, 160));
        nvgFill(args.vg);
        
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, centerX + 6, centerY - 6);
        nvgLineTo(args.vg, centerX + 8, centerY - 10);
        nvgLineTo(args.vg, centerX + 4, centerY - 8);
        nvgClosePath(args.vg);
        nvgFillColor(args.vg, nvgRGB(200, 180, 160));
        nvgFill(args.vg);
        
        // Cow eyes
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, centerX - 3, centerY - 1, 1.5f);
        nvgFillColor(args.vg, nvgRGB(80, 80, 80));
        nvgFill(args.vg);
        
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, centerX + 3, centerY - 1, 1.5f);
        nvgFillColor(args.vg, nvgRGB(80, 80, 80));
        nvgFill(args.vg);
        
        // Cow nostrils
        nvgBeginPath(args.vg);
        nvgEllipse(args.vg, centerX, centerY + 3, 3.0f, 2.0f);
        nvgFillColor(args.vg, nvgRGB(200, 190, 180));
        nvgFill(args.vg);
        
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, centerX - 1, centerY + 3, 0.8f);
        nvgFillColor(args.vg, nvgRGB(100, 90, 80));
        nvgFill(args.vg);
        
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, centerX + 1, centerY + 3, 0.8f);
        nvgFillColor(args.vg, nvgRGB(100, 90, 80));
        nvgFill(args.vg);
    }
};

struct HouseWidget : Widget {
    HouseWidget(Vec pos, Vec size) {
        box.pos = pos;
        box.size = size;
    }
    
    void draw(const DrawArgs &args) override {
        float centerX = box.size.x / 2.0f;
        float centerY = box.size.y / 2.0f;
        
        // Save current transform
        nvgSave(args.vg);
        
        // Rotate the house by -10 degrees
        nvgTranslate(args.vg, centerX, centerY);
        nvgRotate(args.vg, -10.0f * M_PI / 180.0f);
        nvgTranslate(args.vg, -centerX, -centerY);
        
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
        
        // House base (outline only)
        nvgBeginPath(args.vg);
        nvgRect(args.vg, centerX - 8, centerY, 16, 10);
        nvgStroke(args.vg);
        
        // House roof (outline only)
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, centerX - 10, centerY);
        nvgLineTo(args.vg, centerX, centerY - 8);
        nvgLineTo(args.vg, centerX + 10, centerY);
        nvgClosePath(args.vg);
        nvgStroke(args.vg);
        
        nvgStrokeWidth(args.vg, 0.6f);
        
        // Door (outline only)
        nvgBeginPath(args.vg);
        nvgRect(args.vg, centerX - 2, centerY + 4, 4, 6);
        nvgStroke(args.vg);
        
        // Door knob
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, centerX + 1, centerY + 7, 0.5f);
        nvgStroke(args.vg);
        
        // Windows (outline only)
        nvgBeginPath(args.vg);
        nvgRect(args.vg, centerX - 6, centerY + 2, 2.5f, 2.5f);
        nvgStroke(args.vg);
        
        nvgBeginPath(args.vg);
        nvgRect(args.vg, centerX + 3.5f, centerY + 2, 2.5f, 2.5f);
        nvgStroke(args.vg);
        
        // Window cross
        nvgStrokeWidth(args.vg, 0.4f);
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, centerX - 4.75f, centerY + 2);
        nvgLineTo(args.vg, centerX - 4.75f, centerY + 4.5f);
        nvgStroke(args.vg);
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, centerX - 6, centerY + 3.25f);
        nvgLineTo(args.vg, centerX - 3.5f, centerY + 3.25f);
        nvgStroke(args.vg);
        
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, centerX + 4.75f, centerY + 2);
        nvgLineTo(args.vg, centerX + 4.75f, centerY + 4.5f);
        nvgStroke(args.vg);
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, centerX + 3.5f, centerY + 3.25f);
        nvgLineTo(args.vg, centerX + 6, centerY + 3.25f);
        nvgStroke(args.vg);
        
        // Chimney (outline only)
        nvgStrokeWidth(args.vg, 0.6f);
        nvgBeginPath(args.vg);
        nvgRect(args.vg, centerX + 6, centerY - 6, 2, 4);
        nvgStroke(args.vg);
        
        // Restore transform
        nvgRestore(args.vg);
    }
};

struct FluteWidget : Widget {
    FluteWidget(Vec pos, Vec size) {
        box.pos = pos;
        box.size = size;
    }
    
    void draw(const DrawArgs &args) override {
        float centerX = box.size.x / 2.0f;
        float centerY = box.size.y / 2.0f;
        
        // Save current transform
        nvgSave(args.vg);
        
        // Rotate the flute by -15 degrees
        nvgTranslate(args.vg, centerX, centerY);
        nvgRotate(args.vg, -15.0f * M_PI / 180.0f);
        nvgTranslate(args.vg, -centerX, -centerY);
        
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
        
        // Main flute body (long rectangle)
        nvgBeginPath(args.vg);
        nvgRect(args.vg, centerX - 15, centerY - 1.5f, 30, 3);
        nvgStroke(args.vg);
        
        // Flute mouthpiece (smaller rectangle on left)
        nvgBeginPath(args.vg);
        nvgRect(args.vg, centerX - 18, centerY - 1, 3, 2);
        nvgStroke(args.vg);
        
        nvgStrokeWidth(args.vg, 0.5f);
        
        // Tone holes (small circles along the body)
        float holePositions[] = {-10, -6, -2, 2, 6, 10};
        for (int i = 0; i < 6; ++i) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, centerX + holePositions[i], centerY, 0.8f);
            nvgStroke(args.vg);
        }
        
        // Keys (small rectangles above some holes)
        nvgStrokeWidth(args.vg, 0.4f);
        nvgBeginPath(args.vg);
        nvgRect(args.vg, centerX - 7, centerY - 3, 2, 1.5f);
        nvgStroke(args.vg);
        
        nvgBeginPath(args.vg);
        nvgRect(args.vg, centerX + 1, centerY - 3, 2, 1.5f);
        nvgStroke(args.vg);
        
        nvgBeginPath(args.vg);
        nvgRect(args.vg, centerX + 9, centerY - 3, 2, 1.5f);
        nvgStroke(args.vg);
        
        // Key connectors (thin lines)
        nvgStrokeWidth(args.vg, 0.3f);
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, centerX - 6, centerY - 2.25f);
        nvgLineTo(args.vg, centerX - 6, centerY - 1.5f);
        nvgStroke(args.vg);
        
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, centerX + 2, centerY - 2.25f);
        nvgLineTo(args.vg, centerX + 2, centerY - 1.5f);
        nvgStroke(args.vg);
        
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, centerX + 10, centerY - 2.25f);
        nvgLineTo(args.vg, centerX + 10, centerY - 1.5f);
        nvgStroke(args.vg);
        
        // Restore transform
        nvgRestore(args.vg);
    }
};

struct BPFFreqParamQuantity : ParamQuantity {
    std::string getDisplayValueString() override {
        float freq = std::pow(10.0f, getValue() * std::log10(8000.0f / 20.0f) + std::log10(20.0f));
        return string::f("%.0f Hz", freq);
    }
};

struct BPFGainParamQuantity : ParamQuantity {
    std::string getDisplayValueString() override {
        float gain = std::pow(10.0f, getValue() * std::log10(10.0f / 0.1f) + std::log10(0.1f));
        return string::f("%.1fx", gain);
    }
};

struct ADGenerator : Module {
    enum ParamId {
        ATK_ALL_PARAM,
        DEC_ALL_PARAM,
        AUTO_ROUTE_PARAM,
        TRACK1_ATTACK_PARAM,
        TRACK1_DECAY_PARAM,
        TRACK1_CURVE_PARAM,
        TRACK1_BPF_ENABLE_PARAM,
        TRACK1_BPF_FREQ_PARAM,
        TRACK1_BPF_GAIN_PARAM,
        TRACK2_ATTACK_PARAM,
        TRACK2_DECAY_PARAM,
        TRACK2_CURVE_PARAM,
        TRACK2_BPF_ENABLE_PARAM,
        TRACK2_BPF_FREQ_PARAM,
        TRACK2_BPF_GAIN_PARAM,
        TRACK3_ATTACK_PARAM,
        TRACK3_DECAY_PARAM,
        TRACK3_CURVE_PARAM,
        TRACK3_BPF_ENABLE_PARAM,
        TRACK3_BPF_FREQ_PARAM,
        TRACK3_BPF_GAIN_PARAM,
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
        AUTO_ROUTE_LIGHT,
        TRACK1_BPF_LIGHT,
        TRACK2_BPF_LIGHT,
        TRACK3_BPF_LIGHT,
        LIGHTS_LEN
    };

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
            attackCoeff = 1.0f - std::exp(-sampleTime / std::max(0.0005f, attackTime * 0.1f));
            releaseCoeff = 1.0f - std::exp(-sampleTime / std::max(0.001f, releaseTime * 0.5f));
            
            attackCoeff = clamp(attackCoeff, 0.0f, 1.0f);
            releaseCoeff = clamp(releaseCoeff, 0.0f, 1.0f);
            
            float rectified = std::abs(triggerVoltage) / 10.0f;
            rectified = clamp(rectified, 0.0f, 1.0f);
            
            float targetCoeff;
            if (rectified > followerState) {
                float progress = attackCoeff;
                targetCoeff = applyCurve(progress, curve);
            } else {
                float progress = releaseCoeff;
                targetCoeff = applyCurve(progress, curve);
            }
            
            targetCoeff = clamp(targetCoeff, 0.0f, 1.0f);
            
            followerState += (rectified - followerState) * targetCoeff;
            followerState = clamp(followerState, 0.0f, 1.0f);
            
            return followerState;
        }
        
        float processTriggerEnvelope(float triggerVoltage, float sampleTime, float attack, float decay, float curve) {
            bool isHighVoltage = (std::abs(triggerVoltage) > 9.5f);
            
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
            
            float triggerEnv = processTriggerEnvelope(triggerVoltage, sampleTime, attackTime, decayTime, curve);
            float followerEnv = processEnvelopeFollower(triggerVoltage, sampleTime, attackTime, decayTime, curve);
            
            float output = std::max(triggerEnv, followerEnv);
            
            return output * 10.0f;
        }
    };
    
    ADEnvelope envelopes[3];

    ADGenerator() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        configParam(ATK_ALL_PARAM, -1.0f, 1.0f, 0.0f, "Attack All");
        configParam(DEC_ALL_PARAM, -1.0f, 1.0f, 0.0f, "Decay All");
        configSwitch(AUTO_ROUTE_PARAM, 0.0f, 1.0f, 0.0f, "Auto Route", {"Off", "On"});
        
        for (int i = 0; i < 3; ++i) {
            int baseParam = TRACK1_ATTACK_PARAM + i * 6;
            
            configParam(baseParam, 0.0f, 1.0f, 0.1f, string::f("Track %d Attack", i + 1), " s", 0.0f, 1.0f, std::pow(10.0f, -2.0f));
            configParam(baseParam + 1, 0.0f, 1.0f, 0.3f, string::f("Track %d Decay", i + 1), " s", 0.0f, 1.0f, std::pow(10.0f, -2.0f));
            configParam(baseParam + 2, -1.0f, 1.0f, 0.0f, string::f("Track %d Curve", i + 1));
            configSwitch(baseParam + 3, 0.0f, 1.0f, 0.0f, string::f("Track %d BPF Enable", i + 1), {"Off", "On"});
            
            configParam<BPFFreqParamQuantity>(baseParam + 4, 0.0f, 1.0f, 
                i == 0 ? 0.25f : (i == 1 ? 0.5f : 0.75f), string::f("Track %d BPF Frequency", i + 1));
            
            configParam<BPFGainParamQuantity>(baseParam + 5, 0.0f, 1.0f, 0.5f, string::f("Track %d BPF Gain", i + 1));
            
            configInput(TRACK1_TRIG_INPUT + i, string::f("Track %d", i + 1));
            configOutput(TRACK1_OUTPUT + i, string::f("Track %d", i + 1));
            configLight(TRACK1_BPF_LIGHT + i, string::f("Track %d BPF Light", i + 1));
        }
        
        configOutput(SUM_OUTPUT, "Sum");
        configLight(AUTO_ROUTE_LIGHT, "Auto Route Light");
    }

    void onReset() override {
        for (int i = 0; i < 3; ++i) {
            envelopes[i].reset();
            bpfFilters[i].reset();
        }
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
    }

    float getBPFFrequency(int track) {
        float param = params[TRACK1_BPF_FREQ_PARAM + track * 6].getValue();
        return std::pow(10.0f, param * std::log10(8000.0f / 20.0f) + std::log10(20.0f));
    }
    
    float getBPFGain(int track) {
        float param = params[TRACK1_BPF_GAIN_PARAM + track * 6].getValue();
        return std::pow(10.0f, param * std::log10(10.0f / 0.1f) + std::log10(0.1f));
    }

    void process(const ProcessArgs& args) override {
        float sumOutput = 0.0f;
        float atkAll = params[ATK_ALL_PARAM].getValue();
        float decAll = params[DEC_ALL_PARAM].getValue();
        bool autoRouteEnabled = params[AUTO_ROUTE_PARAM].getValue() > 0.5f;
        
        lights[AUTO_ROUTE_LIGHT].setBrightness(autoRouteEnabled ? 1.0f : 0.0f);
        
        float inputSignals[3];
        
        if (autoRouteEnabled) {
            float input1Signal = inputs[TRACK1_TRIG_INPUT].getVoltage();
            inputSignals[0] = input1Signal;
            inputSignals[1] = input1Signal;
            inputSignals[2] = input1Signal;
        } else {
            inputSignals[0] = inputs[TRACK1_TRIG_INPUT].getVoltage();
            inputSignals[1] = inputs[TRACK2_TRIG_INPUT].getVoltage();
            inputSignals[2] = inputs[TRACK3_TRIG_INPUT].getVoltage();
        }
        
        for (int i = 0; i < 3; ++i) {
            bool bpfEnabled = params[TRACK1_BPF_ENABLE_PARAM + i * 6].getValue() > 0.5f;
            lights[TRACK1_BPF_LIGHT + i].setBrightness(bpfEnabled ? 1.0f : 0.0f);
            
            float processedSignal = inputSignals[i];
            if (bpfEnabled) {
                float cutoff = getBPFFrequency(i);
                processedSignal = bpfFilters[i].process(inputSignals[i], cutoff, args.sampleRate);
            }
            
            float attackParam = params[TRACK1_ATTACK_PARAM + i * 6].getValue();
            float decayParam = params[TRACK1_DECAY_PARAM + i * 6].getValue();
            float curveParam = params[TRACK1_CURVE_PARAM + i * 6].getValue();
            
            float envelopeOutput = envelopes[i].process(args.sampleTime, processedSignal, attackParam, decayParam, curveParam, atkAll, decAll);
            
            if (bpfEnabled) {
                float gain = getBPFGain(i);
                envelopeOutput *= gain;
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
        setPanel(createPanel(asset::plugin(pluginInstance, "res/EuclideanRhythm.svg")));
        
        box.size = Vec(8 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

        addChild(new EnhancedTextLabel(Vec(0, 1), Vec(box.size.x, 20), "ADGenerator", 12.f, nvgRGB(255, 200, 0), true));
        addChild(new EnhancedTextLabel(Vec(0, 13), Vec(box.size.x, 20), "MADZINE", 10.f, nvgRGB(255, 200, 0), false));

        // Add UFO in the empty space (white outline, tilted with beam, moved right 2 pixels)
        addChild(new UFOWidget(Vec(80, 285), Vec(40, 25)));

        // Add Flute under Track 1 CURV (white outline, tilted, moved up 5 pixels)
        addChild(new FluteWidget(Vec(78, 125), Vec(40, 25)));

        // Add House under Track 2 CURV (moved right 2 pixels)
        addChild(new HouseWidget(Vec(80, 205), Vec(40, 25)));

        addChild(new EnhancedTextLabel(Vec(15, 30), Vec(30, 15), "ATK ALL", 7.f, nvgRGB(255, 255, 255), true));
        addParam(createParamCentered<Trimpot>(Vec(30, 50), module, ADGenerator::ATK_ALL_PARAM));
        
        addChild(new EnhancedTextLabel(Vec(50, 30), Vec(30, 15), "DEC ALL", 7.f, nvgRGB(255, 255, 255), true));
        addParam(createParamCentered<Trimpot>(Vec(65, 50), module, ADGenerator::DEC_ALL_PARAM));
        
        addChild(new EnhancedTextLabel(Vec(83, 30), Vec(30, 15), "ROUTE", 7.f, nvgRGB(255, 255, 255), true));
        addParam(createParamCentered<VCVLatch>(Vec(98, 50), module, ADGenerator::AUTO_ROUTE_PARAM));
        addChild(createLightCentered<MediumLight<RedLight>>(Vec(98, 65), module, ADGenerator::AUTO_ROUTE_LIGHT));

        float trackY[3] = {95, 185, 275};
        
        for (int i = 0; i < 3; ++i) {
            float y = trackY[i];
            float x = 10;

            addChild(new EnhancedTextLabel(Vec(x - 5, y - 25), Vec(25, 10), "IN", 7.f, nvgRGB(255, 255, 255), true));
            addInput(createInputCentered<PJ301MPort>(Vec(x + 7, y - 3), module, ADGenerator::TRACK1_TRIG_INPUT + i));
            x += 27;

            addChild(new EnhancedTextLabel(Vec(x - 5, y - 25), Vec(25, 10), "ATK", 7.f, nvgRGB(255, 255, 255), true));
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(x + 7, y - 3), module, ADGenerator::TRACK1_ATTACK_PARAM + i * 6));
            x += 27;

            addChild(new EnhancedTextLabel(Vec(x - 5, y - 25), Vec(25, 10), "DEC", 7.f, nvgRGB(255, 255, 255), true));
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(x + 7, y - 3), module, ADGenerator::TRACK1_DECAY_PARAM + i * 6));
            x += 27;

            addChild(new EnhancedTextLabel(Vec(x - 5, y - 25), Vec(25, 10), "CURV", 7.f, nvgRGB(255, 255, 255), true));
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(x + 7, y - 3), module, ADGenerator::TRACK1_CURVE_PARAM + i * 6));
            x += 27;

            x = 10;
            y += 35;

            addChild(new EnhancedTextLabel(Vec(x - 5, y - 25), Vec(25, 10), "BPF", 7.f, nvgRGB(255, 255, 255), true));
            addParam(createParamCentered<VCVLatch>(Vec(x + 7, y - 3), module, ADGenerator::TRACK1_BPF_ENABLE_PARAM + i * 6));
            addChild(createLightCentered<MediumLight<BlueLight>>(Vec(x + 7, y + 12), module, ADGenerator::TRACK1_BPF_LIGHT + i));
            x += 27;

            addChild(new EnhancedTextLabel(Vec(x - 5, y - 25), Vec(25, 10), "FREQ", 7.f, nvgRGB(255, 255, 255), true));
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(x + 7, y - 3), module, ADGenerator::TRACK1_BPF_FREQ_PARAM + i * 6));
            x += 27;

            addChild(new EnhancedTextLabel(Vec(x - 5, y - 25), Vec(25, 10), "GAIN", 7.f, nvgRGB(255, 255, 255), true));
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(x + 7, y - 3), module, ADGenerator::TRACK1_BPF_GAIN_PARAM + i * 6));
        }
        
        addChild(new WhiteBackgroundBox(Vec(0, 330), Vec(box.size.x, box.size.y - 325)));
        
        addOutput(createOutputCentered<PJ301MPort>(Vec(13, 358), module, ADGenerator::TRACK1_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(44, 358), module, ADGenerator::TRACK2_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(75, 358), module, ADGenerator::TRACK3_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(104, 358), module, ADGenerator::SUM_OUTPUT));
        
        addChild(new EnhancedTextLabel(Vec(7, 337), Vec(12, 10), "1", 7.f, nvgRGB(255, 133, 133), true));
        addChild(new EnhancedTextLabel(Vec(38, 337), Vec(12, 10), "2", 7.f, nvgRGB(255, 133, 133), true));
        addChild(new EnhancedTextLabel(Vec(69, 337), Vec(12, 10), "3", 7.f, nvgRGB(255, 133, 133), true));
        addChild(new EnhancedTextLabel(Vec(96, 337), Vec(16, 10), "MIYA", 7.f, nvgRGB(255, 133, 133), true));
    }
};

Model* modelADGenerator = createModel<ADGenerator, ADGeneratorWidget>("ADGenerator");