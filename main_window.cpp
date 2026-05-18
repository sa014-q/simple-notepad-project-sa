#include "main_window.h"

#include "notepad_exception.h"
#include "sort.h"
#include "ui_find_replace_dialog.h"
#include "ui_word_frequency_dialog.h"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFontDialog>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QRegularExpression>
#include <QShortcut>
#include <QStatusBar>
#include <QTableWidgetItem>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QToolBar>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

static constexpr int default_font_size = 12;
static constexpr int min_font_size = 6;
static constexpr int max_font_size = 72;

main_window::main_window()
{
    setWindowTitle("Notepad");
    resize(900, 650);

    editor = new line_number_editor(this);
    editor->setFont(QFont("Courier New", default_font_size));
    setCentralWidget(editor);

    transforms.push_back(std::make_unique<uppercase_transform>());
    transforms.push_back(std::make_unique<lowercase_transform>());
    transforms.push_back(std::make_unique<capitalize_transform>());
    transforms.push_back(std::make_unique<sentence_case_transform>());
    transforms.push_back(std::make_unique<swap_case_transform>());

    setup_file_menu();
    setup_edit_menu();
    setup_format_menu();
    setup_view_menu();
    setup_format_toolbar();
    setup_search_menu();
    setup_tools_menu();
    setup_status_bar();
    setup_spell_checker();
}

main_window::~main_window() = default;

// ---------------------------------------------------------------------------
// set_font_size — единственное место где меняется размер шрифта
// ---------------------------------------------------------------------------

void main_window::set_font_size(int pt)
{
    pt = std::clamp(pt, min_font_size, max_font_size);
    QFont f = editor->font();
    f.setPointSize(pt);
    editor->setFont(f);
    // line_number_editor::changeEvent(FontChange) подхватит и обновит поля.
}

// ---------------------------------------------------------------------------
// Menus
// ---------------------------------------------------------------------------

void main_window::setup_file_menu()
{
    auto* file_menu = menuBar()->addMenu("File");

    const auto* action_new = file_menu->addAction("New");
    connect(action_new, &QAction::triggered, this, [this] {
        editor->clear();
        current_file.clear();
        update_title();
    });

    file_menu->addSeparator();

    const auto* action_open = file_menu->addAction("Open...");
    connect(action_open, &QAction::triggered, this, [this] { open_file(); });

    const auto* action_save = file_menu->addAction("Save");
    connect(action_save, &QAction::triggered, this, [this] { save_file(); });

    const auto* action_save_as = file_menu->addAction("Save As...");
    connect(action_save_as, &QAction::triggered, this, [this] { save_file_as(); });

    file_menu->addSeparator();

    const auto* action_exit = file_menu->addAction("Exit");
    connect(action_exit, &QAction::triggered, this, [] { QApplication::quit(); });
}

void main_window::setup_edit_menu()
{
    auto* edit_menu = menuBar()->addMenu("Edit");

    auto* action_undo = edit_menu->addAction("Undo");
    action_undo->setShortcut(QKeySequence::Undo);
    connect(action_undo, &QAction::triggered, editor, &QTextEdit::undo);

    auto* action_redo = edit_menu->addAction("Redo");
    action_redo->setShortcut(QKeySequence::Redo);
    connect(action_redo, &QAction::triggered, editor, &QTextEdit::redo);

    edit_menu->addSeparator();

    auto* action_cut = edit_menu->addAction("Cut");
    action_cut->setShortcut(QKeySequence::Cut);
    connect(action_cut, &QAction::triggered, editor, &QTextEdit::cut);

    auto* action_copy = edit_menu->addAction("Copy");
    action_copy->setShortcut(QKeySequence::Copy);
    connect(action_copy, &QAction::triggered, editor, &QTextEdit::copy);

    auto* action_paste = edit_menu->addAction("Paste");
    action_paste->setShortcut(QKeySequence::Paste);
    connect(action_paste, &QAction::triggered, editor, &QTextEdit::paste);

    edit_menu->addSeparator();

    auto* action_select_all = edit_menu->addAction("Select All");
    action_select_all->setShortcut(QKeySequence::SelectAll);
    connect(action_select_all, &QAction::triggered, editor, &QTextEdit::selectAll);
}

