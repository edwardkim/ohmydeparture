#include "CodexHistory.h"
#include <QJsonObject>
#include <cstdio>
#include <stdexcept>

static void check(bool condition,const char* text) {if (!condition) throw std::runtime_error(text);}
static QJsonObject turn(const QJsonArray& items) {
    return {{"id","turn-preview"},{"status","completed"},{"time",1792908000},{"items",items}};
}
int main() {
    try {
        const auto rows=codexHistoryRows({turn({
            QJsonObject{{"label","User"},{"text","Keep <text> & commands as recorded"}},
            QJsonObject{{"label","Command"},{"status","completed"},{"text","make test\nexit: 0\nValidation passed"}}
        })});
        check(rows.size()==5,"turn header and multiline event previews");
        check(rows[0].toObject()["event"]=="TURN" && rows[0].toObject()["result"]=="COMPLETED","turn outcome remains separate from project state");
        check(rows[1].toObject()["event"]=="Command" && rows[1].toObject()["text"]=="make test","newest item shown first");
        check(rows[2].toObject()["event"].toString().isEmpty() && rows[2].toObject()["text"]=="exit: 0","continuation lines keep recorded tool output");
        check(rows[4].toObject()["text"]=="Keep <text> & commands as recorded"
              && rows[4].toObject()["result"]=="RECORDED","plain text is preserved without inventing an outcome");
        const auto wrapped=codexHistoryRows({turn({QJsonObject{{"kind","agentMessage"},{"text",QString(35,'a')+" word"}}})});
        check(wrapped.size()==3 && wrapped[1].toObject()["event"]=="RESPONSE"
              && wrapped[1].toObject()["text"]==QString(35,'a') && wrapped[2].toObject()["text"]=="word",
              "word wrapping preserves whole words and readable event types");
        const auto wide=codexHistoryRows({turn({QJsonObject{{"label","Agent"},{"text",QString(100,QChar(0xac00))}}})});
        check(wide.size()==4 && wide.last().toObject()["text"].toString().endsWith("..."),"CJK previews use at most three flap lines and mark truncation");
        for (int i=1;i<wide.size();++i) {
            int cells=0;for (auto c:wide[i].toObject()["text"].toString()) cells+=c.unicode()==0xac00?2:1;
            check(cells<=38,"wide text fits content cells");
        }
        QJsonArray items;for (int i=0;i<100;++i) items.append(QJsonObject{{"label","Event"},{"text",QString("item-%1").arg(i)}});
        const auto bounded=codexHistoryRows({turn(items)});
        check(bounded.size()==21 && bounded[1].toObject()["text"]=="item-99" && bounded.last().toObject()["text"]=="item-80","preview is limited to twenty recent items");
        check(codexHistoryRows({}).isEmpty(),"empty history stays empty for the controller's explicit placeholder");
        check(codexHistoryRows({turn({})}).size()==1,"turn without items retains its recorded outcome");
        std::puts("PASS: recent history order, partial bounds, multiline/CJK wrapping, literal contents, turn outcomes");
        return 0;
    } catch (const std::exception& error) {std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}
