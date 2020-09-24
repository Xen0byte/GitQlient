#include "FileDiffWidget.h"

#include <GitBase.h>
#include <GitHistory.h>
#include <FileDiffView.h>
#include <CommitInfo.h>
#include <GitCache.h>
#include <GitQlientSettings.h>
#include <CheckBox.h>
#include <FileEditor.h>
#include <GitLocal.h>
#include <DiffHelper.h>
#include <LineNumberArea.h>

#include <QLineEdit>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollBar>
#include <QDateTime>
#include <QStackedWidget>
#include <QMessageBox>

FileDiffWidget::FileDiffWidget(const QSharedPointer<GitBase> &git, QSharedPointer<GitCache> cache, QWidget *parent)
   : IDiffWidget(git, cache, parent)
   , mBack(new QPushButton())
   , mGoPrevious(new QPushButton())
   , mGoNext(new QPushButton())
   , mEdition(new QPushButton())
   , mFullView(new QPushButton())
   , mSplitView(new QPushButton())
   , mSave(new QPushButton())
   , mStage(new QPushButton())
   , mRevert(new QPushButton())
   , mFileNameLabel(new QLabel())
   , mTitleFrame(new QFrame())
   , mNewFile(new FileDiffView())
   , mOldFile(new FileDiffView())
   , mFileEditor(new FileEditor())
   , mViewStackedWidget(new QStackedWidget())
{
   mNewFile->addNumberArea(new LineNumberArea(mNewFile));
   mOldFile->addNumberArea(new LineNumberArea(mOldFile));

   mNewFile->setObjectName("newFile");
   mOldFile->setObjectName("oldFile");

   const auto optionsLayout = new QHBoxLayout();
   optionsLayout->setContentsMargins(5, 5, 0, 0);
   optionsLayout->setSpacing(5);
   optionsLayout->addWidget(mBack);
   optionsLayout->addWidget(mGoPrevious);
   optionsLayout->addWidget(mGoNext);
   optionsLayout->addWidget(mFullView);
   optionsLayout->addWidget(mSplitView);
   optionsLayout->addWidget(mEdition);
   optionsLayout->addWidget(mSave);
   optionsLayout->addWidget(mStage);
   optionsLayout->addWidget(mRevert);
   optionsLayout->addStretch();

   const auto searchNew = new QLineEdit();
   searchNew->setObjectName("SearchInput");
   searchNew->setPlaceholderText(tr("Press Enter to search a text... "));
   connect(searchNew, &QLineEdit::editingFinished, this,
           [this, searchNew]() { DiffHelper::findString(searchNew->text(), mNewFile, this); });

   const auto newFileLayout = new QVBoxLayout();
   newFileLayout->setContentsMargins(QMargins());
   newFileLayout->setSpacing(5);
   newFileLayout->addWidget(searchNew);
   newFileLayout->addWidget(mNewFile);

   const auto searchOld = new QLineEdit();
   searchOld->setPlaceholderText(tr("Press Enter to search a text... "));
   searchOld->setObjectName("SearchInput");
   connect(searchOld, &QLineEdit::editingFinished, this,
           [this, searchOld]() { DiffHelper::findString(searchOld->text(), mNewFile, this); });

   const auto oldFileLayout = new QVBoxLayout();
   oldFileLayout->setContentsMargins(QMargins());
   oldFileLayout->setSpacing(5);
   oldFileLayout->addWidget(searchOld);
   oldFileLayout->addWidget(mOldFile);

   const auto diffLayout = new QHBoxLayout();
   diffLayout->setContentsMargins(10, 0, 10, 0);
   diffLayout->addLayout(newFileLayout);
   diffLayout->addLayout(oldFileLayout);

   const auto diffFrame = new QFrame();
   diffFrame->setLayout(diffLayout);

   mViewStackedWidget->addWidget(diffFrame);
   mViewStackedWidget->addWidget(mFileEditor);

   mTitleFrame->setObjectName("fileTitleFrame");
   mTitleFrame->setVisible(false);

   const auto titleLayout = new QHBoxLayout(mTitleFrame);
   titleLayout->setContentsMargins(0, 10, 0, 10);
   titleLayout->setSpacing(0);
   titleLayout->addStretch();
   titleLayout->addWidget(mFileNameLabel);
   titleLayout->addStretch();

   const auto vLayout = new QVBoxLayout(this);
   vLayout->setContentsMargins(QMargins());
   vLayout->setSpacing(5);
   vLayout->addWidget(mTitleFrame);
   vLayout->addLayout(optionsLayout);
   vLayout->addWidget(mViewStackedWidget);

   GitQlientSettings settings;
   mFileVsFile
       = settings.localValue(mGit->getGitQlientSettingsDir(), GitQlientSettings::SplitFileDiffView, false).toBool();

   mBack->setIcon(QIcon(":/icons/back"));
   mBack->setToolTip(tr("Return to the view"));
   connect(mBack, &QPushButton::clicked, this, &FileDiffWidget::exitRequested);

   mGoPrevious->setIcon(QIcon(":/icons/arrow_up"));
   mGoPrevious->setToolTip(tr("Previous change"));
   connect(mGoPrevious, &QPushButton::clicked, this, &FileDiffWidget::moveChunkUp);

   mGoNext->setToolTip(tr("Next change"));
   mGoNext->setIcon(QIcon(":/icons/arrow_down"));
   connect(mGoNext, &QPushButton::clicked, this, &FileDiffWidget::moveChunkDown);

   mEdition->setIcon(QIcon(":/icons/edit"));
   mEdition->setCheckable(true);
   mEdition->setToolTip(tr("Edit file"));
   connect(mEdition, &QPushButton::toggled, this, &FileDiffWidget::enterEditionMode);

   mFullView->setIcon(QIcon(":/icons/text-file"));
   mFullView->setCheckable(true);
   mFullView->setToolTip(tr("Full file view"));
   connect(mFullView, &QPushButton::toggled, this, &FileDiffWidget::setFullViewEnabled);

   mSplitView->setIcon(QIcon(":/icons/split_view"));
   mSplitView->setCheckable(true);
   mSplitView->setToolTip(tr("Split file view"));
   connect(mSplitView, &QPushButton::toggled, this, &FileDiffWidget::setSplitViewEnabled);

   mSave->setIcon(QIcon(":/icons/save"));
   mSave->setDisabled(true);
   mSave->setToolTip(tr("Save"));
   connect(mSave, &QPushButton::clicked, mFileEditor, &FileEditor::saveFile);
   connect(mSave, &QPushButton::clicked, mEdition, &QPushButton::toggle);

   mStage->setIcon(QIcon(":/icons/staged"));
   mStage->setToolTip(tr("Stage file"));
   connect(mStage, &QPushButton::clicked, this, &FileDiffWidget::stageFile);

   mRevert->setIcon(QIcon(":/icons/close"));
   mRevert->setToolTip(tr("Revert changes"));
   connect(mRevert, &QPushButton::clicked, this, &FileDiffWidget::revertFile);

   mViewStackedWidget->setCurrentIndex(0);

   if (!mFileVsFile)
      mOldFile->setHidden(true);

   connect(mNewFile, &FileDiffView::signalScrollChanged, mOldFile, &FileDiffView::moveScrollBarToPos);
   connect(mOldFile, &FileDiffView::signalScrollChanged, mNewFile, &FileDiffView::moveScrollBarToPos);

   setAttribute(Qt::WA_DeleteOnClose);
}