void main_window::setup_format_menu()
{
    auto* format_menu = menuBar()->addMenu("Format");

    auto* text_case_menu = format_menu->addMenu("Text Case");

    format_menu->addSeparator();

    auto* action_font = format_menu->addAction("Font...");
    connect(action_font, &QAction::triggered, this, [this] {
        show_font_dialog();
    });

    for (const auto& transform : transforms) {
        const auto* action = text_case_menu->addAction(
            QString::fromStdString(transform->name()));
        connect(action, &QAction::triggered, this, [this, &transform] {
            apply_transform(*transform);
        });
    }
}

// ---------------------------------------------------------------------------
// View menu — Zoom In / Zoom Out / Reset Zoom
// QShortcut на главное окно чтобы шорткаты работали всегда,
// независимо от того, какой виджет сейчас в фокусе.
// ---------------------------------------------------------------------------
void main_window::setup_view_menu()
{
    auto* view_menu = menuBar()->addMenu("View");

    auto* action_zoom_in = view_menu->addAction("Zoom In\tCtrl++");
    connect(action_zoom_in, &QAction::triggered, this, [this] {
        set_font_size(editor->font().pointSize() + 2);
    });

    auto* action_zoom_out = view_menu->addAction("Zoom Out\tCtrl+-");
    connect(action_zoom_out, &QAction::triggered, this, [this] {
        set_font_size(editor->font().pointSize() - 2);
    });

    auto* action_zoom_reset = view_menu->addAction("Reset Zoom\tCtrl+0");
    connect(action_zoom_reset, &QAction::triggered, this, [this] {
        set_font_size(default_font_size);
    });

           // Zoom In: Ctrl+= (физический +) и Ctrl++
    auto* sc_in1 = new QShortcut(QKeySequence("Ctrl+="), this);
    sc_in1->setContext(Qt::ApplicationShortcut);
    connect(sc_in1, &QShortcut::activated, this, [this] {
        set_font_size(editor->font().pointSize() + 2);
    });

    auto* sc_in2 = new QShortcut(QKeySequence("Ctrl++"), this);
    sc_in2->setContext(Qt::ApplicationShortcut);
    connect(sc_in2, &QShortcut::activated, this, [this] {
        set_font_size(editor->font().pointSize() + 2);
    });

           // Zoom Out: Ctrl+- и Ctrl+_
    auto* sc_out1 = new QShortcut(QKeySequence("Ctrl+-"), this);
    sc_out1->setContext(Qt::ApplicationShortcut);
    connect(sc_out1, &QShortcut::activated, this, [this] {
        set_font_size(editor->font().pointSize() - 2);
    });

    auto* sc_out2 = new QShortcut(QKeySequence("Ctrl+_"), this);
    sc_out2->setContext(Qt::ApplicationShortcut);
    connect(sc_out2, &QShortcut::activated, this, [this] {
        set_font_size(editor->font().pointSize() - 2);
    });

           // Reset Zoom: Ctrl+0 и Ctrl+Shift+0
    auto* sc_reset1 = new QShortcut(QKeySequence("Ctrl+0"), this);
    sc_reset1->setContext(Qt::ApplicationShortcut);
    connect(sc_reset1, &QShortcut::activated, this, [this] {
        set_font_size(default_font_size);
    });

    auto* sc_reset2 = new QShortcut(QKeySequence("Ctrl+Shift+0"), this);
    sc_reset2->setContext(Qt::ApplicationShortcut);
    connect(sc_reset2, &QShortcut::activated, this, [this] {
        set_font_size(default_font_size);
    });
}

