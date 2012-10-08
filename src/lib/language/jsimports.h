#ifndef JSIMPORTS_H
#define JSIMPORTS_H

#include <tools/codelocation.h>
#include <QStringList>

namespace qbs {

/**
  * Represents JavaScript import of the form
  *    import 'fileOrDirectory' as scopeName
  *
  * There can be several filenames per scope
  * if we import a whole directory.
  */
class JsImport
{
public:
    QString scopeName;
    QStringList fileNames;
    CodeLocation location;
};

typedef QList<JsImport> JsImports;

} // namespace qbs

#endif // JSIMPORTS_H
