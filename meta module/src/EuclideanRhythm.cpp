#include "plugin.hpp"

struct DivMultParamQuantity : ParamQuantity {
    std::string getDisplayValueString() override {
        int value = (int)std::round(getValue());
        if (value > 0) {
            return string::f("%dx", value + 1);
        } else if (value < 0) {
            return string::f("1/%dx", -value + 1);
        } else {
            return "1x";
        }
    }
};

void generateEuclideanRhythm(bool pattern[], int length, int fill, int shift) {
    for (int i = 0; i < 32; ++i) {
        pattern[i] = false;
    }
    if (fill == 0 || length == 0) return;
    if (fill > length) fill = length;

    shift = shift % length;
    if (shift < 0) shift += length;

    for (int i = 0; i < fill; ++i) {
        int index = (int)std::floor((float)i * length / fill);
        pattern[index] = true;
    }
    
    if (shift > 0) {
        bool temp[32];
        for (int i = 0; i < length; ++i) {
            temp[i] = pattern[i];
        }
        for (int i = 0; i < length; ++i) {
            int newIndex = (i + shift) % length;
            pattern[newIndex] = temp[i];
        }
    }
}

struct EuclideanRhythm : Module {
    enum ParamId {
        MANUAL_RESET_PARAM,
        TRACK1_DIVMULT_PARAM,
        TRACK1_LENGTH_PARAM,
        TRACK1_FILL_PARAM,
        TRACK1_SHIFT_PARAM,
        TRACK1_LENGTH_CV_ATTEN_PARAM,
        TRACK1_FILL_CV_ATTEN_PARAM,
        TRACK1_SHIFT_CV_ATTEN_PARAM,
        TRACK2_DIVMULT_PARAM,
        TRACK2_LENGTH_PARAM,
        TRACK2_FILL_PARAM,
        TRACK2_SHIFT_PARAM,
        TRACK2_LENGTH_CV_ATTEN_PARAM,
        TRACK2_FILL_CV_ATTEN_PARAM,
        TRACK2_SHIFT_CV_ATTEN_PARAM,
        TRACK3_DIVMULT_PARAM,
        TRACK3_LENGTH_PARAM,
        TRACK3_FILL_PARAM,
        TRACK3_SHIFT_PARAM,
        TRACK3_LENGTH_CV_ATTEN_PARAM,
        TRACK3_FILL_CV_ATTEN_PARAM,
        TRACK3_SHIFT_CV_ATTEN_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        GLOBAL_CLOCK_INPUT,
        GLOBAL_RESET_INPUT,
        TRACK1_LENGTH_CV_INPUT,
        TRACK1_FILL_CV_INPUT,
        TRACK1_SHIFT_CV_INPUT,
        TRACK2_LENGTH_CV_INPUT,
        TRACK2_FILL_CV_INPUT,
        TRACK2_SHIFT_CV_INPUT,
        TRACK3_LENGTH_CV_INPUT,
        TRACK3_FILL_CV_INPUT,
        TRACK3_SHIFT_CV_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        TRACK1_TRIG_OUTPUT,
        TRACK2_TRIG_OUTPUT,
        TRACK3_TRIG_OUTPUT,
        MASTER_TRIG_OUTPUT,
        CHAIN_12_OUTPUT,
        CHAIN_23_OUTPUT,
        CHAIN_123_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        TRACK1_LIGHT,
        TRACK2_LIGHT,
        TRACK3_LIGHT,
        CHAIN_12_T1_LIGHT,
        CHAIN_12_T2_LIGHT,
        CHAIN_23_T2_LIGHT,
        CHAIN_23_T3_LIGHT,
        CHAIN_123_T1_LIGHT,
        CHAIN_123_T2_LIGHT,
        CHAIN_123_T3_LIGHT, 
        OR_RED_LIGHT,
        OR_GREEN_LIGHT,
        OR_BLUE_LIGHT,
        LIGHTS_LEN
    };

    dsp::SchmittTrigger clockTrigger;
    dsp::SchmittTrigger resetTrigger;
    dsp::SchmittTrigger manualResetTrigger;
    
    float globalClockSeconds = 0.5f;
    float secondsSinceLastClock = -1.0f;
    
