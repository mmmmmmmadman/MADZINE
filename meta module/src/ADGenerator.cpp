#include "plugin.hpp"

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

    bool autoRouteEnabled = false;
    bool bpfEnabled[3] = {false, false, false};
    float bpfCutoffs[3] = {200.0f, 1000.0f, 5000.0f};
    float bpfGains[3] = {3.0f, 3.0f, 3.0f};
    
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
        
        Phase oldPhase = IDLE;
        float oldOutput = 0.0f;
        float oldAttackTime = 0.01f;
        float oldDecayTime = 1.0f;
        float oldPhaseTime = 0.0f;
        float oldCurve = 0.0f;
        dsp::SchmittTrigger oldTrigger;
        
        void reset() {
            phase = IDLE;
            triggerOutput = 0.0f;
            followerOutput = 0.0f;
            followerState = 0.0f;
            phaseTime = 0.0f;
            oldPhase = IDLE;
            oldOutput = 0.0f;
            oldPhaseTime = 0.0f;
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
        
        float processOldVersion(float sampleTime, float triggerVoltage, float attack, float decay, float curveParam, float atkAll, float decAll) {
            float atkOffset = atkAll * 0.5f;
            float decOffset = decAll * 0.5f;
            
            oldAttackTime = std::pow(10.0f, (attack - 0.5f) * 6.0f) + atkOffset;
            oldDecayTime = std::pow(10.0f, (decay - 0.5f) * 6.0f) + decOffset;
            
            oldAttackTime = std::max(0.001f, oldAttackTime);
            oldDecayTime = std::max(0.001f, oldDecayTime);
            
            oldCurve = curveParam;
            
            if (oldPhase == IDLE && oldTrigger.process(triggerVoltage)) {
                oldPhase = ATTACK;
                oldPhaseTime = 0.0f;
            }
            
            switch (oldPhase) {
                case IDLE:
                    oldOutput = 0.0f;
                    break;
                    
                case ATTACK:
                    oldPhaseTime += sampleTime;
                    if (oldPhaseTime >= oldAttackTime) {
                        oldPhase = DECAY;
                        oldPhaseTime = 0.0f;
                        oldOutput = 1.0f;
                    } else {
                        float t = oldPhaseTime / oldAttackTime;
                        oldOutput = applyCurve(t, oldCurve);
                    }
                    break;
                    
                case DECAY:
                    oldPhaseTime += sampleTime;
                    if (oldPhaseTime >= oldDecayTime) {
                        oldOutput = 0.0f;
                        oldPhase = IDLE;
                        oldPhaseTime = 0.0f;
                    } else {
                        float t = oldPhaseTime / oldDecayTime;
                        oldOutput = 1.0f - applyCurve(t, oldCurve);
                    }
                    break;
            }
            
            oldOutput = clamp(oldOutput, 0.0f, 1.0f);
            return oldOutput * 10.0f;
        }
        
        float process(float sampleTime, float triggerVoltage, float attack, float decay, float curveParam, float atkAll, float decAll, bool useBPF) {
            if (!useBPF) {
                return processOldVersion(sampleTime, triggerVoltage, attack, decay, curveParam, atkAll, decAll);
            } else {
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
        }
    };
    
    ADEnvelope envelopes[3];

    ADGenerator() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        configParam(ATK_ALL_PARAM, -1.0f, 1.0f, 0.0f, "Attack All");
        configParam(DEC_ALL_PARAM, -1.0f, 1.0f, 0.0f, "Decay All");
        configParam(AUTO_ROUTE_PARAM, 0.0f, 1.0f, 0.0f, "Auto Route");
        
        for (int i = 0; i < 3; ++i) {
            configParam(TRACK1_ATTACK_PARAM + i * 6, 0.0f, 1.0f, 0.1f, string::f("Track %d Attack", i + 1), " s", 0.0f, 1.0f, std::pow(10.0f, -2.0f));
            configParam(TRACK1_DECAY_PARAM + i * 6, 0.0f, 1.0f, 0.3f, string::f("Track %d Decay", i + 1), " s", 0.0f, 1.0f, std::pow(10.0f, -2.0f));
            configParam(TRACK1_CURVE_PARAM + i * 6, -0.99f, 0.99f, 0.0f, string::f("Track %d Curve", i + 1));
            configParam(TRACK1_BPF_ENABLE_PARAM + i * 6, 0.0f, 1.0f, 0.0f, string::f("Track %d BPF Enable", i + 1));
            configParam(TRACK1_BPF_FREQ_PARAM + i * 6, 20.0f, 8000.0f, i == 0 ? 200.0f : (i == 1 ? 1000.0f : 5000.0f), string::f("Track %d BPF Frequency", i + 1), " Hz");
            configParam(TRACK1_BPF_GAIN_PARAM + i * 6, 0.1f, 10.0f, 3.0f, string::f("Track %d BPF Gain", i + 1), "x");
            
            configInput(TRACK1_TRIG_INPUT + i, string::f("Track %d Trigger", i + 1));
            configOutput(TRACK1_OUTPUT + i, string::f("Track %d Envelope", i + 1));
        }
        
        configOutput(SUM_OUTPUT, "Sum");
        configLight(AUTO_ROUTE_LIGHT, "Auto Route Light");
        for (int i = 0; i < 3; ++i) {
            configLight(TRACK1_BPF_LIGHT + i, string::f("Track %d BPF Light", i + 1));
        }
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
        
        json_t* bpfEnabledJ = json_array();
        for (int i = 0; i < 3; ++i) {
            json_array_append_new(bpfEnabledJ, json_boolean(bpfEnabled[i]));
        }
        json_object_set_new(rootJ, "bpfEnabled", bpfEnabledJ);
        
        json_t* bpfCutoffsJ = json_array();
        for (int i = 0; i < 3; ++i) {
            json_array_append_new(bpfCutoffsJ, json_real(bpfCutoffs[i]));
        }
        json_object_set_new(rootJ, "bpfCutoffs", bpfCutoffsJ);
        
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
        
        json_t* bpfEnabledJ = json_object_get(rootJ, "bpfEnabled");
        if (bpfEnabledJ) {
            for (int i = 0; i < 3; ++i) {
                json_t* enabledJ = json_array_get(bpfEnabledJ, i);
                if (enabledJ) {
                    bpfEnabled[i] = json_boolean_value(enabledJ);
                }
            }
        }
        
        json_t* bpfCutoffsJ = json_object_get(rootJ, "bpfCutoffs");
        if (bpfCutoffsJ) {
            for (int i = 0; i < 3; ++i) {
                json_t* cutoffJ = json_array_get(bpfCutoffsJ, i);
                if (cutoffJ) {
                    bpfCutoffs[i] = json_real_value(cutoffJ);
                }
            }
        }
        
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
        
        autoRouteEnabled = params[AUTO_ROUTE_PARAM].getValue() > 0.5f;
        
        for (int i = 0; i < 3; ++i) {
            bpfEnabled[i] = params[TRACK1_BPF_ENABLE_PARAM + i * 6].getValue() > 0.5f;
            bpfCutoffs[i] = params[TRACK1_BPF_FREQ_PARAM + i * 6].getValue();
            bpfGains[i] = params[TRACK1_BPF_GAIN_PARAM + i * 6].getValue();
        }
        
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
            float processedSignal = inputSignals[i];
            if (bpfEnabled[i]) {
                processedSignal = bpfFilters[i].process(inputSignals[i], bpfCutoffs[i], args.sampleRate);
            }
            
            float attackParam = params[TRACK1_ATTACK_PARAM + i * 6].getValue();
            float decayParam = params[TRACK1_DECAY_PARAM + i * 6].getValue();
            float curveParam = params[TRACK1_CURVE_PARAM + i * 6].getValue();
            
            float envelopeOutput = envelopes[i].process(args.sampleTime, processedSignal, attackParam, decayParam, curveParam, atkAll, decAll, bpfEnabled[i]);
            
            if (bpfEnabled[i]) {
                envelopeOutput *= bpfGains[i];
            }
            
            outputs[TRACK1_OUTPUT + i].setVoltage(envelopeOutput);
            
            sumOutput += envelopeOutput * 0.33f;
        }
        
        sumOutput = clamp(sumOutput, 0.0f, 10.0f);
        outputs[SUM_OUTPUT].setVoltage(sumOutput);
        
        lights[AUTO_ROUTE_LIGHT].setBrightness(autoRouteEnabled ? 1.0f : 0.0f);
        for (int i = 0; i < 3; ++i) {
            lights[TRACK1_BPF_LIGHT + i].setBrightness(bpfEnabled[i] ? 1.0f : 0.0f);
        }
    }
};

