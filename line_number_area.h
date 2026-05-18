#ifndef LINE_NUMBER_AREA_H
#define LINE_NUMBER_AREA_H

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextEdit>
#include <QWidget>

class line_number_editor;

class line_number_area : public QWidget {
    Q_OBJECT
public:
    explicit line_number_area(line_number_editor* editor);
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    line_number_editor* code_editor;
};

class line_number_editor : public QTextEdit {
    Q_OBJECT
public:
    explicit line_number_editor(QWidget* parent = nullptr)
        : QTextEdit(parent)
        , number_area(new line_number_area(this))
    {
        update_left_margin();
        connect(document(), &QTextDocument::blockCountChanged,
            this, &line_number_editor::update_left_margin);
        connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int) { number_area->update(); });
        connect(this, &QTextEdit::cursorPositionChanged,
            this, [this] { number_area->update(); });
    }

    [[nodiscard]] int line_number_area_width() const
    {
        int digits = 1;
        int max_lines = std::max(1, document()->blockCount());
        while (max_lines >= 10) {
            max_lines /= 10;
            ++digits;
        }
        return 8 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    }

    void line_number_area_paint(QPaintEvent* event)
    {
        QPainter painter(number_area);
        const QPalette pal = QApplication::palette();
        painter.fillRect(event->rect(), pal.color(QPalette::Window));

               // Always use the same font as the editor so numbers scale with zoom.
        painter.setFont(font());

        const int current_block = textCursor().blockNumber();
        const int offset_y = static_cast<int>(-verticalScrollBar()->value());

        QTextBlock block = document()->begin();
        int block_number = 0;

        while (block.isValid()) {
            const QRectF br = document()->documentLayout()->blockBoundingRect(block);
            const int top = static_cast<int>(br.translated(0, offset_y).top());
            const int bottom = top + static_cast<int>(br.height());

            if (top <= event->rect().bottom() && bottom >= event->rect().top()) {
                painter.setPen(block_number == current_block
                        ? pal.color(QPalette::Text)
                        : pal.color(QPalette::PlaceholderText));
                painter.drawText(0, top,
                    number_area->width() - 4,
                    fontMetrics().height(),
                    Qt::AlignRight,
                    QString::number(block_number + 1));
            }
            block = block.next();
            ++block_number;
        }
    }

protected:
    // Recalculate margin whenever font changes (zoom, family change, etc.)
    void changeEvent(QEvent* event) override
    {
        QTextEdit::changeEvent(event);
        if (event->type() == QEvent::FontChange) {
            update_left_margin();
            number_area->update();
        }
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QTextEdit::resizeEvent(event);
        reposition_number_area();
    }

public slots:
    void update_left_margin()
    {
        const int w = line_number_area_width();
        setViewportMargins(w, 0, 0, 0);
        reposition_number_area();
    }

private:
    void reposition_number_area()
    {
        const QRect cr = contentsRect();
        number_area->setGeometry(cr.left(), cr.top(),
            line_number_area_width(), cr.height());
    }

    line_number_area* number_area;
};

inline line_number_area::line_number_area(line_number_editor* editor)
    : QWidget(editor)
    , code_editor(editor)
{
}

inline QSize line_number_area::sizeHint() const
{
    return { code_editor->line_number_area_width(), 0 };
}

inline void line_number_area::paintEvent(QPaintEvent* event)
{
    code_editor->line_number_area_paint(event);
}

#endif // LINE_NUMBER_AREA_H