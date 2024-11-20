#ifndef MAIDENHEAD_VALIDATOR_HPP__
#define MAIDENHEAD_VALIDATOR_HPP__

#include <QRegularExpressionValidator>

class MaidenheadValidator final : public QRegularExpressionValidator
{
public:

  MaidenheadValidator(int      mandatoryFields,
                      int      maxFields,
                      QObject* parent = nullptr);

  State validate(QString & input, int & pos) const override;
};

#endif
