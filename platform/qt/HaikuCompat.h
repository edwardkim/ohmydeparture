/*
 * The small subset of Haiku's Interface Kit used by the original renderer.
 * Qt owns the Linux window, raster painting, input events and popup menus.
 * This is an application-specific adapter, not a general Haiku emulation.
 */
#ifndef RDEPARTURES_HAIKU_COMPAT_H
#define RDEPARTURES_HAIKU_COMPAT_H

#include "HaikuTypes.h"

#include <QAction>
#include <QActionGroup>
#include <QFont>
#include <QImage>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QTimer>
#include <QWidget>

#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

struct BPoint {
    float x = 0, y = 0;
    BPoint() = default;
    BPoint(float x, float y) : x(x), y(y) {}
    QPointF QtPoint() const { return {x, y}; }
};

inline const BPoint B_ORIGIN;

struct BRect {
    float left = 0, top = 0, right = -1, bottom = -1;
    BRect() = default;
    BRect(float l, float t, float r, float b) : left(l), top(t), right(r), bottom(b) {}
    void Set(float l, float t, float r, float b) { *this = BRect(l, t, r, b); }
    float Width() const { return right - left; }
    float Height() const { return bottom - top; }
    int IntegerWidth() const { return static_cast<int>(std::floor(Width())); }
    int IntegerHeight() const { return static_cast<int>(std::floor(Height())); }
    bool IsValid() const { return right >= left && bottom >= top; }
    BPoint LeftBottom() const { return {left, bottom}; }
    bool Contains(BPoint p) const {
        return p.x >= left && p.x <= right && p.y >= top && p.y <= bottom;
    }
    BRect InsetByCopy(float x, float y) const {
        return {left + x, top + y, right - x, bottom - y};
    }
    void OffsetBy(float x, float y) { left += x; right += x; top += y; bottom += y; }
    bool operator==(const BRect& b) const {
        return left == b.left && top == b.top && right == b.right && bottom == b.bottom;
    }
    bool operator!=(const BRect& b) const { return !(*this == b); }
    // Haiku rectangle coordinates include the last pixel.
    QRectF QtRect() const { return {left, top, Width() + 1, Height() + 1}; }
};

struct rgb_color { uint8 red, green, blue, alpha; };
inline rgb_color make_color(uint8 r, uint8 g, uint8 b, uint8 a = 255) { return {r, g, b, a}; }
inline constexpr rgb_color B_TRANSPARENT_COLOR = {0, 0, 0, 0};

class BString {
public:
    BString() = default;
    BString(const char* s) : value_(s ? s : "") {}
    BString(const char* s, int32 n) : value_(s, n) {}
    const char* String() const { return value_.c_str(); }
    int32 Length() const { return static_cast<int32>(value_.size()); }
    bool operator==(const BString& s) const { return value_ == s.value_; }
    bool operator==(const char* s) const { return value_ == s; }
    template<class T> BString& operator<<(const T& value) {
        std::ostringstream stream;
        stream << value;
        value_ += stream.str();
        return *this;
    }
    BString& operator<<(const BString& s) { value_ += s.value_; return *this; }
private:
    std::string value_;
};

struct font_height { float ascent, descent, leading; };
struct escapement_delta { float nonspace, space; };
using font_family = char[256];
using font_style = char[256];
constexpr uint32 B_FORCE_ANTIALIASING = 1;
constexpr uint32 B_SCREEN_METRIC = 0;
constexpr uint32 B_REGULAR_FACE = 0;

class BFont {
public:
    BFont();
    status_t SetFamilyAndStyle(const char* family, const char* style);
    void SetSize(float size);
    void SetStretch(int percent) { font_.setStretch(percent); }
    float Size() const { return size_; }
    void SetFlags(uint32) { font_.setStyleStrategy(QFont::PreferAntialias); }
    void SetFace(uint32) {
        // A named "Bold" style otherwise overrides setBold(false) in Qt.
        font_.setStyleName(QString());
        font_.setBold(false);
        font_.setItalic(false);
    }
    float StringWidth(const char* text) const;
    void GetHeight(font_height* height) const;
    void GetHasGlyphs(const char* text, int32 count, bool* has) const;
    void GetBoundingBoxesForStrings(const char** strings, int32 count, uint32,
        escapement_delta*, BRect* boxes) const;
    const QFont& QtFont() const { return font_; }
private:
    QFont font_;
    float size_ = 12;
};

const BFont* default_bold_font();
#define be_bold_font default_bold_font()
int32 count_font_families();
status_t get_font_family(int32 index, font_family* family, uint32* flags);
status_t get_font_style(const char* family, int32 index, font_style* style, uint32* flags);
bigtime_t system_time();

class BMessage {
public:
    explicit BMessage(uint32 what) : what(what) {}
    void AddInt32(const char* key, int32 value) { values_[key] = value; }
    status_t FindInt32(const char* key, int32* out) const {
        auto it = values_.find(key);
        if (it == values_.end()) return -1;
        *out = it->second;
        return B_OK;
    }
    uint32 what;
private:
    std::map<std::string, int32> values_;
};

class BRegion {
public:
    BRegion() = default;
    explicit BRegion(BRect rect) { Include(rect); }
    void Include(BRect rect) { region_ += rect.QtRect().toAlignedRect(); }
    int CountRects() const { return region_.rectCount(); }
    const QRegion& QtRegion() const { return region_; }
private:
    QRegion region_;
};

