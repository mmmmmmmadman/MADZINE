# MADZINE VCV Rack Plugin

ifdef CROSS_COMPILE
ifneq (,$(findstring mingw32,$(CROSS_COMPILE)))
    WIN_CROSS_COMPILE := 1
    RACK_DIR ?= ../Rack-SDK-win-x64
endif
endif

ifeq ($(ARCH), x64)
    RACK_DIR ?= ../Rack-SDK-x64
    FLAGS += -arch x86_64
    CFLAGS += -arch x86_64
    CXXFLAGS += -arch x86_64 -march=x86-64
    LDFLAGS += -arch x86_64
    override CFLAGS := $(filter-out -march=armv8-a+fp+simd,$(CFLAGS))
    override CXXFLAGS := $(filter-out -march=armv8-a+fp+simd,$(CXXFLAGS))
else ifndef WIN_CROSS_COMPILE
    RACK_DIR ?= ../Rack-SDK
endif

SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res $(wildcard LICENSE*) $(wildcard presets)

include $(RACK_DIR)/plugin.mk

ifdef WIN_CROSS_COMPILE
    override FLAGS := $(filter-out --target=%,$(FLAGS))
    override CFLAGS := $(filter-out --target=%,$(CFLAGS))
    override CXXFLAGS := $(filter-out --target=%,$(CXXFLAGS))
    override LDFLAGS := $(filter-out --target=%,$(LDFLAGS))
    FLAGS += -DWIN32
endif