struct ADGeneratorWidget : ModuleWidget {
    ADGeneratorWidget(ADGenerator* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "ADGenerator.png")));
        
        box.size = Vec(8 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

        addParam(createParamCentered<Trimpot>(Vec(30, 50), module, ADGenerator::ATK_ALL_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(65, 50), module, ADGenerator::DEC_ALL_PARAM));
        
        addParam(createParamCentered<VCVButton>(Vec(98, 50), module, ADGenerator::AUTO_ROUTE_PARAM));
        addChild(createLightCentered<MediumLight<RedLight>>(Vec(98, 65), module, ADGenerator::AUTO_ROUTE_LIGHT));

        float trackY[3] = {95, 185, 275};
        
        for (int i = 0; i < 3; ++i) {
            float y = trackY[i];
            float x = 10;

            addInput(createInputCentered<PJ301MPort>(Vec(x + 7, y - 3), module, ADGenerator::TRACK1_TRIG_INPUT + i));
            x += 27;

            addParam(createParamCentered<RoundBlackKnob>(Vec(x + 7, y - 3), module, ADGenerator::TRACK1_ATTACK_PARAM + i * 6));
            x += 27;

            addParam(createParamCentered<RoundBlackKnob>(Vec(x + 7, y - 3), module, ADGenerator::TRACK1_DECAY_PARAM + i * 6));
            x += 27;

            addParam(createParamCentered<RoundBlackKnob>(Vec(x + 7, y - 3), module, ADGenerator::TRACK1_CURVE_PARAM + i * 6));
            x += 27;

            x = 10;
            y += 35;

            addParam(createParamCentered<VCVButton>(Vec(x + 7, y - 3), module, ADGenerator::TRACK1_BPF_ENABLE_PARAM + i * 6));
            addChild(createLightCentered<MediumLight<BlueLight>>(Vec(x + 7, y + 12), module, ADGenerator::TRACK1_BPF_LIGHT + i));
            x += 27;

            addParam(createParamCentered<RoundBlackKnob>(Vec(x + 7, y - 3), module, ADGenerator::TRACK1_BPF_FREQ_PARAM + i * 6));
            x += 27;

            addParam(createParamCentered<RoundBlackKnob>(Vec(x + 7, y - 3), module, ADGenerator::TRACK1_BPF_GAIN_PARAM + i * 6));
        }
        
        addOutput(createOutputCentered<PJ301MPort>(Vec(13, 358), module, ADGenerator::TRACK1_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(44, 358), module, ADGenerator::TRACK2_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(75, 358), module, ADGenerator::TRACK3_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(104, 358), module, ADGenerator::SUM_OUTPUT));
    }
};

Model* modelADGenerator = createModel<ADGenerator, ADGeneratorWidget>("ADGenerator");