TARGET = vascularize
TEMPLATE = app

CONFIG += c++2a

CONFIG -= app_bundle qt

QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15

LIBS += -L/usr/local/lib -lfmt -lopenvdb -ltbb -lHalf


macx {
    INCLUDEPATH += /usr/local/include
    
    QMAKE_CXXFLAGS_DEBUG += -fsanitize=address
    QMAKE_LFLAGS_DEBUG += -fsanitize=address

} else {
    # These are heavy on GCC
    #QMAKE_CXXFLAGS += -fsanitize=address
    #QMAKE_LFLAGS += -fsanitize=address
}

HEADERS += \
    boundingbox.h \
    generate_vessels.h \
    glm_include.h \
    global.h \
    grid.h \
    jobcontroller.h \
    mesh_write.h \
    mutable_mesh.h \
    simplegraph.h \
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
    voxelmesh.cpp \
    wavefrontimport.cpp
