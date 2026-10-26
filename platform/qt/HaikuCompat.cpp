#include "HaikuCompat.h"

#include <QApplication>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <chrono>

BApplication* be_app = nullptr;

bigtime_t system_time()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

BFont::BFont()
{
    font_.setFamily("Noto Sans");
    font_.setBold(true);
    SetSize(12);
}

const BFont* default_bold_font()
{
    static const BFont font;
    return &font;
}

status_t BFont::SetFamilyAndStyle(const char* family, const char* style)
{
    const QString name = QString::fromUtf8(family);
    if (!QFontDatabase::families().contains(name, Qt::CaseInsensitive)) return -1;
    const QString requestedStyle = QString::fromUtf8(style);
    const QStringList styles = QFontDatabase::styles(name);
    if (!styles.contains(requestedStyle, Qt::CaseInsensitive)) return -1;
    font_ = QFontDatabase::font(name, requestedStyle, 12);
    SetSize(size_);
    return B_OK;
}

void BFont::SetSize(float size)
{
    size_ = std::max(1.0f, size);
    font_.setPixelSize(std::max(1, int(std::lround(size_))));
}

float BFont::StringWidth(const char* text) const
{
    return QFontMetricsF(font_).horizontalAdvance(QString::fromUtf8(text));
}

void BFont::GetHeight(font_height* height) const
{
    const QFontMetricsF metrics(font_);
    *height = {float(metrics.ascent()), float(metrics.descent()), float(metrics.leading())};
}

void BFont::GetHasGlyphs(const char* text, int32 count, bool* has) const
{
    const auto codepoints = QString::fromUtf8(text).toUcs4();
    const QFontMetricsF metrics(font_);
    for (int32 i = 0; i < count; ++i)
        has[i] = i < codepoints.size() && metrics.inFontUcs4(codepoints[i]);
}

void BFont::GetBoundingBoxesForStrings(const char** strings, int32 count,
    uint32, escapement_delta*, BRect* boxes) const
{
    const QFontMetricsF metrics(font_);
    for (int32 i = 0; i < count; ++i) {
        const QRectF r = metrics.tightBoundingRect(QString::fromUtf8(strings[i]));
        boxes[i] = {float(r.left()), float(r.top()), float(r.right()), float(r.bottom())};
    }
}

int32 count_font_families() { return QFontDatabase::families().size(); }

status_t get_font_family(int32 index, font_family* family, uint32* flags)
{
    const auto families = QFontDatabase::families();
    if (index < 0 || index >= families.size()) return -1;
    qstrncpy(*family, families[index].toUtf8().constData(), sizeof(font_family));
    *flags = 0;
    return B_OK;
}

status_t get_font_style(const char* family, int32 index, font_style* style, uint32* flags)
{
    const auto styles = QFontDatabase::styles(QString::fromUtf8(family));
    if (index < 0 || index >= styles.size()) return -1;
    qstrncpy(*style, styles[index].toUtf8().constData(), sizeof(font_style));
    *flags = 0;
    return B_OK;
}

BView::BView(BRect frame, const char* name, uint32, uint32)
{
    setObjectName(QString::fromUtf8(name));
    resize(frame.IntegerWidth() + 1, frame.IntegerHeight() + 1);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

BView::~BView() { EndDrawing(); }

BRect BView::Bounds() const
{
    return bitmap_ ? bitmap_->Bounds() : BRect(0, 0, width() - 1, height() - 1);
}

BPoint BView::ConvertToScreen(BPoint point) const
{
    const QPoint global = mapToGlobal(point.QtPoint().toPoint());
    return {float(global.x()), float(global.y())};
}

void BView::BeginDrawing(QPaintDevice* device)
{
    painter_ = std::make_unique<QPainter>(device);
    painter_->setRenderHint(QPainter::TextAntialiasing);
    SetDrawingMode(mode_);
}

void BView::EndDrawing() { painter_.reset(); }

QPen BView::Pen() const
{
    QPen pen(color_, penSize_, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    return pen;
}

void BView::SetDrawingMode(int mode)
{
    mode_ = mode;
    if (painter_) painter_->setCompositionMode(mode == B_OP_OVER
        ? QPainter::CompositionMode_SourceOver : QPainter::CompositionMode_Source);
}

void BView::FillRect(BRect rect) { painter_->fillRect(rect.QtRect(), color_); }

void BView::FillRoundRect(BRect rect, float rx, float ry)
{
    painter_->setPen(Qt::NoPen);
    painter_->setBrush(color_);
    painter_->setRenderHint(QPainter::Antialiasing, true);
    painter_->drawRoundedRect(rect.QtRect(), rx, ry);
    painter_->setRenderHint(QPainter::Antialiasing, false);
}

void BView::StrokeRoundRect(BRect rect, float rx, float ry)
{
    painter_->setPen(Pen());
    painter_->setBrush(Qt::NoBrush);
    painter_->drawRoundedRect(QRectF(rect.left, rect.top, rect.Width(), rect.Height()), rx, ry);
}

void BView::StrokeRect(BRect rect)
{
    painter_->setPen(Pen());
    painter_->setBrush(Qt::NoBrush);
    painter_->drawRect(QRectF(rect.left, rect.top, rect.Width(), rect.Height()));
}

void BView::StrokeLine(BPoint from, BPoint to)
{
    painter_->setPen(Pen());
    painter_->drawLine(from.QtPoint(), to.QtPoint());
}

void BView::FillEllipse(BPoint center, float rx, float ry)
{
    painter_->setPen(Qt::NoPen);
    painter_->setBrush(color_);
    painter_->setRenderHint(QPainter::Antialiasing, true);
    painter_->drawEllipse(center.QtPoint(), rx, ry);
    painter_->setRenderHint(QPainter::Antialiasing, false);
}

void BView::StrokeEllipse(BPoint center, float rx, float ry)
{
    painter_->setPen(Pen());
    painter_->setBrush(Qt::NoBrush);
    painter_->setRenderHint(QPainter::Antialiasing, true);
    painter_->drawEllipse(center.QtPoint(), rx, ry);
    painter_->setRenderHint(QPainter::Antialiasing, false);
}

void BView::FillShape(const BShape* shape)
{
    painter_->save();
    painter_->translate(penPosition_);
    painter_->setRenderHint(QPainter::Antialiasing, true);
    painter_->fillPath(shape->QtPath(), color_);
    painter_->restore();
}

void BView::DrawString(const char* text, BPoint at)
{
    painter_->setPen(QPen(color_));
    painter_->setFont(font_);
    painter_->drawText(at.QtPoint(), QString::fromUtf8(text));
}

void BView::DrawBitmap(BBitmap* bitmap, BRect source, BRect dest)
{
    painter_->drawImage(dest.QtRect(), bitmap->Image(), source.QtRect());
}

void BView::ConstrainClippingRegion(const BRegion* region)
{
    if (region) painter_->setClipRegion(region->QtRegion());
    else painter_->setClipping(false);
}

void BView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (!attached_ && !bitmap_) {
        attached_ = true;
        AttachedToWindow();
    }
}

void BView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (attached_) FrameResized(width() - 1, height() - 1);
}

