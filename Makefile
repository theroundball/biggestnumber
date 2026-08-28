# Butano build configuration for the SIPS card test.
TARGET          := sips
BUILD           := build
LIBBUTANO       := ../../butano
PYTHON          := python
SOURCES     	:=  src ../../common/src
INCLUDES        :=  include ../../common/include
DATA            :=
GRAPHICS    	:=  graphics ../../common/graphics
AUDIO           :=
AUDIOBACKEND    := null
AUDIOTOOL       :=
DMGAUDIO        :=
DMGAUDIOBACKEND := null
ROMTITLE        := SIPS
ROMCODE         := SIPS
USERFLAGS       := -DBN_CFG_SPRITES_MAX_ITEMS=384
USERCXXFLAGS    :=
USERASFLAGS     :=
USERLDFLAGS     :=
USERLIBDIRS     :=
USERLIBS        :=
DEFAULTLIBS    :=
STACKTRACE      :=
USERBUILD       :=
EXTTOOL         :=

ifndef LIBBUTANOABS
export LIBBUTANOABS := $(realpath $(LIBBUTANO))
endif

include $(LIBBUTANOABS)/butano.mak