class BShape {
public:
    void MoveTo(BPoint point) { path_.moveTo(point.QtPoint()); }
    void LineTo(BPoint point) { path_.lineTo(point.QtPoint()); }
    void Close() { path_.closeSubpath(); }
    const QPainterPath& QtPath() const { return path_; }
private:
    QPainterPath path_;
};

constexpr uint32 B_FOLLOW_ALL = 0, B_FOLLOW_NONE = 0;
constexpr uint32 B_WILL_DRAW = 0, B_FRAME_EVENTS = 0;
constexpr uint32 B_RGB32 = 0;
constexpr int B_OP_COPY = 0, B_OP_OVER = 1;
constexpr char B_ESCAPE = 27, B_RIGHT_ARROW = 29, B_LEFT_ARROW = 28;
constexpr uint32 B_QUIT_REQUESTED = 0x71756974;

class BBitmap;

class BView : public QWidget {
public:
    BView(BRect frame, const char* name, uint32, uint32);
    ~BView() override;
    virtual void AttachedToWindow() {}
    virtual void Draw(BRect) {}
    virtual void FrameResized(float, float) {}
    virtual void MessageReceived(BMessage*) {}
    virtual void MouseDown(BPoint) {}
    virtual void MouseMoved(BPoint, uint32, const BMessage*) {}
    virtual void KeyDown(const char*, int32) {}
    BRect Bounds() const;
    QWidget* Window() const { return attached_ ? window() : nullptr; }
    void SetViewColor(rgb_color) {}
    void MakeFocus(bool focus) { if (focus) setFocus(); }
    void Invalidate() { update(); }
    void Invalidate(const BRegion* region) { update(region->QtRegion()); }
    BPoint ConvertToScreen(BPoint point) const;
    void DrawBitmap(BBitmap* bitmap, BRect source, BRect dest);

    void SetHighColor(rgb_color color) { color_ = QColor(color.red, color.green, color.blue, color.alpha); }
    void SetHighColor(uint8 r, uint8 g, uint8 b) { SetHighColor(make_color(r, g, b)); }
    void SetLowColor(rgb_color) {}
    void SetDrawingMode(int mode);
    void SetFont(const BFont* font) { font_ = font->QtFont(); }
    void SetPenSize(float size) { penSize_ = size; }
    void MovePenTo(BPoint point) { penPosition_ = point.QtPoint(); }
    void FillRect(BRect rect);
    void FillRoundRect(BRect rect, float rx, float ry);
    void StrokeRoundRect(BRect rect, float rx, float ry);
    void StrokeRect(BRect rect);
    void StrokeLine(BPoint from, BPoint to);
    void FillEllipse(BPoint center, float rx, float ry);
    void StrokeEllipse(BPoint center, float rx, float ry);
    void FillShape(const BShape* shape);
    void DrawString(const char* text, BPoint at);
    void ConstrainClippingRegion(const BRegion* region);
    void Sync() {} // Qt's raster paint engine executes drawing synchronously.

protected:
    void showEvent(QShowEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    friend class BBitmap;
    void BeginDrawing(QPaintDevice* device);
    void EndDrawing();
    QPen Pen() const;
    BBitmap* bitmap_ = nullptr;
    bool attached_ = false;
    QColor color_ = Qt::black;
    QFont font_;
    float penSize_ = 1;
    QPointF penPosition_;
    int mode_ = B_OP_COPY;
    std::unique_ptr<QPainter> painter_;
};

class BBitmap {
public:
    BBitmap(BRect bounds, uint32, bool = false);
    ~BBitmap();
    BBitmap(const BBitmap&) = delete;
    BBitmap& operator=(const BBitmap&) = delete;
    BRect Bounds() const { return {0, 0, float(image_.width() - 1), float(image_.height() - 1)}; }
    void* Bits() { return image_.bits(); }
    int32 BytesPerRow() const { return image_.bytesPerLine(); }
    const QImage& Image() const { return image_; }
    void AddChild(BView* view);
    bool Lock();
    void Unlock();
private:
    QImage image_;
    std::unique_ptr<BView> view_;
    int locks_ = 0;
};

struct BMessenger {
    explicit BMessenger(BView* receiver) : receiver(receiver) {}
    BView* receiver;
};

class BMessageRunner {
public:
    BMessageRunner(BMessenger target, const BMessage* message, bigtime_t interval);
private:
    QTimer timer_;
};

class BApplication {
public:
    void PostMessage(uint32 what);
    void ObscureCursor();
};
extern BApplication* be_app;

class BMenuItem {
public:
    BMenuItem(const char* text, BMessage* message) : text_(QString::fromUtf8(text)), message_(message) {}
    void SetMarked(bool marked) { marked_ = marked; checkable_ = true; }
    BMessage* Message() { return message_.get(); }
private:
    friend class BPopUpMenu;
    QString text_;
    std::unique_ptr<BMessage> message_;
    bool marked_ = false;
    bool checkable_ = false;
    QAction* action_ = nullptr;
};

class BPopUpMenu {
public:
    BPopUpMenu(const char*, bool, bool) : selections_(&menu_) { selections_.setExclusive(true); }
    void AddItem(BMenuItem* item);
    void AddSeparatorItem() { menu_.addSeparator(); }
    BMenuItem* Go(BPoint at, bool, bool);
private:
    QMenu menu_;
    QActionGroup selections_;
    std::vector<std::unique_ptr<BMenuItem>> items_;
};

#endif
