#include "spell/SpellChecker.h"
#include "spell/SpellUtils.h"
#include "ui/SpellCheckLineEdit.h"
#include "ui/ThemeColors.h"

#include <QAction>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QMenu>
#include <QPixmap>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest/QtTest>

// Tokenization always works; the Hunspell-dependent cases self-skip on builds
// without the library (or without a loadable dictionary).
class TestSpellCheck : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void tokenizer_extractsWords();
    void tokenizer_skipsCodesAndAcronyms();
    void discovery_findsAndDedupes();
    void choose_prefersExactThenPrefix();
    void spellChecker_loadsAndChecks();
    void spellChecker_addWord();
    void lineEdit_marksAndReplaces();
    void lineEdit_contextMenuOffersSpelling();
    void lineEdit_comboFallbackShowsMenu();
    void lineEdit_paintsSquiggle();

private:
    QString writeDictionary(const QString &dir, const QString &base);

    QTemporaryDir m_tmp;
    QString m_aff;
    QString m_dic;
};

QString TestSpellCheck::writeDictionary(const QString &dir, const QString &base) {
    QDir().mkpath(dir);
    QFile aff(dir + "/" + base + ".aff");
    if (aff.open(QIODevice::WriteOnly | QIODevice::Text))
        aff.write("SET UTF-8\nTRY esianrtolcdugmphbyfvkwz'\n");
    aff.close();
    QFile dic(dir + "/" + base + ".dic");
    if (dic.open(QIODevice::WriteOnly | QIODevice::Text))
        dic.write("6\nhello\nworld\nthe\nquick\nbrown\nspelling\n");
    dic.close();
    return dir + "/" + base + ".aff";
}

void TestSpellCheck::initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_tmp.isValid());
    const QString dir = m_tmp.path() + "/dict";
    m_aff = writeDictionary(dir, "xx_XX");
    m_dic = dir + "/xx_XX.dic";
}

void TestSpellCheck::tokenizer_extractsWords() {
    const QString text = QStringLiteral("Fix, the bug! and don't run.");
    const QVector<SpellUtils::WordSpan> spans = SpellUtils::words(text);
    QStringList found;
    for (const SpellUtils::WordSpan &s : spans)
        found << text.mid(s.start, s.length);
    QCOMPARE(found, (QStringList{"Fix", "the", "bug", "and", "don't", "run"}));
}

void TestSpellCheck::tokenizer_skipsCodesAndAcronyms() {
    const QString text = QStringLiteral("a I 42 0xDEADBEEF API foo_bar baz getElementById");
    const QVector<SpellUtils::WordSpan> spans = SpellUtils::words(text);
    QStringList found;
    for (const SpellUtils::WordSpan &s : spans)
        found << text.mid(s.start, s.length);
    QCOMPARE(found, (QStringList{"foo", "bar", "baz"}));
}

void TestSpellCheck::discovery_findsAndDedupes() {
    const QString dir = m_tmp.path() + "/discover";
    writeDictionary(dir, "yy_YY");
    // Hyphenation files share the .dic extension but are not dictionaries.
    QFile hyph(dir + "/hyph_yy_YY.dic");
    QVERIFY(hyph.open(QIODevice::WriteOnly | QIODevice::Text));
    hyph.write("2\nfoo\nbar\n");
    hyph.close();

    const QVector<SpellUtils::Dictionary> found =
        SpellUtils::discoverDictionaries({dir});
    QCOMPARE(found.size(), 1);
    QCOMPARE(found.first().language, QStringLiteral("yy_YY"));
    QVERIFY(found.first().isValid());
}

void TestSpellCheck::choose_prefersExactThenPrefix() {
    const QVector<SpellUtils::Dictionary> available = {
        {"en_GB", "/a/en_GB.aff", "/a/en_GB.dic"},
        {"en_US", "/a/en_US.aff", "/a/en_US.dic"},
        {"de_DE", "/a/de_DE.aff", "/a/de_DE.dic"},
    };
    QCOMPARE(SpellUtils::chooseDictionary(available, "en_US").language,
             QStringLiteral("en_US"));
    QCOMPARE(SpellUtils::chooseDictionary(available, "de").language,
             QStringLiteral("de_DE"));
    QCOMPARE(SpellUtils::chooseDictionary(available, "").language,
             available.first().language);
    QCOMPARE(SpellUtils::chooseDictionary({}, "en").isValid(), false);
}

void TestSpellCheck::spellChecker_loadsAndChecks() {
    SpellChecker checker;
    if (!checker.load(m_aff, m_dic))
        QSKIP("Hunspell / dictionary unavailable");
    QVERIFY(checker.isLoaded());
    QCOMPARE(checker.language(), QStringLiteral("xx_XX"));
    QVERIFY(checker.spell("hello"));
    QVERIFY(!checker.spell("helo"));
    QVERIFY(!checker.spell("helllo"));
}

