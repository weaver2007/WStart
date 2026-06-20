#include "QJsonDocument"

#include "QJsonArray"

#include <QLocale>
#include <QStringList>
#include <QVariant>

namespace {

QString escapeJsonString(const QString& value) {
    QString result;
    result.reserve(value.size() + 2);
    result.append('"');
    for (int i = 0; i < value.size(); ++i) {
        const QChar ch = value.at(i);
        switch (ch.unicode()) {
        case '"':
            result.append("\\\"");
            break;
        case '\\':
            result.append("\\\\");
            break;
        case '\b':
            result.append("\\b");
            break;
        case '\f':
            result.append("\\f");
            break;
        case '\n':
            result.append("\\n");
            break;
        case '\r':
            result.append("\\r");
            break;
        case '\t':
            result.append("\\t");
            break;
        default:
            if (ch.unicode() < 0x20) {
                result.append(QString("\\u%1").arg(ch.unicode(), 4, 16, QLatin1Char('0')));
            } else {
                result.append(ch);
            }
            break;
        }
    }
    result.append('"');
    return result;
}

QString indentString(int depth) {
    return QString(depth * 2, QLatin1Char(' '));
}

QString serializeVariant(const QVariant& value, bool indented, int depth);

QString serializeList(const QVariantList& list, bool indented, int depth) {
    if (list.isEmpty()) {
        return "[]";
    }

    QStringList parts;
    for (QVariantList::const_iterator it = list.constBegin(); it != list.constEnd(); ++it) {
        const QString item = serializeVariant(*it, indented, depth + 1);
        parts << (indented ? indentString(depth + 1) + item : item);
    }

    if (!indented) {
        return QString("[%1]").arg(parts.join(","));
    }
    return QString("[\n%1\n%2]").arg(parts.join(",\n"), indentString(depth));
}

QString serializeMap(const QVariantMap& map, bool indented, int depth) {
    if (map.isEmpty()) {
        return "{}";
    }

    QStringList parts;
    for (QVariantMap::const_iterator it = map.constBegin(); it != map.constEnd(); ++it) {
        const QString entry = QString("%1%2%3").arg(escapeJsonString(it.key()), indented ? ": " : ":",
                                                    serializeVariant(it.value(), indented, depth + 1));
        parts << (indented ? indentString(depth + 1) + entry : entry);
    }

    if (!indented) {
        return QString("{%1}").arg(parts.join(","));
    }
    return QString("{\n%1\n%2}").arg(parts.join(",\n"), indentString(depth));
}

QString serializeVariant(const QVariant& value, bool indented, int depth) {
    switch (value.type()) {
    case QVariant::Map:
        return serializeMap(value.toMap(), indented, depth);
    case QVariant::List:
    case QVariant::StringList:
        return serializeList(value.toList(), indented, depth);
    case QVariant::Bool:
        return value.toBool() ? "true" : "false";
    case QVariant::Int:
    case QVariant::UInt:
    case QVariant::LongLong:
    case QVariant::ULongLong:
        return value.toString();
    case QVariant::Double:
        return QLocale::c().toString(value.toDouble(), 'g', 15);
    case QVariant::Invalid:
        return "null";
    default:
        return escapeJsonString(value.toString());
    }
}

int hexValue(QChar ch) {
    if (ch >= QLatin1Char('0') && ch <= QLatin1Char('9')) {
        return ch.unicode() - '0';
    }
    if (ch >= QLatin1Char('a') && ch <= QLatin1Char('f')) {
        return ch.unicode() - 'a' + 10;
    }
    if (ch >= QLatin1Char('A') && ch <= QLatin1Char('F')) {
        return ch.unicode() - 'A' + 10;
    }
    return -1;
}

class JsonParser {
public:
    explicit JsonParser(const QByteArray& json) : m_text(QString::fromUtf8(json)) {}

    bool parse(QVariant* value) {
        skipWhitespace();
        if (!parseValue(value)) {
            return false;
        }
        skipWhitespace();
        return m_pos == m_text.size();
    }

private:
    void skipWhitespace() {
        while (m_pos < m_text.size() && m_text.at(m_pos).isSpace()) {
            ++m_pos;
        }
    }

    bool consume(QChar expected) {
        skipWhitespace();
        if (m_pos >= m_text.size() || m_text.at(m_pos) != expected) {
            return false;
        }
        ++m_pos;
        return true;
    }

    bool consumeLiteral(const QString& literal) {
        skipWhitespace();
        if (m_text.mid(m_pos, literal.size()) != literal) {
            return false;
        }
        m_pos += literal.size();
        return true;
    }

    bool parseValue(QVariant* value) {
        skipWhitespace();
        if (m_pos >= m_text.size()) {
            return false;
        }

        const QChar ch = m_text.at(m_pos);
        if (ch == QLatin1Char('{')) {
            QVariantMap map;
            if (!parseObject(&map)) {
                return false;
            }
            *value = map;
            return true;
        }
        if (ch == QLatin1Char('[')) {
            QVariantList list;
            if (!parseArray(&list)) {
                return false;
            }
            *value = list;
            return true;
        }
        if (ch == QLatin1Char('"')) {
            QString stringValue;
            if (!parseString(&stringValue)) {
                return false;
            }
            *value = stringValue;
            return true;
        }
        if (ch == QLatin1Char('-') || ch.isDigit()) {
            return parseNumber(value);
        }
        if (consumeLiteral("true")) {
            *value = true;
            return true;
        }
        if (consumeLiteral("false")) {
            *value = false;
            return true;
        }
        if (consumeLiteral("null")) {
            *value = QVariant();
            return true;
        }
        return false;
    }