void BView::paintEvent(QPaintEvent* event)
{
    BeginDrawing(this);
    const QRect rect = event->rect();
    Draw(BRect(rect.left(), rect.top(), rect.right(), rect.bottom()));
    EndDrawing();
}

void BView::mousePressEvent(QMouseEvent* event)
{
    unsetCursor();
    if (event->button() == Qt::LeftButton)
        MouseDown(BPoint(event->position().x(), event->position().y()));
}

void BView::mouseMoveEvent(QMouseEvent* event)
{
    unsetCursor();
    MouseMoved(BPoint(event->position().x(), event->position().y()), 0, nullptr);
}

void BView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F11) {
        if (isFullScreen()) showNormal(); else showFullScreen();
        event->accept();
        return;
    }
    char key = 0;
    switch (event->key()) {
        case Qt::Key_Escape: key = B_ESCAPE; break;
        case Qt::Key_Right: key = B_RIGHT_ARROW; break;
        case Qt::Key_Left: key = B_LEFT_ARROW; break;
    }
    const QByteArray text = key ? QByteArray(1, key) : event->text().toUtf8();
    if (!text.isEmpty()) KeyDown(text.constData(), text.size());
}

BBitmap::BBitmap(BRect bounds, uint32, bool)
    : image_(bounds.IntegerWidth() + 1, bounds.IntegerHeight() + 1, QImage::Format_RGB32)
{
    if (image_.isNull()) throw std::bad_alloc();
    image_.fill(Qt::black);
}

BBitmap::~BBitmap() { view_.reset(); }

void BBitmap::AddChild(BView* view)
{
    view_.reset(view);
    view->bitmap_ = this;
}

bool BBitmap::Lock()
{
    if (locks_++ == 0 && view_) view_->BeginDrawing(&image_);
    return true;
}

void BBitmap::Unlock()
{
    if (--locks_ == 0 && view_) view_->EndDrawing();
}

BMessageRunner::BMessageRunner(BMessenger target, const BMessage* message, bigtime_t interval)
{
    QObject::connect(&timer_, &QTimer::timeout, target.receiver,
        [receiver = target.receiver, copy = *message]() mutable { receiver->MessageReceived(&copy); });
    timer_.setTimerType(Qt::PreciseTimer);
    timer_.start(std::max(1, int(interval / 1000)));
}

void BApplication::PostMessage(uint32 what)
{
    if (what == B_QUIT_REQUESTED) QCoreApplication::quit();
}

void BApplication::ObscureCursor()
{
    for (QWidget* widget : QApplication::topLevelWidgets())
        if (widget->isVisible() && widget->objectName() == "board") widget->setCursor(Qt::BlankCursor);
}

void BPopUpMenu::AddItem(BMenuItem* item)
{
    items_.emplace_back(item);
    item->action_ = menu_.addAction(item->text_);
    item->action_->setCheckable(item->checkable_);
    if (item->checkable_) {
        selections_.addAction(item->action_);
        item->action_->setChecked(item->marked_);
    }
}

BMenuItem* BPopUpMenu::Go(BPoint at, bool, bool)
{
    const QAction* chosen = menu_.exec(at.QtPoint().toPoint());
    for (const auto& item : items_) if (item->action_ == chosen) return item.get();
    return nullptr;
}
