#ifndef SPELL_CHECKER_HIGHLIGHTER_H
#define SPELL_CHECKER_HIGHLIGHTER_H

#include "spell_checker.h"

#include <QColor>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextDocument>

class spell_checker_highlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit spell_checker_highlighter(spell_checker* checker, QTextDocument* parent = nullptr)
        : QSyntaxHighlighter(parent)
        , checker(checker)
    {
    }

protected:
    void highlightBlock(const QString& text) override
    {
        if (!checker || !checker->loaded()) {
            return;
        }

        int word_start = -1;

        auto check_and_highlight = [&](int end) {
            if (word_start < 0) {
                return;
            }
            const int length = end - word_start;
            const QString word = text.mid(word_start, length);

            if (!checker->is_correct(word)) {
                // Grab whatever format is already on the range (bold, underline, etc.)
                // and add the spell-check underline on top without removing anything.
                QTextCharFormat fmt = format(word_start);
                fmt.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
                fmt.setUnderlineColor(QColor(255, 50, 50));
                setFormat(word_start, length, fmt);
            }

            word_start = -1;
        };

        for (int i = 0; i < text.size(); ++i) {
            const QChar ch = text[i];
            if (ch.isLetter() || (ch == u'\'' && word_start >= 0)) {
                if (word_start < 0) {
                    word_start = i;
                }
            } else {
                check_and_highlight(i);
            }
        }
        check_and_highlight(text.size());
    }

private:
    spell_checker* checker;
};

#endif // SPELL_CHECKER_HIGHLIGHTER_H