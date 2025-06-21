#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
    pluginInstance = p;

    p->addModel(modelSwingLFO);
    p->addModel(modelEuclideanRhythm);
    p->addModel(modelADGenerator);
}