void FileDiffWidget::clear()
{
   mNewFile->clear();
}

bool FileDiffWidget::reload()
{
   if (mCurrentSha == CommitInfo::ZERO_SHA)
      return configure(mCurrentSha, mPreviousSha, mCurrentFile, mEdition->isChecked());

   return false;
}

bool FileDiffWidget::configure(const QString &currentSha, const QString &previousSha, const QString &file,
                               bool editMode)
{

   mFileNameLabel->setText(file);

   const auto isWip = currentSha == CommitInfo::ZERO_SHA;
   mBack->setVisible(isWip);
   mEdition->setVisible(isWip);
   mSave->setVisible(isWip);
   mStage->setVisible(isWip);
   mRevert->setVisible(isWip);
   mTitleFrame->setVisible(isWip);

   mCurrentFile = file;
   mCurrentSha = currentSha;
   mPreviousSha = previousSha;

   auto destFile = file;

   if (destFile.contains("-->"))
      destFile = destFile.split("--> ").last().split("(").first().trimmed();

   QScopedPointer<GitHistory> git(new GitHistory(mGit));
   auto text = git->getFileDiff(currentSha == CommitInfo::ZERO_SHA ? QString() : currentSha, previousSha, destFile);

   auto pos = 0;
   for (auto i = 0; i < 5; ++i)
      pos = text.indexOf("\n", pos + 1);

   text = text.mid(pos + 1);

   if (!text.isEmpty())
   {
      QPair<QStringList, QVector<DiffInfo::ChunkInfo>> oldData;
      QPair<QStringList, QVector<DiffInfo::ChunkInfo>> newData;

      mChunks = DiffHelper::processDiff(text, mFileVsFile, newData, oldData);

      mOldFile->blockSignals(true);
      mOldFile->loadDiff(oldData.first.join('\n'), oldData.second);
      mOldFile->blockSignals(false);

      mNewFile->blockSignals(true);
      mNewFile->loadDiff(newData.first.join('\n'), newData.second);
      mNewFile->blockSignals(false);

      GitQlientSettings settings;
      mFileVsFile
          = settings.localValue(mGit->getGitQlientSettingsDir(), GitQlientSettings::SplitFileDiffView, false).toBool();

      if (editMode)
      {
         mEdition->setChecked(true);
         mSave->setEnabled(true);
      }
      else
      {
         mEdition->setChecked(false);
         mSave->setDisabled(true);
         mFullView->setChecked(!mFileVsFile);
         mSplitView->setChecked(mFileVsFile);
      }

      return true;
   }

   return false;
}