    dsp::PulseGenerator orRedPulse;
    dsp::PulseGenerator orGreenPulse;
    dsp::PulseGenerator orBluePulse;

    struct TrackState {
        int divMultValue = 0;
        int division = 1;
        int multiplication = 1;
        float dividedClockSeconds = 0.5f;
        float multipliedClockSeconds = 0.5f;
        float dividedProgressSeconds = 0.0f;
        float gateSeconds = 0.0f;
        int dividerCount = 0;
        bool shouldStep = false;
        bool prevMultipliedGate = false;
        
        int currentStep = 0;
        int length = 16;
        int fill = 4;
        int shift = 0;
        bool pattern[32];
        bool gateState = false;
        bool cycleCompleted = false;
        dsp::PulseGenerator trigPulse;

        void reset() {
            dividedProgressSeconds = 0.0f;
            dividerCount = 0;
            shouldStep = false;
            prevMultipliedGate = false;
            currentStep = 0;
            for (int i = 0; i < 32; ++i) {
                pattern[i] = false;
            }
            gateState = false;
            cycleCompleted = false;
        }
        
        void updateDivMult(int divMultParam) {
            divMultValue = divMultParam;
            if (divMultValue > 0) {
                division = 1;
                multiplication = divMultValue + 1;
            } else if (divMultValue < 0) {
                division = -divMultValue + 1;
                multiplication = 1;
            } else {
                division = 1;
                multiplication = 1;
            }
        }
        
        bool processClockDivMult(bool globalClock, float globalClockSeconds, float sampleTime) {
            dividedClockSeconds = globalClockSeconds * (float)division;
            multipliedClockSeconds = dividedClockSeconds / (float)multiplication;
            gateSeconds = std::max(0.001f, multipliedClockSeconds * 0.5f);
            
            if (globalClock) {
                if (dividerCount < 1) {
                    dividedProgressSeconds = 0.0f;
                } else {
                    dividedProgressSeconds += sampleTime;
                }
                ++dividerCount;
                if (dividerCount >= division) {
                    dividerCount = 0;
                }
            } else {
                dividedProgressSeconds += sampleTime;
            }
            
            shouldStep = false;
            if (dividedProgressSeconds < dividedClockSeconds) {
                float multipliedProgressSeconds = dividedProgressSeconds / multipliedClockSeconds;
                multipliedProgressSeconds -= (float)(int)multipliedProgressSeconds;
                multipliedProgressSeconds *= multipliedClockSeconds;
                
                bool currentMultipliedGate = multipliedProgressSeconds <= gateSeconds;
                
                if (currentMultipliedGate && !prevMultipliedGate) {
                    shouldStep = true;
                }
                prevMultipliedGate = currentMultipliedGate;
            }
            
            return shouldStep;
        }
        
        void stepTrack() {
            cycleCompleted = false;
            currentStep = (currentStep + 1) % length;
            if (currentStep == 0) {
                cycleCompleted = true;
            }
            gateState = pattern[currentStep];
            if (gateState) {
                trigPulse.trigger(0.01f);
            }
        }
    };
    TrackState tracks[3];

    struct ChainedSequence {
        int currentTrackIndex = 0;
        int trackIndices[4];
        int trackCount = 0;
        int globalClockCount = 0;
        int trackStartClock[3] = {0, 0, 0};
        
        ChainedSequence() {
            for (int i = 0; i < 4; ++i) {
                trackIndices[i] = -1;
            }
        }
        
        void setTrackIndices(int indices[], int count) {
            trackCount = count;
            for (int i = 0; i < count && i < 4; ++i) {
                trackIndices[i] = indices[i];
            }
        }
        
        void reset() {
            currentTrackIndex = 0;
            globalClockCount = 0;
            for (int i = 0; i < 3; ++i) {
                trackStartClock[i] = 0;
            }
        }
        
        int calculateTrackCycleClock(const TrackState& track) {
            return track.length * track.division / track.multiplication;
        }
        
