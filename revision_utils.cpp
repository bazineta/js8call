#include "revision_utils.hpp"

#include <cstring>

#include <QCoreApplication>
#include <QRegularExpression>

QString revision (QString const&)
{
  return "";
}

QString version (bool include_patch)
{
#if defined (CMAKE_BUILD)
  QString v {WSJTX_STRINGIZE (WSJTX_VERSION_MAJOR) "." WSJTX_STRINGIZE (WSJTX_VERSION_MINOR)};
  if (include_patch)
    {
      v += "." WSJTX_STRINGIZE (WSJTX_VERSION_PATCH)
#if 0
# if defined (WSJTX_RC)
        + "-rc" WSJTX_STRINGIZE (WSJTX_RC)
# endif
#endif
        ;
    }
#else
  QString v {"Not for Release"};
#endif

  return v;
}

QString
program_title(QString const&)
{
  return QString {"%1 de KN4CRD (v%2)"}
                 .arg(QCoreApplication::applicationName())
                 .arg(QCoreApplication::applicationVersion());
}
