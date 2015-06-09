include(../libexec.pri)

TARGET = qbs-javac-scan

PATHPREFIX = $$PWD/io/qt/qbs/

JAVACLASSPATH += $$PWD
JAVASOURCES += \
    $$PATHPREFIX/Artifact.java \
    $$PATHPREFIX/ArtifactListJsonWriter.java \
    $$PATHPREFIX/ArtifactListTextWriter.java \
    $$PATHPREFIX/ArtifactListWriter.java \
    $$PATHPREFIX/ArtifactListXmlWriter.java \
    $$PATHPREFIX/tools/JavaCompilerScannerTool.java \
    $$PATHPREFIX/tools/utils/JavaCompilerOptions.java \
    $$PATHPREFIX/tools/utils/JavaCompilerScanner.java \
    $$PATHPREFIX/tools/utils/NullFileObject.java

JAVAMAINCLASS = io.qt.qbs.tools.JavaCompilerScannerTool

# from mkspecs/features/java.prf
TEMPLATE = lib

CLASS_DIR = .classes
CLASS_DIR_MARKER = .classes.marker

CONFIG -= qt

# Without these, qmake adds a name prefix and versioning postfixes (as well as file
# links) to the target. This is hardcoded in the qmake code, so for now we use
# the plugin configs to get what we want.
CONFIG += plugin no_plugin_name_prefix

javac.input = JAVASOURCES
javac.output = $$CLASS_DIR_MARKER
javac.CONFIG += combine
javac.commands = javac -source 1.6 -target 1.6 -Xlint:unchecked -cp $$shell_quote($$system_path($$join(JAVACLASSPATH, $$DIRLIST_SEPARATOR))) -d $$shell_quote($$CLASS_DIR) ${QMAKE_FILE_IN} $$escape_expand(\\n\\t) \
    @echo Nothing to see here. Move along. > $$CLASS_DIR_MARKER
QMAKE_EXTRA_COMPILERS += javac

mkpath($$absolute_path($$CLASS_DIR, $$OUT_PWD)) | error("Aborting.")

# Disable all linker flags since we are overriding the regular linker
QMAKE_LFLAGS =
QMAKE_CFLAGS =
QMAKE_LFLAGS_CONSOLE =
QMAKE_LFLAGS_WINDOWS =
QMAKE_LFLAGS_DLL =
QMAKE_LFLAGS_DEBUG =
QMAKE_LFLAGS_RELEASE =
QMAKE_LFLAGS_RPATH =
QMAKE_LFLAGS_PLUGIN =
QMAKE_LIBS =
QMAKE_LIBS_OPENGL_ES2 =
QMAKE_LIBDIR =
QMAKE_EXTENSION_SHLIB = jar

# nmake
QMAKE_LINK = jar
QMAKE_LFLAGS = cfe $(DESTDIR_TARGET) $$JAVAMAINCLASS -C $$CLASS_DIR .

# Demonstrate an excellent reason why qbs exists
QMAKE_LINK_O_FLAG = && "echo "

# make
QMAKE_LINK_SHLIB_CMD = jar cfe $(TARGET) $$JAVAMAINCLASS -C $$CLASS_DIR .

# Force link step to always happen, since we are always updating the
# .class files
PRE_TARGETDEPS += FORCE
