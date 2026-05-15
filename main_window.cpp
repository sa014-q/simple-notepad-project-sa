#include "main_window.h"

#include "notepad_exception.h"
#include "ui_find_replace_dialog.h"
#include "ui_word_frequency_dialog.h"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QHeaderView>
#include <QKeySequence>
#include <QMenuBar>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStatusBar>
#include <QTableWidgetItem>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextStream>
#include <QToolBar>
#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

main_window::main_window()
{
    setWindowTitle("Notepad");
    resize(800, 600);

    editor = new QTextEdit(this);
    setCentralWidget(editor);

    transforms.push_back(std::make_unique<uppercase_transform>());
    transforms.push_back(std::make_unique<lowercase_transform>());
    transforms.push_back(std::make_unique<capitalize_transform>());
    transforms.push_back(std::make_unique<sentence_case_transform>());
    transforms.push_back(std::make_unique<swap_case_transform>());

    setup_file_menu();
    setup_edit_menu();
    setup_format_menu();
    setup_format_toolbar();
    setup_search_menu();
    setup_tools_menu();
}

main_window::~main_window() = default;

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
    connect(action_open, &QAction::triggered, this, [this] {
        open_file();
    });

    const auto* action_save = file_menu->addAction("Save");
    connect(action_save, &QAction::triggered, this, [this] {
        save_file();
    });

    const auto* action_save_as = file_menu->addAction("Save As...");
    connect(action_save_as, &QAction::triggered, this, [this] {
        save_file_as();
    });

    file_menu->addSeparator();

    const auto* action_exit = file_menu->addAction("Exit");
    connect(action_exit, &QAction::triggered, this, [] {
        QApplication::quit();
    });
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

    for (const auto& transform : transforms) {
        const auto* action = text_case_menu->addAction(QString::fromStdString(transform->name()));
        connect(action, &QAction::triggered, this, [this, &transform] {
            apply_transform(*transform);
        });
    }
}

void main_window::setup_format_toolbar()
{
    auto* toolbar = addToolBar("Format");
    toolbar->setIconSize(QSize(16, 16));

    auto* action_bold = toolbar->addAction(QIcon("data/images/bold.svg"), "Bold");
    action_bold->setCheckable(true);
    action_bold->setShortcut(QKeySequence("Ctrl+B"));
    connect(action_bold, &QAction::triggered, this, [this](const bool is_checked) {
        QTextCharFormat char_format;
        char_format.setFontWeight(is_checked ? QFont::Bold : QFont::Normal);
        editor->mergeCurrentCharFormat(char_format);
    });

    auto* action_italic = toolbar->addAction(QIcon("data/images/italic.svg"), "Italic");
    action_italic->setCheckable(true);
    action_italic->setShortcut(QKeySequence("Ctrl+I"));
    connect(action_italic, &QAction::triggered, this, [this](const bool is_checked) {
        QTextCharFormat char_format;
        char_format.setFontItalic(is_checked);
        editor->mergeCurrentCharFormat(char_format);
    });

    auto* action_underline = toolbar->addAction(QIcon("data/images/underline.svg"), "Underline");
    action_underline->setCheckable(true);
    action_underline->setShortcut(QKeySequence("Ctrl+U"));
    connect(action_underline, &QAction::triggered, this, [this](const bool is_checked) {
        QTextCharFormat char_format;
        char_format.setFontUnderline(is_checked);
        editor->mergeCurrentCharFormat(char_format);
    });

    connect(editor, &QTextEdit::currentCharFormatChanged,
        this, [action_bold, action_italic, action_underline](const QTextCharFormat& format) {
            action_bold->setChecked(format.fontWeight() == QFont::Bold);
            action_italic->setChecked(format.fontItalic());
            action_underline->setChecked(format.fontUnderline());
        });
}

void main_window::setup_search_menu()
{
    auto* search_menu = menuBar()->addMenu("Search");

    auto* action_find_replace = search_menu->addAction("Find / Replace...");
    action_find_replace->setShortcut(QKeySequence::Find);
    connect(action_find_replace, &QAction::triggered, this, [this] {
        show_find_replace_dialog();
    });
}

void main_window::setup_tools_menu()
{
    auto* tools_menu = menuBar()->addMenu("Tools");

    const auto* action_word_freq = tools_menu->addAction("Word Frequency...");
    connect(action_word_freq, &QAction::triggered, this, [this] {
        show_word_frequency();
    });
}

void main_window::apply_transform(const text_transform& transform) const
{
    auto text_cursor = editor->textCursor();
    if (!text_cursor.hasSelection()) {
        text_cursor.select(QTextCursor::Document);
    }
    const int start_pos = text_cursor.selectionStart();
    const QString selected_text = text_cursor.selectedText().replace(QChar::ParagraphSeparator, '\n');
    const std::string original_string = selected_text.toStdString();
    const auto transformed_result = transform.apply(original_string);

    text_cursor.beginEditBlock();
    for (std::size_t i = 0; i < transformed_result.size(); ++i) {
        if (original_string[i] != transformed_result[i]) {
            text_cursor.setPosition(start_pos + static_cast<int>(i));
            text_cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
            text_cursor.insertText(QString(QChar(transformed_result[i])), text_cursor.charFormat());
        }
    }
    text_cursor.endEditBlock();
}

void main_window::open_file()
{
    const auto selected_path = QFileDialog::getOpenFileName(this, "Open File");
    if (selected_path.isEmpty()) {
        return;
    }
    try {
        QFile target_file(selected_path);
        if (!target_file.exists()) {
            throw file_not_found_exception(selected_path.toStdString());
        }
        if (!target_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            throw file_read_exception(selected_path.toStdString());
        }
        QTextStream input_stream(&target_file);
        const auto file_data = input_stream.readAll();
        if (input_stream.status() != QTextStream::Ok) {
            throw file_read_exception(selected_path.toStdString());
        }
        editor->setPlainText(file_data);
        current_file = selected_path;
        update_title();
    } catch (const notepad_exception& error) {
        QMessageBox::critical(this, "Error", error.what());
    }
}

