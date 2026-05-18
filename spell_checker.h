#ifndef SPELL_CHECKER_H
#define SPELL_CHECKER_H

#include <QChar>
#include <QDebug>
#include <QString>
#include <QVector>
#include <algorithm>
#include <fstream>
#include <set>
#include <string>
#include <vector>

class spell_checker {
public:
    explicit spell_checker(const std::string& dict_path)
    {
        std::ifstream file(dict_path);
        if (!file.is_open()) {
            qWarning() << "spell_checker: cannot open dictionary:"
                       << QString::fromStdString(dict_path);
            return;
        }
        std::string word;
        while (std::getline(file, word)) {
            // Strip Windows-style \r if present.
            if (!word.empty() && word.back() == '\r') {
                word.pop_back();
            }
            if (!word.empty()) {
                word_set.insert(word);
            }
        }
        qDebug() << "spell_checker: loaded" << word_set.size()
                 << "words from" << QString::fromStdString(dict_path);
    }

           // Returns true if the dictionary was loaded successfully.
    [[nodiscard]] bool loaded() const { return !word_set.empty(); }

           // Returns true when the word is spelled correctly.
    [[nodiscard]] bool is_correct(const QString& word) const
    {
        if (word_set.empty()) {
            return true; // No dictionary — don't underline anything.
        }
        const std::string cleaned = clean(word);
        if (cleaned.size() <= 1) {
            return true; // Single letters are always correct.
        }
        return word_set.count(cleaned) > 0;
    }

           // Returns up to max_count spelling suggestions (edit distance <= 2).
    [[nodiscard]] QVector<QString> suggestions(const QString& word, int max_count = 5) const
    {
        const std::string target = clean(word);
        if (target.empty()) {
            return {};
        }

        std::vector<std::pair<int, std::string>> candidates;
        for (const auto& dict_word : word_set) {
            const int len_diff = static_cast<int>(dict_word.size())
            - static_cast<int>(target.size());
            if (std::abs(len_diff) > 2) {
                continue;
            }
            const int dist = edit_distance(target, dict_word);
            if (dist <= 2) {
                candidates.emplace_back(dist, dict_word);
            }
            if (static_cast<int>(candidates.size()) >= max_count * 20) {
                break;
            }
        }

        std::sort(candidates.begin(), candidates.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        QVector<QString> result;
        const int limit = std::min(max_count, static_cast<int>(candidates.size()));
        result.reserve(limit);
        for (int i = 0; i < limit; ++i) {
            result.append(QString::fromStdString(candidates[i].second));
        }
        return result;
    }

private:
    std::set<std::string> word_set;

           // Lowercases and keeps only ASCII letters.
    [[nodiscard]] static std::string clean(const QString& word)
    {
        std::string result;
        result.reserve(static_cast<std::size_t>(word.size()));
        for (const QChar ch : word) {
            if (ch.isLetter() && ch.unicode() < 128) {
                result += static_cast<char>(ch.toLower().toLatin1());
            }
        }
        return result;
    }

           // Classic two-row Levenshtein distance.
    [[nodiscard]] static int edit_distance(const std::string& a, const std::string& b)
    {
        const int m = static_cast<int>(a.size());
        const int n = static_cast<int>(b.size());
        std::vector<int> prev(n + 1);
        std::vector<int> curr(n + 1);
        for (int j = 0; j <= n; ++j) {
            prev[j] = j;
        }
        for (int i = 1; i <= m; ++i) {
            curr[0] = i;
            for (int j = 1; j <= n; ++j) {
                if (a[i - 1] == b[j - 1]) {
                    curr[j] = prev[j - 1];
                } else {
                    curr[j] = 1 + std::min({ prev[j], curr[j - 1], prev[j - 1] });
                }
            }
            std::swap(prev, curr);
        }
        return prev[n];
    }
};

#endif // SPELL_CHECKER_H