        float processStep(TrackState tracks[], float sampleTime, bool globalClockTriggered) {
            if (trackCount == 0) {
                return 0.0f;
            }
            
            if (globalClockTriggered) {
                globalClockCount++;
            }
            
            if (currentTrackIndex >= trackCount) {
                currentTrackIndex = 0;
            }
            
            int activeTrackIdx = trackIndices[currentTrackIndex];
            if (activeTrackIdx < 0 || activeTrackIdx >= 3) {
                return 0.0f;
            }
            
            TrackState& activeTrack = tracks[activeTrackIdx];
            int trackCycleClock = calculateTrackCycleClock(activeTrack);
            int elapsedClock = globalClockCount - trackStartClock[activeTrackIdx];
            
            if (elapsedClock >= trackCycleClock) {
                currentTrackIndex++;
                if (currentTrackIndex >= trackCount) {
                    currentTrackIndex = 0;
                }
                activeTrackIdx = trackIndices[currentTrackIndex];
                trackStartClock[activeTrackIdx] = globalClockCount;
            }
            
            return tracks[activeTrackIdx].trigPulse.process(sampleTime) ? 10.0f : 0.0f;
        }
    };
    ChainedSequence chain12, chain23, chain123;

    EuclideanRhythm() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        configInput(GLOBAL_CLOCK_INPUT, "Global Clock");
        configInput(GLOBAL_RESET_INPUT, "Global Reset");
        configParam(MANUAL_RESET_PARAM, 0.0f, 1.0f, 0.0f, "Manual Reset");

        int chain12_indices[] = {0, 1};
        chain12.setTrackIndices(chain12_indices, 2);
        int chain23_indices[] = {1, 2};
        chain23.setTrackIndices(chain23_indices, 2);
        int chain123_indices[] = {0, 1, 0, 2};
        chain123.setTrackIndices(chain123_indices, 4);

        for (int i = 0; i < 3; ++i) {
            int paramBase = TRACK1_DIVMULT_PARAM + i * 7;
            int inputBase = TRACK1_LENGTH_CV_INPUT + i * 3;
            
            configParam(paramBase, -3.0f, 3.0f, 0.0f, string::f("T%d Div/Mult", i+1));
            getParamQuantity(paramBase)->snapEnabled = true;
            delete paramQuantities[paramBase];
            paramQuantities[paramBase] = new DivMultParamQuantity;
            paramQuantities[paramBase]->module = this;
            paramQuantities[paramBase]->paramId = paramBase;
            paramQuantities[paramBase]->minValue = -3.0f;
            paramQuantities[paramBase]->maxValue = 3.0f;
            paramQuantities[paramBase]->defaultValue = 0.0f;
            paramQuantities[paramBase]->name = string::f("T%d Div/Mult", i+1);
            paramQuantities[paramBase]->snapEnabled = true;
            
            configParam(paramBase + 1, 1.0f, 32.0f, 16.0f, string::f("T%d Length", i+1));
            getParamQuantity(paramBase + 1)->snapEnabled = true;
            configParam(paramBase + 2, 0.0f, 100.0f, 25.0f, string::f("T%d Fill", i+1), "%");
            configParam(paramBase + 3, 0.0f, 31.0f, 0.0f, string::f("T%d Shift", i+1));
            getParamQuantity(paramBase + 3)->snapEnabled = true;
            configParam(paramBase + 4, -1.0f, 1.0f, 0.0f, string::f("T%d Length CV", i+1));
            configParam(paramBase + 5, -1.0f, 1.0f, 0.0f, string::f("T%d Fill CV", i+1));
            configParam(paramBase + 6, -1.0f, 1.0f, 0.0f, string::f("T%d Shift CV", i+1));
            
            configInput(inputBase, string::f("T%d Length CV", i+1));
            configInput(inputBase + 1, string::f("T%d Fill CV", i+1));
            configInput(inputBase + 2, string::f("T%d Shift CV", i+1));
            configOutput(TRACK1_TRIG_OUTPUT + i, string::f("T%d Trigger", i+1));
            configLight(TRACK1_LIGHT + i, string::f("T%d Light", i+1));
        }
        
        configOutput(MASTER_TRIG_OUTPUT, "Master Trigger Sum");
        configOutput(CHAIN_12_OUTPUT, "Chain 1+2");
        configOutput(CHAIN_23_OUTPUT, "Chain 2+3");
        configOutput(CHAIN_123_OUTPUT, "Chain 1+2+3");
        