void main_window::save_file()
{
    if (current_file.isEmpty()) {
        save_file_as();
        return;
    }
    try {
        QFile target_file(current_file);
        if (!target_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            throw file_write_exception(current_file.toStdString());
        }
        QTextStream output_stream(&target_file);
        output_stream << editor->toPlainText();
    } catch (const notepad_exception& error) {
        QMessageBox::critical(this, "Error", error.what());
    }
}

void main_window::save_file_as()
{
    const auto new_path = QFileDialog::getSaveFileName(this, "Save File As");
    if (new_path.isEmpty()) {
        return;
    }
    current_file = new_path;
    save_file();
    update_title();
}

void main_window::update_title()
{
    if (current_file.isEmpty()) {
        setWindowTitle("Notepad");
    } else {
        setWindowTitle("Notepad: " + current_file);
    }
}

void main_window::show_find_replace_dialog()
{
    if (!find_replace_dlg) {
        find_replace_dlg = new QDialog(this);
        find_replace_ui = std::make_unique<Ui::find_replace_dialog>();
        find_replace_ui->setupUi(find_replace_dlg);

        auto get_search_flags = [this] {
            auto flags = QTextDocument::FindFlags();
            if (find_replace_ui->case_sensitive_check->isChecked()) {
                flags |= QTextDocument::FindCaseSensitively;
            }
            return flags;
        };

        connect(find_replace_ui->find_next_button, &QPushButton::clicked,
            find_replace_dlg, [this, get_search_flags] {
                find_next(find_replace_ui->find_input->text(), get_search_flags());
            });
        connect(find_replace_ui->replace_button, &QPushButton::clicked,
            find_replace_dlg, [this, get_search_flags] {
                replace_current(find_replace_ui->find_input->text(),
                    find_replace_ui->replace_input->text(), get_search_flags());
            });
        connect(find_replace_ui->replace_all_button, &QPushButton::clicked,
            find_replace_dlg, [this, get_search_flags] {
                replace_all(find_replace_ui->find_input->text(),
                    find_replace_ui->replace_input->text(), get_search_flags());
            });
        connect(find_replace_ui->close_button, &QPushButton::clicked,
            find_replace_dlg, [this] { find_replace_dlg->hide(); });
    }

    find_replace_dlg->show();
    find_replace_dlg->raise();
    find_replace_dlg->activateWindow();
}

void main_window::find_next(const QString& search_term, const QTextDocument::FindFlags search_flags) const
{
    if (search_term.isEmpty()) {
        return;
    }
    auto search_result = editor->document()->find(search_term, editor->textCursor(), search_flags);
    if (search_result.isNull()) {
        auto start_cursor = editor->textCursor();
        start_cursor.movePosition(QTextCursor::Start);
        search_result = editor->document()->find(search_term, start_cursor, search_flags);
    }
    if (!search_result.isNull()) {
        editor->setTextCursor(search_result);
    }
}

void main_window::replace_current(const QString& search_term, const QString& replacement_string,
    const QTextDocument::FindFlags search_flags) const
{
    if (auto active_cursor = editor->textCursor(); active_cursor.hasSelection()) {
        active_cursor.insertText(replacement_string);
        editor->setTextCursor(active_cursor);
    }
    find_next(search_term, search_flags);
}

void main_window::replace_all(const QString& search_term, const QString& replacement_string,
    const QTextDocument::FindFlags search_flags) const
{
    if (search_term.isEmpty()) {
        return;
    }
    auto move_cursor = editor->textCursor();
    move_cursor.movePosition(QTextCursor::Start);
    editor->setTextCursor(move_cursor);

    while (true) {
        const auto match = editor->document()->find(search_term, editor->textCursor(), search_flags);
        if (match.isNull()) {
            break;
        }
        editor->setTextCursor(match);
        auto edit_cursor = editor->textCursor();
        edit_cursor.insertText(replacement_string);
        editor->setTextCursor(edit_cursor);
    }
}

void main_window::show_word_frequency()
{
    const auto full_text = editor->toPlainText().toLower().toStdString();

    std::map<std::string, int> word_map;
    std::istringstream text_stream(full_text);
    std::string word_buffer;
    while (text_stream >> word_buffer) {
        std::erase_if(word_buffer, [](const unsigned char ch) {
            return !std::isalpha(ch);
        });
        if (!word_buffer.empty()) {
            ++word_map[word_buffer];
        }
    }

    std::vector<std::pair<std::string, int>> sorted_list(word_map.begin(), word_map.end());
    std::sort(sorted_list.begin(), sorted_list.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    auto* freq_dialog = new QDialog(this);
    freq_dialog->setAttribute(Qt::WA_DeleteOnClose);
    Ui::word_frequency_dialog freq_ui;
    freq_ui.setupUi(freq_dialog);

    freq_ui.frequency_table->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    freq_ui.frequency_table->horizontalHeaderItem(1)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    freq_ui.frequency_table->setRowCount(static_cast<int>(sorted_list.size()));
    for (int i = 0; i < static_cast<int>(sorted_list.size()); ++i) {
        const auto& [word, count] = sorted_list[i];
        freq_ui.frequency_table->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(word)));
        auto* item_count = new QTableWidgetItem(QString::number(count));
        item_count->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        freq_ui.frequency_table->setItem(i, 1, item_count);
    }
    freq_ui.frequency_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    freq_ui.frequency_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    freq_dialog->exec();
}