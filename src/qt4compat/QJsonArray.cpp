#include "QJsonArray"

#include "QJsonObject"

namespace {

QVariant normalizeJsonVariant(const QVariant& value) {
    if (value.type() == QVariant::Map) {
        return QJsonObject::fromVariantMap(value.toMap()).toVariantMap();
    }
    if (value.type() == QVariant::List || value.type() == QVariant::StringList) {
        return QJsonArray::fromVariantList(value.toList()).toVariantList();
    }
    return value;
}

} // namespace

QJsonValue::QJsonValue(const QVariant& value) : m_value(normalizeJsonVariant(value)) {}
QJsonValue::QJsonValue(const QJsonArray& value) : m_value(value.toVariantList()) {}
QJsonValue::QJsonValue(const QJsonObject& value) : m_value(value.toVariantMap()) {}

bool QJsonValue::isObject() const {
    return m_value.type() == QVariant::Map;
}

bool QJsonValue::isArray() const {
    return m_value.type() == QVariant::List;
}

QString QJsonValue::toString(const QString& defaultValue) const {
    return m_value.isValid() ? m_value.toString() : defaultValue;
}

int QJsonValue::toInt(int defaultValue) const {
    return m_value.isValid() ? m_value.toInt() : defaultValue;
}

bool QJsonValue::toBool(bool defaultValue) const {
    return m_value.isValid() ? m_value.toBool() : defaultValue;
}

QJsonObject QJsonValue::toObject() const {
    return QJsonObject::fromVariantMap(m_value.toMap());
}

QJsonArray QJsonValue::toArray() const {
    return QJsonArray::fromVariantList(m_value.toList());
}

QVariantList QJsonArray::toVariantList() const {
    QVariantList result;
    for (QVector<QJsonValue>::const_iterator it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
        result.append(it->toVariant());
    }
    return result;
}

QJsonArray QJsonArray::fromVariantList(const QVariantList& values) {
    QJsonArray result;
    for (QVariantList::const_iterator it = values.constBegin(); it != values.constEnd(); ++it) {
        result.append(QJsonValue(*it));
    }
    return result;
}

QVariantMap QJsonObject::toVariantMap() const {
    QVariantMap result;
    for (QMap<QString, QJsonValue>::const_iterator it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
        result.insert(it.key(), it.value().toVariant());
    }
    return result;
}

QJsonObject QJsonObject::fromVariantMap(const QVariantMap& values) {
    QJsonObject result;
    for (QVariantMap::const_iterator it = values.constBegin(); it != values.constEnd(); ++it) {
        result[it.key()] = QJsonValue(it.value());
    }
    return result;
}