        configLight(OR_RED_LIGHT, "OR Red Light");
        configLight(OR_GREEN_LIGHT, "OR Green Light");
        configLight(OR_BLUE_LIGHT, "OR Blue Light");
    }

    void onReset() override {
        secondsSinceLastClock = -1.0f;
        globalClockSeconds = 0.5f;
        for (int i = 0; i < 3; ++i) {
            tracks[i].reset();
        }
        chain12.reset();
        chain23.reset();
        chain123.reset();
    }

    void process(const ProcessArgs& args) override {
        bool globalClockActive = inputs[GLOBAL_CLOCK_INPUT].isConnected();
        bool globalClockTriggered = false;
        bool globalResetTriggered = false;
        bool manualResetTriggered = false;
        
        if (globalClockActive) {
            float clockVoltage = inputs[GLOBAL_CLOCK_INPUT].getVoltage();
            globalClockTriggered = clockTrigger.process(clockVoltage);
        }
        
        if (inputs[GLOBAL_RESET_INPUT].isConnected()) {
            globalResetTriggered = resetTrigger.process(inputs[GLOBAL_RESET_INPUT].getVoltage());
        }
        
        manualResetTriggered = manualResetTrigger.process(params[MANUAL_RESET_PARAM].getValue());
        
        if (globalResetTriggered || manualResetTriggered) {
            onReset();
            return;
        }
        
        if (globalClockTriggered) {
            if (secondsSinceLastClock > 0.0f) {
                globalClockSeconds = secondsSinceLastClock;
                globalClockSeconds = clamp(globalClockSeconds, 0.01f, 10.0f);
            }
            secondsSinceLastClock = 0.0f;
        }
        
        if (secondsSinceLastClock >= 0.0f) {
            secondsSinceLastClock += args.sampleTime;
        }

        for (int i = 0; i < 3; ++i) {
            TrackState& track = tracks[i];
            
            int divMultParam = (int)std::round(params[TRACK1_DIVMULT_PARAM + i * 7].getValue());
            track.updateDivMult(divMultParam);

            float lengthParam = params[TRACK1_LENGTH_PARAM + i * 7].getValue();
            float lengthCV = 0.0f;
            if (inputs[TRACK1_LENGTH_CV_INPUT + i * 3].isConnected()) {
                float lengthCVAtten = params[TRACK1_LENGTH_CV_ATTEN_PARAM + i * 7].getValue();
                lengthCV = inputs[TRACK1_LENGTH_CV_INPUT + i * 3].getVoltage() * lengthCVAtten;
            }
            track.length = (int)std::round(clamp(lengthParam + lengthCV, 1.0f, 32.0f));

            float fillParam = params[TRACK1_FILL_PARAM + i * 7].getValue();
            float fillCV = 0.0f;
            if (inputs[TRACK1_FILL_CV_INPUT + i * 3].isConnected()) {
                float fillCVAtten = params[TRACK1_FILL_CV_ATTEN_PARAM + i * 7].getValue();
                fillCV = inputs[TRACK1_FILL_CV_INPUT + i * 3].getVoltage() * fillCVAtten * 10.0f;
            }
            float fillPercentage = clamp(fillParam + fillCV, 0.0f, 100.0f);
            track.fill = (int)std::round((fillPercentage / 100.0f) * track.length);

            float shiftParam = params[TRACK1_SHIFT_PARAM + i * 7].getValue();
            float shiftCV = 0.0f;
            if (inputs[TRACK1_SHIFT_CV_INPUT + i * 3].isConnected()) {
                float shiftCVAtten = params[TRACK1_SHIFT_CV_ATTEN_PARAM + i * 7].getValue();
                shiftCV = inputs[TRACK1_SHIFT_CV_INPUT + i * 3].getVoltage() * shiftCVAtten;
            }
            track.shift = (int)std::round(clamp(shiftParam + shiftCV, 0.0f, (float)track.length - 1.0f));

            generateEuclideanRhythm(track.pattern, track.length, track.fill, track.shift);

            bool trackClockTrigger = track.processClockDivMult(globalClockTriggered, globalClockSeconds, args.sampleTime);

            if (trackClockTrigger && globalClockActive) {
                track.stepTrack();
            }
            
            float trigOutput = track.trigPulse.process(args.sampleTime) ? 10.0f : 0.0f;
            outputs[TRACK1_TRIG_OUTPUT + i].setVoltage(trigOutput);
            
            lights[TRACK1_LIGHT + i].setBrightness(track.gateState ? 1.0f : 0.0f);
        }
        
        float masterTrigSum = 0.0f;
        for (int i = 0; i < 3; ++i) {
            if (outputs[TRACK1_TRIG_OUTPUT + i].getVoltage() > 0.0f) {
                masterTrigSum = 10.0f;
                break;
            }
        }
        outputs[MASTER_TRIG_OUTPUT].setVoltage(masterTrigSum);
        
        bool track1Active = outputs[TRACK1_TRIG_OUTPUT].getVoltage() > 0.0f;
        bool track2Active = outputs[TRACK2_TRIG_OUTPUT].getVoltage() > 0.0f;
        bool track3Active = outputs[TRACK3_TRIG_OUTPUT].getVoltage() > 0.0f;
        
        if (track1Active) {
            orRedPulse.trigger(0.03f);
        }
        if (track2Active) {
            orGreenPulse.trigger(0.03f);
        }
        if (track3Active) {
            orBluePulse.trigger(0.03f);
        }
        
        lights[OR_RED_LIGHT].setBrightness(orRedPulse.process(args.sampleTime) ? 1.0f : 0.0f);
        lights[OR_GREEN_LIGHT].setBrightness(orGreenPulse.process(args.sampleTime) ? 1.0f : 0.0f);
        lights[OR_BLUE_LIGHT].setBrightness(orBluePulse.process(args.sampleTime) ? 1.0f : 0.0f);
        
        if (globalClockActive) {
            float chain12Output = chain12.processStep(tracks, args.sampleTime, globalClockTriggered);
            outputs[CHAIN_12_OUTPUT].setVoltage(chain12Output);
            
            float chain23Output = chain23.processStep(tracks, args.sampleTime, globalClockTriggered);
            outputs[CHAIN_23_OUTPUT].setVoltage(chain23Output);
            
            float chain123Output = chain123.processStep(tracks, args.sampleTime, globalClockTriggered);
            outputs[CHAIN_123_OUTPUT].setVoltage(chain123Output);
            
            lights[CHAIN_12_T1_LIGHT].setBrightness(chain12.currentTrackIndex == 0 ? 1.0f : 0.0f);
            lights[CHAIN_12_T2_LIGHT].setBrightness(chain12.currentTrackIndex == 1 ? 1.0f : 0.0f);
            
            lights[CHAIN_23_T2_LIGHT].setBrightness(chain23.currentTrackIndex == 0 ? 1.0f : 0.0f);
            lights[CHAIN_23_T3_LIGHT].setBrightness(chain23.currentTrackIndex == 1 ? 1.0f : 0.0f);
            
            int activeTrack123 = chain123.trackIndices[chain123.currentTrackIndex];
            lights[CHAIN_123_T1_LIGHT].setBrightness(activeTrack123 == 0 ? 1.0f : 0.0f);
            lights[CHAIN_123_T2_LIGHT].setBrightness(activeTrack123 == 1 ? 1.0f : 0.0f);
            lights[CHAIN_123_T3_LIGHT].setBrightness(activeTrack123 == 2 ? 1.0f : 0.0f);
        }
    }
};

