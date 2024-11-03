#ifndef MESSAGE_HPP
#define MESSAGE_HPP

/**
 * (C) 2018 Jordan Sherer <kn4crd@gmail.com> - All Rights Reserved
 **/

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QSharedDataPointer>
#include <QString>
#include <QVariant>

class Message final
{
public:

    // Constructors

    Message();
    Message(QString const & type, QString const & value="");
    Message(QString const & type, QString const & value, QVariantMap const & params);

    // Assignment and copy construction

    Message & operator = (Message const &);
    Message              (Message const &);

    // Destructor

    ~Message();

    // Accessors

    qint64      id()     const;
    QString     type()   const;
    QString     value()  const;
    QVariantMap params() const;

    // Manipulators

    qint64 ensureId();
    void   setType (QString const &);
    void   setValue(QString const &);

    // Serialization
    
    void read (QJsonObject const &);
    void write(QJsonObject       &) const;

    // Conversions

    QByteArray  toJson()       const;
    QVariantMap toVariantMap() const;

private:

    // Shared data implementation

    struct Data;
    QSharedDataPointer<Data> d_;
};

Q_DECLARE_TYPEINFO(Message, Q_MOVABLE_TYPE);

#endif // MESSAGE_HPP