void main_window::setup_format_toolbar()
{
    auto* toolbar = addToolBar("Format");
    toolbar->setIconSize(QSize(16, 16));

    auto* action_bold = toolbar->addAction(QIcon("data/images/bold.svg"), "Bold");
    action_bold->setCheckable(true);
    action_bold->setShortcut(QKeySequence("Ctrl+B"));
    connect(action_bold, &QAction::triggered, this, [this](bool checked) {
        QTextCharFormat fmt;
        fmt.setFontWeight(checked ? QFont::Bold : QFont::Normal);
        editor->mergeCurrentCharFormat(fmt);
    });

    auto* action_italic = toolbar->addAction(QIcon("data/images/italic.svg"), "Italic");
    action_italic->setCheckable(true);
    action_italic->setShortcut(QKeySequence("Ctrl+I"));
    connect(action_italic, &QAction::triggered, this, [this](bool checked) {
        QTextCharFormat fmt;
        fmt.setFontItalic(checked);
        editor->mergeCurrentCharFormat(fmt);
    });

    auto* action_underline = toolbar->addAction(QIcon("data/images/underline.svg"), "Underline");
    action_underline->setCheckable(true);
    action_underline->setShortcut(QKeySequence("Ctrl+U"));
    connect(action_underline, &QAction::triggered, this, [this](bool checked) {
        QTextCharFormat fmt;
        fmt.setFontUnderline(checked);
        editor->mergeCurrentCharFormat(fmt);
    });

    connect(editor, &QTextEdit::currentCharFormatChanged,
        this, [action_bold, action_italic, action_underline](const QTextCharFormat& fmt) {
            action_bold->setChecked(fmt.fontWeight() == QFont::Bold);
            action_italic->setChecked(fmt.fontItalic());
            action_underline->setChecked(fmt.fontUnderline());
        });
}

void main_window::setup_search_menu()
{
    auto* search_menu = menuBar()->addMenu("Search");

    auto* action_fr = search_menu->addAction("Find / Replace...");
    action_fr->setShortcut(QKeySequence::Find);
    connect(action_fr, &QAction::triggered, this, [this] {
        show_find_replace_dialog();
    });
}

void main_window::setup_tools_menu()
{
    auto* tools_menu = menuBar()->addMenu("Tools");

    const auto* action_wf = tools_menu->addAction("Word Frequency...");
    connect(action_wf, &QAction::triggered, this, [this] { show_word_frequency(); });

    tools_menu->addSeparator();

    const auto* action_spell = tools_menu->addAction("Check Spelling...");
    connect(action_spell, &QAction::triggered, this, [this] {
        if (highlighter) {
            highlighter->rehighlight();
        }
    });
}

// ---------------------------------------------------------------------------
// Status bar
// ---------------------------------------------------------------------------