struct EuclideanRhythmWidget : ModuleWidget {
    EuclideanRhythmWidget(EuclideanRhythm* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "EuclideanRhythm.png")));
        
        box.size = Vec(8 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

        addInput(createInputCentered<PJ301MPort>(Vec(33, 56), module, EuclideanRhythm::GLOBAL_CLOCK_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(77, 56), module, EuclideanRhythm::GLOBAL_RESET_INPUT));
        addParam(createParamCentered<VCVButton>(Vec(100, 56), module, EuclideanRhythm::MANUAL_RESET_PARAM));

        float trackY[3] = {77, 159, 241};
        
        for (int i = 0; i < 3; ++i) {
            float y = trackY[i];
            float x = 1;

            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(x + 12, y + 22), module, EuclideanRhythm::TRACK1_LENGTH_PARAM + i * 7));
            addInput(createInputCentered<PJ301MPort>(Vec(x + 12, y + 47), module, EuclideanRhythm::TRACK1_LENGTH_CV_INPUT + i * 3));
            addParam(createParamCentered<Trimpot>(Vec(x + 12, y + 69), module, EuclideanRhythm::TRACK1_LENGTH_CV_ATTEN_PARAM + i * 7));
            x += 31;

            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(x + 12, y + 22), module, EuclideanRhythm::TRACK1_FILL_PARAM + i * 7));
            addInput(createInputCentered<PJ301MPort>(Vec(x + 12, y + 47), module, EuclideanRhythm::TRACK1_FILL_CV_INPUT + i * 3));
            addParam(createParamCentered<Trimpot>(Vec(x + 12, y + 69), module, EuclideanRhythm::TRACK1_FILL_CV_ATTEN_PARAM + i * 7));
            x += 31;

            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(x + 12, y + 22), module, EuclideanRhythm::TRACK1_SHIFT_PARAM + i * 7));
            addInput(createInputCentered<PJ301MPort>(Vec(x + 12, y + 47), module, EuclideanRhythm::TRACK1_SHIFT_CV_INPUT + i * 3));
            addParam(createParamCentered<Trimpot>(Vec(x + 12, y + 69), module, EuclideanRhythm::TRACK1_SHIFT_CV_ATTEN_PARAM + i * 7));
            x += 30;

            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(x + 12, y + 22), module, EuclideanRhythm::TRACK1_DIVMULT_PARAM + i * 7));
        }
        
        for (int i = 0; i < 3; ++i) {
            float y = trackY[i];
            float outputX = 106;
            float outputY = y + 69;
            
            addOutput(createOutputCentered<PJ301MPort>(Vec(outputX, outputY), module, EuclideanRhythm::TRACK1_TRIG_OUTPUT + i));
        }
        
        float chainOutputY = 358;
        float chainPositions[3] = {13, 44, 75};
        
        addOutput(createOutputCentered<PJ301MPort>(Vec(chainPositions[0], chainOutputY), module, EuclideanRhythm::CHAIN_12_OUTPUT));
        addChild(createLightCentered<SmallLight<RedLight>>(Vec(chainPositions[0] - 8, chainOutputY + 17), module, EuclideanRhythm::CHAIN_12_T1_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(chainPositions[0] + 8, chainOutputY + 17), module, EuclideanRhythm::CHAIN_12_T2_LIGHT));
        
        addOutput(createOutputCentered<PJ301MPort>(Vec(chainPositions[1], chainOutputY), module, EuclideanRhythm::CHAIN_23_OUTPUT));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(chainPositions[1] - 8, chainOutputY + 17), module, EuclideanRhythm::CHAIN_23_T2_LIGHT));
        addChild(createLightCentered<SmallLight<BlueLight>>(Vec(chainPositions[1] + 8, chainOutputY + 17), module, EuclideanRhythm::CHAIN_23_T3_LIGHT));
        
        addOutput(createOutputCentered<PJ301MPort>(Vec(chainPositions[2], chainOutputY), module, EuclideanRhythm::CHAIN_123_OUTPUT));
        addChild(createLightCentered<SmallLight<RedLight>>(Vec(chainPositions[2] - 10, chainOutputY + 17), module, EuclideanRhythm::CHAIN_123_T1_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(chainPositions[2], chainOutputY + 17), module, EuclideanRhythm::CHAIN_123_T2_LIGHT));
        addChild(createLightCentered<SmallLight<BlueLight>>(Vec(chainPositions[2] + 10, chainOutputY + 17), module, EuclideanRhythm::CHAIN_123_T3_LIGHT));
        
        float outputY = 358;
        float outputSpacing = 31;
        float startX = 13;
        
        float mixX = startX + 3 * outputSpacing - 2;
        addOutput(createOutputCentered<PJ301MPort>(Vec(mixX, outputY), module, EuclideanRhythm::MASTER_TRIG_OUTPUT));
        addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(Vec(mixX + 8, outputY + 17), module, EuclideanRhythm::OR_RED_LIGHT));
    }
};

Model* modelEuclideanRhythm = createModel<EuclideanRhythm, EuclideanRhythmWidget>("EuclideanRhythm");