    bool parseObject(QVariantMap* map) {
        if (!consume(QLatin1Char('{'))) {
            return false;
        }
        skipWhitespace();
        if (consume(QLatin1Char('}'))) {
            return true;
        }

        while (m_pos < m_text.size()) {
            QString key;
            if (!parseString(&key) || !consume(QLatin1Char(':'))) {
                return false;
            }

            QVariant value;
            if (!parseValue(&value)) {
                return false;
            }
            map->insert(key, value);

            skipWhitespace();
            if (consume(QLatin1Char('}'))) {
                return true;
            }
            if (!consume(QLatin1Char(','))) {
                return false;
            }
        }
        return false;
    }

    bool parseArray(QVariantList* list) {
        if (!consume(QLatin1Char('['))) {
            return false;
        }
        skipWhitespace();
        if (consume(QLatin1Char(']'))) {
            return true;
        }

        while (m_pos < m_text.size()) {
            QVariant value;
            if (!parseValue(&value)) {
                return false;
            }
            list->append(value);

            skipWhitespace();
            if (consume(QLatin1Char(']'))) {
                return true;
            }
            if (!consume(QLatin1Char(','))) {
                return false;
            }
        }
        return false;
    }

    bool parseString(QString* value) {
        if (!consume(QLatin1Char('"'))) {
            return false;
        }

        QString result;
        while (m_pos < m_text.size()) {
            const QChar ch = m_text.at(m_pos++);
            if (ch == QLatin1Char('"')) {
                *value = result;
                return true;
            }
            if (ch != QLatin1Char('\\')) {
                result.append(ch);
                continue;
            }
            if (m_pos >= m_text.size()) {
                return false;
            }

            const QChar escaped = m_text.at(m_pos++);
            switch (escaped.unicode()) {
            case '"':
                result.append('"');
                break;
            case '\\':
                result.append('\\');
                break;
            case '/':
                result.append('/');
                break;
            case 'b':
                result.append('\b');
                break;
            case 'f':
                result.append('\f');
                break;
            case 'n':
                result.append('\n');
                break;
            case 'r':
                result.append('\r');
                break;
            case 't':
                result.append('\t');
                break;
            case 'u': {
                if (m_pos + 4 > m_text.size()) {
                    return false;
                }
                uint code = 0;
                for (int i = 0; i < 4; ++i) {
                    const int digit = hexValue(m_text.at(m_pos++));
                    if (digit < 0) {
                        return false;
                    }
                    code = (code << 4) | static_cast<uint>(digit);
                }
                result.append(QChar(static_cast<ushort>(code)));
                break;
            }
            default:
                return false;
            }
        }
        return false;
    }

    bool parseNumber(QVariant* value) {
        skipWhitespace();
        const int start = m_pos;
        if (m_pos < m_text.size() && m_text.at(m_pos) == QLatin1Char('-')) {
            ++m_pos;
        }
        if (m_pos >= m_text.size()) {
            return false;
        }
        if (m_text.at(m_pos) == QLatin1Char('0')) {
            ++m_pos;
        } else if (m_text.at(m_pos).isDigit()) {
            while (m_pos < m_text.size() && m_text.at(m_pos).isDigit()) {
                ++m_pos;
            }
        } else {
            return false;
        }

        bool isDouble = false;
        if (m_pos < m_text.size() && m_text.at(m_pos) == QLatin1Char('.')) {
            isDouble = true;
            ++m_pos;
            if (m_pos >= m_text.size() || !m_text.at(m_pos).isDigit()) {
                return false;
            }
            while (m_pos < m_text.size() && m_text.at(m_pos).isDigit()) {
                ++m_pos;
            }
        }
        if (m_pos < m_text.size() && (m_text.at(m_pos) == QLatin1Char('e') || m_text.at(m_pos) == QLatin1Char('E'))) {
            isDouble = true;
            ++m_pos;
            if (m_pos < m_text.size() &&
                (m_text.at(m_pos) == QLatin1Char('+') || m_text.at(m_pos) == QLatin1Char('-'))) {
                ++m_pos;
            }
            if (m_pos >= m_text.size() || !m_text.at(m_pos).isDigit()) {
                return false;
            }
            while (m_pos < m_text.size() && m_text.at(m_pos).isDigit()) {
                ++m_pos;
            }
        }

        const QString token = m_text.mid(start, m_pos - start);
        bool ok = false;
        if (!isDouble) {
            const qlonglong integer = token.toLongLong(&ok);
            if (ok) {
                *value = integer;
                return true;
            }
        }

        const double number = QLocale::c().toDouble(token, &ok);
        if (!ok) {
            return false;
        }
        *value = number;
        return true;
    }

    QString m_text;
    int m_pos = 0;
};

} // namespace

QByteArray QJsonDocument::toJson(JsonFormat format) const {
    if (!m_isObject) {
        return QByteArray();
    }
    const bool indented = format == Indented;
    return serializeMap(m_object.toVariantMap(), indented, 0).toUtf8();
}

QJsonDocument QJsonDocument::fromJson(const QByteArray& json) {
    QVariant value;
    JsonParser parser(json);
    if (!parser.parse(&value) || value.type() != QVariant::Map) {
        return QJsonDocument();
    }
    return QJsonDocument(QJsonObject::fromVariantMap(value.toMap()));
}
