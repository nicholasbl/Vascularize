TARGET = vascularize
TEMPLATE = app

CONFIG += c++2a

CONFIG -= app_bundle qt

QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15

LIBS += -L/usr/local/lib -lopenvdb -ltbb -lHalf

INCLUDEPATH += $$PWD/third_party/fmt/


QMAKE_CXXFLAGS_DEBUG += -fsanitize=address
QMAKE_LFLAGS_DEBUG += -fsanitize=address

macx {
    INCLUDEPATH += /usr/local/include
} else {
    # GCC specific configuration
}

HEADERS += \
    boundingbox.h \
    generate_vessels.h \
    glm_include.h \
    global.h \
    jobcontroller.h \
    mesh_write.h \
    mutable_mesh.h \
    simplegraph.h \
    third_party/fmt/fmt/chrono.h \
    third_party/fmt/fmt/color.h \
    third_party/fmt/fmt/compile.h \
    third_party/fmt/fmt/core.h \
    third_party/fmt/fmt/format-inl.h \
    third_party/fmt/fmt/format.h \
    third_party/fmt/fmt/locale.h \
    third_party/fmt/fmt/ostream.h \
    third_party/fmt/fmt/posix.h \
    third_party/fmt/fmt/printf.h \
    third_party/fmt/fmt/ranges.h \
    third_party/fmt/fmt/safe-duration-cast.h \
    voxelmesh.h \
    wavefrontimport.h \
    xrange.h

SOURCES += \
    boundingbox.cpp \
    generate_vessels.cpp \
    global.cpp \
    jobcontroller.cpp \
    main.cpp \
    mesh_write.cpp \
    mutable_mesh.cpp \
    simplegraph.cpp \
    third_party/fmt/src/format.cc \
    third_party/fmt/src/posix.cc \
    voxelmesh.cpp \
    wavefrontimport.cpp
