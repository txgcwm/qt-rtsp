QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

TARGET = Kamera
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES +=  main.cpp \
            DummySink.cpp \
            StreamState.cpp \
            AppWindow.cpp \
            RtspClient.cpp \
            RtspCallback.cpp \
            RtspSession.cpp

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

unix {
#    INCLUDEPATH += /usr/local/include
#    INCLUDEPATH += /usr/local/include/liveMedia
#    INCLUDEPATH += /usr/local/include/groupsock
#    INCLUDEPATH += /usr/local/include/UsageEnvironment
#    INCLUDEPATH += /usr/local/include/BasicUsageEnvironment
#    LIBS += -L/usr/local/lib
#    LIBS += -lliveMedia -lgroupsock -lBasicUsageEnvironment -lUsageEnvironment
#    LIBS += -lavcodec -lavformat -lavutil -lavfilter -lavdevice -lswscale -lswresample
#    LIBS += -liconv -lx264 -lbz2 -lz -lm
#    LIBS += -framework AudioToolbox -framework CoreMedia -framework AVFoundation -framework VideoToolbox -framework CoreVideo -framework CoreFoundation
#    LIBS += -framework Security -framework VideoDecodeAcceleration

    INCLUDEPATH +=  $$PWD/prebuild/linux/live555/include/ \
                    $$PWD/prebuild/linux/live555/include/liveMedia \
                    $$PWD/prebuild/linux/live555/include/groupsock \
                    $$PWD/prebuild/linux/live555/include/UsageEnvironment \
                    $$PWD/prebuild/linux/live555/include/BasicUsageEnvironment

    INCLUDEPATH += $$PWD/prebuild/linux/ffmpeg/include/

    LIBS += -L$$PWD/prebuild/linux/ffmpeg/lib -lavformat -lavdevice -lavcodec -lavutil -lavfilter \
                                         -lswscale -lswresample

    LIBS += -L$$PWD/prebuild/linux/live555/lib -lliveMedia -lgroupsock \
                                         -lBasicUsageEnvironment -lUsageEnvironment

    LIBS += -lz
}

win32 {
    INCLUDEPATH +=  $$PWD/prebuild/win32/live555/include/ \
                    $$PWD/prebuild/win32/live555/include/liveMedia \
                    $$PWD/prebuild/win32/live555/include/groupsock \
                    $$PWD/prebuild/win32/live555/include/UsageEnvironment \
                    $$PWD/prebuild/win32/live555/include/BasicUsageEnvironment

    INCLUDEPATH += $$PWD/prebuild/win32/ffmpeg/include/

    LIBS += -L$$PWD/prebuild/win32/ffmpeg/lib -lavcodec -lavformat -lavutil -lavfilter \
                                        -lavdevice -lswscale -lswresample

    LIBS += -L$$PWD/prebuild/win32/live555/lib -lliveMedia -lgroupsock \
                                         -lBasicUsageEnvironment -lUsageEnvironment
}

HEADERS +=  StreamState.h \
            DummySink.h \
            AppWindow.h \
            RtspClient.h \
            RtspCallback.h \
            RtspSession.h