void main_window::setup_status_bar()
{
    status_words = new QLabel("Words: 0");
    status_lines = new QLabel("Lines: 1");
    status_cursor = new QLabel("Ln 1, Col 1");

    statusBar()->addWidget(status_words);
    statusBar()->addWidget(new QLabel("  |  "));
    statusBar()->addWidget(status_lines);
    statusBar()->addPermanentWidget(status_cursor);

    connect(editor, &QTextEdit::textChanged, this, [this] {
        const QString text = editor->toPlainText();
        const int words = text.split(
                                  QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();
        status_words->setText("Words: " + QString::number(words));
        status_lines->setText(
            "Lines: " + QString::number(editor->document()->blockCount()));
    });

    connect(editor, &QTextEdit::cursorPositionChanged,
        this, &main_window::update_cursor_position);
}

void main_window::update_cursor_position()
{
    const QTextCursor cursor = editor->textCursor();
    status_cursor->setText(
        QString("Ln %1, Col %2")
            .arg(cursor.blockNumber() + 1)
            .arg(cursor.columnNumber() + 1));
}

// ---------------------------------------------------------------------------
// Spell checker
// ---------------------------------------------------------------------------

void main_window::setup_spell_checker()
{
    QString dict_path = "data/words.txt";
    if (!QFile::exists(dict_path)) {
        dict_path = QApplication::applicationDirPath() + "/data/words.txt";
    }

    checker = std::make_unique<spell_checker>(dict_path.toStdString());
    highlighter = new spell_checker_highlighter(checker.get(), editor->document());

    editor->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(editor, &QTextEdit::customContextMenuRequested,
        this, &main_window::show_suggestions_menu);
}

void main_window::show_suggestions_menu(const QPoint& pos)
{
    QTextCursor cursor = editor->cursorForPosition(pos);
    cursor.select(QTextCursor::WordUnderCursor);
    const QString word = cursor.selectedText();

    QMenu* menu = editor->createStandardContextMenu();

    if (!word.isEmpty() && !checker->is_correct(word)) {
        menu->addSeparator();
        const QVector<QString> suggs = checker->suggestions(word);

        if (suggs.isEmpty()) {
            auto* na = menu->addAction("(No suggestions)");
            na->setEnabled(false);
        } else {
            for (const QString& s : suggs) {
                auto* action = menu->addAction(s);
                connect(action, &QAction::triggered, this, [this, cursor, s]() mutable {
                    cursor.insertText(s);
                    editor->setTextCursor(cursor);
                });
            }
        }
    }

    menu->exec(editor->mapToGlobal(pos));
    menu->deleteLater();
}

// ---------------------------------------------------------------------------
// Text transforms
// ---------------------------------------------------------------------------

void main_window::apply_transform(const text_transform& transform) const
{
    auto cursor = editor->textCursor();
    if (!cursor.hasSelection()) {
        cursor.select(QTextCursor::Document);
    }
    const int start = cursor.selectionStart();
    const QString selected = cursor.selectedText().replace(QChar::ParagraphSeparator, '\n');
    const std::string original = selected.toStdString();
    const auto result = transform.apply(original);

    cursor.beginEditBlock();
    for (std::size_t i = 0; i < result.size(); ++i) {
        if (original[i] != result[i]) {
            cursor.setPosition(start + static_cast<int>(i));
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
            cursor.insertText(QString(QChar(result[i])), cursor.charFormat());
        }
    }
    cursor.endEditBlock();
}

// ---------------------------------------------------------------------------
// File operations
// ---------------------------------------------------------------------------

void main_window::open_file()
{
    const auto path = QFileDialog::getOpenFileName(this, "Open File");
    if (path.isEmpty()) {
        return;
    }
    try {
        QFile f(path);
        if (!f.exists()) {
            throw file_not_found_exception(path.toStdString());
        }
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            throw file_read_exception(path.toStdString());
        }
        QTextStream stream(&f);
        const QString data = stream.readAll();
        if (stream.status() != QTextStream::Ok) {
            throw file_read_exception(path.toStdString());
        }
        editor->setPlainText(data);
        current_file = path;
        update_title();
    } catch (const notepad_exception& e) {
        QMessageBox::critical(this, "Error", e.what());
    }
}

void main_window::save_file()
{
    if (current_file.isEmpty()) {
        save_file_as();
        return;
    }
    try {
        QFile f(current_file);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            throw file_write_exception(current_file.toStdString());
        }
        QTextStream stream(&f);
        stream << editor->toPlainText();
    } catch (const notepad_exception& e) {
        QMessageBox::critical(this, "Error", e.what());
    }
}

void main_window::save_file_as()
{
    const auto path = QFileDialog::getSaveFileName(this, "Save File As");
    if (path.isEmpty()) {
        return;
    }
    current_file = path;
    save_file();
    update_title();
}

void main_window::update_title()
{
    setWindowTitle(current_file.isEmpty() ? "Notepad" : "Notepad: " + current_file);
}

// ---------------------------------------------------------------------------
// Find / Replace
// ---------------------------------------------------------------------------

void main_window::show_find_replace_dialog()
{
    if (!find_replace_dlg) {
        find_replace_dlg = new QDialog(this);
        find_replace_ui = std::make_unique<Ui::find_replace_dialog>();
        find_replace_ui->setupUi(find_replace_dlg);

        auto get_flags = [this] {
            QTextDocument::FindFlags flags;
            if (find_replace_ui->case_sensitive_check->isChecked()) {
                flags |= QTextDocument::FindCaseSensitively;
            }
            return flags;
        };

        connect(find_replace_ui->find_next_button, &QPushButton::clicked,
            find_replace_dlg, [this, get_flags] {
                find_next(find_replace_ui->find_input->text(), get_flags());
            });
        connect(find_replace_ui->replace_button, &QPushButton::clicked,
            find_replace_dlg, [this, get_flags] {
                replace_current(find_replace_ui->find_input->text(),
                    find_replace_ui->replace_input->text(), get_flags());
            });
        connect(find_replace_ui->replace_all_button, &QPushButton::clicked,
            find_replace_dlg, [this, get_flags] {
                replace_all(find_replace_ui->find_input->text(),
                    find_replace_ui->replace_input->text(), get_flags());
            });
        connect(find_replace_ui->close_button, &QPushButton::clicked,
            find_replace_dlg, [this] { find_replace_dlg->hide(); });
    }
    find_replace_dlg->show();
    find_replace_dlg->raise();
    find_replace_dlg->activateWindow();
}

