#ifndef MAIDENHEAD_VALIDATOR_HPP__
#define MAIDENHEAD_VALIDATOR_HPP__

#include <QRegularExpressionValidator>

//
// MaidenheadValidator - QValidator implementation for grid locators
//
class MaidenheadValidator final : public QRegularExpressionValidator
{
public:

  MaidenheadValidator(int      mandatoryFields,
                      int      maxFields,
                      QObject* parent = nullptr);
};

#endif