void FileDiffWidget::setSplitViewEnabled(bool enable)
{
   mFileVsFile = enable;

   mOldFile->setVisible(mFileVsFile);

   GitQlientSettings settings;
   settings.setLocalValue(mGit->getGitQlientSettingsDir(), GitQlientSettings::SplitFileDiffView, mFileVsFile);

   configure(mCurrentSha, mPreviousSha, mCurrentFile);

   mFullView->blockSignals(true);
   mFullView->setChecked(!mFileVsFile);
   mFullView->blockSignals(false);

   mGoNext->setEnabled(true);
   mGoPrevious->setEnabled(true);

   if (enable)
   {
      mSave->setDisabled(true);
      mEdition->blockSignals(true);
      mEdition->setChecked(false);
      mEdition->blockSignals(false);
      endEditFile();
   }
}

void FileDiffWidget::setFullViewEnabled(bool enable)
{
   mFileVsFile = !enable;

   mOldFile->setVisible(mFileVsFile);

   GitQlientSettings settings;
   settings.setLocalValue(mGit->getGitQlientSettingsDir(), GitQlientSettings::SplitFileDiffView, mFileVsFile);

   configure(mCurrentSha, mPreviousSha, mCurrentFile);

   mSplitView->blockSignals(true);
   mSplitView->setChecked(mFileVsFile);
   mSplitView->blockSignals(false);

   mGoNext->setDisabled(true);
   mGoPrevious->setDisabled(true);

   if (enable)
   {
      mSave->setDisabled(true);
      mEdition->blockSignals(true);
      mEdition->setChecked(false);
      mEdition->blockSignals(false);
      endEditFile();
   }
}

void FileDiffWidget::hideBackButton() const
{
   mBack->setVisible(true);
}

void FileDiffWidget::moveChunkUp()
{
   for (auto i = mChunks.count() - 1; i >= 0; --i)
   {
      if (auto chunkStart = mChunks.at(i).startLine; chunkStart < mCurrentChunkLine)
      {
         mCurrentChunkLine = chunkStart;

         mNewFile->moveScrollBarToPos(mCurrentChunkLine - 1);
         mOldFile->moveScrollBarToPos(mCurrentChunkLine - 1);

         break;
      }
   }
}

void FileDiffWidget::moveChunkDown()
{
   const auto endIter = mChunks.constEnd();
   auto iter = mChunks.constBegin();

   for (; iter != endIter; ++iter)
      if (iter->startLine > mCurrentChunkLine)
         break;

   if (iter != endIter)
   {
      mCurrentChunkLine = iter->startLine;

      mNewFile->moveScrollBarToPos(mCurrentChunkLine - 1);
      mOldFile->moveScrollBarToPos(mCurrentChunkLine - 1);
   }
}

void FileDiffWidget::enterEditionMode(bool enter)
{
   if (enter)
   {
      mSave->setEnabled(true);
      mSplitView->blockSignals(true);
      mSplitView->setChecked(!enter);
      mSplitView->blockSignals(false);

      mFullView->blockSignals(true);
      mFullView->setChecked(!enter);
      mFullView->blockSignals(false);

      mFileEditor->editFile(mCurrentFile);
      mViewStackedWidget->setCurrentIndex(1);
   }
   else if (mFileVsFile)
      setSplitViewEnabled(true);
   else
      setFullViewEnabled(true);
}

void FileDiffWidget::endEditFile()
{
   mViewStackedWidget->setCurrentIndex(0);
}

void FileDiffWidget::stageFile()
{
   QScopedPointer<GitLocal> git(new GitLocal(mGit));
   const auto ret = git->markFileAsResolved(mCurrentFile);

   if (ret.success)
   {
      emit fileStaged(mCurrentFile);
      emit exitRequested();
   }
}

void FileDiffWidget::revertFile()
{
   const auto ret = QMessageBox::warning(
       this, tr("Revert all changes"),
       tr("Please, take into account that this will revert all the changes you have performed so far."),
       QMessageBox::Ok, QMessageBox::Cancel);

   if (ret == QMessageBox::Ok)
   {
      QScopedPointer<GitLocal> git(new GitLocal(mGit));
      const auto ret = git->checkoutFile(mCurrentFile);

      if (ret)
      {
         emit fileReverted(mCurrentFile);
         emit exitRequested();
      }
   }
}