void main_window::find_next(const QString& term, const QTextDocument::FindFlags flags) const
{
    if (term.isEmpty()) {
        return;
    }
    auto result = editor->document()->find(term, editor->textCursor(), flags);
    if (result.isNull()) {
        auto start = editor->textCursor();
        start.movePosition(QTextCursor::Start);
        result = editor->document()->find(term, start, flags);
    }
    if (!result.isNull()) {
        editor->setTextCursor(result);
    }
}

void main_window::replace_current(const QString& term, const QString& replacement,
    const QTextDocument::FindFlags flags) const
{
    if (auto cursor = editor->textCursor(); cursor.hasSelection()) {
        cursor.insertText(replacement);
        editor->setTextCursor(cursor);
    }
    find_next(term, flags);
}

void main_window::replace_all(const QString& term, const QString& replacement,
    const QTextDocument::FindFlags flags) const
{
    if (term.isEmpty()) {
        return;
    }
    auto cursor = editor->textCursor();
    cursor.movePosition(QTextCursor::Start);
    editor->setTextCursor(cursor);

    while (true) {
        const auto match = editor->document()->find(term, editor->textCursor(), flags);
        if (match.isNull()) {
            break;
        }
        editor->setTextCursor(match);
        auto c = editor->textCursor();
        c.insertText(replacement);
        editor->setTextCursor(c);
    }
}

// ---------------------------------------------------------------------------
// Word frequency
// ---------------------------------------------------------------------------

void main_window::show_word_frequency()
{
    const std::string full_text = editor->toPlainText().toLower().toStdString();

    std::map<std::string, int> word_map;
    std::istringstream stream(full_text);
    std::string buf;
    while (stream >> buf) {
        std::erase_if(buf, [](const unsigned char ch) { return !std::isalpha(ch); });
        if (!buf.empty()) {
            ++word_map[buf];
        }
    }

    std::vector<std::pair<std::string, int>> sorted_list(word_map.begin(), word_map.end());
    merge_sort(sorted_list, [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    auto* freq_dialog = new QDialog(this);
    freq_dialog->setAttribute(Qt::WA_DeleteOnClose);
    Ui::word_frequency_dialog freq_ui;
    freq_ui.setupUi(freq_dialog);

    freq_ui.frequency_table->horizontalHeaderItem(0)->setTextAlignment(
        Qt::AlignLeft | Qt::AlignVCenter);
    freq_ui.frequency_table->horizontalHeaderItem(1)->setTextAlignment(
        Qt::AlignRight | Qt::AlignVCenter);

    freq_ui.frequency_table->setRowCount(static_cast<int>(sorted_list.size()));
    for (int i = 0; i < static_cast<int>(sorted_list.size()); ++i) {
        const auto& [word, count] = sorted_list[i];
        freq_ui.frequency_table->setItem(
            i, 0, new QTableWidgetItem(QString::fromStdString(word)));
        auto* cnt = new QTableWidgetItem(QString::number(count));
        cnt->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        freq_ui.frequency_table->setItem(i, 1, cnt);
    }

    freq_ui.frequency_table->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    freq_ui.frequency_table->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);

    freq_dialog->exec();
}


// ---------------------------------------------------------------------------
// Font dialog
// ---------------------------------------------------------------------------

void main_window::show_font_dialog()
{
    bool ok = false;
    const QFont current = editor->currentFont();
    const QFont chosen = QFontDialog::getFont(&ok, current, this, "Font");
    if (!ok) return;

    QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection()) {
        QTextCharFormat fmt;
        fmt.setFont(chosen);
        cursor.mergeCharFormat(fmt);
        editor->mergeCurrentCharFormat(fmt);
    } else {
        QFont editor_font = editor->font();
        int current_size = editor_font.pointSize();

        QFont new_font = chosen;
        new_font.setPointSize(current_size);
        editor->setFont(new_font);
        editor->update_left_margin();
    }
}