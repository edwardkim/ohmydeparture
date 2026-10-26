#include "CodexHistory.h"
#include <QDateTime>
#include <QJsonObject>
#include <QHash>
#include <QStringList>

namespace {
bool wide(uint c) {
    return (c>=0x1100&&c<=0x115f)||(c>=0x2e80&&c<=0xa4cf)||(c>=0xac00&&c<=0xd7a3)
        ||(c>=0xf900&&c<=0xfaff)||(c>=0xfe30&&c<=0xfe4f)||(c>=0xff00&&c<=0xff60)
        ||(c>=0xffe0&&c<=0xffe6);
}
QStringList previewLines(QString text) {
    text.replace('\t', "    ");text.remove('\r');
    QStringList lines;QString line;int cells=0;bool truncated=false;
    for (int i=0;i<text.size();) {
        const int length=text[i].isHighSurrogate()&&i+1<text.size()&&text[i+1].isLowSurrogate()?2:1;
        const uint point=length==2?QChar::surrogateToUcs4(text[i],text[i+1]):text[i].unicode();
        const int width=wide(point)?2:1;
        if (point=='\n' || cells+width>38) {
            if (point!='\n') {
                const int space=line.lastIndexOf(' ');
                if (space>0) {i-=line.size()-space-1;line.truncate(space);}
            }
            lines.append(line);line.clear();cells=0;
            if (lines.size()==3) {truncated=i<text.size();break;}
            if (point=='\n') {++i;continue;}
            continue;
        }
        if (point!='\r') {line+=text.mid(i,length);cells+=width;}
        i+=length;
    }
    if (lines.size()<3 && (!line.isEmpty()||lines.isEmpty())) lines.append(line);
    if (truncated) {
        auto& last=lines.last();
        while (last.size()>0 && [&]{int n=0;for (const auto c:last.toUcs4()) n+=wide(c)?2:1;return n>35;}())
            last.chop(last.back().isLowSurrogate()?2:1);
        last+="...";
    }
    return lines;
}
QString resultLabel(const QString& state) {
    static const QHash<QString,QString> labels={{"completed","COMPLETED"},{"inProgress","IN PROGRESS"},
        {"failed","FAILED"},{"interrupted","INTERRUPTED"},{"recorded","RECORDED"},
        {"error","ERROR"},{"pending","PENDING"},{"running","RUNNING"}};
    return labels.value(state,state.isEmpty()?QString("RECORDED"):state.toUpper());
}
QString eventLabel(const QJsonObject& item) {
    static const QHash<QString,QString> labels={{"userMessage","USER"},{"agentMessage","RESPONSE"},
        {"commandExecution","COMMAND"},{"collabAgentToolCall","SUBAGENT"},{"mcpToolCall","TOOL"},
        {"dynamicToolCall","TOOL"},{"fileChange","FILE EDIT"},{"webSearch","WEB SEARCH"}};
    return labels.value(item["kind"].toString(),item["label"].toString("EVENT"));
}
}

QJsonArray codexHistoryRows(const QJsonArray& turns) {
    QJsonArray rows;int events=0,turnCount=0;
    for (const auto& value:turns) {
        if (turnCount++==10 || events==20) break;
        const auto turn=value.toObject();
        const qint64 time=qint64(turn["time"].toDouble());
        const auto timeText=time>0?QDateTime::fromSecsSinceEpoch(time).toString("HH:mm"):"--:--";
        const auto state=turn["status"].toString();
        rows.append(QJsonObject{{"time",timeText},{"event","TURN"},
            {"text","Turn "+turn["id"].toString().left(12)+" / turn outcome"},
            {"result",resultLabel(state)},{"status",state}});
        const auto items=turn["items"].toArray();
        for (int index=items.size()-1;index>=0&&events<20;--index,++events) {
            const auto item=items[index].toObject();
            const auto lines=previewLines(item["text"].toString());
            for (int line=0;line<lines.size();++line) {
                rows.append(QJsonObject{{"time",line==0?timeText:QString()},
                    {"event",line==0?eventLabel(item):QString()},
                    {"text",lines[line]},{"result",line==0?resultLabel(item["status"].toString()):QString()},
                    {"status",item["status"].toString()}});
            }
        }
    }
    return rows;
}
