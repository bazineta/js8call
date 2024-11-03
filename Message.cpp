/**
 * This file is part of JS8Call.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * (C) 2018 Jordan Sherer <kn4crd@gmail.com> - All Rights Reserved
 *
 **/

#include "Message.hpp"
#include <QJsonDocument>
#include "DriftingDateTime.h"

/******************************************************************************/
// Constants
/******************************************************************************/

namespace
{
  constexpr qint64 EPOCH = 1499299200000; // July 6, 2017
}

/******************************************************************************/
// Local Utilities
/******************************************************************************/

namespace
{
  auto
  generateId()
  {
    return QString::number(DriftingDateTime::currentMSecsSinceEpoch() - EPOCH);
  }

#if USE_SNOWFLAKE
  quint64
  snowflake(quint64 const epoch,
            quint16 const machine,
            quint16 const sequence)
  {
    quint64 value = (DriftingDateTime::currentMSecsSinceEpoch() - epoch) << 22;
    value |= machine  & 0x3FF << 12;
    value |= sequence & 0xFFF;
    return value;
  }
#endif
}

/******************************************************************************/
// Message::Data Implementation
/******************************************************************************/

struct Message::Data final : public QSharedData
{
  // Return the ID value from the variant map, if there is one, or zero
  // if there's no ID in the map.

  qint64
  id() const
  {
    return params_.value("_ID").toLongLong(); 
  }

  // If there's a non-zero ID in the variant map, return it, otherwise
  // generate one, insert it, and return it.

  qint64
  ensureId()
  {
    if (auto const it  = params_.find("_ID");
                   it != params_.end())
    {
      if (auto const id = it->toLongLong()) return id;
    }

    return params_.insert("_ID", generateId())->toLongLong();
  }

  // Data members

  QString     type_;
  QString     value_;
  QVariantMap params_;
};

/******************************************************************************/
// Constructors
/******************************************************************************/

Message::Message()
: d_ {new Message::Data}
{}

Message::Message(QString const & type,
                 QString const & value)
: Message()
{
  setType (type);
  setValue(value);
  ensureId();
}

Message::Message(QString     const & type,
                 QString     const & value,
                 QVariantMap const & params)
: Message(type, value)
{
  d_->params_ = params;
  ensureId();
}

/******************************************************************************/
// Copying and Destruction
/******************************************************************************/

Message &
Message::operator=(Message const &) = default;
Message::Message  (Message const &) = default;
Message::~Message()                 = default;

/******************************************************************************/
// Accessors
/******************************************************************************/

qint64
Message::id() const
{
  return d_->id();
}

QString
Message::type() const
{
  return d_->type_;
}

QString
Message::value() const
{
  return d_->value_;
}

QVariantMap
Message::params() const
{
  return d_->params_;
}

/******************************************************************************/
// Manipulators
/******************************************************************************/

qint64
Message::ensureId()
{
  return d_->ensureId();
}

void
Message::setType(QString const & type)
{
  d_->type_ = type;
}

void
Message::setValue(QString const & value)
{
  d_->value_ = value;
}

/******************************************************************************/
// Serialization
/******************************************************************************/

void
Message::read(QJsonObject const & json)
{
  if (auto const it  = json.find("type");
                 it != json.end() && it->isString())
  {
    d_->type_ = it->toString();
  }

  if (auto const it = json.find("value");
                 it != json.end() && it->isString())
  {
    d_->value_ = it->toString();
  }

  if (auto const it  = json.find("params");
                 it != json.end() && it->isObject())
  {
    d_->params_ = it->toObject().toVariantMap();
  }
}

void
Message::write(QJsonObject & json) const
{
  json["type"]   = d_->type_;
  json["value"]  = d_->value_;
  json["params"] = QJsonObject::fromVariantMap(d_->params_);
}

/******************************************************************************/
// Conversions
/******************************************************************************/

QByteArray
Message::toJson() const
{
  QJsonObject object;

  write(object);

  return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QVariantMap
Message::toVariantMap() const
{
  return {
    { "type",   d_->type_   },
    { "value",  d_->value_  },
    { "params", d_->params_ }
  };
}

/******************************************************************************/
