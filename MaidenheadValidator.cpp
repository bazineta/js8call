#include "MaidenheadValidator.hpp"
#include <QRegularExpression>

namespace
{
  // Consolidated patterns without groups
  constexpr QStringView patternAR  = u"[A-R]{2}";
  constexpr QStringView pattern09  = u"[0-9]{2}";
  constexpr QStringView patternAX  = u"[A-X]{2}";
  constexpr QStringView patterns[] =
  {
    patternAR, // Field part
    pattern09, // Square part
    patternAX, // Subsquare part
    pattern09, // Extended square part
    patternAX, // Ultra part
    pattern09  // Hyper part
  };

auto
buildRegex(int const mandatoryFields,
           int const maxFields)
{
  QString regex = QString("^");

  // Limit the number of fields to the maximum allowed
  auto const fields = std::min(maxFields, static_cast<int>(std::size(patterns)));

  // Add mandatory fields
  for (int i = 0; i < mandatoryFields && i < fields; ++i) {
      regex += patterns[i];  // Mandatory fields must always be present
  }

  // Add optional fields with sequential dependency
  for (int i = mandatoryFields; i < fields; ++i) {
      regex += QString("(?:");          // Open non-capturing group
      for (int j = 0; j <= i; ++j) {   // Add all fields up to the current one
          regex += patterns[j];
      }
      regex += QString(")?");          // Close non-capturing group
  }

  regex += QString("$");
  return regex;
}

} // namespace

MaidenheadValidator::MaidenheadValidator(int      mandatoryFields,
                                         int      maxFields,
                                         QObject* parent)
: QRegularExpressionValidator(QRegularExpression(buildRegex(mandatoryFields, maxFields),
                              QRegularExpression::CaseInsensitiveOption),
                              parent)
{}