void TestSpellCheck::spellChecker_addWord() {
    SpellChecker checker;
    if (!checker.load(m_aff, m_dic))
        QSKIP("Hunspell / dictionary unavailable");
    QVERIFY(!checker.spell("versionary"));
    checker.addWord("versionary");
    QVERIFY(checker.spell("versionary"));
}

void TestSpellCheck::lineEdit_marksAndReplaces() {
    SpellCheckLineEdit edit;
    if (!edit.checker()->load(m_aff, m_dic))
        QSKIP("Hunspell / dictionary unavailable");
    edit.setSpellCheckingEnabled(true);
    edit.setText(QStringLiteral("teh hello"));
    edit.invalidateSpelling();

    const QVector<SpellUtils::WordSpan> spans = edit.misspelledSpans();
    QCOMPARE(spans.size(), 1);
    QCOMPARE(edit.text().mid(spans.first().start, spans.first().length),
             QStringLiteral("teh"));
    QCOMPARE(edit.misspelledWordAtIndex(0), 0);
    QVERIFY(edit.misspelledWordAtIndex(1) >= 0);

    QVERIFY(edit.replaceMisspelled(0, QStringLiteral("the")));
    QCOMPARE(edit.text(), QStringLiteral("the hello"));
    QVERIFY(edit.misspelledSpans().isEmpty());

    // Disabling clears the underline cache.
    edit.setSpellCheckingEnabled(false);
    edit.setText(QStringLiteral("teh"));
    edit.invalidateSpelling();
    QVERIFY(edit.misspelledSpans().isEmpty());
}

void TestSpellCheck::lineEdit_contextMenuOffersSpelling() {
    SpellCheckLineEdit edit;
    edit.resize(200, 24);
    edit.setText(QStringLiteral("hello"));
    const QPoint local(4, 10);
    QContextMenuEvent event(QContextMenuEvent::Mouse, local, edit.mapToGlobal(local));
    QVERIFY(QCoreApplication::sendEvent(&edit, &event));

    QMenu *menu = nullptr;
    for (QMenu *candidate : edit.findChildren<QMenu *>()) {
        if (candidate->parent() == &edit) {
            menu = candidate;
            break;
        }
    }
    QVERIFY(menu != nullptr);
    bool hasToggle = false;
    bool hasLanguage = false;
    for (QAction *action : menu->actions()) {
        if (action->text() == QStringLiteral("Check spelling"))
            hasToggle = true;
        if (action->menu() && action->text() == QStringLiteral("Spelling language"))
            hasLanguage = true;
    }
    QVERIFY(hasToggle);
    QVERIFY(hasLanguage);
    delete menu;
}

void TestSpellCheck::lineEdit_comboFallbackShowsMenu() {
    // QComboBox::setLineEdit() forces NoContextMenu on the line edit; the
    // widget must still surface its spelling menu on right-click.
    QComboBox combo;
    combo.setEditable(true);
    auto *edit = new SpellCheckLineEdit(&combo);
    combo.setLineEdit(edit);
    QCOMPARE(edit->contextMenuPolicy(), Qt::NoContextMenu);

    const QPoint local(4, 10);
    QContextMenuEvent event(QContextMenuEvent::Mouse, local, edit->mapToGlobal(local));
    QVERIFY(QCoreApplication::sendEvent(edit, &event));

    QMenu *menu = nullptr;
    for (QMenu *candidate : edit->findChildren<QMenu *>()) {
        if (candidate->parent() == edit) {
            menu = candidate;
            break;
        }
    }
    QVERIFY(menu != nullptr);
    delete menu;
}

void TestSpellCheck::lineEdit_paintsSquiggle() {
    SpellCheckLineEdit edit;
    if (!edit.checker()->load(m_aff, m_dic))
        QSKIP("Hunspell / dictionary unavailable");
    edit.setSpellCheckingEnabled(true);
    edit.setText(QStringLiteral("teh hello"));
    edit.resize(160, 24);
    edit.show();
    QVERIFY(QTest::qWaitForWindowExposed(&edit));

    QPixmap pixmap(edit.size());
    pixmap.fill(Qt::white);
    edit.render(&pixmap);
    const QImage image = pixmap.toImage();

    // The squiggle is red; count reddish pixels (antialiasing blends them, so
    // look for a clear red dominance rather than an exact colour match).
    int reddish = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor c = image.pixelColor(x, y);
            if (c.red() > 140 && c.red() > c.green() + 60 && c.red() > c.blue() + 60)
                ++reddish;
        }
    }
    QVERIFY(reddish > 3);
    edit.hide();
}

QTEST_MAIN(TestSpellCheck)
#include "test_spellcheck.moc"
